// Copyright 2024, Gaussian Splatting Plugin. All Rights Reserved.
#pragma once

#include "Loaders/GSLoaderBase.h"

/**
 * Loads 3D Gaussian Splatting data from PlayCanvas SOG / SOGS format.
 *
 * SOG (Spatially Ordered Gaussians) / SOGS stores Gaussian attributes in
 * 2D image grids. The format uses a JSON manifest (.sog or .sogs) that
 * references PNG/WebP image files containing quantised attribute data.
 *
 * The loader expects either:
 *   - A .sog / .sogs JSON manifest file, OR
 *   - A directory containing the manifest + attribute images
 *
 * Supported attribute images:
 *   position.png  – RGB encodes quantised XYZ position
 *   color.png     – RGB encodes SH DC colour
 *   opacity.png   – R channel encodes opacity
 *   scale.png     – RGB encodes log-scale XYZ
 *   rotation.png  – RGBA encodes quaternion
 *
 * Reference: https://developer.playcanvas.com/user-manual/gaussian-splatting/formats/sog/
 */
class GAUSSIANSPLATTING_API FGSSogLoader : public IGSLoader
{
public:
	virtual bool CanLoad(const FString& Extension) const override;
	virtual FGSLoadResult Load(const FString& FilePath) const override;

private:
	/** Parse the JSON manifest and return attribute image paths. */
	static bool ParseManifest(
		const FString& ManifestPath,
		FString&       OutPositionImage,
		FString&       OutColorImage,
		FString&       OutOpacityImage,
		FString&       OutScaleImage,
		FString&       OutRotationImage,
		FString&       OutSHImage,
		int32&         OutWidth,
		int32&         OutHeight,
		FString&       OutError);

	/**
	 * Load a PNG image file as raw RGBA pixels.
	 * Returns false if the image could not be loaded.
	 */
	static bool LoadPNGImage(
		const FString& ImagePath,
		TArray<uint8>& OutPixels,
		int32& OutWidth,
		int32& OutHeight);

	/** Decode attribute images into splat data. */
	static bool DecodeAttributes(
		const TArray<uint8>& PosPixels,  int32 PosW, int32 PosH,
		const TArray<uint8>& ColPixels,  int32 ColW, int32 ColH,
		const TArray<uint8>& OpacPixels, int32 OpaW, int32 OpaH,
		const TArray<uint8>& SclPixels,  int32 SclW, int32 SclH,
		const TArray<uint8>& RotPixels,  int32 RotW, int32 RotH,
		int32                NumSplats,
		TArray<FGSSplatData>& OutSplats);
};
