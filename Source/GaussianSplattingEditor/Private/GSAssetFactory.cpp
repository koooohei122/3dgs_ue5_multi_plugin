// Copyright 2024, Gaussian Splatting Plugin. All Rights Reserved.

#include "GSAssetFactory.h"
#include "GaussianSplattingAsset.h"
#include "GaussianSplattingActor.h"
#include "GaussianSplattingComponent.h"
#include "Loaders/GSPlyLoader.h"
#include "Loaders/GSSpzLoader.h"
#include "Loaders/GSSogLoader.h"
#include "Misc/Paths.h"
#include "Misc/FeedbackContext.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "GaussianSplattingEditor"

// ===========================================================================
// UGSAssetFactory
// ===========================================================================

UGSAssetFactory::UGSAssetFactory()
{
	bCreateNew  = false;
	bEditAfterNew = false;
	bEditorImport = true;
	SupportedClass = UGaussianSplattingAsset::StaticClass();

	// Register all file extensions this factory handles
	Formats.Add(TEXT("ply;3D Gaussian Splatting PLY"));
	Formats.Add(TEXT("spz;Scaniverse SPZ (Gaussian Splatting)"));
	Formats.Add(TEXT("sog;PlayCanvas SOG Gaussian Splatting"));
	Formats.Add(TEXT("sogs;PlayCanvas SOGS Gaussian Splatting"));
}

bool UGSAssetFactory::FactoryCanImport(const FString& Filename)
{
	FString Ext = FPaths::GetExtension(Filename).ToLower();
	return Ext == TEXT("ply")
	    || Ext == TEXT("spz")
	    || Ext == TEXT("sog")
	    || Ext == TEXT("sogs");
}

FText UGSAssetFactory::GetDisplayName() const
{
	return LOCTEXT("GSAssetFactoryDisplayName", "Gaussian Splatting Asset");
}

UObject* UGSAssetFactory::FactoryCreateFile(
	UClass*          InClass,
	UObject*         InParent,
	FName            InName,
	EObjectFlags     Flags,
	const FString&   Filename,
	const TCHAR*     Parms,
	FFeedbackContext* Warn,
	bool&            bOutOperationCanceled)
{
	bOutOperationCanceled = false;

	const FString Ext = FPaths::GetExtension(Filename).ToLower();

	// Select loader
	TUniquePtr<IGSLoader> Loader;
	if (Ext == TEXT("ply"))  Loader = MakeUnique<FGSPlyLoader>();
	else if (Ext == TEXT("spz"))  Loader = MakeUnique<FGSSpzLoader>();
	else if (Ext == TEXT("sog") || Ext == TEXT("sogs")) Loader = MakeUnique<FGSSogLoader>();

	if (!Loader)
	{
		if (Warn) Warn->Logf(ELogVerbosity::Error,
			TEXT("GaussianSplatting: No loader for extension '%s'"), *Ext);
		return nullptr;
	}

	// Show progress dialog
	if (Warn) Warn->BeginSlowTask(
		FText::Format(LOCTEXT("GSLoading", "Loading {0}..."), FText::FromString(Filename)),
		true);

	FGSLoadResult LoadResult = Loader->Load(Filename);

	if (Warn) Warn->EndSlowTask();

	if (!LoadResult.bSuccess)
	{
		if (Warn) Warn->Logf(ELogVerbosity::Error,
			TEXT("GaussianSplatting: Failed to load '%s': %s"),
			*Filename, *LoadResult.ErrorMessage);
		return nullptr;
	}

	// Create asset object
	UGaussianSplattingAsset* Asset = NewObject<UGaussianSplattingAsset>(InParent, InName, Flags);
	Asset->SourceFilePath  = Filename;
	Asset->SetSplatData(MoveTemp(LoadResult.Splats), LoadResult.SHDegree, LoadResult.Format);

	UE_LOG(LogTemp, Log,
		TEXT("GaussianSplatting: Imported %d splats (SH degree %d, format %d) from '%s'"),
		Asset->NumSplats, Asset->SHDegree, (int32)Asset->SourceFormat, *Filename);

	return Asset;
}

// ===========================================================================
// UGSActorFactory
// ===========================================================================

UGSActorFactory::UGSActorFactory()
{
	DisplayName  = LOCTEXT("GSActorFactoryDisplayName", "Gaussian Splat Actor");
	NewActorClass = AGSActor::StaticClass();
	bUseSurfaceOrientation = false;
}

bool UGSActorFactory::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
	if (AssetData.GetClass() == UGaussianSplattingAsset::StaticClass())
	{
		return true;
	}
	if (AssetData.IsValid() &&
	    AssetData.GetClass() &&
	    AssetData.GetClass()->IsChildOf(UGaussianSplattingAsset::StaticClass()))
	{
		return true;
	}
	return false;
}

void UGSActorFactory::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	Super::PostSpawnActor(Asset, NewActor);

	AGSActor* GSActor = Cast<AGSActor>(NewActor);
	UGaussianSplattingAsset* GSAsset = Cast<UGaussianSplattingAsset>(Asset);

	if (GSActor && GSAsset)
	{
		GSActor->SetGaussianAsset(GSAsset);
	}
}

AActor* UGSActorFactory::GetDefaultActor(const FAssetData& AssetData)
{
	return AGSActor::StaticClass()->GetDefaultObject<AGSActor>();
}

#undef LOCTEXT_NAMESPACE
