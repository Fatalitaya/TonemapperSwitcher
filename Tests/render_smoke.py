"""Run with UnrealEditor-Cmd -run=pythonscript -script=... -AllowCommandletRendering.
Requires PythonScriptPlugin and EditorScriptingUtilities in a disposable project.
"""
import unreal as u
import math
import json
from pathlib import Path

world = u.EditorLoadingAndSavingUtils.new_blank_map(False)
mat = u.AssetToolsHelpers.get_asset_tools().create_asset("TS_Emissive", "/Game/TSVerification", u.Material, u.MaterialFactoryNew())
mat.set_editor_property("shading_model", u.MaterialShadingModel.MSM_UNLIT)
expr = u.MaterialEditingLibrary.create_material_expression(mat, u.MaterialExpressionConstant3Vector)
expr.set_editor_property("constant", u.LinearColor(4.0, 0.5, 0.1, 1.0))
u.MaterialEditingLibrary.connect_material_property(expr, "", u.MaterialProperty.MP_EMISSIVE_COLOR)
u.MaterialEditingLibrary.recompile_material(mat)
u.SystemLibrary.execute_console_command(world, "Editor.AsyncAssetCompilationFinishAll")
actor = u.EditorLevelLibrary.spawn_actor_from_class(u.StaticMeshActor, u.Vector(300, 0, 0))
actor.static_mesh_component.set_static_mesh(u.load_asset("/Engine/BasicShapes/Cube"))
actor.static_mesh_component.set_material(0, mat)
u.SystemLibrary.execute_console_command(world, "Editor.AsyncAssetCompilationFinishAll")
actor.set_actor_scale3d(u.Vector(2, 2, 2))
camera = u.EditorLevelLibrary.spawn_actor_from_class(u.SceneCapture2D, u.Vector(0, 0, 0), u.Rotator(0, 0, 0))
cap = camera.capture_component2d
cap.set_editor_property("capture_every_frame", False)
cap.set_editor_property("capture_on_movement", False)
cap.set_editor_property("always_persist_rendering_state", True)
cap.set_editor_property("capture_source", u.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
rt = u.RenderingLibrary.create_render_target2d(world, 128, 128, u.TextureRenderTargetFormat.RTF_RGBA16F,
    u.LinearColor(0, 0, 0, 0), False, True)
cap.texture_target = rt
settings = cap.post_process_settings
settings.override_auto_exposure_method = True
settings.auto_exposure_method = u.AutoExposureMethod.AEM_MANUAL
settings.override_auto_exposure_apply_physical_camera_exposure = True
settings.auto_exposure_apply_physical_camera_exposure = False
settings.override_auto_exposure_bias = True
settings.auto_exposure_bias = 0.0
settings.override_bloom_intensity = True
settings.bloom_intensity = 0.0
settings.override_vignette_intensity = True
settings.vignette_intensity = 0.0
settings.override_local_exposure_highlight_contrast_scale = True
settings.local_exposure_highlight_contrast_scale = 1.0
settings.override_local_exposure_shadow_contrast_scale = True
settings.local_exposure_shadow_contrast_scale = 1.0
cap.post_process_settings = settings
cap.post_process_blend_weight = 1.0

def command(text):
    u.SystemLibrary.execute_console_command(world, text)

def sample(mode):
    command("r.TonemapperSwithcer.Mode " + str(mode))
    for _ in range(3):
        cap.capture_scene()
        pixel = u.RenderingLibrary.read_render_target_raw_pixel(world, rt, 64, 64, False)
    rgb = [pixel.r, pixel.g, pixel.b]
    assert all(math.isfinite(x) for x in rgb), rgb
    return rgb

command("r.AntiAliasingMethod 0")
command("r.Tonemapper.Sharpen 0")
command("r.BackbufferQuantizationDitheringOverride 16")
command("r.PostProcessing.PreferCompute 1")
results = {}
results["native"] = sample(0)
results["agx"] = sample(1)
black = u.RenderingLibrary.read_render_target_raw_pixel(world, rt, 0, 0, False)
assert all(math.isfinite(x) and abs(x) < 0.01 for x in [black.r, black.g, black.b]), black
results["restored"] = sample(0)
assert max(results["native"]) > 0.05, results
assert max(abs(a-b) for a,b in zip(results["native"], results["agx"])) > 0.01, results
assert max(abs(a-b) for a,b in zip(results["native"], results["restored"])) < 0.005, results
settings.auto_exposure_bias = -2.0
cap.post_process_settings = settings
results["agx_minus_2ev"] = sample(1)
assert sum(results["agx_minus_2ev"]) < sum(results["agx"]) - 0.02, results
# Non-multiple-of-four LUT size exercises dispatch bounds.
command("r.LUT.Size 31")
results["agx_lut31"] = sample(1)
assert max(abs(a-b) for a,b in zip(results["agx_minus_2ev"], results["agx_lut31"])) < 0.05, results
command("r.LUT.Size 32")
command("r.PostProcessing.PreferCompute 0")
results["agx_raster"] = sample(1)
assert max(abs(a-b) for a,b in zip(results["agx_minus_2ev"], results["agx_raster"])) < 0.005, results
cap.set_editor_property("capture_source", u.SceneCaptureSource.SCS_FINAL_COLOR_HDR)
results["hdr_native"] = sample(0)
results["hdr_fallback"] = sample(1)
assert max(abs(a-b) for a,b in zip(results["hdr_native"], results["hdr_fallback"])) < 0.005, results
command("r.TonemapperSwithcer.Mode 0")
out = Path(u.Paths.project_saved_dir()) / "TonemapperSwithcerSmoke.json"
out.write_text(json.dumps(results, indent=2))
u.log("TONEMAPPER_SWITHCER_SMOKE_PASSED " + json.dumps(results))
