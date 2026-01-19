# Compilando ImHex com Suporte MCP

## ⚠️ Informação Importante

O suporte ao **MCP Server** foi adicionado ao ImHex em **16 de dezembro de 2024** (commit `e696d384c`). Esta funcionalidade **NÃO está disponível nas versões estáveis** ainda.

### Status Atual

- **Última versão estável**: v1.38.1 (SEM suporte MCP)
- **Suporte MCP**: Apenas em versões nightly (development)
- **Commits desde v1.38.1**: 231 commits (incluindo MCP)

### Suas Opções

Você tem 3 opções para usar o MCP Server:

1. ✅ **Compilar do código-fonte** (recomendado - você já está neste repo)
2. ⏳ **Aguardar próxima versão estável** (v1.39.0 ou superior)
3. 🔧 **Baixar build nightly** (se disponível para seu sistema)

---

## Opção 1: Compilar do Código-Fonte (Recomendado)

Você já está no repositório correto! Vamos compilar.

### Pré-requisitos

#### Windows
- **Visual Studio 2022** (Community Edition é suficiente)
  - Workload: "Desktop development with C++"
- **CMake** 3.16 ou superior
- **Git**
- **Python** 3.8+ (para os scripts de build)

#### Linux (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install build-essential cmake git pkg-config \
    libglfw3-dev libglm-dev libfreetype-dev libmbedtls-dev \
    libcurl4-openssl-dev libdbus-1-dev libfmt-dev \
    python3 python3-pip
```

#### macOS
```bash
# Instalar Homebrew se não tiver: https://brew.sh

brew install cmake git pkg-config glfw glm freetype mbedtls \
    curl dbus fmt python@3
```

### Verificar Branch Atual

```bash
# Você deve estar no branch que contém MCP
git log --oneline | grep -i mcp | head -5
```

Saída esperada:
```
550fe8e4a impr: Add MCP Client information to footer icon tooltip
7df4b1157 impr: Make sure all data is received by MCP bridge
ba7e789a8 feat: Add support for executing patterns using MCP
...
```

### Compilação

#### Windows

```cmd
# 1. Criar diretório de build
mkdir build
cd build

# 2. Gerar arquivos de projeto
cmake .. -G "Visual Studio 17 2022" -A x64

# 3. Compilar
cmake --build . --config Release -j

# 4. Executar
cd Release
.\imhex.exe
```

#### Linux

```bash
# 1. Criar diretório de build
mkdir build
cd build

# 2. Configurar CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# 3. Compilar
cmake --build . -j$(nproc)

# 4. Executar
./imhex
```

#### macOS

```bash
# 1. Criar diretório de build
mkdir build
cd build

# 2. Configurar CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# 3. Compilar
cmake --build . -j$(sysctl -n hw.ncpu)

# 4. Executar
./imhex
```

### Tempo de Compilação

- **Primeira compilação**: 10-30 minutos (dependendo do hardware)
- **Compilações incrementais**: 1-5 minutos

### Solução de Problemas na Compilação

#### Erro: "CMake version too old"
```bash
# Instalar CMake mais recente
# Windows: baixar de https://cmake.org/download/
# Linux: usar snap
sudo snap install cmake --classic
```

#### Erro: "Could not find package X"
```bash
# Windows: geralmente resolvido pelo CMake automaticamente
# Linux: instalar dependências faltantes
sudo apt install libX-dev  # substituir X pelo pacote faltante
```

#### Erro: "Out of memory during compilation"
```bash
# Reduzir paralelismo
cmake --build . -j2  # ao invés de -j$(nproc)
```

---

## Opção 2: Build Nightly

Alguns sistemas podem ter builds nightly pré-compilados:

### Windows
```powershell
# Baixar do GitHub Actions (requer login)
# https://github.com/WerWolv/ImHex/actions
```

### Linux (AppImage)
```bash
# Verificar releases nightly
# https://github.com/WerWolv/ImHex/releases/tag/nightly

