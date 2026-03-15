// Copyright 2024, Gaussian Splatting Plugin. All Rights Reserved.

#include "Rendering/GSViewExtension.h"
#include "Rendering/GSSceneProxy.h"
#include "Rendering/GSGlobalShaders.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "SceneView.h"
#include "ScreenPass.h"
#include "RHIStaticStates.h"
#include "PipelineStateCache.h"
#include "CommonRenderResources.h"
#include "PostProcess/PostProcessMaterial.h"

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
	ENQUEUE_RENDER_COMMAND(GS_ReleaseQuadIB)([this](FRHICommandListImmediate&)
	{
		QuadIndexBuffer.SafeRelease();
	});
}

// ---------------------------------------------------------------------------
// Proxy registry
// ---------------------------------------------------------------------------

void FGSViewExtension::RegisterProxy(FGSSceneProxy* Proxy)
{
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
// EnsureQuadIndexBuffer  (render thread)
// ---------------------------------------------------------------------------

void FGSViewExtension::EnsureQuadIndexBuffer()
{
	if (QuadIndexBuffer.IsValid()) return;

	// Two CCW triangles: [0,1,2] and [0,2,3]
	const uint16 Indices[] = { 0, 1, 2, 0, 2, 3 };
	FRHIResourceCreateInfo CI(TEXT("GS_QuadIB"));
	QuadIndexBuffer = RHICreateIndexBuffer(sizeof(uint16), sizeof(Indices), BUF_Static, CI);
	void* Data = RHILockBuffer(QuadIndexBuffer, 0, sizeof(Indices), RLM_WriteOnly);
	FMemory::Memcpy(Data, Indices, sizeof(Indices));
	RHIUnlockBuffer(QuadIndexBuffer);
}

// ---------------------------------------------------------------------------
// SubscribeToPostProcessingPass  (game thread, called each frame)
// ---------------------------------------------------------------------------

void FGSViewExtension::SubscribeToPostProcessingPass(
	EPostProcessingPass Pass,
	FAfterPassCallbackDelegateArray& InOutPassCallbacks,
	bool bIsPassEnabled)
{
	// Inject after the MotionBlur pass so we have fully-composited scene colour.
	// Gaussians are rendered before tone mapping so they are correctly exposed.
	if (Pass == EPostProcessingPass::MotionBlur)
	{
		const FAfterPassCallbackDelegate Delegate =
			FAfterPassCallbackDelegate::CreateRaw(
				this, &FGSViewExtension::RenderGaussianSplats_PP);
		InOutPassCallbacks.Add(Delegate);
	}
}

// ---------------------------------------------------------------------------
// RenderGaussianSplats_PP  (render thread – PP chain callback)
// ---------------------------------------------------------------------------

FScreenPassTexture FGSViewExtension::RenderGaussianSplats_PP(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FPostProcessMaterialInputs& InInputs)
{
	check(IsInRenderingThread());

	// Retrieve scene colour from the PP inputs
	FScreenPassTexture SceneColor = InInputs.GetInput(EPostProcessMaterialInput::SceneColor);
	if (!SceneColor.IsValid() || Proxies.IsEmpty())
	{
		return SceneColor;
	}

	EnsureQuadIndexBuffer();

	for (FGSSceneProxy* Proxy : Proxies)
	{
		if (!Proxy || Proxy->NumSplats == 0)   continue;
		if (!Proxy->SplatBufferSRV.IsValid())   continue;

		if (Proxy->RenderMode == EGSRenderMode::DepthSorted)
		{
			Proxy->UpdateSortOrder_RenderThread(View.ViewMatrices.GetViewOrigin());
		}

		if (Proxy->RenderMode == EGSRenderMode::DepthAwareOIT)
		{
			RenderProxy_OIT(GraphBuilder, View, Proxy, SceneColor.Texture);
		}
		else
		{
			RenderProxy_Sorted(GraphBuilder, View, Proxy, SceneColor.Texture);
		}
	}

	return SceneColor;
}

// ---------------------------------------------------------------------------
// Helper: fill common VS parameters
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

	const FIntRect ViewRect  = View.UnscaledViewRect;
	Params.GS_ViewportSize   = FVector2f((float)ViewRect.Width(), (float)ViewRect.Height());

	const float HalfW = (float)ViewRect.Width()  * 0.5f;
	const float HalfH = (float)ViewRect.Height() * 0.5f;
	Params.GS_FocalLength = FVector2f(
		VM.GetProjectionMatrix().M[0][0] * HalfW,
		VM.GetProjectionMatrix().M[1][1] * HalfH);

	Params.GS_SHDegree = Proxy->SHDegree;
	Params.SplatBuffer = Proxy->SplatBufferSRV;
	Params.OrderBuffer = Proxy->OrderBufferSRV;
}

