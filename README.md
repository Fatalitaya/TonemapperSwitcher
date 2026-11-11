# TonemapperSwithcer — UE 5.8.3

不修改引擎源码或覆盖引擎 Shader 的桌面 SDR Tonemapper 插件。插件名按项目约定保留 `Swithcer` 拼写。

## 使用

放在 `Engine/Plugins/Runtime/TonemapperSwithcer`，编译后重启编辑器。插件默认启用，但默认使用原生 Tonemapper。

```text
r.TonemapperSwithcer.Mode 1   // Minimal AgX
r.TonemapperSwithcer.Mode 0   // 恢复原生 UE Tonemapper
```

也可以在项目 `Config/DefaultEngine.ini` 中设置：

```ini
[SystemSettings]
r.TonemapperSwithcer.Mode=1
```

模式 0 不注册替换回调，原生路径、项目所选 Filmic / ACES 和其他替换材质恢复正常。模式 1 在 `ReplacingTonemapper` 接管整个 Pass；多个替换扩展并存时，引擎只执行最高优先级的一个。

## 实现与支持范围

- 原生 5.8.3 桌面 Tonemap Pass 的 C++ 和 Shader 复制到插件内，保留 raster / compute、PreExposure、当前帧 Eye Adaptation、普通 Bloom / Dirt Mask、Scene Color Tint、暗角、色差、锐化、Film Grain、抖动和 Alpha 处理。
- 插件构建自己的 3D AgX LUT，保留白平衡、全局及阴影／中间调／高光调色、贡献 LUT 混合和 SDR 输出编码。LUT 每视图每帧构建，不读写引擎的 Tonemapping LUT 缓存。
- AgX 使用参考仓库的 **Minimal AgX 解析近似、Base look**，不是 Blender OCIO AgX 的逐像素复刻。UE Film Slope / Toe / Shoulder / Blue Correction / Tone Curve Amount 不控制这条 AgX 曲线。
- HDR 输出、移动端、卷积 Bloom、所列调试视图自动保留原生路径。SDR SceneCapture 的 Final Color LDR 可使用 AgX。
- **AgX 模式不应用 Local Exposure**：替换回调没有提供其双边网格、模糊亮度或融合纹理。需要局部曝光时切回原生模式；AgX 对比测试应将 Highlight/Shadow Contrast Scale、Detail Strength 设为 1，关闭对比度曲线。原生模式不受影响。
- 不输出原生 Tonemap 的 VRS 亮度历史。VR、多视图、打包平台需在目标项目中验证。

插件使用当前版本的 Renderer 私有头文件来复用 `FViewInfo` 和参数结构，但不依赖新增引擎导出符号。升级引擎需要同步检查复制的代码。

## 构建与验证

Windows 源码引擎可在插件目录执行：

```powershell
powershell -ExecutionPolicy Bypass -File .\BuildLocal.ps1
```

脚本只编译此编辑器模块，并为本机引擎生成模块加载清单。常规项目／引擎构建会由 UBT 生成清单；不要把本机 `Binaries` 复制给不同构建的引擎。

执行 `Tests/RunSmoke.ps1` 可自动构建并创建最小测试工程，在 DX12 / SM6 下运行验证。`Tests/render_smoke.py` 创建 HDR 自发光物体，以真实 GPU SceneCapture 检查模式切换、原生恢复、曝光、黑色输入、非整组 LUT 尺寸、compute / raster 输出和 HDR 回退。测试结果写入测试工程的 `Saved/TonemapperSwithcerSmoke.json`。测试工具依赖 PythonScriptPlugin 和 EditorScriptingUtilities，插件本身不依赖它们。

参考实现来自 UE5-AgxTonemapper 的 `56b79b05`；本插件 Git 基线是远程 `5.8.3` 分支。来源和许可证见 `NOTICE.md`。
