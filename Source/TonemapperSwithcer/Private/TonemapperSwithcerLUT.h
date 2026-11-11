// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DataDrivenShaderPlatformInfo.h"
#include "GlobalShader.h"
inline constexpr uint32 GMaxTonemappingLUTBlendCount = 5;
#include "RenderGraphUtils.h"
#include "SceneManagement.h"

BEGIN_SHADER_PARAMETER_STRUCT(FTonemapperOuterLUTShaderParameters, )
	SHADER_PARAMETER_STRUCT_REF(FWorkingColorSpaceShaderParameters, WorkingColorSpace)
	SHADER_PARAMETER_TEXTURE_ARRAY(Texture2D, Textures, [GMaxTonemappingLUTBlendCount])
	SHADER_PARAMETER_SAMPLER_ARRAY(SamplerState, Samplers, [GMaxTonemappingLUTBlendCount])
	SHADER_PARAMETER_SCALAR_ARRAY(float, LUTWeights, [GMaxTonemappingLUTBlendCount])
	SHADER_PARAMETER(FVector4f, OverlayColor)
	SHADER_PARAMETER(FVector3f, ColorScale)
	SHADER_PARAMETER(FVector4f, ColorSaturation)
	SHADER_PARAMETER(FVector4f, ColorContrast)
	SHADER_PARAMETER(FVector4f, ColorGamma)
	SHADER_PARAMETER(FVector4f, ColorGain)
	SHADER_PARAMETER(FVector4f, ColorOffset)
	SHADER_PARAMETER(FVector4f, ColorSaturationShadows)
	SHADER_PARAMETER(FVector4f, ColorContrastShadows)
	SHADER_PARAMETER(FVector4f, ColorGammaShadows)
	SHADER_PARAMETER(FVector4f, ColorGainShadows)
	SHADER_PARAMETER(FVector4f, ColorOffsetShadows)
	SHADER_PARAMETER(FVector4f, ColorSaturationMidtones)
	SHADER_PARAMETER(FVector4f, ColorContrastMidtones)
	SHADER_PARAMETER(FVector4f, ColorGammaMidtones)
	SHADER_PARAMETER(FVector4f, ColorGainMidtones)
	SHADER_PARAMETER(FVector4f, ColorOffsetMidtones)
	SHADER_PARAMETER(FVector4f, ColorSaturationHighlights)
	SHADER_PARAMETER(FVector4f, ColorContrastHighlights)
	SHADER_PARAMETER(FVector4f, ColorGammaHighlights)
	SHADER_PARAMETER(FVector4f, ColorGainHighlights)
	SHADER_PARAMETER(FVector4f, ColorOffsetHighlights)
	SHADER_PARAMETER(float, LUTInvMax)
	SHADER_PARAMETER(float, InvLUTSizeMinusOne)
	SHADER_PARAMETER(float, WhiteTemp)
	SHADER_PARAMETER(float, WhiteTint)
	SHADER_PARAMETER(float, ColorCorrectionShadowsMax)
	SHADER_PARAMETER(float, ColorCorrectionHighlightsMin)
	SHADER_PARAMETER(float, ColorCorrectionHighlightsMax)
	// Filmic-only fields are retained in the shared Outer LUT parameter struct for Identity.
	// Minimal AgX intentionally does not consume them.
	SHADER_PARAMETER(float, BlueCorrection)
	SHADER_PARAMETER(float, ExpandGamut)
	SHADER_PARAMETER(float, ToneCurveAmount)
	SHADER_PARAMETER(uint32, TonemappingSDRInvEOTF)
	SHADER_PARAMETER(uint32, bIsTemperatureWhiteBalance)
	SHADER_PARAMETER(uint32, TonemappingMethod)
	SHADER_PARAMETER(FVector3f, MappingPolynomial)
	SHADER_PARAMETER(uint32, OutputDevice)
	SHADER_PARAMETER(uint32, OutputGamut)
	SHADER_PARAMETER(float, OutputMaxLuminance)
	SHADER_PARAMETER(uint32, LUTShaper)
	SHADER_PARAMETER(FVector3f, InverseGamma)
	SHADER_PARAMETER(float, FilmSlope)
	SHADER_PARAMETER(float, FilmToe)
	SHADER_PARAMETER(float, FilmShoulder)
	SHADER_PARAMETER(float, FilmBlackClip)
	SHADER_PARAMETER(float, FilmWhiteClip)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, RWOutputTexture)
END_SHADER_PARAMETER_STRUCT()

class FTonemapperBuildAgXOuterLUTCS : public FGlobalShader
{
public:
	static constexpr int32 GroupSize = 4;

	class FBlendCount : SHADER_PERMUTATION_RANGE_INT("TONEMAPPER_LUT_BLEND_COUNT", 1, GMaxTonemappingLUTBlendCount);
	using FPermutationDomain = TShaderPermutationDomain<FBlendCount>;

	DECLARE_GLOBAL_SHADER(FTonemapperBuildAgXOuterLUTCS);
	SHADER_USE_PARAMETER_STRUCT(FTonemapperBuildAgXOuterLUTCS, FGlobalShader);
	using FParameters = FTonemapperOuterLUTShaderParameters;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5)
			&& RHIVolumeTextureRenderingSupportGuaranteed(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GroupSize);
	}
};
