// Copyright 2024, Gaussian Splatting Plugin. All Rights Reserved.

#include "Loaders/GSSogLoader.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Json.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"

// ---------------------------------------------------------------------------
// CanLoad
// ---------------------------------------------------------------------------

bool FGSSogLoader::CanLoad(const FString& Extension) const
{
	return Extension == TEXT("sog") || Extension == TEXT("sogs");
}

// ---------------------------------------------------------------------------
// Load
// ---------------------------------------------------------------------------

FGSLoadResult FGSSogLoader::Load(const FString& FilePath) const
{
	FGSLoadResult Result;

	FString PosImg, ColImg, OpaImg, SclImg, RotImg, SHImg;
	int32 GridW = 0, GridH = 0;
	FString ManifestError;

	if (!ParseManifest(FilePath, PosImg, ColImg, OpaImg, SclImg, RotImg, SHImg,
	                   GridW, GridH, ManifestError))
	{
		Result.ErrorMessage = ManifestError;
		return Result;
	}

	const int32 NumSplats = GridW * GridH;
	if (NumSplats <= 0)
	{
		Result.ErrorMessage = TEXT("SOG: Grid dimensions are zero");
		return Result;
	}

	// Load attribute images
	TArray<uint8> PosPixels, ColPixels, OpaPixels, SclPixels, RotPixels;
	int32 PosW=0, PosH=0, ColW=0, ColH=0, OpaW=0, OpaH=0, SclW=0, SclH=0, RotW=0, RotH=0;

	FString BaseDir = FPaths::GetPath(FilePath);

	auto TryLoad = [&](const FString& Img, TArray<uint8>& Pixels, int32& W, int32& H) -> bool
	{
		if (Img.IsEmpty()) return true; // optional
		FString Full = FPaths::IsRelative(Img) ? BaseDir / Img : Img;
		return LoadPNGImage(Full, Pixels, W, H);
	};

	if (!TryLoad(PosImg, PosPixels, PosW, PosH) ||
	    !TryLoad(ColImg, ColPixels, ColW, ColH) ||
	    !TryLoad(OpaImg, OpaPixels, OpaW, OpaH) ||
	    !TryLoad(SclImg, SclPixels, SclW, SclH) ||
	    !TryLoad(RotImg, RotPixels, RotW, RotH))
	{
		Result.ErrorMessage = TEXT("SOG: Failed to load one or more attribute images");
		return Result;
	}

	if (!DecodeAttributes(PosPixels, PosW, PosH,
	                      ColPixels, ColW, ColH,
	                      OpaPixels, OpaW, OpaH,
	                      SclPixels, SclW, SclH,
	                      RotPixels, RotW, RotH,
	                      NumSplats,
	                      Result.Splats))
	{
		Result.ErrorMessage = TEXT("SOG: Failed to decode attribute images");
		return Result;
	}

	Result.Format   = EGSSourceFormat::PlayCanvasSOG;
	Result.SHDegree = 0; // SOG typically stores only DC colour
	Result.bSuccess = Result.Splats.Num() > 0;
	return Result;
}

// ---------------------------------------------------------------------------
// ParseManifest
// ---------------------------------------------------------------------------

bool FGSSogLoader::ParseManifest(
	const FString& ManifestPath,
	FString& OutPosImg,
	FString& OutColImg,
	FString& OutOpaImg,
	FString& OutSclImg,
	FString& OutRotImg,
	FString& OutSHImg,
	int32&   OutW,
	int32&   OutH,
	FString& OutError)
{
	FString JsonStr;
	if (!FFileHelper::LoadFileToString(JsonStr, *ManifestPath))
	{
		OutError = FString::Printf(TEXT("SOG: Cannot open manifest: %s"), *ManifestPath);
		return false;
	}

	TSharedPtr<FJsonObject> RootObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
	if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid())
	{
		OutError = TEXT("SOG: Failed to parse JSON manifest");
		return false;
	}

	// Common manifest fields
	OutW = (int32)RootObj->GetNumberField(TEXT("width"));
	OutH = (int32)RootObj->GetNumberField(TEXT("height"));

	OutPosImg = RootObj->GetStringField(TEXT("position"));
	OutColImg = RootObj->GetStringField(TEXT("color"));
	OutOpaImg = RootObj->GetStringField(TEXT("opacity"));
	OutSclImg = RootObj->GetStringField(TEXT("scale"));
	OutRotImg = RootObj->GetStringField(TEXT("rotation"));
	RootObj->TryGetStringField(TEXT("sphericalHarmonics"), OutSHImg);

	if (OutW <= 0 || OutH <= 0)
	{
		OutError = TEXT("SOG: Invalid grid dimensions in manifest");
		return false;
	}

	return true;
}

// ---------------------------------------------------------------------------
// LoadPNGImage
// ---------------------------------------------------------------------------

