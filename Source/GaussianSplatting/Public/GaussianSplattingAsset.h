// Copyright 2024, Gaussian Splatting Plugin. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Engine/StreamableManager.h"
#include "Rendering/GSTypes.h"
#include "GaussianSplattingAsset.generated.h"

/**
 * UGaussianSplattingAsset
 *
 * A UE content-browser asset that holds all Gaussian splat data
 * imported from PLY / SPZ / SOG files.
 *
 * Large splat arrays are stored as FByteBulkData so they are
 * efficiently cooked and streamed without inflating the .uasset header.
 */
UCLASS(BlueprintType, hidecategories=(Object))
class GAUSSIANSPLATTING_API UGaussianSplattingAsset : public UObject
{
	GENERATED_BODY()

public:
	UGaussianSplattingAsset();

	//~ UObject interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// -----------------------------------------------------------------------
	// Metadata (UPROPERTY – serialised in asset header, visible in Details)
	// -----------------------------------------------------------------------

	/** Number of Gaussian splats */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gaussian Splatting|Info")
	int32 NumSplats = 0;

	/** SH degree detected in the source file (0 = DC-only, 1/2/3 = higher order) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gaussian Splatting|Info")
	int32 SHDegree = 0;

	/** File format the asset was imported from */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gaussian Splatting|Info")
	EGSSourceFormat SourceFormat = EGSSourceFormat::Unknown;

	/** Bounding box of all splat positions */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gaussian Splatting|Info")
	FBox BoundingBox = FBox(ForceInit);

	/** Path of the original source file (for re-import) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gaussian Splatting|Source")
	FString SourceFilePath;

	/** MD5 hash of the source file at import time (used to detect changes) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gaussian Splatting|Source")
	FString SourceFileHash;

	// -----------------------------------------------------------------------
	// Raw splat data (bulk – NOT a UPROPERTY)
	// -----------------------------------------------------------------------

	/**
	 * Raw Gaussian splat data stored as bulk binary.
	 * Each element is a FGSSplatData (256 bytes).
	 * The count is NumSplats.
	 */
	FByteBulkData BulkSplatData;

	// -----------------------------------------------------------------------
	// Runtime helpers
	// -----------------------------------------------------------------------

	/** Returns a read-only view of the splat data. Valid only while asset is loaded. */
	const FGSSplatData* GetSplatData() const;

	/** Populates asset from a pre-parsed array (call from the importer). */
	void SetSplatData(TArray<FGSSplatData>&& InSplats, int32 InSHDegree, EGSSourceFormat InFormat);

	/** Total GPU memory (bytes) required to upload all splat data */
	int64 GetGPUMemoryBytes() const { return (int64)NumSplats * sizeof(FGSSplatData); }
};
