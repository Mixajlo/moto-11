<#
.SYNOPSIS
  Opens the guided relay walk-through in its own window (so you can watch it on screen
  while you look at the hardware).

.EXAMPLE
  .\tools\run-relay-test.ps1                    # bench, auto-detect COM port
  .\tools\run-relay-test.ps1 -Mode bike
  .\tools\run-relay-test.ps1 -Mode bike -Port socket://vstrom-bike.local:23
  .\tools\run-relay-test.ps1 -NoReset
#>
param(
    [ValidateSet('bench', 'leds', 'bike')] [string] $Mode = 'bench',
    [string] $Port = 'auto',
    [switch] $NoReset
)

$py = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe'
if (-not (Test-Path $py)) { throw "PlatformIO Python not found at $py (install the PlatformIO IDE extension)." }
$script = Join-Path $PSScriptRoot 'relay_test.py'

$pyArgs = "'$script' --profile $Mode --port '$Port'"
if ($NoReset) { $pyArgs += ' --no-reset' }
$inner = "`$host.UI.RawUI.WindowTitle = 'moto-11 relay test ($Mode)'; & '$py' $pyArgs; Read-Host 'Press Enter to close'"
Start-Process powershell -ArgumentList '-NoProfile', '-Command', $inner -Wait
