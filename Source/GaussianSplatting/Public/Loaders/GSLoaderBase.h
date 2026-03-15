// Copyright 2024, Gaussian Splatting Plugin. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Rendering/GSTypes.h"

/**
 * Result from any GS file loader.
 */
struct FGSLoadResult
{
	/** Loaded splat data (empty on failure) */
	TArray<FGSSplatData> Splats;

	/** SH degree found in the file (0-3) */
	int32 SHDegree = 0;

	/** Detected source format */
	EGSSourceFormat Format = EGSSourceFormat::Unknown;

	/** True when loading succeeded */
	bool bSuccess = false;

	/** Human-readable error message on failure */
	FString ErrorMessage;
};

/**
 * Abstract base for all GS file loaders.
 * Loaders are stateless – call Load() with a file path.
 */
class GAUSSIANSPLATTING_API IGSLoader
{
public:
	virtual ~IGSLoader() = default;

	/** Returns true if this loader can handle the given file extension (lowercase, no dot). */
	virtual bool CanLoad(const FString& Extension) const = 0;

	/** Synchronously loads a file and returns the result. */
	virtual FGSLoadResult Load(const FString& FilePath) const = 0;
};

/** Inline sigmoid function (float) */
FORCEINLINE float GSSigmoid(float x)
{
	return 1.0f / (1.0f + FMath::Exp(-x));
}
