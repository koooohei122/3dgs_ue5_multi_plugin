// Copyright 2024, Gaussian Splatting Plugin. All Rights Reserved.

#include "Rendering/GSSceneProxy.h"
#include "GaussianSplattingComponent.h"
#include "GaussianSplattingAsset.h"
#include "Rendering/GSViewExtension.h"
#include "RHI.h"
#include "RHICommandList.h"
#include "RenderUtils.h"
#include "Engine/Engine.h"
#include "Algo/Sort.h"

// ---------------------------------------------------------------------------
// FGSSceneProxy constructor
// ---------------------------------------------------------------------------

FGSSceneProxy::FGSSceneProxy(UGSRenderComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent, InComponent->GetFName())
{
	bVerifyUsedMaterials = false;

	RenderMode     = InComponent->RenderMode;
	LocalToWorld_RT = InComponent->GetComponentTransform().ToMatrixWithScale();

	UGaussianSplattingAsset* Asset = InComponent->GaussianAsset;
	if (!Asset || Asset->NumSplats == 0)
	{
		return;
	}

	NumSplats = Asset->NumSplats;
	SHDegree  = FMath::Min(InComponent->SHDegreeOverride >= 0
	                       ? InComponent->SHDegreeOverride
	                       : Asset->SHDegree,
	                       GS_MAX_SH_DEGREE);

	// Read splat data on game thread – GPU upload deferred to render thread init
	const FGSSplatData* SplatPtr = Asset->GetSplatData();
	if (!SplatPtr)
	{
		return;
	}

	TArray<FGSSplatData> Splats;
	Splats.Append(SplatPtr, NumSplats);
	Asset->BulkSplatData.Unlock();

	// Extract positions for CPU sort
	SplatPositions.SetNumUninitialized(NumSplats);
	SortedIndices.SetNumUninitialized(NumSplats);
	for (int32 i = 0; i < NumSplats; i++)
	{
		SplatPositions[i] = Splats[i].Position;
		SortedIndices[i]  = (uint32)i;
	}

	// Upload to GPU (must happen on render thread)
	ENQUEUE_RENDER_COMMAND(GS_InitProxy)(
		[this, Splats = MoveTemp(Splats)](FRHICommandListImmediate& RHICmdList) mutable
		{
			InitGPUResources(Splats);
		});

	// Register with the view extension so it renders us
	FGSViewExtension::Get().RegisterProxy(this);
}

FGSSceneProxy::~FGSSceneProxy()
{
	FGSViewExtension::Get().UnregisterProxy(this);

	// GPU resources released automatically via RHI smart pointers
}

// ---------------------------------------------------------------------------
// InitGPUResources (render thread)
// ---------------------------------------------------------------------------

void FGSSceneProxy::InitGPUResources(const TArray<FGSSplatData>& Splats)
{
	check(IsInRenderingThread());
	if (Splats.Num() == 0) return;

	const int64 BufferSize = (int64)Splats.Num() * sizeof(FGSSplatData);

	// Create structured buffer for splat data
	FRHIResourceCreateInfo CreateInfo(TEXT("GS_SplatBuffer"));
	SplatBuffer = RHICreateStructuredBuffer(
		sizeof(FGSSplatData),
		BufferSize,
		BUF_ShaderResource,
		ERHIAccess::SRVMask,
		CreateInfo);

	// Upload data
	void* MappedData = RHILockBuffer(SplatBuffer, 0, BufferSize, RLM_WriteOnly);
	FMemory::Memcpy(MappedData, Splats.GetData(), BufferSize);
	RHIUnlockBuffer(SplatBuffer);

	SplatBufferSRV = RHICreateShaderResourceView(SplatBuffer);

	// Create identity order buffer (for OIT mode; will be updated each frame in sorted mode)
	FRHIResourceCreateInfo OrderCI(TEXT("GS_OrderBuffer"));
	const int64 OrderSize = (int64)NumSplats * sizeof(uint32);
	OrderBuffer = RHICreateVertexBuffer(OrderSize, BUF_Dynamic | BUF_ShaderResource, OrderCI);

	uint32* OrderData = (uint32*)RHILockBuffer(OrderBuffer, 0, OrderSize, RLM_WriteOnly);
	for (int32 i = 0; i < NumSplats; i++) OrderData[i] = (uint32)i;
	RHIUnlockBuffer(OrderBuffer);

	OrderBufferSRV = RHICreateShaderResourceView(
		OrderBuffer, sizeof(uint32), PF_R32_UINT);
}

// ---------------------------------------------------------------------------
// UpdateSortOrder_RenderThread
// ---------------------------------------------------------------------------

void FGSSceneProxy::UpdateSortOrder_RenderThread(const FVector& CameraWorldPos)
{
	check(IsInRenderingThread());
	if (NumSplats == 0 || !OrderBuffer.IsValid()) return;

	// Camera forward in splat space (approximate as position-based depth)
	// Sort by squared distance desc (back-to-front)
	FVector3f CamF(CameraWorldPos);

	// Parallel sort using UE's Sort with a lambda
	for (int32 i = 0; i < NumSplats; i++) SortedIndices[i] = (uint32)i;

	Algo::Sort(SortedIndices, [&](uint32 A, uint32 B)
	{
		float dA = FVector3f::DistSquared(SplatPositions[A], CamF);
		float dB = FVector3f::DistSquared(SplatPositions[B], CamF);
		return dA > dB; // back-to-front
	});

	// Upload sorted indices
	const int64 OrderSize = (int64)NumSplats * sizeof(uint32);
	uint32* OrderData = (uint32*)RHILockBuffer(OrderBuffer, 0, OrderSize, RLM_WriteOnly);
	FMemory::Memcpy(OrderData, SortedIndices.GetData(), OrderSize);
	RHIUnlockBuffer(OrderBuffer);
}

// ---------------------------------------------------------------------------
// FPrimitiveSceneProxy interface
// ---------------------------------------------------------------------------

SIZE_T FGSSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

FPrimitiveViewRelevance FGSSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Relevance;
	Relevance.bDrawRelevance       = IsShown(View);
	Relevance.bShadowRelevance     = false;
	Relevance.bDynamicRelevance    = true;
	Relevance.bTranslucentSelfShadow = false;
	Relevance.bUsesLightingChannels = false;
	Relevance.bRenderCustomDepth   = ShouldRenderCustomDepth();
	return Relevance;
}
