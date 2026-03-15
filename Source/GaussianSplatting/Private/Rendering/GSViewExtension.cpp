// Copyright 2024, Gaussian Splatting Plugin. All Rights Reserved.

#include "Rendering/GSViewExtension.h"
#include "Rendering/GSSceneProxy.h"
#include "Rendering/GSGlobalShaders.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "SceneView.h"
#include "PostProcess/SceneRenderTargets.h"
#include "ScreenPass.h"
#include "RHIStaticStates.h"
#include "PipelineStateCache.h"
#include "CommonRenderResources.h"

TSharedPtr<FGSViewExtension, ESPMode::ThreadSafe> FGSViewExtension::SingletonPtr;

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------

FGSViewExtension& FGSViewExtension::Get()
{
	if (!SingletonPtr.IsValid())
	{
		SingletonPtr = FSceneViewExtensions::NewExtension<FGSViewExtension>();
	}
	return *SingletonPtr;
}

FGSViewExtension::FGSViewExtension(const FAutoRegister& AutoRegister)
	: FSceneViewExtensionBase(AutoRegister)
{}

FGSViewExtension::~FGSViewExtension()
{
	QuadIndexBuffer.SafeRelease();
}

// ---------------------------------------------------------------------------
// Proxy registry (render thread)
// ---------------------------------------------------------------------------

void FGSViewExtension::RegisterProxy(FGSSceneProxy* Proxy)
{
	check(IsInRenderingThread() || IsInGameThread());
	ENQUEUE_RENDER_COMMAND(GS_RegisterProxy)([this, Proxy](FRHICommandListImmediate&)
	{
		Proxies.AddUnique(Proxy);
	});
}

void FGSViewExtension::UnregisterProxy(FGSSceneProxy* Proxy)
{
	ENQUEUE_RENDER_COMMAND(GS_UnregisterProxy)([this, Proxy](FRHICommandListImmediate&)
	{
		Proxies.Remove(Proxy);
	});
}

// ---------------------------------------------------------------------------
// EnsureQuadIndexBuffer
// ---------------------------------------------------------------------------

void FGSViewExtension::EnsureQuadIndexBuffer()
{
	if (QuadIndexBuffer.IsValid()) return;

	// [0,1,2, 0,2,3]  – two CCW triangles forming a quad
	uint16 Indices[] = { 0, 1, 2, 0, 2, 3 };
	FRHIResourceCreateInfo CI(TEXT("GS_QuadIB"));
	QuadIndexBuffer = RHICreateIndexBuffer(sizeof(uint16), sizeof(Indices),
	                                        BUF_Static, CI);
	void* Data = RHILockBuffer(QuadIndexBuffer, 0, sizeof(Indices), RLM_WriteOnly);
	FMemory::Memcpy(Data, Indices, sizeof(Indices));
	RHIUnlockBuffer(QuadIndexBuffer);
}

// ---------------------------------------------------------------------------
// PostRenderView_RenderThread
// ---------------------------------------------------------------------------

void FGSViewExtension::PostRenderView_RenderThread(
	FRDGBuilder& GraphBuilder,
	FSceneView&  InView)
{
	check(IsInRenderingThread());
	if (Proxies.IsEmpty()) return;

	EnsureQuadIndexBuffer();

	if (InView.bIsSceneCapture || !InView.Family)
	{
		return; // skip scene captures
	}

	// Retrieve scene colour from FSceneRenderTargets (UE5 approach)
	FSceneRenderTargets& SceneRT = FSceneRenderTargets::Get(GraphBuilder.RHICmdList);
	if (!SceneRT.GetSceneColor())
	{
		return;
	}
	FRDGTextureRef SceneColorRT = GraphBuilder.RegisterExternalTexture(
		SceneRT.GetSceneColor(), TEXT("GS_SceneColor"));

	// Sort and render each proxy
	for (FGSSceneProxy* Proxy : Proxies)
	{
		if (!Proxy || Proxy->NumSplats == 0) continue;
		if (!Proxy->SplatBufferSRV.IsValid())  continue;

		// Update CPU sort (sorted mode only)
		if (Proxy->RenderMode == EGSRenderMode::DepthSorted)
		{
			Proxy->UpdateSortOrder_RenderThread(InView.ViewMatrices.GetViewOrigin());
		}

		RenderProxy_RenderThread(GraphBuilder, InView, Proxy, SceneColorRT);
	}
}

// ---------------------------------------------------------------------------
// RenderProxy_RenderThread
// ---------------------------------------------------------------------------

void FGSViewExtension::RenderProxy_RenderThread(
	FRDGBuilder&     GraphBuilder,
	const FSceneView& View,
	FGSSceneProxy*   Proxy,
	FRDGTextureRef   SceneColorRT)
{
	if (Proxy->RenderMode == EGSRenderMode::DepthAwareOIT)
	{
		RenderProxy_OIT(GraphBuilder, View, Proxy, SceneColorRT);
	}
	else
	{
		RenderProxy_Sorted(GraphBuilder, View, Proxy, SceneColorRT);
	}
}