// ---------------------------------------------------------------------------
// RenderProxy_Sorted
// ---------------------------------------------------------------------------

void FGSViewExtension::RenderProxy_Sorted(
	FRDGBuilder&      GraphBuilder,
	const FSceneView& View,
	FGSSceneProxy*    Proxy,
	FRDGTextureRef    SceneColorRT)
{
	TShaderMapRef<FGSSplatVS> VS(View.ShaderMap);
	TShaderMapRef<FGSSplatPS> PS(View.ShaderMap);

	struct FPassParams
	{
		FGSSplatVS::FParameters VS;
		FGSSplatPS::FParameters PS;
	};
	FPassParams* PassParams = GraphBuilder.AllocParameters<FPassParams>();
	FillVSParams(View, Proxy, PassParams->VS);
	PassParams->PS.RenderTargets[0] =
		FRenderTargetBinding(SceneColorRT, ERenderTargetLoadAction::ELoad);

	const int32    NumInstances = Proxy->NumSplats;
	FBufferRHIRef  LocalQuadIB  = QuadIndexBuffer;

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("GaussianSplatting::Sorted (%d)", NumInstances),
		PassParams,
		ERDGPassFlags::Raster,
		[VS, PS, PassParams, NumInstances, LocalQuadIB](FRHICommandList& RHICmdList)
		{
			FGraphicsPipelineStateInitializer PSOInit;
			RHICmdList.ApplyCachedRenderTargets(PSOInit);

			// Premultiplied-alpha: dst = src.rgb + dst.rgb*(1-src.a)
			PSOInit.BlendState = TStaticBlendState<
				CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha,
				         BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI();
			PSOInit.RasterizerState   = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			PSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			PSOInit.BoundShaderState.VertexDeclarationRHI =
				GEmptyVertexDeclaration.VertexDeclarationRHI;
			PSOInit.BoundShaderState.VertexShaderRHI = VS.GetVertexShader();
			PSOInit.BoundShaderState.PixelShaderRHI  = PS.GetPixelShader();
			PSOInit.PrimitiveType = PT_TriangleList;

			SetGraphicsPipelineState(RHICmdList, PSOInit, 0);
			SetShaderParameters(RHICmdList, VS, VS.GetVertexShader(), PassParams->VS);

			RHICmdList.SetStreamSource(0, nullptr, 0);
			RHICmdList.DrawIndexedPrimitive(
				LocalQuadIB,
				0, 0,            // BaseVertexIndex, MinIndex
				4,               // NumVertices per quad instance
				0, 2,            // StartIndex, NumPrimitives (2 triangles)
				NumInstances);
		});
}

// ---------------------------------------------------------------------------
// RenderProxy_OIT  (Mobile-GS Depth-Aware OIT)
// ---------------------------------------------------------------------------

