$ErrorActionPreference = 'Stop'
$pluginDir = Split-Path $PSScriptRoot -Parent
$engineDir = [IO.Path]::GetFullPath((Join-Path $pluginDir '../../..'))
& (Join-Path $pluginDir 'BuildLocal.ps1')
$verificationDir = Join-Path $pluginDir 'Saved/Verification'
New-Item -ItemType Directory -Path $verificationDir -Force | Out-Null
$projectPath = Join-Path $verificationDir 'TonemapperSwithcerTest.uproject'
$project = @{
    FileVersion = 3
    DisableEnginePluginsByDefault = $true
    Plugins = @(
        @{ Name = 'TonemapperSwithcer'; Enabled = $true },
        @{ Name = 'PythonScriptPlugin'; Enabled = $true },
        @{ Name = 'EditorScriptingUtilities'; Enabled = $true }
    )
} | ConvertTo-Json -Depth 5
[IO.File]::WriteAllText($projectPath, $project, [Text.UTF8Encoding]::new($false))
$editor = Join-Path $engineDir 'Binaries/Win64/UnrealEditor-Cmd.exe'
$script = Join-Path $PSScriptRoot 'render_smoke.py'
$log = Join-Path $pluginDir 'Saved/render-smoke-sm6.log'
& $editor $projectPath -run=pythonscript "-script=$script" -AllowCommandletRendering -unattended -RenderOffscreen -NoSound -nosplash -dx12 -sm6 -stdout -FullStdOutLogOutput *> $log
if ($LASTEXITCODE -ne 0) { throw "Render smoke test failed ($LASTEXITCODE). See $log" }
if (-not (Select-String -LiteralPath $log -SimpleMatch 'TONEMAPPER_SWITHCER_SMOKE_PASSED' -Quiet)) {
    throw "Render smoke test did not complete. See $log"
}
Get-Content -LiteralPath (Join-Path $verificationDir 'Saved/TonemapperSwithcerSmoke.json')
