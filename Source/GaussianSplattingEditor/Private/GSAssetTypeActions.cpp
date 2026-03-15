// Copyright 2024, Gaussian Splatting Plugin. All Rights Reserved.

#include "GSAssetTypeActions.h"
#include "GSAssetFactory.h"
#include "GaussianSplattingAsset.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Toolkits/SimpleAssetEditor.h"
#include "AssetRegistry/AssetData.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "GaussianSplattingEditor"

FText FGSAssetTypeActions::GetName() const
{
	return LOCTEXT("GaussianSplatName", "Gaussian Splat");
}

FColor FGSAssetTypeActions::GetTypeColor() const
{
	// A distinctive violet-blue colour in the Content Browser
	return FColor(120, 80, 220);
}

UClass* FGSAssetTypeActions::GetSupportedClass() const
{
	return UGaussianSplattingAsset::StaticClass();
}

uint32 FGSAssetTypeActions::GetCategories()
{
	// Place in the "Rendering" category group
	return EAssetTypeCategories::Rendering;
}

void FGSAssetTypeActions::OpenAssetEditor(
	const TArray<UObject*>& InObjects,
	TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	// Open a simple property editor for each selected asset
	FSimpleAssetEditor::CreateEditor(EToolkitMode::Standalone, EditWithinLevelEditor, InObjects);
}

UActorFactory* FGSAssetTypeActions::GetDefaultAssetFactory()
{
	// The engine will use this factory when the user drags the asset into the level viewport.
	// UGSActorFactory is registered as a UActorFactory subclass; UE picks it up automatically
	// via the UObject CDO registry.
	return GetMutableDefault<UGSActorFactory>();
}

#undef LOCTEXT_NAMESPACE
