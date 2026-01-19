# ImHex MCP Connector - Cliente Python

Cliente Python para conexão com o servidor MCP (Model Context Protocol) do ImHex, permitindo análise programática de arquivos binários através de uma API simples.

## Visão Geral

O ImHex é um editor hexadecimal avançado que possui suporte nativo ao MCP. Este cliente Python fornece:

- ✅ Conexão TCP ao servidor MCP do ImHex (porta 19743)
- ✅ Protocolo JSON-RPC 2.0 completo
- ✅ 7 ferramentas MCP disponíveis
- ✅ API de alto nível com type hints
- ✅ Wrappers convenientes para operações comuns
- ✅ Suporte a context manager

## ⚠️ Pré-requisitos Importantes

### 1. ImHex com Suporte MCP

**ATENÇÃO**: O suporte ao MCP Server foi adicionado em **16/12/2024** e **NÃO está nas versões estáveis** ainda.

- ❌ **Versões v1.38.1 e anteriores**: NÃO têm suporte MCP
- ✅ **Versões nightly/development**: TÊM suporte MCP
- ✅ **Compilação do código-fonte**: Você JÁ está no repositório correto!

**📦 Como Obter ImHex com MCP**:
- **Recomendado**: Compile este repositório seguindo [docs/building_imhex_with_mcp.md](docs/building_imhex_with_mcp.md)
- **Alternativa**: Aguarde versão v1.39.0+ ser lançada

### 2. Outros Requisitos

- **Python** 3.8 ou superior
- ImHex compilado e rodando
- **MCP Server habilitado** nas configurações

### Habilitando MCP no ImHex (Após Compilar)

**Guia Rápido:**
1. Abra o ImHex
2. Vá em **Edit** > **Settings** (ou `Ctrl+,`)
3. Clique em **General** no painel esquerdo
4. Role até a seção **Network**
5. Marque a checkbox **"MCP Server"**
6. O servidor inicia automaticamente na porta 19743

**📖 Guia Detalhado**: Ver [docs/how_to_enable_mcp_server.md](docs/how_to_enable_mcp_server.md) para instruções completas, solução de problemas e verificação de status

## Instalação

```bash
# Clone o repositório
git clone https://github.com/WerWolv/ImHex.git
cd ImHex

# Instale o pacote Python (modo desenvolvimento)
pip install -e src/
```

## Uso Rápido

### Exemplo Básico

```python
from imhex_mcp_client import ImHexMCPClient, open_file, read_data

# Conectar ao ImHex
client = ImHexMCPClient()
client.connect()
client.initialize("MeuApp", "1.0.0")

# Abrir um arquivo
ds = open_file(client, "/path/to/firmware.bin")
print(f"Aberto: {ds.name} ({ds.size} bytes)")

# Ler os primeiros 256 bytes
data = read_data(client, address=0, size=256)
print(f"Magic bytes: {data[:4].hex()}")

# Desconectar
client.disconnect()
```

### Usando Context Manager

```python
from imhex_mcp_client import imhex_session, open_file, read_data

with imhex_session("MeuApp", "1.0.0") as client:
    ds = open_file(client, "/bin/ls")
    header = read_data(client, 0, 64)

    if header[:4] == b'\x7fELF':
        print("Arquivo ELF detectado!")
```

### Análise com Pattern Language

```python
from imhex_mcp_client import (
    imhex_session,
    open_file,
    execute_pattern_code,
    get_patterns,
    get_pattern_console_content
)

pattern_code = """
struct ELFHeader {
    char magic[4];
    u8 class;
    u8 endian;
    u8 version;
    u8 os_abi;
    u64 padding;
    u16 type;
    u16 machine;
};

ELFHeader elf_header @ 0x00;
"""

with imhex_session() as client:
    open_file(client, "/bin/bash")

    result_code = execute_pattern_code(client, pattern_code)

    if result_code == 0:
        patterns = get_patterns(client)
        print("ELF Header:")
        print(f"  Magic: {patterns['elf_header']['magic']}")
        print(f"  Class: {patterns['elf_header']['class']}")
        print(f"  Endian: {patterns['elf_header']['endian']}")
    else:
        console = get_pattern_console_content(client)
        print(f"Erro: {console}")
```

### Hex Dump

```python
from imhex_mcp_client import imhex_session, open_file, hex_dump

with imhex_session() as client:
    open_file(client, "/path/to/file.bin")
    dump = hex_dump(client, address=0, size=256, width=16)
    print(dump)
```

Saída:
```
00000000: 7f 45 4c 46 02 01 01 00 00 00 00 00 00 00 00 00  .ELF............
00000010: 02 00 3e 00 01 00 00 00 10 5f 00 00 00 00 00 00  ..>......_......
00000020: 40 00 00 00 00 00 00 00 ...
```

### Trabalhando com Múltiplos Arquivos

```python
from imhex_mcp_client import (
    imhex_session,
    open_file,
    list_open_data_sources,
    select_data_source,
    read_data
)

with imhex_session() as client:
    # Abrir múltiplos arquivos
    ds1 = open_file(client, "/path/to/file1.bin")
    ds2 = open_file(client, "/path/to/file2.bin")

    # Listar todos abertos
    sources = list_open_data_sources(client)
    for ds in sources:
        print(f"{ds.handle}: {ds.name} ({ds.size} bytes)")

    # Trabalhar com file1
    select_data_source(client, ds1.handle)
    data1 = read_data(client, 0, 16)

    # Trabalhar com file2
    select_data_source(client, ds2.handle)
    data2 = read_data(client, 0, 16)

    # Comparar headers
    if data1 == data2:
        print("Arquivos têm o mesmo header!")
```

