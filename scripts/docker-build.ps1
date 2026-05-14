# Build DragonTool.efi using Docker
# Usage: .\scripts\docker-build.ps1

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent (Split-Path -Parent $PSCommandPath)

Set-Location -LiteralPath $ProjectRoot

Write-Host "[BUILD] Building DragonTool.efi with Docker..." -ForegroundColor Cyan

docker build -t rainbow-dragon-builder -f Dockerfile --target export --output type=local,dest="./build" .

if ($LASTEXITCODE -eq 0) {
    Write-Host "[OK]    Build successful!" -ForegroundColor Green
    Write-Host "[INFO] Output: $ProjectRoot\build\DragonTool.efi" -ForegroundColor Green
} else {
    Write-Host "[FAIL] Build failed" -ForegroundColor Red
    exit 1
}