// ---------------------------------------------------------------------------
// Helper: build common VS parameters
// ---------------------------------------------------------------------------

static void FillVSParams(
	const FSceneView& View,
	FGSSceneProxy*    Proxy,
	auto&             Params)
{
	const FViewMatrices& VM = View.ViewMatrices;

	Params.GS_WorldToView    = FMatrix44f(VM.GetViewMatrix());
	Params.GS_ViewToClip     = FMatrix44f(VM.GetProjectionMatrix());
	Params.GS_WorldToClip    = FMatrix44f(VM.GetViewProjectionMatrix());
	Params.GS_CameraWorldPos = FVector3f(VM.GetViewOrigin());

	FIntRect ViewRect = View.UnscaledViewRect;
	Params.GS_ViewportSize = FVector2f((float)ViewRect.Width(), (float)ViewRect.Height());

	// Focal lengths from projection matrix (px = fx = (M[0][0] * W/2))
	float HalfW = (float)ViewRect.Width()  * 0.5f;
	float HalfH = (float)ViewRect.Height() * 0.5f;
	Params.GS_FocalLength = FVector2f(
		VM.GetProjectionMatrix().M[0][0] * HalfW,
		VM.GetProjectionMatrix().M[1][1] * HalfH);

	Params.GS_SHDegree  = Proxy->SHDegree;
	Params.SplatBuffer  = Proxy->SplatBufferSRV;
	Params.OrderBuffer  = Proxy->OrderBufferSRV;
}

// ---------------------------------------------------------------------------
// RenderProxy_Sorted – standard back-to-front alpha blending
// ---------------------------------------------------------------------------

void FGSViewExtension::RenderProxy_Sorted(
	FRDGBuilder&     GraphBuilder,
	const FSceneView& View,
	FGSSceneProxy*   Proxy,
	FRDGTextureRef   SceneColorRT)
{
	TShaderMapRef<FGSSplatVS> VS(View.ShaderMap);
	TShaderMapRef<FGSSplatPS> PS(View.ShaderMap);

	struct FPassParams
	{
		FGSSplatPS::FParameters PS;
		FGSSplatVS::FParameters VS;
	};
	FPassParams* PassParams = GraphBuilder.AllocParameters<FPassParams>();
	FillVSParams(View, Proxy, PassParams->VS);
	PassParams->PS.RenderTargets[0] = FRenderTargetBinding(SceneColorRT, ERenderTargetLoadAction::ELoad);

	const int32 NumInstances = Proxy->NumSplats;
	FBufferRHIRef LocalQuadIB = QuadIndexBuffer;

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("GaussianSplatting::Sorted (%d splats)", NumInstances),
		PassParams,
		ERDGPassFlags::Raster,
		[VS, PS, PassParams, NumInstances, LocalQuadIB, &View](FRHICommandList& RHICmdList)
		{
			// Setup PSO
			FGraphicsPipelineStateInitializer PSOInit;
			RHICmdList.ApplyCachedRenderTargets(PSOInit);

			// Premultiplied-alpha blending: dst.rgb = src.rgb + dst.rgb*(1-src.a)
			PSOInit.BlendState = TStaticBlendState<
				CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha,
				BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI();
			PSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			PSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			PSOInit.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
			PSOInit.BoundShaderState.VertexShaderRHI = VS.GetVertexShader();
			PSOInit.BoundShaderState.PixelShaderRHI  = PS.GetPixelShader();
			PSOInit.PrimitiveType = PT_TriangleList;

			SetGraphicsPipelineState(RHICmdList, PSOInit, 0);
			SetShaderParameters(RHICmdList, VS, VS.GetVertexShader(), PassParams->VS);

			RHICmdList.SetStreamSource(0, nullptr, 0);
			// Draw: 6 indices per instance (2 triangles), NumInstances Gaussians
			RHICmdList.DrawIndexedPrimitive(LocalQuadIB,
				0,          // BaseVertexIndex
				0,          // MinIndex
				4,          // NumVertices (per instance)
				0,          // StartIndex
				2,          // NumPrimitives (2 triangles)
				NumInstances);
		});
}

// ---------------------------------------------------------------------------
// RenderProxy_OIT – Mobile-GS Depth-Aware OIT
// ---------------------------------------------------------------------------

