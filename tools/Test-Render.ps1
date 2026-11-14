param()
$ErrorActionPreference = 'Stop'
$prismRoot = Split-Path -Parent $PSScriptRoot
$prismVswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path -LiteralPath $prismVswhere)) { throw 'Visual Studio C++ build tools are required.' }
$prismVsRoot = & $prismVswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$prismVsRoot) { throw 'Visual Studio C++ build tools were not found.' }
$prismMsbuild = Join-Path $prismVsRoot 'MSBuild\Current\Bin\MSBuild.exe'
$prismToolset = Get-ChildItem -LiteralPath (Join-Path $prismVsRoot 'MSBuild\Microsoft\VC') -Recurse -Directory |
    Where-Object { $_.Parent.Name -eq 'PlatformToolsets' -and $_.Name -match '^v\d+$' } |
    Sort-Object { [int]($_.Name.Substring(1)) } -Descending | Select-Object -First 1
$prismSdk = Get-ChildItem -LiteralPath (Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Include') -Directory |
    Where-Object { $_.Name -match '^10\.0\.' } | Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
if (!$prismToolset -or !$prismSdk) { throw 'MSVC toolset and Windows SDK are required.' }
& $prismMsbuild (Join-Path $prismRoot 'preview\RenderTest.vcxproj') /m /nologo /v:minimal /p:Configuration=Release /p:Platform=x64 "/p:PlatformToolset=$($prismToolset.Name)" "/p:WindowsTargetPlatformVersion=$($prismSdk.Name)"
if ($LASTEXITCODE -ne 0) { throw 'Renderer build failed.' }
$prismRenders = Join-Path $prismRoot '.build\renders'
New-Item -ItemType Directory -Force -Path $prismRenders | Out-Null
$prismProcess = Start-Process -FilePath (Join-Path $prismRoot '.build\render-test\PrismeRenderTest.exe') -ArgumentList ('"' + $prismRenders + '"') -PassThru -Wait -WindowStyle Hidden
if ($prismProcess.ExitCode -ne 0) { throw "Renderer checks failed: $($prismProcess.ExitCode). See $prismRenders" }
Get-Content -LiteralPath (Join-Path $prismRenders 'prisme-render-test.json')
