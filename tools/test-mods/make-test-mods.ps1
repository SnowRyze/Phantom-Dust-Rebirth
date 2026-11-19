# Builds the mod loader's test plugin mods (Conceptual, step 8) into out\test-mods\:
#   test-plugin.zip       plugins/test_plugin.dll: prints "[test_plugin] loaded by PDPL ..." (shows in pdpl.log / pdnet.log)
#   test-plugin-core.zip  plugins/test_plugin_core.dll: also imports pd_loader_core.dll's plugin_get_module_handle; PDPL
#                         1.2.2 exports nothing, so it should NOT load, and pdlaunch should warn about it in the Mods list
# Put them in LocalState\pdnet\mods (copy, don't move). Needs Visual Studio's C++ tools, as build.bat does.
#   powershell -File tools\test-mods\make-test-mods.ps1
$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$out = Join-Path $root 'out\test-mods'
$work = Join-Path $out 'build'
New-Item -ItemType Directory -Force $work | Out-Null

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw "Visual Studio's C++ tools weren't found." }
foreach ($line in (cmd /c "`"$vs\VC\Auxiliary\Build\vcvarsall.bat`" x64 >nul 2>&1 && set")) { if ($line -match '^([^=]+)=(.*)$') { Set-Item "env:$($Matches[1])" $Matches[2] } }

Push-Location $work
try {
    # An import library naming pd_loader_core.dll's function, as a plugin built against PDPL's plugin_dev kit would use.
    Set-Content 'pd_loader_core.def' "LIBRARY pd_loader_core.dll`r`nEXPORTS`r`n    plugin_get_module_handle`r`n" -Encoding ASCII
    & lib.exe /nologo /def:pd_loader_core.def /out:pd_loader_core.lib /machine:x64 | Out-Null
    $src = Join-Path $PSScriptRoot 'test_plugin.c'
    & cl.exe /nologo /LD /MT /O2 /W3 $src /Fetest_plugin.dll /link /NOIMPLIB /NOEXP | Out-Null
    if ($LASTEXITCODE) { throw 'test_plugin.dll failed to build.' }
    & cl.exe /nologo /LD /MT /O2 /W3 /DTEST_PLUGIN_CORE $src /Fetest_plugin_core.dll /link pd_loader_core.lib /NOIMPLIB /NOEXP | Out-Null
    if ($LASTEXITCODE) { throw 'test_plugin_core.dll failed to build.' }
}
finally { Pop-Location }

# Each mod: plugins/<dll> and a pdmod.json, zipped with "/" names.
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
function MakeMod([string]$zipName, [string]$dll, [string]$id, [string]$name, [string]$description) {
    $zipPath = Join-Path $out $zipName
    if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
    $zip = [IO.Compression.ZipFile]::Open($zipPath, 'Create')
    try {
        [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, (Join-Path $work $dll), "plugins/$dll")
        $json = "{ ""id"": ""$id"", ""name"": ""$name"", ""version"": ""1.0"", ""author"": ""Rebirth"", ""description"": ""$description"", ""online"": ""local"" }"
        $entry = $zip.CreateEntry('pdmod.json')
        $writer = New-Object IO.StreamWriter($entry.Open(), (New-Object Text.UTF8Encoding $false))
        $writer.Write($json); $writer.Dispose()
    }
    finally { $zip.Dispose() }
    Write-Host "  $zipPath"
}
Write-Host 'Test plugin mods:'
MakeMod 'test-plugin.zip' 'test_plugin.dll' 'test-plugin' 'Test Plugin' 'Prints a line to pdpl.log when PDPL loads it.'
MakeMod 'test-plugin-core.zip' 'test_plugin_core.dll' 'test-plugin-core' 'Test Plugin (core)' 'Uses a pd_loader_core.dll function; PDPL 1.2.2 should refuse it.'
