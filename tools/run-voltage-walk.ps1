<#
.SYNOPSIS
  Opens a guided walk on real sensors (INA3221 + PC817, bench supply XY-SK35H) in its
  own window. -Mode supervisor = engine-run state machine; -Mode loads = load policy
  (engine-only grips + shed ladder) on the FOG/GRIP LEDs.

.EXAMPLE
  .\tools\run-voltage-walk.ps1
  .\tools\run-voltage-walk.ps1 -Mode loads
  .\tools\run-voltage-walk.ps1 -Port COM4
#>
param(
    [ValidateSet('supervisor', 'loads')] [string] $Mode = 'supervisor',
    [string] $Port = 'auto'
)

$py = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe'
if (-not (Test-Path $py)) { throw "PlatformIO Python not found at $py (install the PlatformIO IDE extension)." }
$script = Join-Path $PSScriptRoot 'voltage_walk.py'

$inner = "`$host.UI.RawUI.WindowTitle = 'moto-11 voltage walk ($Mode)'; & '$py' '$script' --profile $Mode --port '$Port'; Read-Host 'Press Enter to close'"
Start-Process powershell -ArgumentList '-NoProfile', '-Command', $inner -Wait
