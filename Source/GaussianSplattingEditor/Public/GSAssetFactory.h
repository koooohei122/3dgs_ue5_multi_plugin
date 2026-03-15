// Copyright 2024, Gaussian Splatting Plugin. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "GSAssetFactory.generated.h"

/**
 * UGSAssetFactory
 *
 * Handles importing supported Gaussian Splatting files from disk into the
 * Content Browser as UGaussianSplattingAsset objects.
 *
 * Supported file formats:
 *   .ply  – Standard 3DGS, PostShot, or PlayCanvas PLY
 *   .spz  – Scaniverse SPZ (gzip-compressed)
 *   .sog  – PlayCanvas SOG manifest
 *   .sogs – PlayCanvas SOGS manifest
 */
UCLASS()
class UGSAssetFactory : public UFactory
{
	GENERATED_BODY()

public:
	UGSAssetFactory();

	//~ UFactory interface
	virtual UObject* FactoryCreateFile(
		UClass*              InClass,
		UObject*             InParent,
		FName                InName,
		EObjectFlags         Flags,
		const FString&       Filename,
		const TCHAR*         Parms,
		FFeedbackContext*     Warn,
		bool&                bOutOperationCanceled) override;

	virtual bool FactoryCanImport(const FString& Filename) override;

	virtual FText GetDisplayName() const override;
};

// ---------------------------------------------------------------------------

/**
 * UGSActorFactory
 *
 * Actor factory that spawns an AGSActor pre-configured with the given
 * UGaussianSplattingAsset when the user drags an asset into the level viewport.
 */
UCLASS()
class UGSActorFactory : public UActorFactory
{
	GENERATED_BODY()

public:
	UGSActorFactory();

	//~ UActorFactory interface
	virtual bool CanCreateActorFrom(
		const FAssetData& AssetData,
		FText& OutErrorMsg) override;

	virtual void PostSpawnActor(
		UObject* Asset,
		AActor*  NewActor) override;

	virtual AActor* GetDefaultActor(const FAssetData& AssetData) override;
};
