# Compatibility entry point for existing Stream Deck buttons and shortcuts.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)][string]$Mode,
    [string]$Port,
    [uri]$DeviceUrl,
    [string]$Token,
    [int]$TimeoutMs = 3000,
    [switch]$PassThru
)
$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot '../tools/set-tally-mode.ps1') @PSBoundParameters
