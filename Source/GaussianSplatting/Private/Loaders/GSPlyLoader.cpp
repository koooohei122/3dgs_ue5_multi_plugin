// Copyright 2024, Gaussian Splatting Plugin. All Rights Reserved.

#include "Loaders/GSPlyLoader.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Math/UnrealMathUtility.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace PLYInternal
{
	/** Read a null/newline-terminated string from binary data */
	static FString ReadLine(const uint8* Data, int64 Size, int64& Offset)
	{
		FString Line;
		while (Offset < Size)
		{
			const char C = (char)Data[Offset++];
			if (C == '\n') break;
			if (C != '\r') Line.AppendChar(C);
		}
		return Line;
	}

	/** Read a little-endian float from binary stream */
	static float ReadFloat(const uint8* Data, int64& Offset)
	{
		float V;
		FMemory::Memcpy(&V, Data + Offset, 4);
		Offset += 4;
		return V;
	}

	/** Read a little-endian uint8 from binary stream */
	static uint8 ReadUint8(const uint8* Data, int64& Offset)
	{
		return Data[Offset++];
	}

	/** Read a little-endian uint16 from binary stream */
	static uint16 ReadUint16(const uint8* Data, int64& Offset)
	{
		uint16 V;
		FMemory::Memcpy(&V, Data + Offset, 2);
		Offset += 2;
		return V;
	}

	struct FPropertyDef
	{
		FString TypeName; // "float", "uchar", etc.
		FString Name;
		int32   ByteSize; // in bytes
	};

	static int32 GetTypeSize(const FString& TypeName)
	{
		if (TypeName == TEXT("float") || TypeName == TEXT("int") || TypeName == TEXT("uint"))   return 4;
		if (TypeName == TEXT("double"))                                                          return 8;
		if (TypeName == TEXT("uchar") || TypeName == TEXT("char"))                              return 1;
		if (TypeName == TEXT("ushort") || TypeName == TEXT("short"))                            return 2;
		return 4;
	}

	static float ReadTypedValue(const FPropertyDef& Prop, const uint8* Data, int64& Offset)
	{
		if (Prop.TypeName == TEXT("float"))
		{
			float V;
			FMemory::Memcpy(&V, Data + Offset, 4);
			Offset += 4;
			return V;
		}
		if (Prop.TypeName == TEXT("double"))
		{
			double V;
			FMemory::Memcpy(&V, Data + Offset, 8);
			Offset += 8;
			return (float)V;
		}
		if (Prop.TypeName == TEXT("uchar"))
		{
			uint8 V = Data[Offset++];
			return V / 255.0f;
		}
		if (Prop.TypeName == TEXT("char"))
		{
			int8 V = (int8)Data[Offset++];
			return (float)V;
		}
		if (Prop.TypeName == TEXT("ushort"))
		{
			uint16 V;
			FMemory::Memcpy(&V, Data + Offset, 2);
			Offset += 2;
			return (float)V;
		}
		if (Prop.TypeName == TEXT("short"))
		{
			int16 V;
			FMemory::Memcpy(&V, Data + Offset, 2);
			Offset += 2;
			return (float)V;
		}
		if (Prop.TypeName == TEXT("int"))
		{
			int32 V;
			FMemory::Memcpy(&V, Data + Offset, 4);
			Offset += 4;
			return (float)V;
		}
		// Default: skip and return 0
		Offset += Prop.ByteSize;
		return 0.0f;
	}

} // namespace PLYInternal

// ---------------------------------------------------------------------------
// FGSPlyLoader::CanLoad
// ---------------------------------------------------------------------------

bool FGSPlyLoader::CanLoad(const FString& Extension) const
{
	return Extension == TEXT("ply");
}

// ---------------------------------------------------------------------------
// FGSPlyLoader::Load
// ---------------------------------------------------------------------------

