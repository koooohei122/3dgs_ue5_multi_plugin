// Copyright 2024, Gaussian Splatting Plugin. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GaussianSplattingActor.generated.h"

class UGSRenderComponent;
class UGaussianSplattingAsset;

/**
 * AGSActor
 *
 * A simple actor that wraps a UGSRenderComponent.
 * When you drag a UGaussianSplattingAsset from the Content Browser into the
 * level, the editor spawns one of these actors automatically
 * (via FGSAssetTypeActions::GetDefaultAssetFactory).
 *
 * You can move, rotate, and scale it with the standard UE transform gizmo.
 */
UCLASS(BlueprintType, meta=(DisplayName="Gaussian Splat Actor"))
class GAUSSIANSPLATTING_API AGSActor : public AActor
{
	GENERATED_BODY()

public:
	AGSActor();

	// -----------------------------------------------------------------------
	// Components
	// -----------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gaussian Splatting")
	TObjectPtr<UGSRenderComponent> SplatComponent;

	// -----------------------------------------------------------------------
	// Convenience setters
	// -----------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category="Gaussian Splatting")
	void SetGaussianAsset(UGaussianSplattingAsset* Asset);

	UFUNCTION(BlueprintPure, Category="Gaussian Splatting")
	UGaussianSplattingAsset* GetGaussianAsset() const;
};
