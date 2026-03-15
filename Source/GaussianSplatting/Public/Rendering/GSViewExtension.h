// Copyright 2024, Gaussian Splatting Plugin. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"
#include "RenderGraphResources.h"
#include "Rendering/GSTypes.h"

class FGSSceneProxy;

/**
 * FGSViewExtension
 *
 * A scene view extension that injects Gaussian Splatting rendering into
 * UE5's render pipeline after the base pass (translucent phase).
 *
 * All active FGSSceneProxy instances register here and are rendered each frame.
 */
class GAUSSIANSPLATTING_API FGSViewExtension : public FSceneViewExtensionBase
{
public:
	FGSViewExtension(const FAutoRegister& AutoRegister);
	virtual ~FGSViewExtension();

	/** Singleton accessor – creates the extension on first call. */
	static FGSViewExtension& Get();

	/** Called by FGSSceneProxy ctor (render thread). */
	void RegisterProxy(FGSSceneProxy* Proxy);

	/** Called by FGSSceneProxy dtor (render thread). */
	void UnregisterProxy(FGSSceneProxy* Proxy);

	// -----------------------------------------------------------------------
	// FSceneViewExtensionBase interface
	// -----------------------------------------------------------------------
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupView(FSceneViewFamily& ViewFamily, FSceneView& InView) override {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}

	/**
	 * Injected after the translucent pass – renders all registered Gaussian proxies
	 * for each view in the frame.
	 */
	virtual void PostRenderView_RenderThread(
		FRDGBuilder& GraphBuilder,
		FSceneView& InView) override;

private:
	/** All currently active Gaussian proxies (render thread only). */
	TArray<FGSSceneProxy*> Proxies;

	/** Index buffer shared across all draw calls: [0,1,2, 0,2,3] */
	FBufferRHIRef QuadIndexBuffer;

	void EnsureQuadIndexBuffer();

	void RenderProxy_RenderThread(
		FRDGBuilder&    GraphBuilder,
		const FSceneView& View,
		FGSSceneProxy*  Proxy,
		FRDGTextureRef  SceneColorRT);

	void RenderProxy_Sorted(
		FRDGBuilder&    GraphBuilder,
		const FSceneView& View,
		FGSSceneProxy*  Proxy,
		FRDGTextureRef  SceneColorRT);

	void RenderProxy_OIT(
		FRDGBuilder&    GraphBuilder,
		const FSceneView& View,
		FGSSceneProxy*  Proxy,
		FRDGTextureRef  SceneColorRT);

	static TSharedPtr<FGSViewExtension, ESPMode::ThreadSafe> SingletonPtr;
};
