[CmdletBinding()]
param(
    [ValidateSet('11', '14', '17')]
    [string]$Standard = '14',

    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [string]$BuildDir,

    [string]$VsRoot = 'D:\Program Files (x86)\Microsoft Visual Studio\2019\Community',

    [string]$SdkRoot = 'D:\Windows Kits\10'
)

$ErrorActionPreference = 'Stop'

function Assert-LastExitCode {
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

if (-not (Test-Path -LiteralPath $VsRoot)) {
    throw "Visual Studio not found: $VsRoot"
}

$vcvars = Join-Path $VsRoot 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path -LiteralPath $vcvars)) {
    throw "vcvars64.bat not found: $vcvars"
}

$envLines = cmd /c "`"$vcvars`" >nul 2>&1 && set"
if ($LASTEXITCODE -ne 0) {
    throw "Failed to initialize Visual Studio environment: $vcvars"
}
foreach ($line in $envLines) {
    if ($line -match '^([^=]+)=(.*)$') {
        Set-Item -Path ("Env:" + $matches[1]) -Value $matches[2]
    }
}

$sdkIncludeRoot = Join-Path $SdkRoot 'Include'
if (-not (Test-Path -LiteralPath $sdkIncludeRoot)) {
    $nested = Join-Path $SdkRoot '10\Include'
    if (Test-Path -LiteralPath $nested) {
        $SdkRoot = Join-Path $SdkRoot '10'
        $sdkIncludeRoot = $nested
    }
    else {
        throw "Windows SDK include directory not found under: $SdkRoot"
    }
}

$sdkVersion = Get-ChildItem -LiteralPath $sdkIncludeRoot -Directory |
    Where-Object {
        (Test-Path -LiteralPath (Join-Path $_.FullName 'um\windows.h')) -and
        (Test-Path -LiteralPath (Join-Path $SdkRoot "Lib\$($_.Name)\ucrt\x64\ucrt.lib"))
    } |
    Sort-Object Name -Descending |
    Select-Object -First 1 -ExpandProperty Name

if (-not $sdkVersion) {
    throw "No usable Windows 10 SDK found under: $SdkRoot"
}

$sdkInclude = Join-Path $SdkRoot "Include\$sdkVersion"
$sdkLib = Join-Path $SdkRoot "Lib\$sdkVersion"
$sdkBin = Join-Path $SdkRoot "bin\$sdkVersion\x64"

$env:WindowsSdkDir = "$SdkRoot\"
$env:WindowsSDKVersion = "$sdkVersion\"
$env:WindowsSDKLibVersion = "$sdkVersion\"
$env:UniversalCRTSdkDir = "$SdkRoot\"
$env:UCRTVersion = $sdkVersion

$env:INCLUDE = @(
    (Join-Path $sdkInclude 'ucrt'),
    (Join-Path $sdkInclude 'shared'),
    (Join-Path $sdkInclude 'um'),
    (Join-Path $sdkInclude 'winrt'),
    (Join-Path $sdkInclude 'cppwinrt'),
    $env:INCLUDE
) -join ';'

$env:LIB = @(
    (Join-Path $sdkLib 'ucrt\x64'),
    (Join-Path $sdkLib 'um\x64'),
    $env:LIB
) -join ';'

$debugCrtRoot = Join-Path $VsRoot "VC\Redist\MSVC\$env:VCToolsVersion\debug_nonredist\x64"
$debugCrt = $null
if (Test-Path -LiteralPath $debugCrtRoot) {
    $debugCrt = Get-ChildItem -LiteralPath $debugCrtRoot -Directory -Filter '*.DebugCRT' |
        Select-Object -First 1 -ExpandProperty FullName
}

$sdkUcrtBin = Join-Path $SdkRoot "bin\$sdkVersion\x64\ucrt"
$cmakeBin = Join-Path $VsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
$ninjaBin = Join-Path $VsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja'
$env:PATH = @($sdkBin, $sdkUcrtBin, $debugCrt, $cmakeBin, $ninjaBin, $env:PATH) -join ';'

$cmake = Join-Path $cmakeBin 'cmake.exe'
$ctest = Join-Path $cmakeBin 'ctest.exe'
if (-not (Test-Path -LiteralPath $cmake)) {
    $cmake = 'cmake'
}
if (-not (Test-Path -LiteralPath $ctest)) {
    $ctest = 'ctest'
}

$sourceDir = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $BuildDir) {
    $BuildDir = Join-Path $sourceDir "build\msvc-cxx$Standard"
}
$BuildDir = [System.IO.Path]::GetFullPath($BuildDir)

Write-Host "Visual Studio: $VsRoot"
Write-Host "Windows SDK:    $SdkRoot\$sdkVersion"
Write-Host "C++ standard:   $Standard"
if ($Standard -eq '11') {
    Write-Host "MSVC has no C++11 mode; the default C++14 language level is used."
}
Write-Host "Build dir:      $BuildDir"

& $cmake -S $sourceDir -B $BuildDir -G Ninja `
    "-DCMAKE_BUILD_TYPE=$Configuration" `
    "-DEVENTLY_CXX_STANDARD=$Standard"
Assert-LastExitCode

& $cmake --build $BuildDir
Assert-LastExitCode

$ctestFile = Join-Path $BuildDir 'CTestTestfile.cmake'
if (Test-Path -LiteralPath $ctestFile) {
    & $ctest --test-dir $BuildDir --output-on-failure
    Assert-LastExitCode
}

$testExe = Join-Path $BuildDir 'Test.exe'
if (Test-Path -LiteralPath $testExe) {
    & $testExe
    Assert-LastExitCode
}
