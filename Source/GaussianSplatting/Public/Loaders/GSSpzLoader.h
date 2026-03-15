// Copyright 2024, Gaussian Splatting Plugin. All Rights Reserved.
#pragma once

#include "Loaders/GSLoaderBase.h"

/**
 * Loads 3D Gaussian Splatting data from the SPZ format.
 *
 * SPZ is the Scaniverse/Niantic open format:
 *   https://github.com/nianticlabs/spz
 *
 * The file is a gzip-compressed binary stream with:
 *   - 16-byte header (magic 0x5053474e, version, numPoints, shDegree,
 *     fractionalBits, flags, reserved)
 *   - Column-organised fixed-point / float attribute arrays:
 *       positions (24-bit fixed-point), scales, rotations, alphas,
 *       colors (SH DC), spherical harmonics rest coefficients
 */
class GAUSSIANSPLATTING_API FGSSpzLoader : public IGSLoader
{
public:
	virtual bool CanLoad(const FString& Extension) const override;
	virtual FGSLoadResult Load(const FString& FilePath) const override;

private:
	static constexpr uint32 kSpzMagic   = 0x5053474e;
	static constexpr uint32 kSpzVersion = 2;

	struct FSpzHeader
	{
		uint32 Magic;
		uint32 Version;
		uint32 NumPoints;
		uint8  SHDegree;
		uint8  FractionalBits; // fixed-point bits for position fractions
		uint8  Flags;
		uint8  Reserved;
	};

	/** Decompress gzip data into OutDecompressed. Returns false on error. */
	static bool DecompressGzip(const TArray<uint8>& Compressed, TArray<uint8>& OutDecompressed);

	/** Read a 24-bit signed fixed-point value and convert to float (metres). */
	static float ReadFixed24(const uint8* Data, int64& Offset, int32 FractionalBits);

	static bool ParseSpzBinary(const TArray<uint8>& Binary, FGSLoadResult& OutResult);
};
