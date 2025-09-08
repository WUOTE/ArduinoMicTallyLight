<#
.SYNOPSIS
  Sends a mode command to the Arduino Tally Light.
.DESCRIPTION
  This script automatically finds the COM port for a connected Arduino device
  by looking for its specific hardware Vendor ID (VID_2341), then sends a 
  specified mode command over the serial connection.
.PARAMETER Mode
  The mode to set the tally light to. Must be one of 'off', 'idle', 'hot', 'red', or 'white'.
.EXAMPLE
  .\set-tally-mode.ps1 -Mode hot
#>
param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('off', 'idle', 'hot', 'red', 'white')]
    [string]$Mode
)

# Find the COM port associated with the Arduino by its Hardware Vendor ID
$arduinoPort = (Get-PnpDevice -Class 'Ports' | Where-Object { $_.HardwareID -like '*VID_2341*' }).Name -replace '.*\(|\).*'

if ($arduinoPort) {
    Write-Host "Found Arduino on port: $arduinoPort"
    $port = new-Object System.IO.Ports.SerialPort $arduinoPort, 115200, None, 8, one
    try {
        $port.open()
        $port.WriteLine($Mode)
        $port.Close()
        Write-Host "Successfully sent '$Mode' command to $arduinoPort"
    }
    catch {
        Write-Error "Failed to send command to $arduinoPort. Error: $_"
    }
}
else {
    Write-Error "Could not find an Arduino device. Please check the connection."
}