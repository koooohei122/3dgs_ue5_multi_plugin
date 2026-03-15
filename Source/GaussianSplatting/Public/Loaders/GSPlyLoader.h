// Copyright 2024, Gaussian Splatting Plugin. All Rights Reserved.
#pragma once

#include "Loaders/GSLoaderBase.h"

/**
 * Loads 3D Gaussian Splatting data from PLY files.
 *
 * Handles three PLY sub-formats automatically detected by examining
 * the element/property declarations in the PLY header:
 *
 *  - Standard 3DGS PLY  – properties: x,y,z, scale_0..2, rot_0..3,
 *                          opacity, f_dc_0..2, f_rest_0..44
 *  - PostShot PLY        – properties: x,y,z, (nx,ny,nz), red,green,blue
 *                          Converted to DC-only splats with fixed scale/opacity.
 *  - PlayCanvas PLY      – CHUNK-encoded binary PLY produced by PlayCanvas
 *                          compressor. Contains packed_position, packed_color,
 *                          packed_rotation, packed_scale properties.
 */
class GAUSSIANSPLATTING_API FGSPlyLoader : public IGSLoader
{
public:
	virtual bool CanLoad(const FString& Extension) const override;
	virtual FGSLoadResult Load(const FString& FilePath) const override;

private:
	/** Detects which PLY variant the header describes. */
	static EGSSourceFormat DetectPlyVariant(const TArray<FString>& PropertyNames);

	/** Parses a standard 3DGS PLY (binary little-endian or ASCII). */
	static bool ParseStandardPLY(
		const uint8*         Data,
		int64                DataSize,
		const FString&       HeaderEnd,
		int64                HeaderBytes,
		bool                 bBinaryLE,
		const TArray<FString>& PropNames,
		TArray<FGSSplatData>& OutSplats,
		int32&               OutSHDegree);

	/** Parses a PostShot-style PLY (position + vertex colour only). */
	static bool ParsePostShotPLY(
		const uint8*         Data,
		int64                DataSize,
		int64                HeaderBytes,
		bool                 bBinaryLE,
		int32                NumVertices,
		const TArray<FString>& PropNames,
		TArray<FGSSplatData>& OutSplats);

	/** Parses a PlayCanvas chunk-encoded compressed PLY. */
	static bool ParsePlayCanvasPLY(
		const uint8*         Data,
		int64                DataSize,
		int64                HeaderBytes,
		int32                NumVertices,
		const TArray<FString>& PropNames,
		TArray<FGSSplatData>& OutSplats);
};
