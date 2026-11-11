#pragma once
#include "PostProcess/PostProcessTonemap.h"
struct FTSInputs
{
	// [Optional] Render to the specified output. If invalid, a new texture is created and returned.
	FScreenPassRenderTarget OverrideOutput;

	// [Required] HDR scene color to tonemap.
	FScreenPassTextureSlice SceneColor;

	// [Required] Filtered bloom texture to composite with tonemapped scene color. This should be transparent black for no bloom.
	FScreenPassTexture Bloom;

	// [Optional] structured buffer of multiply parameters to apply to the scene color.
	FRDGBufferRef SceneColorApplyParamaters = nullptr;

	// [Optional] Luminance bilateral grid. If this is null, local exposure is disabled.
	FRDGTextureRef LocalExposureBilateralGridTexture = nullptr;

	// [Optional] Blurred luminance texture used to calculate local exposure.
	FRDGTextureRef BlurredLogLuminanceTexture = nullptr;

	// [Optional] Exposure Fusion. If this is null, local exposure is disabled.
	FScreenPassTextureSlice ExposureFusion;

	// [Optional] Local exposure parameters.
	const FLocalExposureParameters* LocalExposureParameters = nullptr;

	// [Required] Eye adaptation parameters.
	const FEyeAdaptationParameters* EyeAdaptationParameters = nullptr;

	// [Required] Color grading texture used to remap colors.
	FRDGTextureRef ColorGradingTexture = nullptr;

	// Eye adaptation buffer used to compute exposure.
	FRDGBufferRef EyeAdaptationBuffer = nullptr;

	// [Raster Only] Controls whether the alpha channel of the scene texture should be written to the output texture.
	bool bWriteAlphaChannel = false;

	// Configures the tonemapper to only perform gamma correction.
	bool bGammaOnly = false;

	// Whether to leave the final output in HDR.
	bool bOutputInHDR = false;

	bool bMetalMSAAHDRDecode = false;

	// Returns whether ApplyParameters is supported by the tonemapper.
	static bool SupportsSceneColorApplyParametersBuffer(EShaderPlatform Platform);
};


float GetTonemapperSwithcerFixedExposure(const FSceneView& View);
FScreenPassTexture AddTonemapperSwithcerPass(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FTSInputs& Inputs);
FRDGTextureRef BuildTonemapperSwithcerLUT(FRDGBuilder& GraphBuilder, const FSceneView& View);
