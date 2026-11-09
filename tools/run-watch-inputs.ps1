<#
.SYNOPSIS
  Opens the live sense-input watcher (IGN / START / VBUS / supervisor state) in its own window.

.EXAMPLE
  .\tools\run-watch-inputs.ps1                 # 60 s, sensors unchanged
  .\tools\run-watch-inputs.ps1 -Real -Secs 120 # supervisor on real sensors, follow its state
#>
param(
    [int] $Secs = 60,
    [switch] $Real,
    [string] $Port = 'auto'
)

$py = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe'
if (-not (Test-Path $py)) { throw "PlatformIO Python not found at $py (install the PlatformIO IDE extension)." }
$script = Join-Path $PSScriptRoot 'watch_inputs.py'

$pyArgs = "'$script' --secs $Secs --port '$Port'"
if ($Real) { $pyArgs += ' --real' }
$inner = "`$host.UI.RawUI.WindowTitle = 'moto-11 input watch'; & '$py' $pyArgs; Read-Host 'Press Enter to close'"
Start-Process powershell -ArgumentList '-NoProfile', '-Command', $inner -Wait
