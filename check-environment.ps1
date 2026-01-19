#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Verifica se o ambiente está pronto para compilar ImHex

.DESCRIPTION
    Checa todas as dependências necessárias antes de iniciar a compilação
#>

$ErrorActionPreference = "Continue"

Write-Host ""
Write-Host "🔍 Verificação de Ambiente - ImHex Build" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""

$allGood = $true

# CMake
Write-Host "📦 Verificando CMake..." -NoNewline
try {
    $cmakeVersion = cmake --version | Select-String -Pattern "cmake version (\d+\.\d+\.\d+)" | ForEach-Object { $_.Matches.Groups[1].Value }
    if ($cmakeVersion) {
        Write-Host " ✓" -ForegroundColor Green
        Write-Host "   Versão: $cmakeVersion" -ForegroundColor Gray
        
        $major = [int]($cmakeVersion -split '\.')[0]
        $minor = [int]($cmakeVersion -split '\.')[1]
        
        if ($major -lt 3 -or ($major -eq 3 -and $minor -lt 25)) {
            Write-Host "   ⚠️  Versão mínima recomendada: 3.25" -ForegroundColor Yellow
        }
    }
} catch {
    Write-Host " ❌" -ForegroundColor Red
    Write-Host "   CMake não encontrado!" -ForegroundColor Red
    Write-Host "   Baixe de: https://cmake.org/download/" -ForegroundColor Yellow
    $allGood = $false
}

# Visual Studio / C++ Compiler
Write-Host ""
Write-Host "🔨 Verificando Compilador C++..." -NoNewline

$vsPath = $null
$vsPaths = @(
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.exe",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\Common7\IDE\devenv.exe",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\devenv.exe",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.exe"
)

foreach ($path in $vsPaths) {
    if (Test-Path $path) {
        $vsPath = $path
        break
    }
}

if ($vsPath) {
    Write-Host " ✓" -ForegroundColor Green
    $edition = if ($vsPath -match "Community") { "Community" } elseif ($vsPath -match "Professional") { "Professional" } else { "Enterprise" }
    Write-Host "   Visual Studio 2022 $edition encontrado" -ForegroundColor Gray
} else {
    # Tentar gcc/clang
    try {
        $gccVersion = gcc --version 2>$null | Select-Object -First 1
        if ($gccVersion) {
            Write-Host " ✓" -ForegroundColor Green
            Write-Host "   GCC encontrado: $gccVersion" -ForegroundColor Gray
        } else {
            throw
        }
    } catch {
        try {
            $clangVersion = clang --version 2>$null | Select-Object -First 1
            if ($clangVersion) {
                Write-Host " ✓" -ForegroundColor Green
                Write-Host "   Clang encontrado: $clangVersion" -ForegroundColor Gray
            } else {
                throw
            }
        } catch {
            Write-Host " ❌" -ForegroundColor Red
            Write-Host "   Nenhum compilador C++ encontrado!" -ForegroundColor Red
            Write-Host "   Instale Visual Studio 2022: https://visualstudio.microsoft.com/downloads/" -ForegroundColor Yellow
            Write-Host "   Ou MinGW/GCC para Windows" -ForegroundColor Yellow
            $allGood = $false
        }
    }
}

# Python
Write-Host ""
Write-Host "🐍 Verificando Python..." -NoNewline
try {
    $pythonVersion = python --version 2>&1
    if ($pythonVersion -match "Python (\d+\.\d+\.\d+)") {
        $pyVer = $matches[1]
        Write-Host " ✓" -ForegroundColor Green
        Write-Host "   Versão: $pyVer" -ForegroundColor Gray
        
        $major = [int]($pyVer -split '\.')[0]
        $minor = [int]($pyVer -split '\.')[1]
        
        if ($major -lt 3 -or ($major -eq 3 -and $minor -lt 8)) {
            Write-Host "   ⚠️  Python 3.8+ recomendado para cliente MCP" -ForegroundColor Yellow
        }
    }
} catch {
    Write-Host " ⚠️" -ForegroundColor Yellow
    Write-Host "   Python não encontrado (opcional para cliente MCP)" -ForegroundColor Yellow
}

