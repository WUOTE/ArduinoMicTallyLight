#requires -Version 5.1
[CmdletBinding()]
param(
    [switch]$UsbOnly,
    [switch]$FactoryDefaults,
    [string]$ArduinoCli,
    [string]$ConfigFile
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
if (-not $ArduinoCli) {
    $localCli = Join-Path $repo '.tools/arduino-cli/arduino-cli.exe'
    $ArduinoCli = if (Test-Path -LiteralPath $localCli) { $localCli } else { 'arduino-cli' }
}
if (-not $ConfigFile) {
    $localConfig = Join-Path $repo '.tools/arduino-cli.yaml'
    if (Test-Path -LiteralPath $localConfig) { $ConfigFile = $localConfig }
}
$arguments = @()
if ($ConfigFile) { $arguments += @('--config-file', $ConfigFile) }
$variant = if ($UsbOnly) { 'usb' } else { 'wifi' }
if ($FactoryDefaults) { $variant += '-defaults' }
$arguments += @('compile', '--profile', 'uno_r4_wifi', '--warnings', 'all',
    '--build-path', (Join-Path $repo "build/$variant"))
$flags = @()
if ($UsbOnly) { $flags += '-DTALLY_USB_ONLY' }
if ($FactoryDefaults) { $flags += '-DTALLY_NO_LOCAL_CONFIG' }
if ($flags.Count) { $arguments += @('--build-property', "compiler.cpp.extra_flags=$($flags -join ' ')") }
# Optional short local compiler path for GCC 7's Windows MAX_PATH limitation.
$shortCompiler = Join-Path $repo '.tools/gcc/bin'
if (Test-Path -LiteralPath $shortCompiler) {
    $arguments += @('--build-property', "build.compiler_path=$($shortCompiler.Replace('\', '/'))/")
}
$arguments += (Join-Path $repo 'firmware/TallyLight')
& $ArduinoCli @arguments
if ($LASTEXITCODE -ne 0) { throw "Arduino compilation failed (exit $LASTEXITCODE)." }
