// Copyright 2024, Gaussian Splatting Plugin. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphResources.h"
#include "RHI.h"

// ---------------------------------------------------------------------------
// Standard (sorted) Vertex Shader
// ---------------------------------------------------------------------------

class FGSSplatVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FGSSplatVS);
	SHADER_USE_PARAMETER_STRUCT(FGSSplatVS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_SRV(StructuredBuffer<FGSSplatGPUData>, SplatBuffer)
		SHADER_PARAMETER_SRV(Buffer<uint>,                       OrderBuffer)
		SHADER_PARAMETER(FMatrix44f, GS_WorldToView)
		SHADER_PARAMETER(FMatrix44f, GS_ViewToClip)
		SHADER_PARAMETER(FMatrix44f, GS_WorldToClip)
		SHADER_PARAMETER(FVector3f,  GS_CameraWorldPos)
		SHADER_PARAMETER(FVector2f,  GS_ViewportSize)
		SHADER_PARAMETER(FVector2f,  GS_FocalLength)
		SHADER_PARAMETER(int32,      GS_SHDegree)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5)
		    || IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES3_1);
	}

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("GS_OIT_MODE"), 0);
	}
};

// ---------------------------------------------------------------------------
// Standard Pixel Shader
// ---------------------------------------------------------------------------

class FGSSplatPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FGSSplatPS);
	SHADER_USE_PARAMETER_STRUCT(FGSSplatPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5)
		    || IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES3_1);
	}

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("GS_OIT_MODE"), 0);
	}
};

// ---------------------------------------------------------------------------
// OIT Accumulation Vertex Shader
// ---------------------------------------------------------------------------

class FGSSplatOITVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FGSSplatOITVS);
	SHADER_USE_PARAMETER_STRUCT(FGSSplatOITVS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_SRV(StructuredBuffer<FGSSplatGPUData>, SplatBuffer)
		SHADER_PARAMETER_SRV(Buffer<uint>,                       OrderBuffer)
		SHADER_PARAMETER(FMatrix44f, GS_WorldToView)
		SHADER_PARAMETER(FMatrix44f, GS_ViewToClip)
		SHADER_PARAMETER(FMatrix44f, GS_WorldToClip)
		SHADER_PARAMETER(FVector3f,  GS_CameraWorldPos)
		SHADER_PARAMETER(FVector2f,  GS_ViewportSize)
		SHADER_PARAMETER(FVector2f,  GS_FocalLength)
		SHADER_PARAMETER(int32,      GS_SHDegree)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5)
		    || IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES3_1);
	}

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("GS_OIT_MODE"), 1);
	}
};

// ---------------------------------------------------------------------------
// OIT Accumulation Pixel Shader
// ---------------------------------------------------------------------------

class FGSSplatOITPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FGSSplatOITPS);
	SHADER_USE_PARAMETER_STRUCT(FGSSplatOITPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5)
		    || IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES3_1);
	}

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("GS_OIT_MODE"), 1);
	}
};

// ---------------------------------------------------------------------------
// OIT Resolve Vertex + Pixel Shaders
// ---------------------------------------------------------------------------

class FGSOITResolveVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FGSOITResolveVS);
	SHADER_USE_PARAMETER_STRUCT(FGSOITResolveVS, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5)
		    || IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES3_1);
	}
};

class FGSOITResolvePS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FGSOITResolvePS);
	SHADER_USE_PARAMETER_STRUCT(FGSOITResolvePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, GS_AccumTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, GS_AlphaTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, GS_LinearSampler)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5)
		    || IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES3_1);
	}
};
