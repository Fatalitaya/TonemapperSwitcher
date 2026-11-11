// Adapted from UE 5.8.3 and UE5-AgxTonemapper (see NOTICE.md).
#include "TonemapperSwithcerLUT.h"
#include "TonemapperSwithcerPass.h"
#include "Engine/Texture.h"
#include "TextureResource.h"
#include "SceneView.h"
#include "SceneManagement.h"
#include "HAL/IConsoleManager.h"
IMPLEMENT_GLOBAL_SHADER(FTonemapperBuildAgXOuterLUTCS, "/Plugin/TonemapperSwithcer/Private/TonemapperBuildAgXOuterLUT.usf", "MainCS", SF_Compute);
static uint32 GenerateFinalTable(const FFinalPostProcessSettings& Settings, const FTexture* OutTextures[], float OutWeights[], uint32 MaxCount)
{
	// Find the n strongest contributors, drop small contributors.
	uint32 LocalCount = 1;

	// Add the neutral one (done in the shader) as it should be the first and always there.
	OutTextures[0] = nullptr;
	OutWeights[0] = 0.0f;

	// Neutral index is the entry with no LUT texture assigned.
	for (int32 Index = 0; Index < Settings.ContributingLUTs.Num(); ++Index)
	{
		if (Settings.ContributingLUTs[Index].LUTTexture == nullptr)
		{
			OutWeights[0] = Settings.ContributingLUTs[Index].Weight;
			break;
		}
	}

	float OutWeightsSum = OutWeights[0];
	for (; LocalCount < MaxCount; ++LocalCount)
	{
		int32 BestIndex = INDEX_NONE;

		// Find the one with the strongest weight, add until full.
		for (int32 InputIndex = 0; InputIndex < Settings.ContributingLUTs.Num(); ++InputIndex)
		{
			bool bAlreadyInArray = false;

			{
				UTexture* LUTTexture = Settings.ContributingLUTs[InputIndex].LUTTexture;
				FTexture* Internal = LUTTexture ? LUTTexture->GetResource() : nullptr;
				for (uint32 OutputIndex = 0; OutputIndex < LocalCount; ++OutputIndex)
				{
					if (Internal == OutTextures[OutputIndex])
					{
						bAlreadyInArray = true;
						break;
					}
				}
			}

			if (bAlreadyInArray)
			{
				// We already have this one.
				continue;
			}

			// Take the first or better entry.
			if (BestIndex == INDEX_NONE || Settings.ContributingLUTs[BestIndex].Weight <= Settings.ContributingLUTs[InputIndex].Weight)
			{
				BestIndex = InputIndex;
			}
		}

		if (BestIndex == INDEX_NONE)
		{
			// No more elements to process.
			break;
		}

		const float WeightThreshold = 1.0f / 512.0f;

		const float BestWeight = Settings.ContributingLUTs[BestIndex].Weight;

		if (BestWeight < WeightThreshold)
		{
			// Drop small contributor
			break;
		}

		UTexture* BestLUTTexture = Settings.ContributingLUTs[BestIndex].LUTTexture;
		FTexture* BestInternal = BestLUTTexture ? BestLUTTexture->GetResource() : nullptr;

		OutTextures[LocalCount] = BestInternal;
		OutWeights[LocalCount] = BestWeight;
		OutWeightsSum += BestWeight;
	}

	// Normalize the weights.
	if (OutWeightsSum > 0.001f)
	{
		const float OutWeightsSumInverse = 1.0f / OutWeightsSum;

		for (uint32 Index = 0; Index < LocalCount; ++Index)
		{
			OutWeights[Index] *= OutWeightsSumInverse;
		}
	}
	else
	{
		// Just the neutral texture at full weight.
		OutWeights[0] = 1.0f;
		LocalCount = 1;
	}

	return LocalCount;
}