FGSLoadResult FGSPlyLoader::Load(const FString& FilePath) const
{
	FGSLoadResult Result;

	TArray<uint8> RawData;
	if (!FFileHelper::LoadFileToArray(RawData, *FilePath))
	{
		Result.ErrorMessage = FString::Printf(TEXT("Cannot open file: %s"), *FilePath);
		return Result;
	}

	const uint8* Data    = RawData.GetData();
	const int64  DataSize = RawData.Num();

	// ---- Parse PLY header --------------------------------------------------
	if (DataSize < 4 || FMemory::Memcmp(Data, "ply", 3) != 0)
	{
		Result.ErrorMessage = TEXT("Not a valid PLY file (missing 'ply' magic)");
		return Result;
	}

	bool   bBinaryLE   = false;
	bool   bBinaryBE   = false;
	int32  NumVertices = 0;
	int64  HeaderEnd   = 0;

	TArray<PLYInternal::FPropertyDef> Properties;

	int64 Offset = 0;
	while (Offset < DataSize)
	{
		FString Line = PLYInternal::ReadLine(Data, DataSize, Offset);
		Line.TrimStartAndEndInline();

		if (Line == TEXT("end_header"))
		{
			HeaderEnd = Offset;
			break;
		}

		if (Line.StartsWith(TEXT("format binary_little_endian"))) bBinaryLE = true;
		else if (Line.StartsWith(TEXT("format binary_big_endian")))  bBinaryBE = true;

		if (Line.StartsWith(TEXT("element vertex")))
		{
			TArray<FString> Parts;
			Line.ParseIntoArray(Parts, TEXT(" "), true);
			if (Parts.Num() >= 3) NumVertices = FCString::Atoi(*Parts[2]);
		}

		if (Line.StartsWith(TEXT("property")))
		{
			TArray<FString> Parts;
			Line.ParseIntoArray(Parts, TEXT(" "), true);
			if (Parts.Num() >= 3)
			{
				PLYInternal::FPropertyDef Prop;
				Prop.TypeName = Parts[1];
				Prop.Name     = Parts[2];
				Prop.ByteSize = PLYInternal::GetTypeSize(Prop.TypeName);
				Properties.Add(Prop);
			}
		}
	}

	if (NumVertices <= 0)
	{
		Result.ErrorMessage = TEXT("PLY file has no vertices");
		return Result;
	}

	// Collect property names for variant detection
	TArray<FString> PropNames;
	for (const auto& P : Properties) PropNames.Add(P.Name);

	EGSSourceFormat Variant = DetectPlyVariant(PropNames);

	bool bOk = false;
	if (Variant == EGSSourceFormat::StandardPLY)
	{
		bOk = ParseStandardPLY(Data, DataSize, TEXT(""), HeaderEnd, bBinaryLE, PropNames,
		                        Result.Splats, Result.SHDegree);
		Result.Format = EGSSourceFormat::StandardPLY;
	}
	else if (Variant == EGSSourceFormat::PostShotPLY)
	{
		bOk = ParsePostShotPLY(Data, DataSize, HeaderEnd, bBinaryLE, NumVertices,
		                        PropNames, Result.Splats);
		Result.Format = EGSSourceFormat::PostShotPLY;
		Result.SHDegree = 0;
	}
	else if (Variant == EGSSourceFormat::PlayCanvasPLY)
	{
		bOk = ParsePlayCanvasPLY(Data, DataSize, HeaderEnd, NumVertices,
		                          PropNames, Result.Splats);
		Result.Format = EGSSourceFormat::PlayCanvasPLY;
		Result.SHDegree = 0;
	}

	if (!bOk && Result.ErrorMessage.IsEmpty())
	{
		// Fallback: try parsing as standard PLY properties
		bOk = ParseStandardPLY(Data, DataSize, TEXT(""), HeaderEnd, bBinaryLE, PropNames,
		                        Result.Splats, Result.SHDegree);
		Result.Format = EGSSourceFormat::StandardPLY;
	}

	Result.bSuccess = bOk && Result.Splats.Num() > 0;
	if (!Result.bSuccess && Result.ErrorMessage.IsEmpty())
	{
		Result.ErrorMessage = TEXT("Failed to parse PLY vertex data");
	}
	return Result;
}

// ---------------------------------------------------------------------------
// DetectPlyVariant
// ---------------------------------------------------------------------------

EGSSourceFormat FGSPlyLoader::DetectPlyVariant(const TArray<FString>& PropertyNames)
{
	bool bHasScale    = PropertyNames.Contains(TEXT("scale_0"));
	bool bHasRot      = PropertyNames.Contains(TEXT("rot_0"));
	bool bHasFDc      = PropertyNames.Contains(TEXT("f_dc_0"));
	bool bHasOpacity  = PropertyNames.Contains(TEXT("opacity"));
	bool bHasPacked   = PropertyNames.Contains(TEXT("packed_position")) ||
	                    PropertyNames.Contains(TEXT("packed_color"))     ||
	                    PropertyNames.Contains(TEXT("chunk_pack"));

	if (bHasScale && bHasRot && bHasFDc && bHasOpacity)
	{
		return EGSSourceFormat::StandardPLY;
	}
	if (bHasPacked)
	{
		return EGSSourceFormat::PlayCanvasPLY;
	}
	// PostShot / basic point-cloud PLY
	return EGSSourceFormat::PostShotPLY;
}

