<#
.SYNOPSIS
  Opens the live IMU watcher (accel, tilt, gyro, temp from the MPU-6050) in its own window.

.EXAMPLE
  .\tools\run-watch-imu.ps1              # 60 s
  .\tools\run-watch-imu.ps1 -Secs 180
#>
param(
    [int] $Secs = 60,
    [string] $Port = 'auto'
)

$py = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe'
if (-not (Test-Path $py)) { throw "PlatformIO Python not found at $py (install the PlatformIO IDE extension)." }
$script = Join-Path $PSScriptRoot 'watch_imu.py'

$inner = "`$host.UI.RawUI.WindowTitle = 'moto-11 IMU watch'; & '$py' '$script' --secs $Secs --port '$Port'; Read-Host 'Press Enter to close'"
Start-Process powershell -ArgumentList '-NoProfile', '-Command', $inner -Wait
