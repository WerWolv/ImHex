# 🛠️ Guia de Build - ImHex com MCP

Este guia simplificado ajuda você a compilar o ImHex rapidamente.

## ⚡ Build Rápido (Recomendado)

### Windows

```powershell
# Usando o script automático
.\build.ps1

# Ou com limpeza completa
.\build.ps1 -Clean

# Ou especificando preset
.\build.ps1 -Preset vs2022
```

### Linux/macOS

```bash
# Dar permissão de execução
chmod +x build.sh

# Compilar
./build.sh

# Ou com limpeza
./build.sh --clean
```

---

## 📝 Build Manual (Tradicional)

### Windows - Método 1: Visual Studio

```cmd
REM Criar diretório de build
mkdir build
cd build

REM Gerar projeto Visual Studio
cmake .. -G "Visual Studio 17 2022" -A x64

REM Compilar
cmake --build . --config Release -j

REM Executar
cd Release
imhex.exe
```

### Windows - Método 2: Usando Presets

```cmd
REM Configurar
cmake --preset windows-default

REM Compilar
cmake --build --preset windows-default -j

REM Executar
build\windows\main\gui\Release\imhex.exe
```

### Linux

```bash
# Instalar dependências (Ubuntu/Debian)
sudo apt update
sudo apt install -y build-essential cmake git pkg-config \
    libglfw3-dev libglm-dev libfreetype-dev libmbedtls-dev \
    libcurl4-openssl-dev libdbus-1-dev libfmt-dev

# Compilar
mkdir build && cd build
cmake ..
cmake --build . -j$(nproc)

# Executar
./imhex
```

### macOS

```bash
# Instalar dependências (Homebrew)
brew install cmake glfw glm freetype mbedtls curl fmt

# Compilar
mkdir build && cd build
cmake ..
cmake --build . -j$(sysctl -n hw.ncpu)

# Executar
./imhex
```

---

## 🎯 Opções de Build

### Presets Disponíveis

| Preset | Plataforma | Descrição |
|--------|-----------|-----------|
| `windows-default` | Windows | Build padrão sem vcpkg (mais simples) |
| `vs2022` | Windows | Visual Studio 2022 com vcpkg |
| `x86_64` | Linux/macOS | Build padrão Unix |
| `xcode` | macOS | Projeto Xcode |

### Opções do CMake

```cmake
# Ativar/desativar recursos
-DIMHEX_ENABLE_UNIT_TESTS=ON          # Testes unitários (padrão: ON)
-DIMHEX_OFFLINE_BUILD=ON              # Build offline (padrão: OFF)
-DIMHEX_ENABLE_LTO=ON                 # Link-Time Optimization (padrão: OFF)
-DIMHEX_STATIC_LINK_PLUGINS=ON        # Plugins estáticos (padrão: OFF)
-DIMHEX_USE_DEFAULT_BUILD_SETTINGS=ON # Configurações padrão (padrão: OFF)

# Exemplo
cmake .. -DIMHEX_OFFLINE_BUILD=ON -DIMHEX_ENABLE_LTO=ON
```

---

## 🔍 Solução de Problemas

### ❌ CMake não encontrado

**Windows:**
- Baixe: https://cmake.org/download/
- Ou via chocolatey: `choco install cmake`

**Linux:**
```bash
sudo apt install cmake  # Ubuntu/Debian
sudo dnf install cmake  # Fedora
```

**macOS:**
```bash
brew install cmake
```

### ❌ Visual Studio não encontrado (Windows)

Baixe Visual Studio 2022 Community:
https://visualstudio.microsoft.com/downloads/

Durante instalação, selecione:
- "Desenvolvimento para Desktop com C++"
- "CMake tools for Windows"

### ❌ Dependências faltando (Linux)

```bash
# Ubuntu/Debian
sudo apt install -y build-essential cmake git pkg-config \
    libglfw3-dev libglm-dev libfreetype-dev libmbedtls-dev \
    libcurl4-openssl-dev libdbus-1-dev libfmt-dev \
    liblz4-dev zlib1g-dev

# Fedora
sudo dnf install -y gcc-c++ cmake git pkg-config \
    glfw-devel glm-devel freetype-devel mbedtls-devel \
    libcurl-devel dbus-devel fmt-devel lz4-devel zlib-devel

# Arch
sudo pacman -S base-devel cmake git pkg-config \
    glfw glm freetype2 mbedtls curl dbus fmt lz4 zlib
```

### ❌ Erro de memória durante compilação

Reduza paralelismo:
```bash
# Em vez de -j (todos os CPUs)
cmake --build . -j4  # Apenas 4 jobs

# Ou 
.\build.ps1 -Jobs 4  # Windows
./build.sh --jobs 4  # Linux/macOS
```

### ❌ Executável não encontrado

Procure em:
- **Windows**: `build\windows\main\gui\Release\imhex.exe`
- **Linux/macOS**: `build/x86_64/imhex`

---

## ✅ Verificação Pós-Build

### 1. Executar ImHex

```bash
# Windows
build\windows\main\gui\Release\imhex.exe

# Linux/macOS
./build/x86_64/imhex
```

### 2. Habilitar MCP Server

1. Abra ImHex
2. Menu: **Edit** → **Settings**
3. Aba: **General** → **Network**
4. Marque: **☑ MCP Server**
5. Porta padrão: `8080`
6. Clique: **Save**
7. Reinicie ImHex

### 3. Verificar MCP ativo

```bash
# Testar se servidor está rodando
curl http://localhost:8080/health

# Deve retornar: {"status":"ok"}
```

### 4. Instalar cliente Python

```bash
cd src/imhex_mcp_client
pip install -e .
```

### 5. Testar conexão

```bash
python tests/mcp_connection_test.py
```

Deve mostrar:
```
✓ Conexão estabelecida
✓ Servidor respondendo
✓ MCP funcionando!
```

---

## 📚 Documentação Adicional

- [QUICK_START.md](QUICK_START.md) - Guia completo passo a passo
- [docs/building_imhex_with_mcp.md](docs/building_imhex_with_mcp.md) - Detalhes técnicos
- [docs/how_to_enable_mcp_server.md](docs/how_to_enable_mcp_server.md) - Configuração MCP
- [MCP_CONNECTOR_README.md](MCP_CONNECTOR_README.md) - Sobre MCP Connector

---

## 🚀 Comandos Resumidos

### Windows (PowerShell)
```powershell
# Build completo automático
.\build.ps1

# Executar
.\build\windows\main\gui\Release\imhex.exe

# Instalar cliente
pip install -e src/imhex_mcp_client

# Testar
python tests/mcp_connection_test.py
```

### Linux/macOS
```bash
# Build completo automático
chmod +x build.sh && ./build.sh

# Executar
./build/x86_64/imhex

# Instalar cliente
pip install -e src/imhex_mcp_client

# Testar
python tests/mcp_connection_test.py
```

---

## 💡 Dicas

1. **Primeira compilação**: Pode levar 10-30 minutos
2. **Builds subsequentes**: 2-5 minutos (incremental)
3. **Limpar tudo**: Use `--clean` ou apague pasta `build/`
4. **Paralelismo**: Script detecta automaticamente número de CPUs
5. **Debug vs Release**: Release é ~10x mais rápido em runtime

---

## 🆘 Precisa de ajuda?

- Issues: https://github.com/WerWolv/ImHex/issues
- Discord: https://discord.gg/X63jZ36xBY
- Docs: https://docs.werwolv.net/imhex/
