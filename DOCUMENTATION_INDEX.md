# 📚 Índice de Documentação - ImHex MCP

## 🎯 Início Rápido

### Você está aqui pela primeira vez?
➡️ **[START_HERE.md](START_HERE.md)** - Comece aqui! Status, resumo e próximos passos

### Quer compilar AGORA?
➡️ Execute: `.\build.ps1` (Windows) ou `./build.sh` (Linux/macOS)

---

## 📖 Guias por Categoria

### 🚀 Compilação & Build

| Documento | Descrição | Quando usar |
|-----------|-----------|-------------|
| **[START_HERE.md](START_HERE.md)** ⭐ | Status e resumo completo | Primeira vez / Visão geral |
| **[QUICK_START.md](QUICK_START.md)** | Passo a passo detalhado | Seguir tutorial completo |
| **[BUILD.md](BUILD.md)** | Guia técnico completo | Problemas / Opções avançadas |
| **[BUILD_SETUP_SUMMARY.md](BUILD_SETUP_SUMMARY.md)** | O que foi configurado | Entender mudanças |

### 🔌 MCP (Model Context Protocol)

| Documento | Descrição | Quando usar |
|-----------|-----------|-------------|
| **[docs/how_to_enable_mcp_server.md](docs/how_to_enable_mcp_server.md)** | Habilitar MCP | Após compilação |
| **[docs/building_imhex_with_mcp.md](docs/building_imhex_with_mcp.md)** | Build com MCP | Detalhes técnicos MCP |
| **[docs/mcp_tools_reference.md](docs/mcp_tools_reference.md)** | Referência de ferramentas | Usar ferramentas MCP |
| **[MCP_CONNECTOR_README.md](MCP_CONNECTOR_README.md)** | Sobre MCP Connector | Entender o projeto |

### 🛠️ Scripts e Ferramentas

| Script | Plataforma | Descrição |
|--------|-----------|-----------|
| `build.ps1` | Windows | Build automático |
| `build.sh` | Linux/macOS | Build automático |
| `check-environment.ps1` | Windows | Verificar ambiente |
| `check-environment.sh` | Linux/macOS | Verificar ambiente |

### 📋 Outros Documentos

| Documento | Descrição |
|-----------|-----------|
| **[README.md](README.md)** | README principal do projeto |
| **[INSTALL.md](INSTALL.md)** | Instruções de instalação |
| **[CONTRIBUTING.md](CONTRIBUTING.md)** | Como contribuir |
| **[PLUGINS.md](PLUGINS.md)** | Desenvolvimento de plugins |

---

## 🎯 Fluxo Recomendado

```
1. START_HERE.md
   └─> Entender status e o que fazer

2. .\build.ps1 ou ./build.sh
   └─> Compilar ImHex

3. docs/how_to_enable_mcp_server.md
   └─> Habilitar MCP Server

4. tests/mcp_connection_test.py
   └─> Testar conexão

5. docs/mcp_tools_reference.md
   └─> Usar ferramentas MCP
```

---

## 🔍 Encontre por Pergunta

### "Como compilar?"
- **Rápido**: `.\build.ps1`
- **Completo**: [QUICK_START.md](QUICK_START.md)
- **Técnico**: [BUILD.md](BUILD.md)

### "Como habilitar MCP?"
- [docs/how_to_enable_mcp_server.md](docs/how_to_enable_mcp_server.md)

### "Quais ferramentas MCP existem?"
- [docs/mcp_tools_reference.md](docs/mcp_tools_reference.md)

### "O que foi configurado?"
- [BUILD_SETUP_SUMMARY.md](BUILD_SETUP_SUMMARY.md)
- [START_HERE.md](START_HERE.md)

### "Estou com problema no build"
- [BUILD.md](BUILD.md) - Seção "Troubleshooting"
- `.\check-environment.ps1` - Verificar ambiente

