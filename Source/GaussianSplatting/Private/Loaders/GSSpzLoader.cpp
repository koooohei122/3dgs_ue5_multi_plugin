// Copyright 2024, Gaussian Splatting Plugin. All Rights Reserved.

#include "Loaders/GSSpzLoader.h"
#include "Misc/FileHelper.h"
#include "Math/UnrealMathUtility.h"

// zlib is available in UE via the zlib module dependency
#include "zlib.h"

// ---------------------------------------------------------------------------
// CanLoad
// ---------------------------------------------------------------------------

bool FGSSpzLoader::CanLoad(const FString& Extension) const
{
	return Extension == TEXT("spz");
}

// ---------------------------------------------------------------------------
// Load
// ---------------------------------------------------------------------------

FGSLoadResult FGSSpzLoader::Load(const FString& FilePath) const
{
	FGSLoadResult Result;

	TArray<uint8> RawData;
	if (!FFileHelper::LoadFileToArray(RawData, *FilePath))
	{
		Result.ErrorMessage = FString::Printf(TEXT("Cannot open SPZ file: %s"), *FilePath);
		return Result;
	}

	TArray<uint8> Decompressed;
	if (!DecompressGzip(RawData, Decompressed))
	{
		Result.ErrorMessage = TEXT("SPZ: Failed to decompress gzip stream");
		return Result;
	}

	if (!ParseSpzBinary(Decompressed, Result))
	{
		if (Result.ErrorMessage.IsEmpty())
			Result.ErrorMessage = TEXT("SPZ: Failed to parse binary payload");
		return Result;
	}

	Result.Format    = EGSSourceFormat::SPZ;
	Result.bSuccess  = Result.Splats.Num() > 0;
	return Result;
}

// ---------------------------------------------------------------------------
// DecompressGzip
// ---------------------------------------------------------------------------

bool FGSSpzLoader::DecompressGzip(const TArray<uint8>& Compressed, TArray<uint8>& OutDecompressed)
{
	z_stream ZStream = {};
	// inflateInit2 with windowBits=47 enables gzip auto-detection
	if (inflateInit2(&ZStream, 47) != Z_OK)
	{
		return false;
	}

	// Reserve an initial buffer (10× compressed size is a safe start)
	OutDecompressed.SetNum(FMath::Max((int32)Compressed.Num() * 10, 1024 * 1024));

	ZStream.next_in   = const_cast<uint8*>(Compressed.GetData());
	ZStream.avail_in  = (uInt)Compressed.Num();
	ZStream.next_out  = OutDecompressed.GetData();
	ZStream.avail_out = (uInt)OutDecompressed.Num();

	bool bDone = false;
	while (!bDone)
	{
		int32 Ret = inflate(&ZStream, Z_NO_FLUSH);
		if (Ret == Z_STREAM_END)
		{
			bDone = true;
		}
		else if (Ret == Z_BUF_ERROR || ZStream.avail_out == 0)
		{
			// Need more output space
			int64 Written = ZStream.next_out - OutDecompressed.GetData();
			int64 NewSize = OutDecompressed.Num() * 2;
			OutDecompressed.SetNum((int32)NewSize);
			ZStream.next_out  = OutDecompressed.GetData() + Written;
			ZStream.avail_out = (uInt)(NewSize - Written);
		}
		else if (Ret != Z_OK)
		{
			inflateEnd(&ZStream);
			return false;
		}
	}

	int64 FinalSize = ZStream.next_out - OutDecompressed.GetData();
	inflateEnd(&ZStream);
	OutDecompressed.SetNum((int32)FinalSize);
	return FinalSize > 0;
}

// ---------------------------------------------------------------------------
// ReadFixed24
// ---------------------------------------------------------------------------

float FGSSpzLoader::ReadFixed24(const uint8* Data, int64& Offset, int32 FractionalBits)
{
	// 24-bit signed integer (little-endian 3 bytes)
	int32 Raw = (int32)Data[Offset]
	          | ((int32)Data[Offset+1] << 8)
	          | ((int32)Data[Offset+2] << 16);
	// Sign-extend from 24 bits
	if (Raw & 0x800000) Raw |= 0xFF000000;
	Offset += 3;
	return (float)Raw / (float)(1 << FractionalBits);
}

// ---------------------------------------------------------------------------
// ParseSpzBinary
// ---------------------------------------------------------------------------

