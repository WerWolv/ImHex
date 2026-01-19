# Início Rápido - ImHex MCP Connector

Guia passo a passo para compilar o ImHex com suporte MCP e usar o cliente Python.

## 📋 Checklist Rápido

- [ ] Compilar ImHex do código-fonte
- [ ] Habilitar MCP Server nas configurações
- [ ] Instalar cliente Python
- [ ] Testar conexão
- [ ] Executar exemplos

---

## ⚡ MÉTODO MAIS RÁPIDO (Recomendado!)

### Windows (PowerShell)

```powershell
# 1. Compilar com script automático
.\build.ps1

# 2. Executar ImHex (caminho exibido no final do build)
.\build\windows\main\gui\Release\imhex.exe

# 3. Habilitar MCP: Edit > Settings > General > Network > MCP Server ✓

# 4. Instalar cliente Python
pip install -e src/imhex_mcp_client

# 5. Testar
python tests/mcp_connection_test.py
```

### Linux/macOS

```bash
# 1. Compilar com script automático
chmod +x build.sh && ./build.sh

# 2. Executar ImHex
./build/x86_64/imhex

# 3. Habilitar MCP: Edit > Settings > General > Network > MCP Server ✓

# 4. Instalar cliente Python
pip install -e src/imhex_mcp_client

# 5. Testar
python tests/mcp_connection_test.py
```

> 💡 **Dica**: Os scripts automáticos (`build.ps1` / `build.sh`) detectam automaticamente suas configurações e otimizam o processo!

---

## 🚀 Passo 1: Compilar ImHex (Método Manual)

### Windows

```cmd
REM 1. Instalar Visual Studio 2022 (Community Edition)
REM    Download: https://visualstudio.microsoft.com/downloads/

REM 2. No repositório ImHex (você já está aqui)
mkdir build
cd build

REM 3. Gerar projeto
cmake .. -G "Visual Studio 17 2022" -A x64

REM 4. Compilar (vai demorar 10-30 minutos)
cmake --build . --config Release -j

REM 5. Executar
cd Release
imhex.exe
```

### Linux (Ubuntu/Debian)

```bash
# 1. Instalar dependências
sudo apt update
sudo apt install -y build-essential cmake git pkg-config \
    libglfw3-dev libglm-dev libfreetype-dev libmbedtls-dev \
    libcurl4-openssl-dev libdbus-1-dev libfmt-dev \
    python3 python3-pip

# 2. No repositório ImHex (você já está aqui)
mkdir build
cd build

# 3. Configurar e compilar (10-30 minutos)
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)

# 4. Executar
./imhex
```

### macOS

```bash
# 1. Instalar dependências
brew install cmake git pkg-config glfw glm freetype \
    mbedtls curl dbus fmt python@3

# 2. No repositório ImHex (você já está aqui)
mkdir build
cd build

# 3. Configurar e compilar (10-30 minutos)
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(sysctl -n hw.ncpu)

# 4. Executar
./imhex
```

---

## ⚙️ Passo 2: Habilitar MCP Server

1. **Abra o ImHex** que você acabou de compilar

2. **Acesse Settings**:
   - Menu: **Edit** > **Settings**
   - Ou tecle: `Ctrl+,` (Windows/Linux) ou `Cmd+,` (macOS)

3. **Navegue até General**:
   - Painel esquerdo: clique em **General**

4. **Encontre seção Network**:
   - Role a página até ver **Network**

5. **Marque "MCP Server"**:
   - ✅ Marque a checkbox **"MCP Server"**
   - O servidor inicia automaticamente

6. **Verifique o indicador**:
   - Rodapé do ImHex (canto inferior direito)
   - 🔴 Vermelho = aguardando conexão
   - 🟢 Verde = cliente conectado

---

## 🐍 Passo 3: Instalar Cliente Python

```bash
# Voltar ao diretório raiz do ImHex
cd ..  # se estiver em build/

# Instalar cliente Python
pip install -e src/

# Verificar instalação
python -c "from imhex_mcp_client import ImHexMCPClient; print('✓ Cliente instalado com sucesso!')"
```

---

## ✅ Passo 4: Testar Conexão

### Teste Automatizado

```bash
python tests/mcp_connection_test.py
```

