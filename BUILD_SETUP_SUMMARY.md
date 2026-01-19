# ✅ Configuração de Build Completa!

## 📦 O que foi configurado

### 1. CMake Presets Atualizados

**Arquivo**: `CMakePresets.json`

Adicionado novo preset `windows-default`:
- Build simplificado sem necessidade de vcpkg
- Gerador Visual Studio 2022
- Configuração Release otimizada
- Caminho de build: `build/windows/`

### 2. Scripts de Build Automáticos

#### Windows: `build.ps1`
- ✅ Detecta automaticamente número de CPUs
- ✅ Opção de limpeza (`-Clean`)
- ✅ Seleção de preset (`-Preset`)
- ✅ Controle de paralelismo (`-Jobs`)
- ✅ Mostra caminho do executável ao final
- ✅ Instruções pós-build incluídas

#### Linux/macOS: `build.sh`
- ✅ Mesmas funcionalidades do Windows
- ✅ Compatível com bash
- ✅ Detecta nproc ou sysctl automaticamente

### 3. Documentação de Build

Criados 3 guias completos:

#### `BUILD.md` (NOVO!)
- Guia completo de build
- Todos os métodos de compilação
- Opções de CMake explicadas
- Troubleshooting detalhado
- Verificação pós-build

#### `QUICK_START.md` (ATUALIZADO!)
- Adicionada seção "Método Mais Rápido"
- Scripts automáticos em destaque
- Passo a passo completo
- Do zero até testar MCP

#### `README.md` (ATUALIZADO!)
- Seção "Quick Build" adicionada
- Links para documentação
- Scripts destacados

---

## 🚀 Como Usar - Comandos Rápidos

### Primeira Compilação

#### Windows
```powershell
# Método 1: Script automático (RECOMENDADO!)
.\build.ps1

# Método 2: Manual com preset
cmake --preset windows-default
cmake --build --preset windows-default -j

# Método 3: Visual Studio tradicional
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release -j
```

#### Linux/macOS
```bash
# Método 1: Script automático (RECOMENDADO!)
chmod +x build.sh && ./build.sh

# Método 2: Manual com preset
cmake --preset x86_64
cmake --build --preset x86_64 -j$(nproc)

# Método 3: Manual tradicional
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

### Rebuilds / Alterações

```powershell
# Windows - incremental (rápido!)
.\build.ps1

# Windows - limpar tudo e recompilar
.\build.ps1 -Clean

# Linux/macOS
./build.sh
./build.sh --clean
```

---

## 📍 Localização dos Arquivos

### Executável Compilado

**Windows:**
```
build/windows/main/gui/Release/imhex.exe
```

**Linux/macOS:**
```
build/x86_64/imhex
```

### Scripts de Build
```
build.ps1    # Windows PowerShell
build.sh     # Linux/macOS Bash
```

### Documentação
```
BUILD.md                           # Guia completo de build
QUICK_START.md                     # Início rápido
README.md                          # README principal (atualizado)
docs/building_imhex_with_mcp.md    # Build com MCP
docs/how_to_enable_mcp_server.md   # Habilitar MCP
```

---

## ✅ Checklist de Uso

### Passo 1: Build
- [ ] Execute `.\build.ps1` (Windows) ou `./build.sh` (Linux/macOS)
- [ ] Aguarde conclusão (10-30 min primeira vez)
- [ ] Anote caminho do executável exibido

### Passo 2: Executar ImHex
- [ ] Execute o binário gerado
- [ ] Verifique se interface abre

### Passo 3: Habilitar MCP
- [ ] Menu: Edit → Settings
- [ ] General → Network
- [ ] Marque: ☑ MCP Server
- [ ] Salve e reinicie

### Passo 4: Cliente Python
- [ ] `cd src/imhex_mcp_client`
- [ ] `pip install -e .`

### Passo 5: Testar
- [ ] `python tests/mcp_connection_test.py`
- [ ] Verificar sucesso ✓

---

## 🔧 Opções Avançadas

### Scripts de Build

#### Windows (build.ps1)
```powershell
# Limpeza completa
.\build.ps1 -Clean

# Preset específico
.\build.ps1 -Preset vs2022

# Controlar paralelismo (útil se ficar sem RAM)
.\build.ps1 -Jobs 4

