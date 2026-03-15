// Copyright 2024, Gaussian Splatting Plugin. All Rights Reserved.

#include "GaussianSplattingAsset.h"
#include "UObject/Package.h"
#include "Misc/SecureHash.h"

UGaussianSplattingAsset::UGaussianSplattingAsset()
{
	BulkSplatData.SetBulkDataFlags(BULKDATA_Force_NOT_InlinePayload);
}

void UGaussianSplattingAsset::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// Metadata is handled by UPROPERTY serialisation above.
	// BulkData serialises itself (may be streamed from separate .ubulk file).
	BulkSplatData.Serialize(Ar, this, INDEX_NONE, false);
}

void UGaussianSplattingAsset::PostLoad()
{
	Super::PostLoad();
}

void UGaussianSplattingAsset::BeginDestroy()
{
	Super::BeginDestroy();
	// BulkData handles its own memory; just make sure nothing references the raw ptr.
}

#if WITH_EDITOR
void UGaussianSplattingAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

const FGSSplatData* UGaussianSplattingAsset::GetSplatData() const
{
	if (NumSplats == 0)
	{
		return nullptr;
	}
	// BulkData must be locked for reading before dereferencing.
	// This is a persistent pointer valid while bulk data remains locked.
	return reinterpret_cast<const FGSSplatData*>(BulkSplatData.LockReadOnly());
}

void UGaussianSplattingAsset::SetSplatData(TArray<FGSSplatData>&& InSplats, int32 InSHDegree, EGSSourceFormat InFormat)
{
	NumSplats  = InSplats.Num();
	SHDegree   = InSHDegree;
	SourceFormat = InFormat;

	// Compute bounding box
	BoundingBox = FBox(ForceInit);
	for (const FGSSplatData& S : InSplats)
	{
		BoundingBox += FVector(S.Position);
	}

	// Store raw bytes in bulk data
	const int64 DataSize = (int64)NumSplats * sizeof(FGSSplatData);
	BulkSplatData.Lock(LOCK_READ_WRITE);
	void* Dst = BulkSplatData.Realloc(DataSize);
	FMemory::Memcpy(Dst, InSplats.GetData(), DataSize);
	BulkSplatData.Unlock();

	MarkPackageDirty();
}