# Git
Write-Host ""
Write-Host "📚 Verificando Git..." -NoNewline
try {
    $gitVersion = git --version 2>&1
    if ($gitVersion) {
        Write-Host " ✓" -ForegroundColor Green
        Write-Host "   $gitVersion" -ForegroundColor Gray
    }
} catch {
    Write-Host " ⚠️" -ForegroundColor Yellow
    Write-Host "   Git não encontrado (opcional, mas recomendado)" -ForegroundColor Yellow
}

# Espaço em disco
Write-Host ""
Write-Host "💾 Verificando espaço em disco..." -NoNewline
$drive = Get-PSDrive -PSProvider FileSystem | Where-Object { $_.Root -eq (Get-Location).Drive.Root }
$freeGB = [math]::Round($drive.Free / 1GB, 2)

if ($freeGB -gt 10) {
    Write-Host " ✓" -ForegroundColor Green
    Write-Host "   Espaço livre: ${freeGB} GB" -ForegroundColor Gray
} elseif ($freeGB -gt 5) {
    Write-Host " ⚠️" -ForegroundColor Yellow
    Write-Host "   Espaço livre: ${freeGB} GB (mínimo recomendado: 10 GB)" -ForegroundColor Yellow
} else {
    Write-Host " ❌" -ForegroundColor Red
    Write-Host "   Espaço livre: ${freeGB} GB (insuficiente!)" -ForegroundColor Red
    Write-Host "   Recomendado: pelo menos 10 GB livres" -ForegroundColor Yellow
    $allGood = $false
}

# RAM
Write-Host ""
Write-Host "💻 Verificando RAM..." -NoNewline
$ram = Get-CimInstance Win32_ComputerSystem
$totalRAMGB = [math]::Round($ram.TotalPhysicalMemory / 1GB, 2)

if ($totalRAMGB -gt 8) {
    Write-Host " ✓" -ForegroundColor Green
    Write-Host "   RAM total: ${totalRAMGB} GB" -ForegroundColor Gray
} elseif ($totalRAMGB -gt 4) {
    Write-Host " ⚠️" -ForegroundColor Yellow
    Write-Host "   RAM total: ${totalRAMGB} GB" -ForegroundColor Yellow
    Write-Host "   Recomendado: 8 GB+ (considere usar -Jobs 4 para limitar paralelismo)" -ForegroundColor Yellow
} else {
    Write-Host " ⚠️" -ForegroundColor Yellow
    Write-Host "   RAM total: ${totalRAMGB} GB" -ForegroundColor Yellow
    Write-Host "   IMPORTANTE: Use -Jobs 2 para evitar erros de memória!" -ForegroundColor Yellow
}

# Resumo
Write-Host ""
Write-Host "=========================================" -ForegroundColor Cyan

if ($allGood) {
    Write-Host "✅ Ambiente pronto para compilar!" -ForegroundColor Green
    Write-Host ""
    Write-Host "🚀 Próximo passo:" -ForegroundColor Cyan
    Write-Host "   .\build.ps1" -ForegroundColor Yellow
    
    if ($totalRAMGB -lt 8) {
        Write-Host ""
        Write-Host "   Ou com paralelismo limitado:" -ForegroundColor Cyan
        Write-Host "   .\build.ps1 -Jobs 4" -ForegroundColor Yellow
    }
} else {
    Write-Host "❌ Corrija os problemas acima antes de compilar" -ForegroundColor Red
    Write-Host ""
    Write-Host "📖 Documentação:" -ForegroundColor Cyan
    Write-Host "   BUILD.md - Guia completo de instalação de dependências" -ForegroundColor Gray
}

Write-Host ""
