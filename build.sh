#!/bin/bash
# Script de build simplificado para ImHex (Linux/macOS)

set -e

PRESET="x86_64"
CLEAN=false
JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Parse argumentos
while [[ $# -gt 0 ]]; do
    case $1 in
        --clean)
            CLEAN=true
            shift
            ;;
        --preset)
            PRESET="$2"
            shift 2
            ;;
        --jobs)
            JOBS="$2"
            shift 2
            ;;
        *)
            echo "Uso: $0 [--clean] [--preset PRESET] [--jobs N]"
            exit 1
            ;;
    esac
done

echo "🔧 ImHex Build Script"
echo "====================="
echo ""

# Limpar se solicitado
if [ "$CLEAN" = true ]; then
    echo "🧹 Limpando diretório de build..."
    rm -rf build
fi

# Verificar CMake
if ! command -v cmake &> /dev/null; then
    echo "❌ CMake não encontrado!"
    exit 1
fi

CMAKE_VERSION=$(cmake --version | head -n1 | cut -d' ' -f3)
echo "✓ CMake $CMAKE_VERSION encontrado"

# Configurar
echo ""
echo "⚙️  Configurando projeto (preset: $PRESET)..."
cmake --preset $PRESET

# Compilar
echo ""
echo "🔨 Compilando ($JOBS jobs paralelos)..."
echo "   Isso pode levar 10-30 minutos na primeira vez..."

START_TIME=$(date +%s)
cmake --build --preset $PRESET -j $JOBS
END_TIME=$(date +%s)
DURATION=$((END_TIME - START_TIME))

echo ""
echo "✅ Build concluído com sucesso!"
echo "   Tempo: $((DURATION / 60))m $((DURATION % 60))s"

# Localizar executável
echo ""
echo "📍 Localizando executável..."

if [ -f "build/x86_64/imhex" ]; then
    EXE_PATH="$(pwd)/build/x86_64/imhex"
    echo "✓ Executável: $EXE_PATH"
    echo ""
    echo "🎉 Próximos passos:"
    echo "   1. Execute: $EXE_PATH"
    echo "   2. Habilite MCP: Edit > Settings > General > Network > MCP Server ✓"
    echo "   3. Instale cliente Python: pip install -e src/imhex_mcp_client"
    echo "   4. Teste: python tests/mcp_connection_test.py"
else
    echo "⚠️  Executável não encontrado em build/x86_64/"
fi

echo ""