## Ferramentas Disponíveis

### 1. open_file
Abre um arquivo do sistema de arquivos.

```python
ds = open_file(client, "/path/to/file.bin")
# Retorna: DataSource(name, type, size, is_writable, handle)
```

### 2. list_open_data_sources
Lista todos os data sources abertos.

```python
sources = list_open_data_sources(client)
for ds in sources:
    print(f"{ds.handle}: {ds.name}")
```

### 3. select_data_source
Seleciona data source ativo.

```python
select_data_source(client, handle=1)
```

### 4. read_data
Lê bytes de data source (max 16 MiB por chamada).

```python
data = read_data(client, address=0x100, size=1024)
print(data.hex())
```

### 5. execute_pattern_code
Executa código Pattern Language.

```python
result_code = execute_pattern_code(client, """
    struct Header {
        u32 magic;
        u32 version;
    };
    Header header @ 0x00;
""")
```

### 6. get_patterns
Recupera patterns gerados.

```python
if result_code == 0:
    patterns = get_patterns(client)
    print(patterns['header']['magic'])
```

### 7. get_pattern_console_content
Lê console output do Pattern Language.

```python
console = get_pattern_console_content(client)
print(console)
```

## Funções Convenientes

### read_file_header
Abre arquivo e lê header em uma operação.

```python
from imhex_mcp_client import imhex_session, read_file_header

with imhex_session() as client:
    header = read_file_header(client, "/path/to/file.bin", header_size=512)
    print(f"Magic: {header[:4].hex()}")
```

### analyze_with_pattern
Abre arquivo e analisa com Pattern Language.

```python
from imhex_mcp_client import imhex_session, analyze_with_pattern

pattern = """
struct PNGHeader {
    char signature[8];
    u32 width;
    u32 height;
};
PNGHeader png @ 0x00;
"""

with imhex_session() as client:
    result = analyze_with_pattern(client, "/path/to/image.png", pattern)

    if 'error' not in result:
        print(f"PNG: {result['png']['width']}x{result['png']['height']}")
    else:
        print(f"Erro: {result['console']}")
```

## Teste de Conexão

Para verificar se o servidor MCP está ativo:

```bash
python tests/mcp_connection_test.py
```

Saída esperada:
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
  • list_open_data_sources: Lists all currently open data sources...
  ...
============================================================
✓ Todos os testes passaram!
============================================================
```

## Documentação

- **Referência de Ferramentas**: [docs/mcp_tools_reference.md](docs/mcp_tools_reference.md)
- **Ferramentas Propostas**: [docs/proposed_tools.md](docs/proposed_tools.md)

## Arquitetura

```
src/imhex_mcp_client/
├── __init__.py          # Exports principais
├── protocol.py          # JSON-RPC 2.0 implementation
├── client.py            # Cliente MCP
└── tools.py             # Wrappers de ferramentas

tests/
└── mcp_connection_test.py  # Teste de conexão

docs/
├── mcp_tools_reference.md  # Documentação das 7 ferramentas
└── proposed_tools.md       # Ferramentas propostas (plugin futuro)
```

## Solução de Problemas

### Conexão Recusada

**Problema**: `ConnectionRefusedError: Connection refused to 127.0.0.1:19743`

**Solução**:
1. Certifique-se de que o ImHex está rodando
2. Habilite MCP Server em **Settings > General > MCP Server**
3. Verifique se a porta 19743 não está bloqueada por firewall

### Timeout

**Problema**: `TimeoutError: Request timed out after 30s`

**Solução**:
- Pattern Language complexo pode demorar - aumente timeout:
  ```python
  client = ImHexMCPClient(timeout=60.0)  # 60 segundos
  ```

### Dados Vazios

**Problema**: `read_data()` retorna bytes vazios

**Solução**:
- Verifique se o arquivo foi aberto com `open_file()` primeiro
- Confirme que o data source foi selecionado com `select_data_source()`
- Verifique se `address` está dentro do tamanho do arquivo

## Limitações

- **Leitura máxima**: 16 MiB por chamada de `read_data()`
- **Conexão**: Apenas localhost (127.0.0.1)
- **Protocolo**: JSON-RPC 2.0 apenas
- **Escrita**: Não suportada nas ferramentas básicas (requer plugin)

## Expansão Futura

O plano inclui desenvolvimento de um **plugin C++** para ImHex que adicionará:

- ✨ Ferramentas de escrita (write_data, apply_patch)
- ✨ Análise avançada (get_entropy, find_patterns)
- ✨ Visualização (get_hex_dump, get_disassembly)
- ✨ Hashing (calculate_hash, hash_file_regions)
- ✨ Gerenciamento de projeto (save_project, load_project)

Ver [docs/proposed_tools.md](docs/proposed_tools.md) para detalhes.

## Recursos

- **ImHex**: https://github.com/WerWolv/ImHex
- **ImHex Docs**: https://docs.werwolv.net/imhex/
- **Pattern Language**: https://docs.werwolv.net/pattern-language/
- **MCP Spec**: https://spec.modelcontextprotocol.io/

## Suporte

- **Issues**: https://github.com/WerWolv/ImHex/issues
- **Discord**: https://discord.gg/X63jZ36xBY
