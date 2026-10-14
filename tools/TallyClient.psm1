Set-StrictMode -Version Latest

function Select-TallyPort {
    param([Parameter(Mandatory = $true)][AllowEmptyCollection()][object[]]$Devices)
    $ports = @($Devices | Where-Object {
        $_.InstanceId -match 'VID_2341&PID_(1002|006D)(?:&|\\)'
    } | ForEach-Object {
        $match = [regex]::Match($_.FriendlyName, '\((COM\d+)\)\s*$')
        if ($match.Success) { $match.Groups[1].Value }
    } | Sort-Object -Unique)
    if ($ports.Count -eq 0) { throw 'No connected UNO R4 WiFi serial port found. Connect the board or specify -Port COM6.' }
    if ($ports.Count -gt 1) { throw "Multiple UNO R4 WiFi ports found ($($ports -join ', ')). Specify -Port." }
    return $ports[0]
}

function Invoke-TallySerialCommand {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][object]$Connection,
        [Parameter(Mandatory = $true)][ValidateSet('off', 'idle', 'hot', 'red', 'white', 'status', 'ping')][string]$Command,
        [ValidateRange(50, 30000)][int]$TimeoutMs = 3000
    )
    $commandText = $Command.ToLowerInvariant()
    $requestId = [guid]::NewGuid().ToString('N').Substring(0, 16)
    $clock = [Diagnostics.Stopwatch]::StartNew()
    $nextSend = 0L
    $sends = 0
    $buffer = ''
    try {
        $Connection.Open()
        $Connection.DiscardInBuffer()
        while ($clock.ElapsedMilliseconds -lt $TimeoutMs) {
            if ($clock.ElapsedMilliseconds -ge $nextSend -and $sends -lt 3) {
                # A leading LF also terminates a partial command left by a previous process/reset.
                $Connection.WriteLine("`n@$requestId $commandText")
                $sends++
                $nextSend = $clock.ElapsedMilliseconds + 750
            }
            $buffer += $Connection.ReadExisting()
            if ($buffer.Length -gt 4096) { throw 'Serial reply exceeded 4096 characters. Check the firmware and baud rate.' }
            while (($newline = $buffer.IndexOf("`n")) -ge 0) {
                $line = $buffer.Substring(0, $newline).Trim()
                $buffer = $buffer.Substring($newline + 1)
                if (-not $line.StartsWith('{')) { continue }
                try { $reply = ConvertFrom-Json -InputObject $line -ErrorAction Stop }
                catch { continue }
                if (-not $reply -or -not $reply.PSObject.Properties['id'] -or $reply.id -cne $requestId) { continue }
                if (-not $reply.PSObject.Properties['ok'] -or $reply.ok -isnot [bool] -or -not $reply.ok) {
                    $reason = if ($reply.PSObject.Properties['error']) { $reply.error } else { 'invalid acknowledgement' }
                    throw "The tally rejected the command: $reason"
                }
                if (-not $reply.PSObject.Properties['mode'] -or
                    $reply.mode -cnotin @('off', 'idle', 'hot', 'red', 'white')) {
                    throw 'The tally returned an invalid mode.'
                }
                if ($commandText -notin @('status', 'ping') -and $reply.mode -cne $commandText) {
                    throw "The tally acknowledged a different mode: $($reply.mode)"
                }
                return $reply
            }
            Start-Sleep -Milliseconds 10
        }
        throw "No matching acknowledgement within $TimeoutMs ms. Check the port and upload the rewritten firmware; old firmware does not acknowledge commands."
    }
    finally {
        try { if ($Connection.IsOpen) { $Connection.Close() } }
        finally { $Connection.Dispose() }
    }
}

Export-ModuleMember -Function Select-TallyPort, Invoke-TallySerialCommand