### "Como usar cliente Python?"
- [QUICK_START.md](QUICK_START.md) - Passo 3
- `examples/basic_usage.py` - Exemplo prático

### "Como contribuir?"
- [CONTRIBUTING.md](CONTRIBUTING.md)

---

## ⚡ Comandos Rápidos

### Verificar ambiente
```powershell
.\check-environment.ps1
```

### Compilar
```powershell
.\build.ps1
```

### Compilar do zero
```powershell
.\build.ps1 -Clean
```

### Instalar cliente Python
```powershell
pip install -e src\imhex_mcp_client
```

### Testar MCP
```powershell
python tests\mcp_connection_test.py
```

### Ver exemplo
```powershell
python examples\basic_usage.py
```

---

## 📊 Mapa de Documentação

```
DOCUMENTAÇÃO IMHEX MCP
│
├── 🚀 INÍCIO
│   ├── START_HERE.md ⭐ (COMECE AQUI!)
│   ├── QUICK_START.md (Tutorial passo a passo)
│   └── README.md (Visão geral do projeto)
│
├── 🔧 BUILD
│   ├── BUILD.md (Guia completo)
│   ├── BUILD_SETUP_SUMMARY.md (Resumo configuração)
│   ├── build.ps1 (Script Windows)
│   ├── build.sh (Script Linux/macOS)
│   ├── check-environment.ps1 (Verificação Windows)
│   └── check-environment.sh (Verificação Linux/macOS)
│
├── 🔌 MCP
│   ├── docs/how_to_enable_mcp_server.md (Habilitar)
│   ├── docs/building_imhex_with_mcp.md (Build)
│   ├── docs/mcp_tools_reference.md (Ferramentas)
│   └── MCP_CONNECTOR_README.md (Sobre)
│
├── 🐍 PYTHON
│   ├── src/imhex_mcp_client/ (Cliente)
│   ├── tests/mcp_connection_test.py (Teste)
│   └── examples/basic_usage.py (Exemplo)
│
└── 📋 OUTROS
    ├── INSTALL.md
    ├── CONTRIBUTING.md
    └── PLUGINS.md
```

---

## 🎯 Por Nível de Experiência

### Iniciante
1. [START_HERE.md](START_HERE.md)
2. Execute: `.\build.ps1`
3. [docs/how_to_enable_mcp_server.md](docs/how_to_enable_mcp_server.md)
4. [QUICK_START.md](QUICK_START.md)

### Intermediário
1. [BUILD.md](BUILD.md)
2. [docs/building_imhex_with_mcp.md](docs/building_imhex_with_mcp.md)
3. [docs/mcp_tools_reference.md](docs/mcp_tools_reference.md)

### Avançado
1. [BUILD_SETUP_SUMMARY.md](BUILD_SETUP_SUMMARY.md)
2. [CONTRIBUTING.md](CONTRIBUTING.md)
3. [PLUGINS.md](PLUGINS.md)
4. `CMakePresets.json`

---

## 📝 Atualizações Recentes

**18 de janeiro de 2026**:
- ✅ Criado START_HERE.md
- ✅ Criado BUILD.md
- ✅ Atualizado QUICK_START.md
- ✅ Atualizado README.md
- ✅ Criados scripts build.ps1/sh
- ✅ Criados scripts check-environment.ps1/sh
- ✅ Atualizado CMakePresets.json
- ✅ Criado BUILD_SETUP_SUMMARY.md
- ✅ Criado DOCUMENTATION_INDEX.md (este arquivo)

---

## 🆘 Precisa de Ajuda?

1. **Consulte a documentação** usando este índice
2. **Execute verificação**: `.\check-environment.ps1`
3. **Leia troubleshooting**: [BUILD.md](BUILD.md) seção "Solução de Problemas"
4. **Issues GitHub**: https://github.com/WerWolv/ImHex/issues
5. **Discord**: https://discord.gg/X63jZ36xBY

---

**Pronto para começar? 👉 [START_HERE.md](START_HERE.md)**
