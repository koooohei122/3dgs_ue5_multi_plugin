// Copyright 2024, Gaussian Splatting Plugin. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class FGaussianSplattingModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static FGaussianSplattingModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FGaussianSplattingModule>("GaussianSplatting");
	}

	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("GaussianSplatting");
	}
};