void FGSViewExtension::RenderProxy_OIT(
	FRDGBuilder&      GraphBuilder,
	const FSceneView& View,
	FGSSceneProxy*    Proxy,
	FRDGTextureRef    SceneColorRT)
{
	const FIntPoint ViewSize(View.UnscaledViewRect.Width(), View.UnscaledViewRect.Height());

	FRDGTextureRef AccumRT = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(ViewSize, PF_FloatRGBA,
			FClearValueBinding::Transparent,
			TexCreate_RenderTargetable | TexCreate_ShaderResource),
		TEXT("GS_OIT_Accum"));

	FRDGTextureRef AlphaRT = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(ViewSize, PF_R32_FLOAT,
			FClearValueBinding::Black,
			TexCreate_RenderTargetable | TexCreate_ShaderResource),
		TEXT("GS_OIT_Alpha"));

	// ---- Pass 1: Accumulate contributions ----
	{
		TShaderMapRef<FGSSplatOITVS> VS(View.ShaderMap);
		TShaderMapRef<FGSSplatOITPS> PS(View.ShaderMap);

		struct FAccumParams
		{
			FGSSplatOITVS::FParameters VS;
			FGSSplatOITPS::FParameters PS;
		};
		FAccumParams* AccumP = GraphBuilder.AllocParameters<FAccumParams>();
		FillVSParams(View, Proxy, AccumP->VS);
		AccumP->PS.RenderTargets[0] = FRenderTargetBinding(AccumRT, ERenderTargetLoadAction::EClear);
		AccumP->PS.RenderTargets[1] = FRenderTargetBinding(AlphaRT, ERenderTargetLoadAction::EClear);

		const int32   NumInstances = Proxy->NumSplats;
		FBufferRHIRef LocalQuadIB  = QuadIndexBuffer;

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("GaussianSplatting::OIT_Accum (%d)", NumInstances),
			AccumP, ERDGPassFlags::Raster,
			[VS, PS, AccumP, NumInstances, LocalQuadIB](FRHICommandList& RHICmdList)
			{
				FGraphicsPipelineStateInitializer PSOInit;
				RHICmdList.ApplyCachedRenderTargets(PSOInit);
				PSOInit.BlendState = TStaticBlendState<
					CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One,
					CW_Red,  BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI();
				PSOInit.RasterizerState   = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
				PSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
				PSOInit.BoundShaderState.VertexDeclarationRHI =
					GEmptyVertexDeclaration.VertexDeclarationRHI;
				PSOInit.BoundShaderState.VertexShaderRHI = VS.GetVertexShader();
				PSOInit.BoundShaderState.PixelShaderRHI  = PS.GetPixelShader();
				PSOInit.PrimitiveType = PT_TriangleList;
				SetGraphicsPipelineState(RHICmdList, PSOInit, 0);
				SetShaderParameters(RHICmdList, VS, VS.GetVertexShader(), AccumP->VS);
				RHICmdList.SetStreamSource(0, nullptr, 0);
				RHICmdList.DrawIndexedPrimitive(LocalQuadIB, 0, 0, 4, 0, 2, NumInstances);
			});
	}

	// ---- Pass 2: Resolve onto scene colour ----
	{
		TShaderMapRef<FGSOITResolveVS> VS(View.ShaderMap);
		TShaderMapRef<FGSOITResolvePS> PS(View.ShaderMap);

		FGSOITResolvePS::FParameters* ResolveP =
			GraphBuilder.AllocParameters<FGSOITResolvePS::FParameters>();
		ResolveP->GS_AccumTexture  = AccumRT;
		ResolveP->GS_AlphaTexture  = AlphaRT;
		ResolveP->GS_LinearSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
		ResolveP->RenderTargets[0] =
			FRenderTargetBinding(SceneColorRT, ERenderTargetLoadAction::ELoad);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("GaussianSplatting::OIT_Resolve"),
			ResolveP, ERDGPassFlags::Raster,
			[VS, PS, ResolveP](FRHICommandList& RHICmdList)
			{
				FGraphicsPipelineStateInitializer PSOInit;
				RHICmdList.ApplyCachedRenderTargets(PSOInit);
				PSOInit.BlendState = TStaticBlendState<
					CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha,
					         BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI();
				PSOInit.RasterizerState   = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
				PSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
				PSOInit.BoundShaderState.VertexDeclarationRHI =
					GEmptyVertexDeclaration.VertexDeclarationRHI;
				PSOInit.BoundShaderState.VertexShaderRHI = VS.GetVertexShader();
				PSOInit.BoundShaderState.PixelShaderRHI  = PS.GetPixelShader();
				PSOInit.PrimitiveType = PT_TriangleList;
				SetGraphicsPipelineState(RHICmdList, PSOInit, 0);
				SetShaderParameters(RHICmdList, PS, PS.GetPixelShader(), *ResolveP);
				RHICmdList.DrawPrimitive(0, 1, 1); // full-screen triangle
			});
	}
}
