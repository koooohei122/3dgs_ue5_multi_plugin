// Copyright 2024, Gaussian Splatting Plugin. All Rights Reserved.

#include "GaussianSplattingComponent.h"
#include "GaussianSplattingAsset.h"
#include "Rendering/GSSceneProxy.h"

UGSRenderComponent::UGSRenderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bUseAsOccluder                    = false;
	CastShadow                        = false;
	bCastDynamicShadow                = false;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

// ---------------------------------------------------------------------------
// CreateSceneProxy
// ---------------------------------------------------------------------------

FPrimitiveSceneProxy* UGSRenderComponent::CreateSceneProxy()
{
	if (!GaussianAsset || GaussianAsset->NumSplats == 0)
	{
		return nullptr;
	}
	return new FGSSceneProxy(this);
}

// ---------------------------------------------------------------------------
// CalcBounds
// ---------------------------------------------------------------------------

FBoxSphereBounds UGSRenderComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (GaussianAsset && GaussianAsset->BoundingBox.IsValid)
	{
		return FBoxSphereBounds(GaussianAsset->BoundingBox).TransformBy(LocalToWorld);
	}
	return FBoxSphereBounds(FVector::ZeroVector, FVector(500.0), 500.0);
}

// ---------------------------------------------------------------------------
// OnRegister / OnUnregister
// ---------------------------------------------------------------------------

void UGSRenderComponent::OnRegister()
{
	Super::OnRegister();
	// Nothing extra needed; CreateSceneProxy handles GPU upload.
}

void UGSRenderComponent::OnUnregister()
{
	Super::OnUnregister();
}

#if WITH_EDITOR
void UGSRenderComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	// Rebuild scene proxy when asset or settings change in editor
	MarkRenderStateDirty();
}
#endif

// ---------------------------------------------------------------------------
// Blueprint helpers
// ---------------------------------------------------------------------------

void UGSRenderComponent::SetGaussianAsset(UGaussianSplattingAsset* NewAsset)
{
	GaussianAsset = NewAsset;
	MarkRenderStateDirty();
	UpdateBounds();
}

int32 UGSRenderComponent::GetNumSplats() const
{
	return GaussianAsset ? GaussianAsset->NumSplats : 0;
}