void FGSViewExtension::RenderProxy_OIT(
	FRDGBuilder&     GraphBuilder,
	const FSceneView& View,
	FGSSceneProxy*   Proxy,
	FRDGTextureRef   SceneColorRT)
{
	FIntPoint ViewSize(View.UnscaledViewRect.Width(), View.UnscaledViewRect.Height());

	// Create OIT accumulation textures
	FRDGTextureDesc AccumDesc = FRDGTextureDesc::Create2D(
		ViewSize, PF_FloatRGBA, FClearValueBinding::Transparent,
		TexCreate_RenderTargetable | TexCreate_ShaderResource);
	FRDGTextureDesc AlphaDesc = FRDGTextureDesc::Create2D(
		ViewSize, PF_R32_FLOAT, FClearValueBinding::Black,
		TexCreate_RenderTargetable | TexCreate_ShaderResource);

	FRDGTextureRef AccumRT = GraphBuilder.CreateTexture(AccumDesc, TEXT("GS_OIT_Accum"));
	FRDGTextureRef AlphaRT = GraphBuilder.CreateTexture(AlphaDesc, TEXT("GS_OIT_Alpha"));

	// ---- Pass 1: Accumulation ----
	{
		TShaderMapRef<FGSSplatOITVS> VS(View.ShaderMap);
		TShaderMapRef<FGSSplatOITPS> PS(View.ShaderMap);

		struct FAccumParams
		{
			FGSSplatOITVS::FParameters VS;
			FGSSplatOITPS::FParameters PS;
		};
		FAccumParams* AccumParams = GraphBuilder.AllocParameters<FAccumParams>();
		FillVSParams(View, Proxy, AccumParams->VS);
		AccumParams->PS.RenderTargets[0] = FRenderTargetBinding(AccumRT, ERenderTargetLoadAction::EClear);
		AccumParams->PS.RenderTargets[1] = FRenderTargetBinding(AlphaRT, ERenderTargetLoadAction::EClear);

		const int32 NumInstances = Proxy->NumSplats;
		FBufferRHIRef LocalQuadIB = QuadIndexBuffer;

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("GaussianSplatting::OIT_Accum (%d splats)", NumInstances),
			AccumParams,
			ERDGPassFlags::Raster,
			[VS, PS, AccumParams, NumInstances, LocalQuadIB](FRHICommandList& RHICmdList)
			{
				FGraphicsPipelineStateInitializer PSOInit;
				RHICmdList.ApplyCachedRenderTargets(PSOInit);

				// Additive blending for accumulation buffers
				PSOInit.BlendState = TStaticBlendState<
					CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One, // RT0 additive
					CW_Red,  BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One  // RT1 alpha additive
				>::GetRHI();
				PSOInit.RasterizerState  = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
				PSOInit.DepthStencilState= TStaticDepthStencilState<false, CF_Always>::GetRHI();
				PSOInit.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
				PSOInit.BoundShaderState.VertexShaderRHI = VS.GetVertexShader();
				PSOInit.BoundShaderState.PixelShaderRHI  = PS.GetPixelShader();
				PSOInit.PrimitiveType = PT_TriangleList;

				SetGraphicsPipelineState(RHICmdList, PSOInit, 0);
				SetShaderParameters(RHICmdList, VS, VS.GetVertexShader(), AccumParams->VS);

				RHICmdList.SetStreamSource(0, nullptr, 0);
				RHICmdList.DrawIndexedPrimitive(LocalQuadIB, 0, 0, 4, 0, 2, NumInstances);
			});
	}

	// ---- Pass 2: Resolve (full-screen composite onto scene colour) ----
	{
		TShaderMapRef<FGSOITResolveVS> VS(View.ShaderMap);
		TShaderMapRef<FGSOITResolvePS> PS(View.ShaderMap);

		FGSOITResolvePS::FParameters* ResolveParams =
			GraphBuilder.AllocParameters<FGSOITResolvePS::FParameters>();
		ResolveParams->GS_AccumTexture = AccumRT;
		ResolveParams->GS_AlphaTexture = AlphaRT;
		ResolveParams->GS_LinearSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
		ResolveParams->RenderTargets[0] = FRenderTargetBinding(SceneColorRT, ERenderTargetLoadAction::ELoad);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("GaussianSplatting::OIT_Resolve"),
			ResolveParams,
			ERDGPassFlags::Raster,
			[VS, PS, ResolveParams](FRHICommandList& RHICmdList)
			{
				FGraphicsPipelineStateInitializer PSOInit;
				RHICmdList.ApplyCachedRenderTargets(PSOInit);

				// Premultiplied-alpha compositing
				PSOInit.BlendState = TStaticBlendState<
					CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha,
					BO_Add,  BF_One, BF_InverseSourceAlpha>::GetRHI();
				PSOInit.RasterizerState   = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
				PSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
				PSOInit.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
				PSOInit.BoundShaderState.VertexShaderRHI = VS.GetVertexShader();
				PSOInit.BoundShaderState.PixelShaderRHI  = PS.GetPixelShader();
				PSOInit.PrimitiveType = PT_TriangleList;

				SetGraphicsPipelineState(RHICmdList, PSOInit, 0);
				SetShaderParameters(RHICmdList, PS, PS.GetPixelShader(), *ResolveParams);

				// Full-screen triangle (3 vertices, no IB)
				RHICmdList.DrawPrimitive(0, 1, 1);
			});
	}
}