// ---------------------------------------------------------------------------
// ParseStandardPLY
// ---------------------------------------------------------------------------

bool FGSPlyLoader::ParseStandardPLY(
	const uint8*         Data,
	int64                DataSize,
	const FString&       /*HeaderEnd*/,
	int64                HeaderBytes,
	bool                 bBinaryLE,
	const TArray<FString>& PropNames,
	TArray<FGSSplatData>& OutSplats,
	int32&               OutSHDegree)
{
	// Build property index map
	TMap<FString, int32> PropIndex;
	for (int32 i = 0; i < PropNames.Num(); i++) PropIndex.Add(PropNames[i], i);

	// Determine SH degree from how many f_rest_ properties exist
	int32 FRestCount = 0;
	for (const FString& N : PropNames)
	{
		if (N.StartsWith(TEXT("f_rest_"))) FRestCount++;
	}

	if      (FRestCount >= 45) OutSHDegree = 3;
	else if (FRestCount >= 24) OutSHDegree = 2;
	else if (FRestCount >= 9)  OutSHDegree = 1;
	else                       OutSHDegree = 0;

	// Determine vertex stride (all properties are floats in standard 3DGS PLY)
	const int32 PropStride = PropNames.Num() * sizeof(float);
	const int64 DataOffset = HeaderBytes;
	const int64 AvailBytes = DataSize - DataOffset;
	if (AvailBytes <= 0) return false;

	const int32 NumVerts = (int32)(AvailBytes / PropStride);
	if (NumVerts <= 0) return false;

	OutSplats.SetNumUninitialized(NumVerts);

	auto GetPropVal = [&](const uint8* Row, const FString& Name) -> float
	{
		const int32* IdxPtr = PropIndex.Find(Name);
		if (!IdxPtr) return 0.0f;
		float V;
		FMemory::Memcpy(&V, Row + (*IdxPtr) * sizeof(float), 4);
		return V;
	};

	for (int32 i = 0; i < NumVerts; i++)
	{
		const uint8* Row = Data + DataOffset + (int64)i * PropStride;
		FGSSplatData& S = OutSplats[i];

		// Position (3DGS uses Y-up right-hand; convert to UE left-hand Z-up: x=x, y=-z, z=y)
		float px = GetPropVal(Row, TEXT("x"));
		float py = GetPropVal(Row, TEXT("y"));
		float pz = GetPropVal(Row, TEXT("z"));
		// UE convention: X=forward, Y=right, Z=up; 3DGS: X=right, Y=up, Z=back
		S.Position = FVector3f(px * 100.0f, -pz * 100.0f, py * 100.0f); // m -> cm

		// Scale (log-space)
		S.Scale = FVector3f(
			GetPropVal(Row, TEXT("scale_0")),
			GetPropVal(Row, TEXT("scale_1")),
			GetPropVal(Row, TEXT("scale_2")));

		// Rotation quaternion (w, x, y, z in 3DGS)
		float rw = GetPropVal(Row, TEXT("rot_0"));
		float rx = GetPropVal(Row, TEXT("rot_1"));
		float ry = GetPropVal(Row, TEXT("rot_2"));
		float rz = GetPropVal(Row, TEXT("rot_3"));
		// Normalise
		float Len = FMath::Sqrt(rw*rw + rx*rx + ry*ry + rz*rz);
		if (Len > 1e-6f) { rw/=Len; rx/=Len; ry/=Len; rz/=Len; }
		// Convert coord system
		S.Rotation = FQuat4f(rx, -rz, ry, rw);

		// Opacity (stored pre-sigmoid)
		S.Opacity = GetPropVal(Row, TEXT("opacity"));

		// SH DC term
		S.SHDc = FVector3f(
			GetPropVal(Row, TEXT("f_dc_0")),
			GetPropVal(Row, TEXT("f_dc_1")),
			GetPropVal(Row, TEXT("f_dc_2")));

		// SH rest coefficients
		for (int32 r = 0; r < GS_SH_REST_COUNT; r++)
		{
			FString PName = FString::Printf(TEXT("f_rest_%d"), r);
			S.SHRest[r] = GetPropVal(Row, PName);
		}
	}

	return true;
}

// ---------------------------------------------------------------------------
// ParsePostShotPLY
// ---------------------------------------------------------------------------

