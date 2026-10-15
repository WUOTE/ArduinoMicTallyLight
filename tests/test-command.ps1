$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot '../tools/TallyClient.psm1') -Force

class FakePort {
    [bool]$IsOpen = $false
    [bool]$Disposed = $false
    [string]$Scenario
    [string]$Pending = ''
    [string]$RequestId = ''
    [int]$Writes = 0
    FakePort([string]$scenario) { $this.Scenario = $scenario }
    [void]Open() {
        if ($this.Scenario -eq 'open-failure') { throw 'port busy' }
        $this.IsOpen = $true
    }
    [void]Close() { $this.IsOpen = $false }
    [void]Dispose() { $this.Disposed = $true }
    [void]DiscardInBuffer() {}
    [void]WriteLine([string]$text) {
        $match = [regex]::Match($text, '^\n@([a-f0-9]{16}) ([a-z]+)$')
        if (-not $match.Success) { throw 'Bad command framing/case' }
        $id = $match.Groups[1].Value
        if ($this.RequestId -and $this.RequestId -cne $id) { throw 'Retry changed request ID' }
        $this.RequestId = $id
        $this.Writes++
        if ($this.Scenario -eq 'timeout') { return }
        if ($this.Scenario -eq 'retry' -and $this.Writes -lt 2) { return }
        $mode = if ($match.Groups[2].Value -in @('ping', 'status')) { 'idle' } else { $match.Groups[2].Value }
        if ($this.Scenario -eq 'wrong-mode') { $mode = 'white' }
        $this.Pending = "old log`n{broken`n{`"ok`":true,`"id`":`"stale`",`"mode`":`"red`"}`n"
        $ok = if ($this.Scenario -eq 'reject') { 'false' } else { 'true' }
        $this.Pending += "{`"ok`":$ok,`"id`":`"$id`",`"mode`":`"$mode`",`"error`":`"test_rejection`"}`n"
    }
    [string]ReadExisting() {
        # Deliberately fragment replies, including in the middle of a JSON token.
        $count = [Math]::Min(17, $this.Pending.Length)
        $result = $this.Pending.Substring(0, $count)
        $this.Pending = $this.Pending.Substring($count)
        return $result
    }
}

function Assert([bool]$Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
function Expect-Failure([scriptblock]$Action, [string]$Pattern) {
    $caught = $false
    try { & $Action | Out-Null }
    catch { $caught = $true; Assert ($_.Exception.Message -match $Pattern) "Wrong error: $($_.Exception.Message)" }
    Assert $caught 'Expected failure'
}

foreach ($scenario in @('success', 'retry')) {
    $port = [FakePort]::new($scenario)
    $reply = Invoke-TallySerialCommand -Connection $port -Command HOT -TimeoutMs 1500
    Assert ($reply.mode -ceq 'hot') 'Mode was not normalized/acknowledged'
    Assert ($port.Disposed -and -not $port.IsOpen) 'Connection leaked'
    if ($scenario -eq 'retry') { Assert ($port.Writes -eq 2) 'Expected one retry' }
}
foreach ($case in @(@('timeout', 'No matching acknowledgement'), @('reject', 'rejected'), @('wrong-mode', 'different mode'), @('open-failure', 'port busy'))) {
    $port = [FakePort]::new($case[0])
    Expect-Failure { Invoke-TallySerialCommand -Connection $port -Command hot -TimeoutMs 200 } $case[1]
    Assert ($port.Disposed -and -not $port.IsOpen) 'Connection leaked on failure'
}
$port = [FakePort]::new('success')
$reply = Invoke-TallySerialCommand -Connection $port -Command status -TimeoutMs 500
Assert ($reply.mode -eq 'idle') 'Status query failed'

$uno = [pscustomobject]@{ InstanceId = 'USB\VID_2341&PID_1002&MI_00\ABC'; FriendlyName = 'Arduino UNO R4 WiFi (COM6)' }
$other = [pscustomobject]@{ InstanceId = 'USB\VID_2341&PID_0043\ABC'; FriendlyName = 'Arduino Uno (COM4)' }
Assert ((Select-TallyPort -Devices @($uno, $other)) -eq 'COM6') 'Discovery picked wrong board'
Expect-Failure { Select-TallyPort -Devices @($other) } 'No connected'
Expect-Failure { Select-TallyPort -Devices @() } 'No connected'
$second = [pscustomobject]@{ InstanceId = 'USB\VID_2341&PID_006D\DEF'; FriendlyName = 'USB Serial Device (COM8)' }
Expect-Failure { Select-TallyPort -Devices @($uno, $second) } 'Multiple'
Assert ((Select-TallyPort -Devices @($uno, $uno)) -eq 'COM6') 'Duplicate device records should not be ambiguous'

# Exercise the public HTTP entry point and legacy wrapper without opening any network socket.
$httpTestState = @{ Request = $null; ReplyMode = '' }
$mockHttp = {
    [CmdletBinding()]
    param([uri]$Uri, [int]$TimeoutSec, [hashtable]$Headers, [string]$Method, [string]$ContentType, [string]$Body)
    $httpTestState.Request = $PSBoundParameters
    $mode = if ($Body) { (ConvertFrom-Json $Body).mode } else { 'idle' }
    if ($httpTestState.ReplyMode) { $mode = $httpTestState.ReplyMode }
    return [pscustomobject]@{ ok = $true; mode = $mode }
}.GetNewClosure()
Set-Item -Path Function:\Invoke-RestMethod -Value $mockHttp
$clientScript = Join-Path $PSScriptRoot '../tools/set-tally-mode.ps1'
$reply = & $clientScript -Mode HOT -DeviceUrl 'http://192.0.2.1' -Token 'test-token' -PassThru
Assert ($reply.mode -ceq 'hot') 'HTTP command did not normalize mode'
Assert ($httpTestState.Request.Uri.AbsoluteUri -eq 'http://192.0.2.1/tally') 'Incorrect HTTP route'
Assert ($httpTestState.Request.Headers.Authorization -eq 'Bearer test-token') 'Missing HTTP authorization'
Assert ($httpTestState.Request.ContentType -eq 'application/json') 'Missing JSON content type'
$reply = & $clientScript -Mode status -DeviceUrl 'http://192.0.2.1/'
Assert ($reply.mode -eq 'idle' -and $httpTestState.Request.Method -eq 'Get') 'HTTP status query failed'
Assert (-not $httpTestState.Request.ContainsKey('Body')) 'Status request unexpectedly had a body'
$wrapper = Join-Path $PSScriptRoot '../PowerShell commands/set-tally-mode.ps1'
$reply = & $wrapper -Mode white -DeviceUrl 'http://192.0.2.1' -PassThru
Assert ($reply.mode -eq 'white') 'Compatibility wrapper did not forward named arguments'
$httpTestState.ReplyMode = 'red'
Expect-Failure { & $wrapper -Mode white -DeviceUrl 'http://192.0.2.1' } 'different mode'
Expect-Failure { & $clientScript -Mode hot -DeviceUrl 'http://user:password@192.0.2.1/' } 'base URL'
Write-Output 'PowerShell tests passed: discovery, USB acknowledgements/retries/errors, HTTP requests/replies, wrapper and cleanup.'
