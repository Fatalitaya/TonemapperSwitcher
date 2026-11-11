#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Misc/CoreDelegates.h"
#include "ShaderCore.h"
#include "SceneViewExtension.h"
#include "SceneRendering.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "TonemapperSwithcerPass.h"
#include "RenderGraphUtils.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarTonemapperSwithcerMode(
	TEXT("r.TonemapperSwithcer.Mode"), 0,
	TEXT("0: native Unreal tonemapper. 1: Minimal AgX (desktop SDR)."), ECVF_RenderThreadSafe);

// Native fixed-exposure calculation, without the histogram/adaptation-only parameters.
float GetTonemapperSwithcerFixedExposure(const FSceneView& View)
{
	const auto& Settings = View.FinalPostProcessSettings;
	const bool bExtended = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange"))->GetInt() == 1;
	const float Attenuation = IConsoleManager::Get().FindConsoleVariable(TEXT("r.EyeAdaptation.LensAttenuation"))->GetFloat();
	const float LuminanceMax = bExtended ? 0.78f / FMath::Max(Attenuation, 0.01f) : 1.f;
	if (View.Family->ExposureSettings.bFixed)
	{
		return 1.f / FMath::Max(0.0001f / 0.18f, LuminanceMax * FMath::Exp2(View.Family->ExposureSettings.FixedEV100));
	}
	if (!View.Family->EngineShowFlags.EyeAdaptation)
	{
		return 1.f;
	}
	EAutoExposureMethod Method = Settings.AutoExposureMethod;
	const int32 Override = IConsoleManager::Get().FindConsoleVariable(TEXT("r.EyeAdaptation.MethodOverride"))->GetInt();
	if (Override >= 1 && Override <= 3)
	{
		Method = Override == 3 ? AEM_Manual : (Override == 2 ? AEM_Basic : AEM_Histogram);
	}
	float MinLuminance, MaxLuminance;
	if (Method == AEM_Manual)
	{
		const float EV100 = Settings.AutoExposureApplyPhysicalCameraExposure
			? FMath::Log2(FMath::Square(Settings.DepthOfFieldFstop) * Settings.CameraShutterSpeed * 100.f / FMath::Max(1.f, Settings.CameraISO)) : 0.f;
		MinLuminance = MaxLuminance = LuminanceMax * FMath::Exp2(EV100);
	}
	else
	{
		MinLuminance = bExtended ? LuminanceMax * FMath::Exp2(Settings.AutoExposureMinBrightness) : Settings.AutoExposureMinBrightness;
		MaxLuminance = bExtended ? LuminanceMax * FMath::Exp2(Settings.AutoExposureMaxBrightness) : Settings.AutoExposureMaxBrightness;
	}
	MinLuminance = FMath::Min(MinLuminance, MaxLuminance);
	return 0.18f * FMath::Exp2(Settings.AutoExposureBias) / FMath::Max(0.0001f, (MinLuminance + MaxLuminance) * 0.09f);
}

class FTonemapperSwithcerViewExtension final : public FSceneViewExtensionBase
{
public:
	FTonemapperSwithcerViewExtension(const FAutoRegister& AutoRegister) : FSceneViewExtensionBase(AutoRegister) {}

	virtual void SubscribeToPostProcessingPass(EPostProcessingPass Pass, const FSceneView& View,
		FPostProcessingPassDelegateArray& Callbacks, bool bIsPassEnabled) override
	{
		if (Pass != EPostProcessingPass::ReplacingTonemapper || !bIsPassEnabled || CVarTonemapperSwithcerMode.GetValueOnRenderThread() != 1)
		{
			return;
		}
		const auto& Flags = View.Family->EngineShowFlags;
		const auto Device = GetTonemapperOutputDeviceParameters(*View.Family);
		// Unsupported outputs retain the engine's complete native pipeline.
		if (View.GetFeatureLevel() < ERHIFeatureLevel::SM5 || IsMobilePlatform(View.GetShaderPlatform())
			|| !RHIVolumeTextureRenderingSupportGuaranteed(View.GetShaderPlatform())
			|| Device.OutputDevice > static_cast<uint32>(EDisplayOutputFormat::SDR_ExplicitGammaMapping)
			|| !Flags.Tonemapper || !Flags.PostProcessing || !Flags.Lighting || View.Family->UseDebugViewPS()
			|| Flags.VisualizeBuffer || Flags.RayTracingDebug || Flags.VisualizeHDR
			|| View.FinalPostProcessSettings.BloomMethod == BM_FFT)
		{
			return;
		}
		Callbacks.Add(FPostProcessingPassDelegate::CreateRaw(this, &FTonemapperSwithcerViewExtension::Render));
	}

	FScreenPassTexture Render(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs)
	{
		FTSInputs Pass;
		Pass.OverrideOutput = Inputs.OverrideOutput;
		Pass.SceneColor = Inputs.GetInput(EPostProcessMaterialInput::SceneColor);
		const auto Bloom = Inputs.GetInput(EPostProcessMaterialInput::CombinedBloom);
		if (Bloom.IsValid())
		{
			Pass.Bloom = FScreenPassTexture::CopyFromSlice(GraphBuilder, Bloom);
		}
		if (View.HasValidEyeAdaptationBuffer())
		{
			Pass.EyeAdaptationBuffer = GraphBuilder.RegisterExternalBuffer(View.GetEyeAdaptationBuffer());
		}
		// Local-exposure intermediates are not exposed by ReplacingTonemapper.
		FEyeAdaptationParameters EyeAdaptation{};
		Pass.EyeAdaptationParameters = &EyeAdaptation;
		Pass.ColorGradingTexture = BuildTonemapperSwithcerLUT(GraphBuilder, View);
		Pass.bWriteAlphaChannel = View.AntiAliasingMethod == AAM_FXAA || View.AntiAliasingMethod == AAM_SMAA
			|| IConsoleManager::Get().FindConsoleVariable(TEXT("r.PostProcessing.PropagateAlpha"))->GetBool();
		return AddTonemapperSwithcerPass(GraphBuilder, static_cast<const FViewInfo&>(View), Pass);
	}
};

class FTonemapperSwithcerModule final : public IModuleInterface
{
	TSharedPtr<FTonemapperSwithcerViewExtension, ESPMode::ThreadSafe> Extension;
	FDelegateHandle PostEngineInitHandle;
public:
	virtual void StartupModule() override
	{
		const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("TonemapperSwithcer"));
		AddShaderSourceDirectoryMapping(TEXT("/Plugin/TonemapperSwithcer"), FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders")));
		// Shader mapping must happen at PostConfigInit; view extensions need GEngine.
		PostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddLambda([this]()
		{
			Extension = FSceneViewExtensions::NewExtension<FTonemapperSwithcerViewExtension>();
		});
	}
	virtual void ShutdownModule() override
	{
		FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);
		Extension.Reset();
		FlushRenderingCommands();
	}
	virtual bool SupportsDynamicReloading() override { return false; }
};

IMPLEMENT_MODULE(FTonemapperSwithcerModule, TonemapperSwithcer)
