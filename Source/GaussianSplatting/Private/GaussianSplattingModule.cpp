// Copyright 2024, Gaussian Splatting Plugin. All Rights Reserved.

#include "GaussianSplattingModule.h"
#include "Rendering/GSViewExtension.h"
#include "Interfaces/IPluginManager.h"
#include "ShaderCore.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FGaussianSplattingModule"

void FGaussianSplattingModule::StartupModule()
{
	// Register virtual shader path so #include "/Plugin/GaussianSplatting/..." works
	FString PluginBaseDir = IPluginManager::Get().FindPlugin(TEXT("GaussianSplatting"))->GetBaseDir();
	FString ShaderDir = FPaths::Combine(PluginBaseDir, TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/GaussianSplatting"), ShaderDir);

	// Eagerly initialize the view extension so it is registered with the
	// renderer before any actor is placed in the world (including PIE start).
	FGSViewExtension::Get();
}

void FGaussianSplattingModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGaussianSplattingModule, GaussianSplatting)
