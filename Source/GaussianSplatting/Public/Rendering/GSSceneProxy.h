// Copyright 2024, Gaussian Splatting Plugin. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "PrimitiveSceneProxy.h"
#include "RenderResource.h"
#include "RHI.h"
#include "Rendering/GSTypes.h"

class UGSRenderComponent;

/**
 * FGSSceneProxy
 *
 * Render-thread counterpart of UGSRenderComponent.
 * Owns the GPU buffers for splat data and sorted-index arrays.
 * Registers itself with FGSViewExtension which drives the actual draw calls.
 */
class FGSSceneProxy final : public FPrimitiveSceneProxy
{
public:
	FGSSceneProxy(UGSRenderComponent* InComponent);
	virtual ~FGSSceneProxy();

	// -----------------------------------------------------------------------
	// FPrimitiveSceneProxy interface
	// -----------------------------------------------------------------------
	virtual uint32 GetMemoryFootprint() const override { return sizeof(*this) + GetAllocatedSize(); }
	virtual SIZE_T GetTypeHash() const override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual bool CanBeOccluded() const override { return false; }

	// -----------------------------------------------------------------------
	// Public rendering state accessed by FGSViewExtension (render thread only)
	// -----------------------------------------------------------------------

	/** Structured buffer (read by the vertex shader) */
	FShaderResourceViewRHIRef  SplatBufferSRV;

	/** Identity order buffer (OIT mode) or sorted-index buffer (sorted mode) */
	FShaderResourceViewRHIRef  OrderBufferSRV;

	/** Number of Gaussians */
	int32 NumSplats = 0;

	/** SH degree (0..3) stored in the asset */
	int32 SHDegree  = 0;

	/** Render mode for this proxy */
	EGSRenderMode RenderMode = EGSRenderMode::DepthSorted;

	/** World transform (pivot offset etc.) already baked into the splat positions at upload */
	FMatrix LocalToWorld_RT;

	/** Called by FGSViewExtension each frame to update sorted order (sorted mode only). */
	void UpdateSortOrder_RenderThread(const FVector& CameraWorldPos);

private:
	/** GPU buffer containing all FGSSplatGPUData structs */
	FStructuredBufferRHIRef SplatBuffer;

	/** Buffer for the sorted splat indices */
	FBufferRHIRef           OrderBuffer;

	/** CPU-side copy of positions for sorting */
	TArray<FVector3f>       SplatPositions;

	/** Temporary sort workspace */
	TArray<uint32>          SortedIndices;

	void InitGPUResources(const TArray<FGSSplatData>& Splats);
};