bool FGSSogLoader::LoadPNGImage(
	const FString& ImagePath,
	TArray<uint8>& OutPixels,
	int32& OutWidth,
	int32& OutHeight)
{
	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *ImagePath))
	{
		return false;
	}

	IImageWrapperModule& ImageWrapperModule =
		FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));

	EImageFormat Format = ImageWrapperModule.DetectImageFormat(FileData.GetData(), FileData.Num());
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(Format);

	if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(FileData.GetData(), FileData.Num()))
	{
		return false;
	}

	if (!ImageWrapper->GetRaw(ERGBFormat::RGBA, 8, OutPixels))
	{
		return false;
	}

	OutWidth  = ImageWrapper->GetWidth();
	OutHeight = ImageWrapper->GetHeight();
	return true;
}

// ---------------------------------------------------------------------------
// DecodeAttributes
// ---------------------------------------------------------------------------

bool FGSSogLoader::DecodeAttributes(
	const TArray<uint8>& PosPixels,  int32 PosW,  int32 PosH,
	const TArray<uint8>& ColPixels,  int32 ColW,  int32 ColH,
	const TArray<uint8>& OpaPixels,  int32 OpaW,  int32 OpaH,
	const TArray<uint8>& SclPixels,  int32 SclW,  int32 SclH,
	const TArray<uint8>& RotPixels,  int32 RotW,  int32 RotH,
	int32 NumSplats,
	TArray<FGSSplatData>& OutSplats)
{
	OutSplats.SetNumZeroed(NumSplats);

	// Position: RGBA (A unused), uint8 → normalise → dequantise using the grid bounds
	// SOG stores positions quantised to [0,1] per axis mapped across the bounding box.
	// Without the bounding-box metadata we store the normalised value (0-1 range, in UE cm).
	// For proper reconstruction the manifest should contain min/max bounds – we default to 10m.
	const float kPosRange = 1000.0f; // 10 m in cm, adjust per scene

	for (int32 i = 0; i < NumSplats; i++)
	{
		FGSSplatData& S = OutSplats[i];

		// Position
		if (PosPixels.Num() > 0 && i * 4 + 3 < PosPixels.Num())
		{
			float nx = PosPixels[i*4+0] / 255.0f;
			float ny = PosPixels[i*4+1] / 255.0f;
			float nz = PosPixels[i*4+2] / 255.0f;
			// Map [0,1] → [-0.5, 0.5] × kPosRange
			S.Position = FVector3f(
				(nx - 0.5f) * kPosRange,
				(ny - 0.5f) * kPosRange,
				(nz - 0.5f) * kPosRange);
		}

		// Colour (DC SH)
		if (ColPixels.Num() > 0 && i * 4 + 2 < ColPixels.Num())
		{
			float r = ColPixels[i*4+0] / 255.0f;
			float g = ColPixels[i*4+1] / 255.0f;
			float b = ColPixels[i*4+2] / 255.0f;
			// Inverse-map sRGB → linear DC coefficient
			const float SH_C0 = 0.28209479177387814f;
			S.SHDc = FVector3f((r - 0.5f) / SH_C0, (g - 0.5f) / SH_C0, (b - 0.5f) / SH_C0);
		}

		// Opacity
		if (OpaPixels.Num() > 0 && i * 4 < OpaPixels.Num())
		{
			float Alpha = OpaPixels[i*4] / 255.0f; // linear [0,1]
			// Convert to pre-sigmoid: inv_sigmoid(α) = log(α / (1-α))
			Alpha = FMath::Clamp(Alpha, 1e-5f, 1.0f - 1e-5f);
			S.Opacity = FMath::Loge(Alpha / (1.0f - Alpha));
		}
		else
		{
			S.Opacity = 0.0f; // sigmoid(0)=0.5
		}

		// Scale
		if (SclPixels.Num() > 0 && i * 4 + 2 < SclPixels.Num())
		{
			// Scale stored as uint8 in log-space normalised to [-10, 0]
			float sx = SclPixels[i*4+0] / 255.0f * 10.0f - 10.0f;
			float sy = SclPixels[i*4+1] / 255.0f * 10.0f - 10.0f;
			float sz = SclPixels[i*4+2] / 255.0f * 10.0f - 10.0f;
			S.Scale = FVector3f(sx, sy, sz);
		}
		else
		{
			S.Scale = FVector3f(-3.0f, -3.0f, -3.0f);
		}

		// Rotation
		if (RotPixels.Num() > 0 && i * 4 + 3 < RotPixels.Num())
		{
			float rw = RotPixels[i*4+0] / 127.5f - 1.0f;
			float rx = RotPixels[i*4+1] / 127.5f - 1.0f;
			float ry = RotPixels[i*4+2] / 127.5f - 1.0f;
			float rz = RotPixels[i*4+3] / 127.5f - 1.0f;
			float Len = FMath::Sqrt(rw*rw + rx*rx + ry*ry + rz*rz);
			if (Len > 1e-6f) { rw/=Len; rx/=Len; ry/=Len; rz/=Len; }
			S.Rotation = FQuat4f(rx, -rz, ry, rw);
		}
		else
		{
			S.Rotation = FQuat4f::Identity;
		}
	}

	return true;
}
