#requires -Version 5.1
[CmdletBinding(DefaultParameterSetName = 'USB')]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateSet('off', 'idle', 'hot', 'red', 'white', 'status', 'ping')]
    [string]$Mode,

    [Parameter(ParameterSetName = 'USB')]
    [ValidatePattern('^COM[1-9][0-9]*$')][string]$Port,

    [Parameter(Mandatory = $true, ParameterSetName = 'HTTP')][uri]$DeviceUrl,
    [Parameter(ParameterSetName = 'HTTP')][string]$Token = '',

    [ValidateRange(250, 30000)][int]$TimeoutMs = 3000,
    [switch]$PassThru
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'TallyClient.psm1') -Force
$command = $Mode.ToLowerInvariant()

if ($PSCmdlet.ParameterSetName -eq 'HTTP') {
    if ($DeviceUrl.Scheme -notin @('http', 'https') -or $DeviceUrl.UserInfo -or $DeviceUrl.Query -or $DeviceUrl.Fragment) {
        throw 'DeviceUrl must be an http(s) base URL without credentials, query or fragment.'
    }
    $request = @{
        Uri = $DeviceUrl.AbsoluteUri.TrimEnd('/')
        TimeoutSec = [int][Math]::Ceiling($TimeoutMs / 1000.0)
        Headers = @{}
        Method = 'Get'
    }
    if ($Token) { $request.Headers['Authorization'] = "Bearer $Token" }
    if ($command -in @('status', 'ping')) {
        $request.Uri += '/status'
    } else {
        $request.Uri += '/tally'
        $request.Method = 'Post'
        $request.ContentType = 'application/json'
        $request.Body = @{ mode = $command } | ConvertTo-Json -Compress
    }
    $reply = Invoke-RestMethod @request
    if (-not $reply.PSObject.Properties['ok'] -or $reply.ok -isnot [bool] -or -not $reply.ok -or
        -not $reply.PSObject.Properties['mode'] -or $reply.mode -cnotin @('off', 'idle', 'hot', 'red', 'white')) {
        throw 'The tally returned an invalid acknowledgement.'
    }
    if ($command -notin @('status', 'ping') -and $reply.mode -cne $command) {
        throw "The tally acknowledged a different mode: $($reply.mode)"
    }
} else {
    if (-not $Port) {
        if (-not (Get-Command Get-PnpDevice -ErrorAction SilentlyContinue)) { throw 'Specify -Port; automatic port discovery requires Windows Get-PnpDevice.' }
        $Port = Select-TallyPort -Devices @(Get-PnpDevice -Class Ports -PresentOnly)
    }
    $connection = [IO.Ports.SerialPort]::new($Port, 115200, [IO.Ports.Parity]::None, 8, [IO.Ports.StopBits]::One)
    $connection.NewLine = "`n"
    $connection.Encoding = [Text.Encoding]::ASCII
    $connection.DtrEnable = $true
    $connection.RtsEnable = $false
    $connection.WriteTimeout = [Math]::Min(500, $TimeoutMs)
    $reply = Invoke-TallySerialCommand -Connection $connection -Command $command -TimeoutMs $TimeoutMs
}

# Silent success is useful for Stream Deck/hotkeys. Errors terminate with a nonzero exit in -File mode.
if ($PassThru -or $command -in @('status', 'ping')) { $reply }
