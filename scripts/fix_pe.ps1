# Fix PE32+ characteristics to add IMAGE_FILE_DLL flag
# Required for some UEFI implementations

param([string]$Path)

if (-not (Test-Path $Path)) {
    Write-Host "File not found: $Path"
    exit 1
}

$bytes = [System.IO.File]::ReadAllBytes($Path)

# PE header offset is at 0x3C (little-endian)
$peOffset = [BitConverter]::ToUInt32($bytes, 0x3C)

# Characteristics are at PE offset + 22 (little-endian, 2 bytes)
$charOffset = $peOffset + 22
$current = [BitConverter]::ToUInt16($bytes, $charOffset)

Write-Host "Current characteristics: 0x$('{0:X4}' -f $current)"

# Add IMAGE_FILE_DLL (0x2000) and IMAGE_FILE_EXECUTABLE_IMAGE (0x0002) 
# Keep existing flags
$newFlags = $current -bor 0x2000
Write-Host "New characteristics: 0x$('{0:X4}' -f $newFlags)"

$bytes[$charOffset] = $newFlags -band 0xFF
$bytes[$charOffset + 1] = ($newFlags -shr 8) -band 0xFF

[System.IO.File]::WriteAllBytes($Path, $bytes)
Write-Host "Fixed: $Path"
