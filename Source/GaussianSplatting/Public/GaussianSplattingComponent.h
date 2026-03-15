// Copyright 2024, Gaussian Splatting Plugin. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "Rendering/GSTypes.h"
#include "GaussianSplattingComponent.generated.h"

class UGaussianSplattingAsset;

/**
 * UGSRenderComponent
 *
 * A primitive component that renders a UGaussianSplattingAsset in the level.
 * Place on any Actor to display Gaussian splat data.
 *
 * Transform (position, rotation, scale) is controlled through the
 * standard Actor/Component transform tools in the UE editor.
 */
UCLASS(ClassGroup=(GaussianSplatting), meta=(BlueprintSpawnableComponent),
       hidecategories=(LOD, Collision, Physics, Lighting))
class GAUSSIANSPLATTING_API UGSRenderComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UGSRenderComponent();

	//~ UPrimitiveComponent
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

	//~ UActorComponent
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// -----------------------------------------------------------------------
	// Properties
	// -----------------------------------------------------------------------

	/** The Gaussian splat asset to render */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gaussian Splatting",
		meta=(AllowedClasses="/Script/GaussianSplatting.GaussianSplattingAsset"))
	TObjectPtr<UGaussianSplattingAsset> GaussianAsset;

	/** Rendering quality / performance mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gaussian Splatting|Rendering")
	EGSRenderMode RenderMode = EGSRenderMode::DepthSorted;

	/**
	 * Override the SH evaluation degree at runtime.
	 * -1 = use the degree stored in the asset.
	 *  0 = DC colour only (cheapest, no view-dependent highlights).
	 *  1, 2, 3 = progressively higher quality / cost.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gaussian Splatting|Rendering",
		meta=(ClampMin=-1, ClampMax=3))
	int32 SHDegreeOverride = -1;

	// -----------------------------------------------------------------------
	// Blueprint helpers
	// -----------------------------------------------------------------------

	/** Swap the asset at runtime and rebuild GPU resources. */
	UFUNCTION(BlueprintCallable, Category="Gaussian Splatting")
	void SetGaussianAsset(UGaussianSplattingAsset* NewAsset);

	/** Number of Gaussians currently loaded */
	UFUNCTION(BlueprintPure, Category="Gaussian Splatting")
	int32 GetNumSplats() const;
};
