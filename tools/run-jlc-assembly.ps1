<#
.SYNOPSIS
  Writes JLCPCB assembly files (BOM + CPL) for both boards into each board's fab-v1\assembly folder.

.DESCRIPTION
  Only parts with an "LCSC" field in the schematic are listed (the ones JLC places).
  Close nothing in KiCad, but save the schematic and board first.

.EXAMPLE
  .\tools\run-jlc-assembly.ps1                 # both boards
  .\tools\run-jlc-assembly.ps1 -Board pwm-board
#>
param(
    [ValidateSet('all', 'mainboard', 'pwm-board')] [string] $Board = 'all',
    [string] $KicadCli = 'C:\Program Files\KiCad\10.0\bin\kicad-cli.exe'
)

$py = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe'
if (-not (Test-Path $py)) { $py = 'python' }
$script = Join-Path $PSScriptRoot 'jlc_assembly.py'
$hw = Join-Path (Split-Path $PSScriptRoot -Parent) 'hardware'

$boards = @{
    'mainboard' = Join-Path $hw 'mainboard\mainboard'
    'pwm-board' = Join-Path $hw 'pwm-board\pwm-board'
}
$names = if ($Board -eq 'all') { $boards.Keys | Sort-Object } else { @($Board) }
foreach ($n in $names) {
    $dir = $boards[$n]
    & $py $script $dir (Join-Path $dir 'fab-v1\assembly') --kicad-cli $KicadCli
    if ($LASTEXITCODE -ne 0) { throw "jlc_assembly failed for $n" }
}
