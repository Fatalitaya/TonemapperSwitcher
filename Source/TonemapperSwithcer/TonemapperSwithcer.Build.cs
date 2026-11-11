using UnrealBuildTool;
using System.IO;
public class TonemapperSwithcer : ModuleRules
{
 public TonemapperSwithcer(ReadOnlyTargetRules Target) : base(Target)
 {
  PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
  PrivateDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "Renderer", "RenderCore", "RHI", "Projects" });
  // Copied native pass uses FViewInfo and shader parameter declarations from this exact engine version.
  PrivateIncludePaths.Add(Path.Combine(EngineDirectory, "Source/Runtime/Renderer/Private"));
 }
}