FRDGTextureRef BuildTonemapperSwithcerLUT(FRDGBuilder& GraphBuilder, const FSceneView& View)
{
 const auto Device = GetTonemapperOutputDeviceParameters(*View.Family);
 const uint32 LUTSize = FMath::Clamp(IConsoleManager::Get().FindConsoleVariable(TEXT("r.LUT.Size"))->GetInt(), 4, 128);
 const auto Desc = FRDGTextureDesc::Create3D(FIntVector(LUTSize), PF_FloatRGBA, FClearValueBinding::None, TexCreate_ShaderResource | TexCreate_UAV);
 FRDGTextureRef OutputTexture = GraphBuilder.CreateTexture(Desc, TEXT("TonemapperSwithcer.AgXLUT"));
 const bool bColorGradingEnabled = View.Family->EngineShowFlags.ColorGrading && IConsoleManager::Get().FindConsoleVariable(TEXT("r.Color.Grading"))->GetInt() != 0;
 const FTexture* Textures[5] = {};
 float Weights[5] = {1.f, 0.f, 0.f, 0.f, 0.f};
 const uint32 BlendCount = View.Family->EngineShowFlags.ColorGrading ? GenerateFinalTable(View.FinalPostProcessSettings, Textures, Weights, 5) : 1;
 const float Min = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Color.Min"))->GetFloat();
 const float Mid = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Color.Mid"))->GetFloat();
 const float Max = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Color.Max"))->GetFloat();
 const FVector3f MappingPolynomial(2.f * Min - 4.f * Mid + 2.f * Max, -3.f * Min + 4.f * Mid - Max, Min);
	FTonemapperBuildAgXOuterLUTCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FTonemapperBuildAgXOuterLUTCS::FBlendCount>(BlendCount);
	TShaderMapRef<FTonemapperBuildAgXOuterLUTCS> ComputeShader(GetGlobalShaderMap(View.GetFeatureLevel()), PermutationVector);

	FTonemapperBuildAgXOuterLUTCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTonemapperBuildAgXOuterLUTCS::FParameters>();
	PassParameters->WorkingColorSpace = GDefaultWorkingColorSpaceUniformBuffer.GetUniformBufferRef();

	for (uint32 Index = 0; Index < GMaxTonemappingLUTBlendCount; ++Index)
	{
		PassParameters->Textures[Index] = nullptr;
		PassParameters->Samplers[Index] = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp, 0, 1>::GetRHI();
		GET_SCALAR_ARRAY_ELEMENT(PassParameters->LUTWeights, Index) = 0.f;
	}

	for (uint32 Index = 0; Index < BlendCount; ++Index)
	{

		if (Index != 0)
		{
			PassParameters->Textures[Index] = Textures[Index]->TextureRHI;
		}
		GET_SCALAR_ARRAY_ELEMENT(PassParameters->LUTWeights, Index) = Weights[Index];
	}

	const FPostProcessSettings& Settings = bColorGradingEnabled
		? View.FinalPostProcessSettings
		: FPostProcessSettings::GetDefault();

	PassParameters->OverlayColor = FVector4f(View.OverlayColor);
	PassParameters->ColorScale = FVector3f(View.ColorScale);

	PassParameters->ColorSaturation = FVector4f(Settings.ColorSaturation);
	PassParameters->ColorContrast = FVector4f(Settings.ColorContrast);
	PassParameters->ColorGamma = FVector4f(Settings.ColorGamma);
	PassParameters->ColorGain = FVector4f(Settings.ColorGain);
	PassParameters->ColorOffset = FVector4f(Settings.ColorOffset);

	PassParameters->ColorSaturationShadows = FVector4f(Settings.ColorSaturationShadows);
	PassParameters->ColorContrastShadows = FVector4f(Settings.ColorContrastShadows);
	PassParameters->ColorGammaShadows = FVector4f(Settings.ColorGammaShadows);
	PassParameters->ColorGainShadows = FVector4f(Settings.ColorGainShadows);
	PassParameters->ColorOffsetShadows = FVector4f(Settings.ColorOffsetShadows);

	PassParameters->ColorSaturationMidtones = FVector4f(Settings.ColorSaturationMidtones);
	PassParameters->ColorContrastMidtones = FVector4f(Settings.ColorContrastMidtones);
	PassParameters->ColorGammaMidtones = FVector4f(Settings.ColorGammaMidtones);
	PassParameters->ColorGainMidtones = FVector4f(Settings.ColorGainMidtones);
	PassParameters->ColorOffsetMidtones = FVector4f(Settings.ColorOffsetMidtones);

	PassParameters->ColorSaturationHighlights = FVector4f(Settings.ColorSaturationHighlights);
	PassParameters->ColorContrastHighlights = FVector4f(Settings.ColorContrastHighlights);
	PassParameters->ColorGammaHighlights = FVector4f(Settings.ColorGammaHighlights);
	PassParameters->ColorGainHighlights = FVector4f(Settings.ColorGainHighlights);
	PassParameters->ColorOffsetHighlights = FVector4f(Settings.ColorOffsetHighlights);

	PassParameters->ColorCorrectionShadowsMax = Settings.ColorCorrectionShadowsMax;
	PassParameters->ColorCorrectionHighlightsMin = Settings.ColorCorrectionHighlightsMin;
	PassParameters->ColorCorrectionHighlightsMax = Settings.ColorCorrectionHighlightsMax;

	PassParameters->WhiteTemp = Settings.WhiteTemp;
	PassParameters->WhiteTint = Settings.WhiteTint;
	PassParameters->bIsTemperatureWhiteBalance = static_cast<uint32>(Settings.TemperatureType == ETemperatureMethod::TEMP_WhiteBalance);

	PassParameters->BlueCorrection = Settings.BlueCorrection;
	PassParameters->ExpandGamut = Settings.ExpandGamut;
	PassParameters->ToneCurveAmount = Settings.ToneCurveAmount;

	PassParameters->FilmSlope = Settings.FilmSlope;
	PassParameters->FilmToe = Settings.FilmToe;
	PassParameters->FilmShoulder = Settings.FilmShoulder;
	PassParameters->FilmBlackClip = Settings.FilmBlackClip;
	PassParameters->FilmWhiteClip = Settings.FilmWhiteClip;

	PassParameters->LUTInvMax = 1.0f;
	PassParameters->InvLUTSizeMinusOne = 1.f / static_cast<float>(LUTSize - 1);
	PassParameters->TonemappingSDRInvEOTF = 0;
	PassParameters->TonemappingMethod = 0;
	PassParameters->MappingPolynomial = MappingPolynomial;

	PassParameters->OutputDevice = static_cast<uint32>(Device.OutputDevice);
	PassParameters->OutputGamut = static_cast<uint32>(Device.OutputGamut);
	PassParameters->OutputMaxLuminance = Device.OutputMaxLuminance;
	PassParameters->LUTShaper = Device.LUTShaper;
	PassParameters->InverseGamma = Device.InverseGamma;

	PassParameters->RWOutputTexture = GraphBuilder.CreateUAV(OutputTexture);

	const uint32 GroupSize = FMath::DivideAndRoundUp<int32>(static_cast<int32>(LUTSize), FTonemapperBuildAgXOuterLUTCS::GroupSize);
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TonemapperSwithcer.AgXLUT %u", LUTSize),
		ERDGPassFlags::Compute,
		ComputeShader,
		PassParameters,
		FIntVector(GroupSize, GroupSize, GroupSize));

 return OutputTexture;
}