**Saída esperada**:
```
============================================================
ImHex MCP Connection Test Suite
============================================================
✓ Conectado a 127.0.0.1:19743
✓ Initialize aceito (protocol 2025-06-18)
ℹ Servidor: ImHex v1.x.x
✓ Notification 'initialized' enviada
✓ 7 ferramentas disponíveis:
  • open_file: Opens a file from the filesystem...
  • list_open_data_sources: Lists all currently open...
  • select_data_source: Selects a data source...
  • read_data: Reads data from the currently selected...
  • execute_pattern_code: Executes pattern code...
  • get_patterns: Gets the patterns...
  • get_pattern_console_content: Reads the console...
============================================================
✓ Todos os testes passaram!
============================================================
```

### Teste Manual Rápido

```python
from imhex_mcp_client import ImHexMCPClient

client = ImHexMCPClient()
client.connect()
print("✓ Conectado!")
client.disconnect()
```

---

## 🎯 Passo 5: Executar Exemplos

### Exemplo Básico

```bash
# Use qualquer arquivo binário que você tenha
python examples/basic_usage.py /bin/ls
```

### Exemplo Interativo

```python
from imhex_mcp_client import imhex_session, open_file, read_data

with imhex_session() as client:
    # Abrir um arquivo
    ds = open_file(client, "/path/to/file.bin")
    print(f"Aberto: {ds.name} ({ds.size} bytes)")

    # Ler primeiros 64 bytes
    data = read_data(client, address=0, size=64)
    print(f"Magic: {data[:4].hex()}")

    # Ver hex dump
    from imhex_mcp_client import hex_dump
    dump = hex_dump(client, 0, 256)
    print(dump)
```

### Análise com Pattern Language

```python
from imhex_mcp_client import (
    imhex_session,
    open_file,
    execute_pattern_code,
    get_patterns
)

pattern = """
struct Header {
    char magic[4];
    u32 version;
    u32 size;
};

Header header @ 0x00;
"""

with imhex_session() as client:
    open_file(client, "/bin/bash")

    result_code = execute_pattern_code(client, pattern)

    if result_code == 0:
        patterns = get_patterns(client)
        print("Header:")
        for key, value in patterns.get('header', {}).items():
            print(f"  {key}: {value}")
```

---

## 🔧 Solução de Problemas Rápida

### Problema: "ConnectionRefusedError"

**Causa**: MCP Server não está habilitado ou ImHex não está rodando

**Solução**:
1. Certifique-se que o ImHex compilado está rodando
2. Verifique Settings > General > Network > MCP Server ✓
3. Veja indicador no rodapé do ImHex

### Problema: "MCP Server" não aparece nas configurações

**Causa**: Versão antiga ou build sem MCP

**Solução**:
```bash
# Verificar se tem MCP
./imhex --help | grep mcp

# Se não aparecer, recompilar:
cd build
git pull
cmake --build . --config Release -j
```

### Problema: Compilação falhou

**Soluções comuns**:

```bash
# Limpar build anterior
rm -rf build
mkdir build
cd build

# Reconfigurar
cmake .. -DCMAKE_BUILD_TYPE=Release

# Compilar com menos threads (menos RAM)
cmake --build . -j2
```

---

## 📚 Próximos Passos

Agora que está funcionando:

1. 📖 **Leia a documentação completa**: [MCP_CONNECTOR_README.md](MCP_CONNECTOR_README.md)

2. 🔍 **Explore as ferramentas**: [docs/mcp_tools_reference.md](docs/mcp_tools_reference.md)

3. 💡 **Veja ferramentas futuras**: [docs/proposed_tools.md](docs/proposed_tools.md)

4. 🛠️ **Desenvolva suas próprias automações**:
   ```python
   from imhex_mcp_client import imhex_session, open_file, read_data

   with imhex_session() as client:
       # Seu código aqui!
       pass
   ```

---

## 📞 Precisa de Ajuda?

- **Compilação do ImHex**: [docs/building_imhex_with_mcp.md](docs/building_imhex_with_mcp.md)
- **Habilitar MCP Server**: [docs/how_to_enable_mcp_server.md](docs/how_to_enable_mcp_server.md)
- **Documentação ImHex**: https://docs.werwolv.net/imhex/
- **Discord ImHex**: https://discord.gg/X63jZ36xBY
- **Issues GitHub**: https://github.com/WerWolv/ImHex/issues

---

## ✨ Resumo em 1 Minuto

```bash
# 1. Compilar ImHex
mkdir build && cd build
cmake .. && cmake --build . -j

# 2. Executar ImHex
./imhex  # ou Release\imhex.exe no Windows

# 3. Habilitar MCP
# Settings > General > Network > MCP Server ✓

# 4. Instalar cliente Python
cd ..
pip install -e src/

# 5. Testar
python tests/mcp_connection_test.py

# 6. Usar!
python examples/basic_usage.py /bin/ls
```

**Pronto! 🎉**
