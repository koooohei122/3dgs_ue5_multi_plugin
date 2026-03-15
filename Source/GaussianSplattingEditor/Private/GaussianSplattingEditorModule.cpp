// Copyright 2024, Gaussian Splatting Plugin. All Rights Reserved.

#include "GaussianSplattingEditorModule.h"
#include "GSAssetTypeActions.h"
#include "GSAssetFactory.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FGaussianSplattingEditorModule"

void FGaussianSplattingEditorModule::StartupModule()
{
	// Register asset type actions so the asset appears correctly in the Content Browser
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	TSharedPtr<FGSAssetTypeActions> Actions = MakeShareable(new FGSAssetTypeActions);
	AssetTools.RegisterAssetTypeActions(Actions.ToSharedRef());
	RegisteredAssetActions.Add(Actions);
}

void FGaussianSplattingEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools =
			FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

		for (const TSharedPtr<IAssetTypeActions>& Action : RegisteredAssetActions)
		{
			AssetTools.UnregisterAssetTypeActions(Action.ToSharedRef());
		}
	}
	RegisteredAssetActions.Empty();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGaussianSplattingEditorModule, GaussianSplattingEditor)
