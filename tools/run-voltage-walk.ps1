<#
.SYNOPSIS
  Opens the guided supervisor voltage walk (real INA3221 + PC817 sensors, bench
  supply XY-SK35H) in its own window.

.EXAMPLE
  .\tools\run-voltage-walk.ps1
  .\tools\run-voltage-walk.ps1 -Port COM4
#>
param(
    [string] $Port = 'auto'
)

$py = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe'
if (-not (Test-Path $py)) { throw "PlatformIO Python not found at $py (install the PlatformIO IDE extension)." }
$script = Join-Path $PSScriptRoot 'voltage_walk.py'

$inner = "`$host.UI.RawUI.WindowTitle = 'moto-11 voltage walk'; & '$py' '$script' --port '$Port'; Read-Host 'Press Enter to close'"
Start-Process powershell -ArgumentList '-NoProfile', '-Command', $inner -Wait