# Combinações
.\build.ps1 -Clean -Preset x86_64 -Jobs 8
```

#### Linux/macOS (build.sh)
```bash
# Limpeza completa
./build.sh --clean

# Preset específico
./build.sh --preset x86_64

# Controlar paralelismo
./build.sh --jobs 4

# Combinações
./build.sh --clean --preset x86_64 --jobs 8
```

### CMake Manual

```bash
# Listar presets disponíveis
cmake --list-presets

# Ver opções de build
cmake -B build -L

# Build com opções específicas
cmake --preset windows-default \
  -DIMHEX_OFFLINE_BUILD=ON \
  -DIMHEX_ENABLE_LTO=ON
```

---

## 🎯 Presets Disponíveis

| Preset | Plataforma | Descrição |
|--------|-----------|-----------|
| `windows-default` | Windows | Build padrão Visual Studio (NOVO!) |
| `vs2022` | Windows | VS2022 com vcpkg |
| `vs2022-x86` | Windows | VS2022 x86 com vcpkg |
| `x86_64` | Linux/macOS | Build padrão Unix |
| `xcode` | macOS | Projeto Xcode |

---

## 📊 Tempos Estimados

| Etapa | Primeira vez | Incremental |
|-------|--------------|-------------|
| Configuração CMake | 1-3 min | 5-30 seg |
| Compilação completa | 10-30 min | - |
| Compilação incremental | - | 30 seg - 5 min |
| Total (primeira vez) | **15-35 min** | **1-6 min** |

> 💡 **Dica**: Builds incrementais (após mudanças pequenas) são muito mais rápidos!

---

## 🆘 Problemas Comuns

### ❌ CMake não encontrado
```powershell
# Windows (Chocolatey)
choco install cmake

# Ou baixar de: https://cmake.org/download/
```

### ❌ Visual Studio não encontrado
Baixe VS2022 Community: https://visualstudio.microsoft.com/downloads/

Componentes necessários:
- ✅ Desenvolvimento para Desktop com C++
- ✅ CMake tools for Windows

### ❌ Erro de memória (RAM)
```powershell
# Reduza paralelismo
.\build.ps1 -Jobs 4  # Em vez de todos os CPUs
```

### ❌ Executável não encontrado
Procure em:
- `build/windows/main/gui/Release/imhex.exe`
- `build/windows/Release/imhex.exe`
- `build/x86_64/imhex`

---

## 📚 Documentação Relacionada

- [BUILD.md](BUILD.md) - Guia completo de build
- [QUICK_START.md](QUICK_START.md) - Início rápido
- [MCP_CONNECTOR_README.md](MCP_CONNECTOR_README.md) - Sobre MCP Connector
- [docs/building_imhex_with_mcp.md](docs/building_imhex_with_mcp.md) - Build com MCP
- [docs/how_to_enable_mcp_server.md](docs/how_to_enable_mcp_server.md) - Habilitar MCP
- [docs/mcp_tools_reference.md](docs/mcp_tools_reference.md) - Referência MCP Tools

---

## 🎉 Próximos Passos

Após build bem-sucedido:

1. **Executar ImHex** e verificar funcionamento
2. **Habilitar MCP Server** nas configurações
3. **Instalar cliente Python**: `pip install -e src/imhex_mcp_client`
4. **Testar conexão**: `python tests/mcp_connection_test.py`
5. **Explorar ferramentas MCP**: Ver `docs/mcp_tools_reference.md`
6. **Executar exemplos**: `python examples/basic_usage.py`

---

## ✨ Novidades desta Configuração

### Scripts Inteligentes
- ✅ Detecção automática de ambiente
- ✅ Otimização de paralelismo
- ✅ Feedback visual em tempo real
- ✅ Instruções pós-build automáticas

### Presets Simplificados
- ✅ Preset Windows sem vcpkg (`windows-default`)
- ✅ Configurações otimizadas por plataforma
- ✅ Menos dependências externas

### Documentação Completa
- ✅ 3 guias complementares (BUILD.md, QUICK_START.md, README.md)
- ✅ Troubleshooting detalhado
- ✅ Comandos prontos para copiar/colar

---

**Pronto para compilar! 🚀**

Execute:
- Windows: `.\build.ps1`
- Linux/macOS: `./build.sh`
