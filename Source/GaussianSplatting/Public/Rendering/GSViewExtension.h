// Copyright 2024, Gaussian Splatting Plugin. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"
#include "PostProcess/PostProcessing.h"
#include "ScreenPass.h"
#include "Rendering/GSTypes.h"

class FGSSceneProxy;

/**
 * FGSViewExtension
 *
 * Injects Gaussian Splatting rendering into UE5's post-processing pipeline
 * via SubscribeToPostProcessingPass (the official UE5.3+ mechanism).
 *
 * Gaussian splats are composited after the translucency pass and before
 * tone mapping, using the scene colour texture provided by the PP chain.
 */
class GAUSSIANSPLATTING_API FGSViewExtension : public FSceneViewExtensionBase
{
public:
	FGSViewExtension(const FAutoRegister& AutoRegister);
	virtual ~FGSViewExtension();

	/** Singleton accessor – creates the extension on first call (game thread). */
	static FGSViewExtension& Get();

	/** Called by FGSSceneProxy ctor (game or render thread safe via ENQUEUE). */
	void RegisterProxy(FGSSceneProxy* Proxy);

	/** Called by FGSSceneProxy dtor. */
	void UnregisterProxy(FGSSceneProxy* Proxy);

	// -----------------------------------------------------------------------
	// FSceneViewExtensionBase interface
	// -----------------------------------------------------------------------
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupView(FSceneViewFamily& ViewFamily, FSceneView& InView) override {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}

	/**
	 * Called each frame (game thread) to register our render delegate into
	 * the post-processing pipeline.  We inject after MotionBlur so we have
	 * access to the fully-composited translucent scene colour.
	 */
	virtual void SubscribeToPostProcessingPass(
		EPostProcessingPass Pass,
		FAfterPassCallbackDelegateArray& InOutPassCallbacks,
		bool bIsPassEnabled) override;

private:
	/** All currently active Gaussian proxies (render thread only). */
	TArray<FGSSceneProxy*> Proxies;

	/** Shared quad index buffer [0,1,2, 0,2,3]. */
	FBufferRHIRef QuadIndexBuffer;

	void EnsureQuadIndexBuffer();

	/**
	 * Render-thread callback invoked by the PP pipeline.
	 * Renders all active Gaussian proxies onto the incoming scene colour and
	 * returns it (possibly modified) to continue the PP chain.
	 */
	FScreenPassTexture RenderGaussianSplats_PP(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FPostProcessMaterialInputs& InInputs);

	void RenderProxy_Sorted(
		FRDGBuilder&         GraphBuilder,
		const FSceneView&    View,
		FGSSceneProxy*       Proxy,
		FRDGTextureRef       SceneColorRT);

	void RenderProxy_OIT(
		FRDGBuilder&         GraphBuilder,
		const FSceneView&    View,
		FGSSceneProxy*       Proxy,
		FRDGTextureRef       SceneColorRT);

	static TSharedPtr<FGSViewExtension, ESPMode::ThreadSafe> SingletonPtr;
};
