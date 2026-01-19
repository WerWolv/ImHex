#!/bin/bash
# Verifica se o ambiente está pronto para compilar ImHex

set +e

echo ""
echo "🔍 Verificação de Ambiente - ImHex Build"
echo "========================================="
echo ""

ALL_GOOD=true

# CMake
echo -n "📦 Verificando CMake..."
if command -v cmake &> /dev/null; then
    CMAKE_VERSION=$(cmake --version | head -n1 | cut -d' ' -f3)
    echo " ✓"
    echo "   Versão: $CMAKE_VERSION"
    
    # Verifica versão mínima
    CMAKE_MAJOR=$(echo $CMAKE_VERSION | cut -d. -f1)
    CMAKE_MINOR=$(echo $CMAKE_VERSION | cut -d. -f2)
    
    if [ "$CMAKE_MAJOR" -lt 3 ] || ([ "$CMAKE_MAJOR" -eq 3 ] && [ "$CMAKE_MINOR" -lt 25 ]); then
        echo "   ⚠️  Versão mínima recomendada: 3.25"
    fi
else
    echo " ❌"
    echo "   CMake não encontrado!"
    echo "   Instale: sudo apt install cmake (Ubuntu/Debian)"
    echo "          : sudo dnf install cmake (Fedora)"
    echo "          : brew install cmake (macOS)"
    ALL_GOOD=false
fi

# Compilador C++
echo ""
echo -n "🔨 Verificando Compilador C++..."
if command -v g++ &> /dev/null; then
    GCC_VERSION=$(g++ --version | head -n1)
    echo " ✓"
    echo "   GCC encontrado: $GCC_VERSION"
elif command -v clang++ &> /dev/null; then
    CLANG_VERSION=$(clang++ --version | head -n1)
    echo " ✓"
    echo "   Clang encontrado: $CLANG_VERSION"
else
    echo " ❌"
    echo "   Nenhum compilador C++ encontrado!"
    echo "   Instale: sudo apt install build-essential (Ubuntu/Debian)"
    echo "          : sudo dnf install gcc-c++ (Fedora)"
    echo "          : xcode-select --install (macOS)"
    ALL_GOOD=false
fi

# Python
echo ""
echo -n "🐍 Verificando Python..."
if command -v python3 &> /dev/null; then
    PYTHON_VERSION=$(python3 --version)
    echo " ✓"
    echo "   $PYTHON_VERSION"
    
    # Verifica versão
    PY_MAJOR=$(python3 -c "import sys; print(sys.version_info.major)")
    PY_MINOR=$(python3 -c "import sys; print(sys.version_info.minor)")
    
    if [ "$PY_MAJOR" -lt 3 ] || ([ "$PY_MAJOR" -eq 3 ] && [ "$PY_MINOR" -lt 8 ]); then
        echo "   ⚠️  Python 3.8+ recomendado para cliente MCP"
    fi
elif command -v python &> /dev/null; then
    PYTHON_VERSION=$(python --version)
    echo " ✓"
    echo "   $PYTHON_VERSION"
else
    echo " ⚠️"
    echo "   Python não encontrado (opcional para cliente MCP)"
fi

# Git
echo ""
echo -n "📚 Verificando Git..."
if command -v git &> /dev/null; then
    GIT_VERSION=$(git --version)
    echo " ✓"
    echo "   $GIT_VERSION"
else
    echo " ⚠️"
    echo "   Git não encontrado (opcional, mas recomendado)"
fi

# Dependências do sistema (Linux)
if [ "$(uname)" = "Linux" ]; then
    echo ""
    echo "📦 Verificando bibliotecas do sistema..."
    
    MISSING_LIBS=()
    
    # Lista de bibliotecas comuns necessárias
    if ! pkg-config --exists glfw3; then MISSING_LIBS+=("libglfw3-dev"); fi
    if ! pkg-config --exists freetype2; then MISSING_LIBS+=("libfreetype-dev"); fi
    if ! pkg-config --exists mbedtls; then MISSING_LIBS+=("libmbedtls-dev"); fi
    if ! pkg-config --exists libcurl; then MISSING_LIBS+=("libcurl4-openssl-dev"); fi
    
    if [ ${#MISSING_LIBS[@]} -eq 0 ]; then
        echo "   ✓ Principais bibliotecas encontradas"
    else
        echo "   ⚠️  Algumas bibliotecas podem estar faltando:"
        for lib in "${MISSING_LIBS[@]}"; do
            echo "      - $lib"
        done
        echo ""
        echo "   Instale com:"
        echo "   sudo apt install ${MISSING_LIBS[*]} (Ubuntu/Debian)"
    fi
fi

# Espaço em disco
echo ""
echo -n "💾 Verificando espaço em disco..."
if [ "$(uname)" = "Darwin" ]; then
    FREE_GB=$(df -g . | tail -1 | awk '{print $4}')
else
    FREE_GB=$(df -BG . | tail -1 | awk '{print $4}' | sed 's/G//')
fi

if [ "$FREE_GB" -gt 10 ]; then
    echo " ✓"
    echo "   Espaço livre: ${FREE_GB} GB"
elif [ "$FREE_GB" -gt 5 ]; then
    echo " ⚠️"
    echo "   Espaço livre: ${FREE_GB} GB (mínimo recomendado: 10 GB)"
else
    echo " ❌"
    echo "   Espaço livre: ${FREE_GB} GB (insuficiente!)"
    echo "   Recomendado: pelo menos 10 GB livres"
    ALL_GOOD=false
fi

# Resumo
echo ""
echo "========================================="

if [ "$ALL_GOOD" = true ]; then
    echo "✅ Ambiente pronto para compilar!"
    echo ""
    echo "🚀 Próximo passo:"
    echo "   ./build.sh"
else
    echo "❌ Corrija os problemas acima antes de compilar"
    echo ""
    echo "📖 Documentação:"
    echo "   BUILD.md - Guia completo de instalação de dependências"
fi

echo ""
