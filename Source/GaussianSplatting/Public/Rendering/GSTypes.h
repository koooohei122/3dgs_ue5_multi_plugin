// Copyright 2024, Gaussian Splatting Plugin. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Math/Vector.h"
#include "Math/Quat.h"

/** Maximum SH degree supported */
#define GS_MAX_SH_DEGREE 3

/** Number of SH coefficients per channel for degree 0 */
#define GS_SH_COEFFS_D0  1
/** Number of SH coefficients per channel up to degree 1 (0+1+2+3) */
#define GS_SH_COEFFS_D1  4
/** Number of SH coefficients per channel up to degree 2 */
#define GS_SH_COEFFS_D2  9
/** Number of SH coefficients per channel up to degree 3 */
#define GS_SH_COEFFS_D3  16

/** Total rest SH floats for all channels (excluding DC), for degree 3 */
#define GS_SH_REST_COUNT 45   // (16-1) coeffs * 3 channels

/** Source file format detected during import */
UENUM(BlueprintType)
enum class EGSSourceFormat : uint8
{
	/** Original 3DGS PLY: full attributes (f_dc, f_rest, scale, rot, opacity) */
	StandardPLY      UMETA(DisplayName = "Standard PLY"),
	/** PostShot-exported PLY: may have only position + vertex color */
	PostShotPLY      UMETA(DisplayName = "PostShot PLY"),
	/** PlayCanvas compressed PLY (CHUNK-based binary format) */
	PlayCanvasPLY    UMETA(DisplayName = "PlayCanvas PLY"),
	/** PlayCanvas SOG/SOGS – grid-texture compressed format */
	PlayCanvasSOG    UMETA(DisplayName = "PlayCanvas SOG/SOGS"),
	/** Scaniverse SPZ – gzip-compressed binary */
	SPZ              UMETA(DisplayName = "SPZ (Scaniverse)"),
	/** Unknown / fallback */
	Unknown          UMETA(DisplayName = "Unknown"),
};

/** Render quality/performance preset */
UENUM(BlueprintType)
enum class EGSRenderMode : uint8
{
	/**
	 * Back-to-front CPU sort + standard alpha blending.
	 * Best visual quality. Recommended for Desktop/Console.
	 */
	DepthSorted      UMETA(DisplayName = "Depth Sorted (High Quality)"),
	/**
	 * Mobile-GS depth-aware Order Independent Transparency.
	 * No per-frame sort. Best performance for mobile / VR.
	 * May show minor transparency artefacts at overlap regions.
	 */
	DepthAwareOIT    UMETA(DisplayName = "Depth-Aware OIT (Mobile)"),
};

/**
 * CPU-side storage for a single Gaussian splat.
 * Matches the GPU buffer layout (see GSTypes.ush) exactly.
 * Total: 256 bytes per splat.
 */
struct FGSSplatData
{
	/** World-space position */
	FVector3f Position  = FVector3f::ZeroVector;
	/** Pre-sigmoid opacity value (apply sigmoid in shader) */
	float     Opacity   = 0.0f;

	/** Rotation quaternion (w, x, y, z) */
	FQuat4f   Rotation  = FQuat4f::Identity;

	/** Scale in log-space (apply exp() in shader) */
	FVector3f Scale     = FVector3f::ZeroVector;
	float     Pad0      = 0.0f;

	/** SH DC term (degree-0 colour coefficient), per RGB channel */
	FVector3f SHDc      = FVector3f::ZeroVector;
	float     Pad1      = 0.0f;

	/**
	 * Higher-order SH rest coefficients.
	 * Layout matches 3DGS PLY convention:
	 *   [0..8]  – degree-1 (3 basis × 3 channels = 9 values, interleaved RGB)
	 *   [9..23] – degree-2 (5 × 3 = 15 values)
	 *   [24..44]– degree-3 (7 × 3 = 21 values)
	 */
	float SHRest[GS_SH_REST_COUNT] = {};
	/** Padding to reach 256-byte alignment */
	float Pad2[3] = {};
};

static_assert(sizeof(FGSSplatData) == 256, "FGSSplatData must be 256 bytes to match GPU buffer stride");
