$ErrorActionPreference = 'Stop'
$engineDir = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$build = Join-Path $engineDir 'Build/BatchFiles/Build.bat'
& $build UnrealEditor Win64 Development -Module=TonemapperSwithcer -NoHotReloadFromIDE -NoUBA
if ($LASTEXITCODE -ne 0) { throw "TonemapperSwithcer build failed ($LASTEXITCODE)." }

# -Module builds omit UBT's target-wide WriteMetadata action. This DLL was linked
# against this engine's import libraries, so use its existing BuildId for loading.
$engineManifest = Get-Content -LiteralPath (Join-Path $engineDir 'Binaries/Win64/UnrealEditor.modules') -Raw | ConvertFrom-Json
$manifest = @{
    BuildId = $engineManifest.BuildId
    Modules = @{ TonemapperSwithcer = 'UnrealEditor-TonemapperSwithcer.dll' }
} | ConvertTo-Json
$manifestPath = Join-Path $PSScriptRoot 'Binaries/Win64/UnrealEditor.modules'
[IO.File]::WriteAllText($manifestPath, $manifest, [Text.UTF8Encoding]::new($false))
