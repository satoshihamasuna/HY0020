param(
    [string]$SdkRoot = 'C:\ncs\v3.4.1',
    [string]$CompilerBin = ''
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$buildDir = Join-Path $projectRoot 'build\uart_test'
$outputPrefix = (Join-Path $buildDir 'uart_test').Replace('\', '/')
Set-Location $PSScriptRoot
if (!$CompilerBin) {
    $compiler = Get-ChildItem 'C:\ncs\toolchains\*\opt\zephyr-sdk\gnu\arm-zephyr-eabi\bin\arm-zephyr-eabi-gcc.exe' |
        Sort-Object FullName | Select-Object -First 1
    if (!$compiler) { throw 'ARM compiler not found. Specify -CompilerBin.' }
    $CompilerBin = $compiler.DirectoryName
}
$mdk = Join-Path $SdkRoot 'modules\hal\nordic\nrfx\bsp\stable\mdk'
$cmsis = Join-Path $SdkRoot 'modules\hal\cmsis\CMSIS\Core\Include'
if (!(Test-Path "$mdk\system_nrf52.c")) { throw "MDK not found: $mdk" }
New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
$gccArgs = @(
    '-mcpu=cortex-m4', '-mthumb', '-mfloat-abi=soft', '-std=gnu11',
    '-Os', '-g3', '-Wall', '-Wextra', '-Werror', '-ffreestanding',
    '-ffunction-sections', '-fdata-sections', '-fno-unwind-tables',
    '-fno-asynchronous-unwind-tables', '-DNRF52832_XXAA',
    "-I$mdk", "-I$cmsis", '-nostdlib',
    'src/startup.c', 'src/main.c', "$mdk\system_nrf52.c",
    '-Tnrf52832.ld', '-Wl,--gc-sections', '-Wl,--build-id=none',
    "-Wl,-Map=$outputPrefix.map", '-o', "$outputPrefix.elf"
)
& "$CompilerBin\arm-zephyr-eabi-gcc.exe" @gccArgs
if ($LASTEXITCODE -ne 0) { throw 'Compilation failed.' }
foreach ($format in @(@('ihex', 'hex'), @('binary', 'bin'))) {
    & "$CompilerBin\arm-zephyr-eabi-objcopy.exe" -O $format[0] "$outputPrefix.elf" "$outputPrefix.$($format[1])"
    if ($LASTEXITCODE -ne 0) { throw 'objcopy failed.' }
}
& "$CompilerBin\arm-zephyr-eabi-size.exe" "$outputPrefix.elf"
if ($LASTEXITCODE -ne 0) { throw 'size failed.' }
Write-Host "Built $outputPrefix.elf, $outputPrefix.hex, $outputPrefix.bin"