bool FGSSpzLoader::ParseSpzBinary(const TArray<uint8>& Binary, FGSLoadResult& OutResult)
{
	const uint8* Data    = Binary.GetData();
	const int64  DataSize = Binary.Num();

	if (DataSize < (int64)sizeof(FSpzHeader))
	{
		OutResult.ErrorMessage = TEXT("SPZ: Binary too small for header");
		return false;
	}

	// Parse header
	FSpzHeader Header;
	FMemory::Memcpy(&Header, Data, sizeof(FSpzHeader));

	if (Header.Magic != kSpzMagic)
	{
		OutResult.ErrorMessage = FString::Printf(TEXT("SPZ: Bad magic 0x%08X (expected 0x%08X)"),
		                                          Header.Magic, kSpzMagic);
		return false;
	}

	const uint32 N             = Header.NumPoints;
	const int32  SHDeg         = Header.SHDegree;
	const int32  FracBits      = Header.FractionalBits;
	const bool   bHasAlpha     = (Header.Flags & 0x01) != 0;
	const bool   bHasSHRest    = (Header.Flags & 0x02) != 0;

	if (N == 0)
	{
		OutResult.ErrorMessage = TEXT("SPZ: Zero points in file");
		return false;
	}

	OutResult.SHDegree = SHDeg;
	OutResult.Splats.SetNumZeroed(N);

	int64 Offset = sizeof(FSpzHeader);

	// --- Positions (24-bit fixed-point, column order X, Y, Z)
	if (Offset + (int64)N * 9 > DataSize) return false;
	for (uint32 i = 0; i < N; i++)
	{
		float px = ReadFixed24(Data, Offset, FracBits);
		float py = ReadFixed24(Data, Offset, FracBits);
		float pz = ReadFixed24(Data, Offset, FracBits);
		// Convert from 3DGS (Y-up RH) to UE (Z-up LH), metres to cm
		OutResult.Splats[i].Position = FVector3f(px * 100.0f, -pz * 100.0f, py * 100.0f);
	}

	// --- Scales (3 × float32 per splat, log-space)
	if (Offset + (int64)N * 12 > DataSize) return false;
	for (uint32 i = 0; i < N; i++)
	{
		float s0, s1, s2;
		FMemory::Memcpy(&s0, Data + Offset,     4); Offset += 4;
		FMemory::Memcpy(&s1, Data + Offset,     4); Offset += 4;
		FMemory::Memcpy(&s2, Data + Offset,     4); Offset += 4;
		OutResult.Splats[i].Scale = FVector3f(s0, s1, s2);
	}

	// --- Rotations (4 × float32, wxyz)
	if (Offset + (int64)N * 16 > DataSize) return false;
	for (uint32 i = 0; i < N; i++)
	{
		float rw, rx, ry, rz;
		FMemory::Memcpy(&rw, Data + Offset,     4); Offset += 4;
		FMemory::Memcpy(&rx, Data + Offset,     4); Offset += 4;
		FMemory::Memcpy(&ry, Data + Offset,     4); Offset += 4;
		FMemory::Memcpy(&rz, Data + Offset,     4); Offset += 4;
		// Convert coord system
		OutResult.Splats[i].Rotation = FQuat4f(rx, -rz, ry, rw);
	}

	// --- Opacities (float32 per splat, pre-sigmoid)
	if (bHasAlpha)
	{
		if (Offset + (int64)N * 4 > DataSize) return false;
		for (uint32 i = 0; i < N; i++)
		{
			float A;
			FMemory::Memcpy(&A, Data + Offset, 4); Offset += 4;
			OutResult.Splats[i].Opacity = A;
		}
	}
	else
	{
		// Default: opacity = 0 (sigmoid(0) = 0.5)
		for (uint32 i = 0; i < N; i++) OutResult.Splats[i].Opacity = 0.0f;
	}

	// --- SH DC colours (3 × float32, RGB)
	if (Offset + (int64)N * 12 > DataSize) return false;
	for (uint32 i = 0; i < N; i++)
	{
		float r, g, b;
		FMemory::Memcpy(&r, Data + Offset,     4); Offset += 4;
		FMemory::Memcpy(&g, Data + Offset,     4); Offset += 4;
		FMemory::Memcpy(&b, Data + Offset,     4); Offset += 4;
		OutResult.Splats[i].SHDc = FVector3f(r, g, b);
	}

	// --- SH rest coefficients
	if (bHasSHRest && SHDeg > 0)
	{
		// Number of rest coefficients per channel: (SHDeg+1)^2 - 1
		int32 CoeffsPerChan = ((SHDeg + 1) * (SHDeg + 1)) - 1;
		int32 TotalPerSplat = CoeffsPerChan * 3; // 3 channels
		TotalPerSplat = FMath::Min(TotalPerSplat, GS_SH_REST_COUNT);

		if (Offset + (int64)N * TotalPerSplat * 4 <= DataSize)
		{
			for (uint32 i = 0; i < N; i++)
			{
				for (int32 c = 0; c < TotalPerSplat; c++)
				{
					float V;
					FMemory::Memcpy(&V, Data + Offset, 4); Offset += 4;
					OutResult.Splats[i].SHRest[c] = V;
				}
			}
		}
	}

	return true;
}
