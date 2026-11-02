[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'Medium')]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('uart', 'ble', 'mouse')]
    [string]$Firmware,
    [string]$ProjectRoot = (Get-Location).Path,
    [string]$OpenOcdPath = (Join-Path $env:USERPROFILE 'Downloads\xpack-openocd-0.12.0-7-win32-x64\xpack-openocd-0.12.0-7\bin\openocd.exe'),
    [switch]$CheckOnly,
    [switch]$Recover
)

$ErrorActionPreference = 'Stop'
if ($CheckOnly -and $Recover) {
    throw '-CheckOnly and -Recover cannot be combined.'
}
$projectPath = (Resolve-Path -LiteralPath $ProjectRoot).Path
$executablePath = (Resolve-Path -LiteralPath $OpenOcdPath).Path
$configPath = Join-Path $projectPath 'openocd\ft232h-hy0020.cfg'
if (!(Test-Path -LiteralPath $configPath -PathType Leaf)) {
    throw "OpenOCD configuration not found: $configPath"
}
$hexRelative = switch ($Firmware) {
    'uart' { 'build\uart_test\uart_test.hex' }
    'ble' { 'build\ble_text\zephyr\zephyr.hex' }
    'mouse' { 'build\mouse_uart_ble\zephyr\zephyr.hex' }
}
$hexPath = (Join-Path $projectPath $hexRelative).Replace('\', '/')
if ($hexPath.Contains('{') -or $hexPath.Contains('}')) {
    throw 'Project paths containing Tcl braces are not supported.'
}
$speed = if ($Firmware -eq 'uart') { 10 } else { 100 }

if ($CheckOnly) {
    $commands = 'adapter speed 10; init; shutdown'
    $action = 'Check SWD connection without erasing or programming Flash'
} else {
    if (!(Test-Path -LiteralPath $hexPath -PathType Leaf)) {
        throw "Firmware HEX not found; build the selected sample first: $hexPath"
    }
    $commands = "adapter speed $speed; program {$hexPath} verify reset exit"
    $action = "Program $Firmware firmware, verify and reset at $speed kHz"
    if ($Recover) {
        $commands = "adapter speed 10; init; nrf52_recover; adapter speed $speed; reset halt; program {$hexPath} verify reset exit"
        $action = "ERASE ALL FLASH AND UICR, unlock APPROTECT, then $action"
    }
}

Write-Host "OpenOCD: $executablePath"
Write-Host "Config:  $configPath"
Write-Host "Command: $commands"
if (!$PSCmdlet.ShouldProcess("HY0020 via FT232H (USB 0403:6014), project $projectPath", $action)) {
    return
}

$previousLocation = Get-Location
try {
    Set-Location -LiteralPath $projectPath
    & $executablePath -f $configPath -c $commands
    if ($LASTEXITCODE -ne 0) {
        throw "OpenOCD failed with exit code $LASTEXITCODE. Inspect its log; the operation may be incomplete."
    }
} finally {
    Set-Location -LiteralPath $previousLocation.Path
}