# Exemplo:
wget https://github.com/WerWolv/ImHex/releases/download/nightly/ImHex-nightly-x86_64.AppImage
chmod +x ImHex-nightly-x86_64.AppImage
./ImHex-nightly-x86_64.AppImage
```

### macOS
```bash
# Baixar DMG do nightly
# https://github.com/WerWolv/ImHex/releases/tag/nightly
```

---

## Opção 3: Aguardar Versão Estável

Se não quiser compilar, você pode aguardar a próxima versão estável:

- **Versão esperada**: v1.39.0 ou v2.0.0
- **Quando**: Indeterminado (ImHex não tem calendário de releases fixo)
- **Acompanhar**: https://github.com/WerWolv/ImHex/releases

---

## Verificando se MCP Está Disponível

Após compilar ou instalar o ImHex:

### 1. Verificar Via GUI

1. Abra o ImHex
2. **Edit** > **Settings** (ou `Ctrl+,`)
3. Clique em **General**
4. Role até a seção **Network**
5. **Deve aparecer**: checkbox "MCP Server" ✓

Se a opção **não aparecer**, o build não tem suporte MCP.

### 2. Verificar Via Linha de Comando

```bash
# Windows
imhex.exe --help | findstr mcp

# Linux/macOS
./imhex --help | grep mcp
```

Saída esperada:
```
  --mcp    Start ImHex in MCP mode
```

### 3. Verificar Versão

```bash
# Windows
imhex.exe --version

# Linux/macOS
./imhex --version
```

Deve mostrar uma versão **posterior a 1.38.1**, exemplo:
```
ImHex v1.39.0-dev-231-g550fe8e4a
```

---

## Depois de Compilar

### 1. Habilitar MCP Server

Siga o guia: [how_to_enable_mcp_server.md](how_to_enable_mcp_server.md)

### 2. Testar Conexão

```bash
# Voltar ao diretório raiz do projeto
cd ..

# Executar teste
python tests/mcp_connection_test.py
```

### 3. Usar o Cliente Python

```bash
# Instalar cliente (se ainda não instalou)
pip install -e src/

# Executar exemplos
python examples/basic_usage.py /path/to/file.bin
```

---

## Estrutura de Build

Após compilação bem-sucedida:

```
build/
├── imhex(.exe)              # Executável principal
├── plugins/                 # Plugins compilados
│   └── builtin/
│       └── builtin.hexplug  # Plugin com suporte MCP
├── lib/                     # Bibliotecas
└── ...
```

O suporte MCP está no plugin `builtin`, especificamente:
- `plugins/builtin/source/content/settings_entries.cpp` (configuração)
- `plugins/builtin/source/content/background_services.cpp` (serviço)
- `lib/libimhex/source/mcp/server.cpp` (servidor MCP)

---

## Dicas para Desenvolvimento

### Recompilar Apenas o Necessário

```bash
# Após modificar código
cd build
cmake --build . --target imhex -j
```

### Build de Debug (para desenvolvimento)

```bash
mkdir build-debug
cd build-debug
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . -j
```

### Limpar Build

```bash
# Remover diretório de build
rm -rf build

# Ou limpar dentro do build
cd build
cmake --build . --target clean
```

---

## Alternativa: Usar Docker (Experimental)

Se não quiser instalar todas as dependências:

```dockerfile
# Dockerfile para compilar ImHex
FROM ubuntu:22.04

RUN apt update && apt install -y \
    build-essential cmake git pkg-config \
    libglfw3-dev libglm-dev libfreetype-dev \
    libmbedtls-dev libcurl4-openssl-dev \
    libdbus-1-dev libfmt-dev

WORKDIR /imhex
COPY . .

RUN mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    cmake --build . -j$(nproc)

CMD ["/imhex/build/imhex"]
```

```bash
# Compilar
docker build -t imhex-mcp .

# Executar (com X11 forwarding no Linux)
docker run -it --rm \
    -e DISPLAY=$DISPLAY \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    imhex-mcp
```

---

## Resumo

### Para Usar MCP Server AGORA:

1. ✅ **Você JÁ está no repositório correto**
2. ✅ **Compilar**: `mkdir build && cd build && cmake .. && cmake --build . -j`
3. ✅ **Executar**: `./imhex` (ou `Release\imhex.exe` no Windows)
4. ✅ **Habilitar MCP**: Settings > General > Network > MCP Server ☑
5. ✅ **Testar**: `python tests/mcp_connection_test.py`

### Para Usar MCP Server DEPOIS:

1. ⏳ **Aguardar** versão v1.39.0+ ser lançada
2. ⏳ **Baixar** release oficial
3. ⏳ **Instalar** normalmente
4. ✅ **Habilitar MCP** e usar

---

## Links Úteis

- **Documentação de Build**: https://docs.werwolv.net/imhex/common/building-from-source
- **Releases**: https://github.com/WerWolv/ImHex/releases
- **Actions (nightlies)**: https://github.com/WerWolv/ImHex/actions
- **Discord**: https://discord.gg/X63jZ36xBY (para ajuda com compilação)
