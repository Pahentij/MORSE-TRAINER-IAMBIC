$ErrorActionPreference = "Stop"

# ============================================================
# ESP32-C3 merged firmware integrity checker
#
# Run from the project root:
#   .\check_firmware.ps1
#
# Checks:
#   1. All required BIN files exist.
#   2. SHA-256 hashes of source files.
#   3. merged.bin size.
#   4. bootloader.bin at 0x0000.
#   5. partitions.bin at 0x8000.
#   6. firmware.bin at 0x10000.
#   7. Every byte of all three source BIN files matches the
#      corresponding area in merged.bin.
# ============================================================

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path

$buildDir = Join-Path $projectRoot ".pio\build\lolin_c3_mini"
$releaseDir = Join-Path $projectRoot "release"

$bootloader = Join-Path $buildDir "bootloader.bin"
$partitions = Join-Path $buildDir "partitions.bin"
$firmware = Join-Path $buildDir "firmware.bin"
$merged = Join-Path $releaseDir "MORSE_TRAINER_IAMBIC_lolin_c3_mini_merged.bin"

Write-Host ""
Write-Host "============================================================"
Write-Host " ESP32-C3 FIRMWARE INTEGRITY CHECK"
Write-Host "============================================================"
Write-Host ""

$files = @(
    $bootloader,
    $partitions,
    $firmware,
    $merged
)

$missing = $files | Where-Object { -not (Test-Path -LiteralPath $_ -PathType Leaf) }

if ($missing.Count -gt 0) {
    Write-Host "ERROR: required file(s) not found:" -ForegroundColor Red

    foreach ($file in $missing) {
        Write-Host "  $file" -ForegroundColor Red
    }

    exit 1
}

Write-Host "Files found:"
Write-Host "  bootloader : $bootloader"
Write-Host "  partitions : $partitions"
Write-Host "  firmware   : $firmware"
Write-Host "  merged     : $merged"
Write-Host ""

# ------------------------------------------------------------
# File sizes
# ------------------------------------------------------------

$bootSize = (Get-Item -LiteralPath $bootloader).Length
$partSize = (Get-Item -LiteralPath $partitions).Length
$firmSize = (Get-Item -LiteralPath $firmware).Length
$mergedSize = (Get-Item -LiteralPath $merged).Length

Write-Host "File sizes:"
Write-Host ("  bootloader.bin : {0:N0} bytes" -f $bootSize)
Write-Host ("  partitions.bin : {0:N0} bytes" -f $partSize)
Write-Host ("  firmware.bin   : {0:N0} bytes" -f $firmSize)
Write-Host ("  merged.bin     : {0:N0} bytes" -f $mergedSize)
Write-Host ""

# ------------------------------------------------------------
# SHA-256
# ------------------------------------------------------------

Write-Host "SHA-256:"
Write-Host ""

Get-FileHash -Algorithm SHA256 -LiteralPath $bootloader |
    Select-Object Path, Hash |
    Format-List

Get-FileHash -Algorithm SHA256 -LiteralPath $partitions |
    Select-Object Path, Hash |
    Format-List

Get-FileHash -Algorithm SHA256 -LiteralPath $firmware |
    Select-Object Path, Hash |
    Format-List

Get-FileHash -Algorithm SHA256 -LiteralPath $merged |
    Select-Object Path, Hash |
    Format-List

# ------------------------------------------------------------
# Read files
# ------------------------------------------------------------

Write-Host "Loading binary files..."

$bootBytes = [System.IO.File]::ReadAllBytes($bootloader)
$partBytes = [System.IO.File]::ReadAllBytes($partitions)
$firmBytes = [System.IO.File]::ReadAllBytes($firmware)
$mergedBytes = [System.IO.File]::ReadAllBytes($merged)

# ------------------------------------------------------------
# Expected flash layout
# ------------------------------------------------------------

$tests = @(
    @{
        Name = "bootloader.bin"
        SourceBytes = $bootBytes
        Offset = 0x0000
    },
    @{
        Name = "partitions.bin"
        SourceBytes = $partBytes
        Offset = 0x8000
    },
    @{
        Name = "firmware.bin"
        SourceBytes = $firmBytes
        Offset = 0x10000
    }
)

Write-Host ""
Write-Host "Checking merged firmware contents..."
Write-Host ""

$allOk = $true
$lastEnd = 0

foreach ($test in $tests) {
    $name = $test.Name
    $sourceBytes = $test.SourceBytes
    $offset = [int64]$test.Offset
    $end = $offset + $sourceBytes.Length

    Write-Host ("{0,-16} address 0x{1:X5}, size {2:N0} bytes" -f `
        $name, $offset, $sourceBytes.Length)

    if ($end -gt $mergedBytes.Length) {
        Write-Host "  ERROR: component extends beyond merged.bin" -ForegroundColor Red
        $allOk = $false
        continue
    }

    $match = $true

    for ($i = 0; $i -lt $sourceBytes.Length; $i++) {
        if ($mergedBytes[$offset + $i] -ne $sourceBytes[$i]) {
            Write-Host (
                "  ERROR: byte mismatch at merged offset 0x{0:X}" -f ($offset + $i)
            ) -ForegroundColor Red

            $match = $false
            $allOk = $false
            break
        }
    }

    if ($match) {
        Write-Host "  OK: byte-for-byte match" -ForegroundColor Green
    }

    $lastEnd = [Math]::Max($lastEnd, $end)
}

# ------------------------------------------------------------
# Final size check
# ------------------------------------------------------------

Write-Host ""
Write-Host "Checking final merged size..."

if ($mergedBytes.Length -eq $lastEnd) {
    Write-Host (
        "  OK: merged.bin ends exactly at 0x{0:X} ({1:N0} bytes)" -f `
        $lastEnd, $mergedBytes.Length
    ) -ForegroundColor Green
}
else {
    Write-Host (
        "  WARNING: merged.bin size is 0x{0:X}, expected 0x{1:X}" -f `
        $mergedBytes.Length, $lastEnd
    ) -ForegroundColor Yellow

    $allOk = $false
}

# ------------------------------------------------------------
# Result
# ------------------------------------------------------------

Write-Host ""
Write-Host "============================================================"

if ($allOk) {
    Write-Host " RESULT: PASS" -ForegroundColor Green
    Write-Host " All binary components are intact."
    Write-Host "============================================================"
    Write-Host ""
    Read-Host "Press Enter to exit"
    exit 0
}
else {
    Write-Host " RESULT: FAIL" -ForegroundColor Red
    Write-Host " Binary integrity check failed."
    Write-Host "============================================================"
    Write-Host ""
    Read-Host "Press Enter to exit"
    exit 1
}
