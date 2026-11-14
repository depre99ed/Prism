param(
    [ValidateSet('x64','Win32')][string]$Platform = 'x64',
    [switch]$Preview,
    [switch]$Tests
)
$ErrorActionPreference = 'Stop'
$glassRoot = $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio avec les outils C++ est requis.' }
$vsRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vsRoot) { throw 'Les outils de compilation C++ de Visual Studio sont introuvables.' }
$msbuild = Join-Path $vsRoot 'MSBuild\Current\Bin\MSBuild.exe'
$toolsets = Get-ChildItem -LiteralPath (Join-Path $vsRoot 'MSBuild\Microsoft\VC') -Recurse -Directory |
    Where-Object { $_.Parent.Name -eq 'PlatformToolsets' -and $_.Name -match '^v\d+$' } |
    Sort-Object { [int]($_.Name.Substring(1)) } -Descending
$toolset = $toolsets[0].Name
$sdkRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Include'
$sdk = (Get-ChildItem -LiteralPath $sdkRoot -Directory | Where-Object { $_.Name -match '^10\.0\.' } |
    Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1).Name
if (!$sdk -or !$toolset) { throw 'Un Windows SDK et un toolset MSVC sont requis.' }
if ($Preview -or $Tests) { $Platform = 'x64' }
$project = if ($Preview) { 'preview\Preview.vcxproj' } elseif ($Tests) { 'tests\Tests.vcxproj' } else { 'src\Xenos.vcxproj' }
& $msbuild (Join-Path $glassRoot $project) /m /nologo /v:minimal /p:Configuration=Release "/p:Platform=$Platform" "/p:PlatformToolset=$toolset" "/p:WindowsTargetPlatformVersion=$sdk"
if ($LASTEXITCODE -ne 0) { throw "Echec de compilation : $LASTEXITCODE" }
if ($Tests) {
    $data = Join-Path $glassRoot '.build\test-data'
    New-Item -ItemType Directory -Force -Path $data | Out-Null
    & (Join-Path $glassRoot '.build\tests\Tests.exe') $data
    if ($LASTEXITCODE -ne 0) { throw 'Les tests de profils ont echoue.' }
}
