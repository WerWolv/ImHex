#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Script de build simplificado para ImHex

.DESCRIPTION
    Compila o ImHex com suporte MCP usando presets otimizados

.PARAMETER Clean
    Limpa o diretório de build antes de compilar

.PARAMETER Preset
    Preset do CMake a usar (padrão: windows-default)

.PARAMETER Jobs
    Número de jobs paralelos (padrão: número de CPUs)

.EXAMPLE
    .\build.ps1
    Compila usando configurações padrão

.EXAMPLE
    .\build.ps1 -Clean
    Limpa e recompila do zero

.EXAMPLE
    .\build.ps1 -Preset x86_64
    Usa preset específico
#>

param(
    [switch]$Clean,
    [string]$Preset = "windows-default",
    [int]$Jobs = 0
)

$ErrorActionPreference = "Stop"

Write-Host "🔧 ImHex Build Script" -ForegroundColor Cyan
Write-Host "=====================" -ForegroundColor Cyan
Write-Host ""

# Detectar número de CPUs se não especificado
if ($Jobs -eq 0) {
    $Jobs = (Get-CimInstance Win32_ComputerSystem).NumberOfLogicalProcessors
}

# Limpar se solicitado
if ($Clean) {
    Write-Host "🧹 Limpando diretório de build..." -ForegroundColor Yellow
    if (Test-Path "build") {
        Remove-Item -Recurse -Force "build"
    }
}

# Verificar se CMake está instalado
try {
    $cmakeVersion = cmake --version | Select-String -Pattern "cmake version (\d+\.\d+\.\d+)" | ForEach-Object { $_.Matches.Groups[1].Value }
    Write-Host "✓ CMake $cmakeVersion encontrado" -ForegroundColor Green
} catch {
    Write-Host "❌ CMake não encontrado! Instale de https://cmake.org/download/" -ForegroundColor Red
    exit 1
}

# Configurar
Write-Host ""
Write-Host "⚙️  Configurando projeto (preset: $Preset)..." -ForegroundColor Cyan
try {
    cmake --preset $Preset
    Write-Host "✓ Configuração concluída" -ForegroundColor Green
} catch {
    Write-Host "❌ Falha na configuração. Verifique os logs acima." -ForegroundColor Red
    exit 1
}

# Compilar
Write-Host ""
Write-Host "🔨 Compilando ($Jobs jobs paralelos)..." -ForegroundColor Cyan
Write-Host "   Isso pode levar 10-30 minutos na primeira vez..." -ForegroundColor Gray

$buildStart = Get-Date

try {
    cmake --build --preset $Preset -j $Jobs
    $buildEnd = Get-Date
    $duration = $buildEnd - $buildStart
    
    Write-Host ""
    Write-Host "✅ Build concluído com sucesso!" -ForegroundColor Green
    Write-Host "   Tempo: $($duration.Minutes)m $($duration.Seconds)s" -ForegroundColor Gray
} catch {
    Write-Host ""
    Write-Host "❌ Falha na compilação. Verifique os logs acima." -ForegroundColor Red
    exit 1
}

# Localizar executável
Write-Host ""
Write-Host "📍 Localizando executável..." -ForegroundColor Cyan

$exePaths = @(
    "build\windows\main\gui\Release\imhex.exe",
    "build\windows\Release\imhex.exe",
    "build\x86_64\imhex.exe"
)

$foundExe = $null
foreach ($path in $exePaths) {
    if (Test-Path $path) {
        $foundExe = (Resolve-Path $path).Path
        break
    }
}

if ($foundExe) {
    Write-Host "✓ Executável: $foundExe" -ForegroundColor Green
    Write-Host ""
    Write-Host "🎉 Próximos passos:" -ForegroundColor Cyan
    Write-Host "   1. Execute: " -NoNewline; Write-Host "$foundExe" -ForegroundColor Yellow
    Write-Host "   2. Habilite MCP: Edit > Settings > General > Network > MCP Server ✓" -ForegroundColor Gray
    Write-Host "   3. Instale cliente Python: " -NoNewline; Write-Host "pip install -e src/imhex_mcp_client" -ForegroundColor Yellow
    Write-Host "   4. Teste: " -NoNewline; Write-Host "python tests/mcp_connection_test.py" -ForegroundColor Yellow
} else {
    Write-Host "⚠️  Executável não encontrado nos caminhos esperados" -ForegroundColor Yellow
    Write-Host "   Procure manualmente em: build\windows\" -ForegroundColor Gray
}

Write-Host ""
