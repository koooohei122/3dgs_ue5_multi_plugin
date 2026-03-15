// Copyright 2024, Gaussian Splatting Plugin. All Rights Reserved.

#include "GaussianSplattingActor.h"
#include "GaussianSplattingComponent.h"
#include "GaussianSplattingAsset.h"

AGSActor::AGSActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Root is a default scene component so the transform gizmo works
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	SplatComponent = CreateDefaultSubobject<UGSRenderComponent>(TEXT("SplatComponent"));
	SplatComponent->SetupAttachment(Root);
}

void AGSActor::SetGaussianAsset(UGaussianSplattingAsset* Asset)
{
	if (SplatComponent)
	{
		SplatComponent->SetGaussianAsset(Asset);
	}
}

UGaussianSplattingAsset* AGSActor::GetGaussianAsset() const
{
	return SplatComponent ? SplatComponent->GaussianAsset : nullptr;
}
