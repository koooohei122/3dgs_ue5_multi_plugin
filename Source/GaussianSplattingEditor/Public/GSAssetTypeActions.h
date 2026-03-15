// Copyright 2024, Gaussian Splatting Plugin. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

class UGaussianSplattingAsset;

/**
 * FGSAssetTypeActions
 *
 * Registers the UGaussianSplattingAsset type with the editor asset system:
 *  - Gives the asset a category and colour in the Content Browser
 *  - Defines the factory used when the asset is dragged into a level
 *    (spawns an AGSActor automatically)
 */
class FGSAssetTypeActions : public FAssetTypeActions_Base
{
public:
	// -----------------------------------------------------------------------
	// FAssetTypeActions_Base interface
	// -----------------------------------------------------------------------
	virtual FText GetName()  const override;
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override;

	/**
	 * Called when the user double-clicks the asset in the Content Browser.
	 * Opens a simple stat window (number of splats, SH degree, etc.)
	 */
	virtual void OpenAssetEditor(
		const TArray<UObject*>& InObjects,
		TSharedPtr<class IToolkitHost> EditWithinLevelEditor) override;

	/**
	 * Returns the actor factory used when dragging the asset into a level.
	 * UActorFactory subclass is returned so the engine auto-spawns AGSActor.
	 */
	virtual class UActorFactory* GetDefaultAssetFactory() override;
};
