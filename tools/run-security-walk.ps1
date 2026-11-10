<#
.SYNOPSIS
  Opens the guided IMU security/crash walk (park reference, MOVED, BUMP, TIPPED, CRASH +
  auto-cancel) in its own window.

.EXAMPLE
  .\tools\run-security-walk.ps1
  .\tools\run-security-walk.ps1 -Port COM4
#>
param(
    [string] $Port = 'auto'
)

$py = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe'
if (-not (Test-Path $py)) { throw "PlatformIO Python not found at $py (install the PlatformIO IDE extension)." }
$script = Join-Path $PSScriptRoot 'security_walk.py'

$inner = "`$host.UI.RawUI.WindowTitle = 'moto-11 security walk'; & '$py' '$script' --port '$Port'; Read-Host 'Press Enter to close'"
Start-Process powershell -ArgumentList '-NoProfile', '-Command', $inner -Wait