bool FGSPlyLoader::ParsePostShotPLY(
	const uint8*         Data,
	int64                DataSize,
	int64                HeaderBytes,
	bool                 bBinaryLE,
	int32                NumVertices,
	const TArray<FString>& PropNames,
	TArray<FGSSplatData>& OutSplats)
{
	TMap<FString, int32> PropIndex;
	for (int32 i = 0; i < PropNames.Num(); i++) PropIndex.Add(PropNames[i], i);

	// Detect if colour is float [0,1] or uchar [0,255] by looking at property types.
	// PostShot typically stores float. We assume float for now.
	const int32 PropStride = PropNames.Num() * sizeof(float);
	const int64 DataOffset = HeaderBytes;
	const int64 AvailBytes = DataSize - DataOffset;
	if (AvailBytes <= 0) return false;

	const int32 NumVerts = FMath::Min(NumVertices, (int32)(AvailBytes / PropStride));
	OutSplats.SetNumUninitialized(NumVerts);

	auto GetPropVal = [&](const uint8* Row, const FString& Name, float Default = 0.0f) -> float
	{
		const int32* IdxPtr = PropIndex.Find(Name);
		if (!IdxPtr) return Default;
		float V;
		FMemory::Memcpy(&V, Row + (*IdxPtr) * sizeof(float), 4);
		return V;
	};

	for (int32 i = 0; i < NumVerts; i++)
	{
		const uint8* Row = Data + DataOffset + (int64)i * PropStride;
		FGSSplatData& S = OutSplats[i];

		float px = GetPropVal(Row, TEXT("x"));
		float py = GetPropVal(Row, TEXT("y"));
		float pz = GetPropVal(Row, TEXT("z"));
		S.Position = FVector3f(px * 100.0f, -pz * 100.0f, py * 100.0f);

		// Reconstruct colour from RGB (map [0,1] to 3DGS DC convention: (c-0.5)/SH_C0)
		float R = GetPropVal(Row, TEXT("red"),   GetPropVal(Row, TEXT("r"), 0.5f));
		float G = GetPropVal(Row, TEXT("green"), GetPropVal(Row, TEXT("g"), 0.5f));
		float B = GetPropVal(Row, TEXT("blue"),  GetPropVal(Row, TEXT("b"), 0.5f));

		// Inverse of SH DC → colour: dc = (color - 0.5) / SH_C0
		const float SH_C0 = 0.28209479177387814f;
		S.SHDc = FVector3f((R - 0.5f) / SH_C0, (G - 0.5f) / SH_C0, (B - 0.5f) / SH_C0);

		// Defaults for missing attributes
		S.Scale    = FVector3f(-3.0f, -3.0f, -3.0f); // log(exp(-3)) ≈ 0.05m
		S.Rotation = FQuat4f::Identity;
		S.Opacity  = 0.0f; // sigmoid(0) = 0.5 alpha
	}

	return true;
}

// ---------------------------------------------------------------------------
// ParsePlayCanvasPLY
// ---------------------------------------------------------------------------

bool FGSPlyLoader::ParsePlayCanvasPLY(
	const uint8*         Data,
	int64                DataSize,
	int64                HeaderBytes,
	int32                NumVertices,
	const TArray<FString>& PropNames,
	TArray<FGSSplatData>& OutSplats)
{
	// PlayCanvas compressed PLY stores data in "chunks" after the header.
	// Each chunk contains quantised splat attributes stored as uint16.
	// Properties include: packed_position (uint32), packed_rotation (uint32),
	//                     packed_scale (uint32), packed_color (uint32)
	// This format is produced by the PlayCanvas splat compressor.
	// Reference: https://github.com/playcanvas/engine/blob/main/src/scene/gsplat/

	TMap<FString, int32> PropIndex;
	for (int32 i = 0; i < PropNames.Num(); i++) PropIndex.Add(PropNames[i], i);

	// PlayCanvas PLY: stride is 4 bytes per packed property
	// packed_position: 3×float16 or similar – implementation detail varies by version.
	// We support the common variant where each splat occupies a fixed stride.
	// For full fidelity, users should export Standard PLY from PlayCanvas editor.
	// This parser provides best-effort decoding of the packed format.

	// For now, fall back to treating as standard (if properties match) or return error.
	// A full PlayCanvas chunk decoder requires detailed format version handling.
	UE_LOG(LogTemp, Warning, TEXT("GaussianSplatting: PlayCanvas PLY format detected. "
		"Attempting best-effort parse. For best results export Standard PLY."));

	// Check for minimal fields
	bool bHasX = PropIndex.Contains(TEXT("x"));
	bool bHasY = PropIndex.Contains(TEXT("y"));
	bool bHasZ = PropIndex.Contains(TEXT("z"));

	if (bHasX && bHasY && bHasZ)
	{
		// Fall back to PostShot-style positional parse
		return ParsePostShotPLY(Data, DataSize, HeaderBytes, true, NumVertices, PropNames, OutSplats);
	}

	return false;
}
