# ImHex MCP Tools - Referência Completa

Este documento descreve as 7 ferramentas MCP (Model Context Protocol) disponíveis no ImHex para análise de arquivos binários.

## Índice

1. [open_file](#1-open_file) - Abre um arquivo do sistema
2. [list_open_data_sources](#2-list_open_data_sources) - Lista data sources abertos
3. [select_data_source](#3-select_data_source) - Seleciona data source ativo
4. [read_data](#4-read_data) - Lê dados binários
5. [execute_pattern_code](#5-execute_pattern_code) - Executa Pattern Language
6. [get_patterns](#6-get_patterns) - Recupera patterns gerados
7. [get_pattern_console_content](#7-get_pattern_console_content) - Lê console do Pattern Language

---

## 1. open_file

**Título**: Open File

**Descrição**: Abre um arquivo do sistema de arquivos no ImHex. Esta é sempre a primeira operação necessária antes de usar qualquer outra ferramenta. Um arquivo permanece aberto até ser fechado pelo usuário.

### Input Schema

```json
{
  "file_path": "string (obrigatório)"
}
```

**Parâmetros**:
- `file_path` (string, obrigatório): Caminho completo do arquivo a ser aberto

### Output Schema

```json
{
  "name": "string",
  "type": "string",
  "size": "number",
  "is_writable": "boolean",
  "handle": "number"
}
```

**Retorno**:
- `name` (string): Nome do data source
- `type` (string): Tipo interno do data source (ex: "hex.builtin.provider.file")
- `size` (number): Tamanho do arquivo em bytes
- `is_writable` (boolean): Se o data source permite escrita
- `handle` (number): Identificador único do data source para referência em outras ferramentas

### Exemplo de Requisição

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "tools/call",
  "params": {
    "name": "open_file",
    "arguments": {
      "file_path": "/home/user/firmware.bin"
    }
  }
}
```

### Exemplo de Resposta

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "content": [
      {
        "type": "text",
        "text": "Opened file: firmware.bin (1048576 bytes)"
      }
    ],
    "structuredContent": {
      "name": "firmware.bin",
      "type": "hex.builtin.provider.file",
      "size": 1048576,
      "is_writable": true,
      "handle": 1
    }
  }
}
```

### Casos de Erro

- **Arquivo não existe**: Operação falha silenciosamente (provider não é criado)
- **Sem permissão de leitura**: Provider criado mas pode não permitir operações
- **Caminho inválido**: Operação falha

---

## 2. list_open_data_sources

**Título**: List Open Data Sources

**Descrição**: Lista todos os data sources atualmente abertos com seus nomes e handles que podem ser usados para referenciá-los em outras ferramentas.

### Input Schema

```json
{}
```

**Parâmetros**: Nenhum

### Output Schema

```json
{
  "data_sources": [
    {
      "name": "string",
      "type": "string",
      "size": "number",
      "is_writable": "boolean",
      "handle": "number"
    }
  ]
}
```

**Retorno**:
- `data_sources` (array): Lista de todos os data sources abertos
  - `name` (string): Nome do data source
  - `type` (string): Tipo interno do data source
  - `size` (number): Tamanho em bytes
  - `is_writable` (boolean): Se permite escrita
  - `handle` (number): Identificador único

### Exemplo de Requisição

```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "method": "tools/call",
  "params": {
    "name": "list_open_data_sources",
    "arguments": {}
  }
}
```

### Exemplo de Resposta

```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "result": {
    "content": [
      {
        "type": "text",
        "text": "Found 2 open data sources"
      }
    ],
    "structuredContent": {
      "data_sources": [
        {
          "name": "firmware.bin",
          "type": "hex.builtin.provider.file",
          "size": 1048576,
          "is_writable": true,
          "handle": 1
        },
        {
          "name": "bootloader.bin",
          "type": "hex.builtin.provider.file",
          "size": 65536,
          "is_writable": false,
          "handle": 2
        }
      ]
    }
  }
}
```

### Casos de Uso

- Verificar quais arquivos estão abertos
- Obter handles para selecionar data sources específicos
- Listar propriedades de todos os arquivos carregados

---

## 3. select_data_source

**Título**: Select a Data Source given its handle

**Descrição**: Seleciona um dos data sources atualmente abertos pelo seu handle para que possa ser usado em operações subsequentes. Quando executar outras ferramentas que operam em um data source, este será o utilizado. O handle retornado é o que foi selecionado. Se falhar ao selecionar, o handle antigo é retornado (geralmente significa que o handle não existe).

### Input Schema

```json
{
  "handle": "number (obrigatório)"
}
```

**Parâmetros**:
- `handle` (number, obrigatório): Handle do data source a selecionar

### Output Schema

```json
{
  "selected_handle": "number"
}
```

**Retorno**:
- `selected_handle` (number): Handle do data source selecionado

### Exemplo de Requisição

```json
{
  "jsonrpc": "2.0",
  "id": 3,
  "method": "tools/call",
  "params": {
    "name": "select_data_source",
    "arguments": {
      "handle": 2
    }
  }
}
```

### Exemplo de Resposta

```json
{
  "jsonrpc": "2.0",
  "id": 3,
  "result": {
    "content": [
      {
        "type": "text",
        "text": "Selected data source with handle 2"
      }
    ],
    "structuredContent": {
      "selected_handle": 2
    }
  }
}
```

### Casos de Erro

- **Handle inválido**: Retorna o handle anterior (não muda seleção)
- **Data source foi fechado**: Retorna o handle anterior

### Notas

- O primeiro arquivo aberto é automaticamente selecionado
- Necessário selecionar antes de usar `read_data` ou `execute_pattern_code`

---

## 4. read_data

**Título**: Read Binary Data

**Descrição**: Lê dados do data source atualmente selecionado no endereço e tamanho especificados. Os dados são retornados como string codificada em base64. O tamanho máximo que pode ser lido de uma vez é 16 MiB; se mais dados forem solicitados, apenas os primeiros 16 MiB serão retornados.

### Input Schema

```json
{
  "address": "number (obrigatório)",
  "size": "number (obrigatório)"
}
```

**Parâmetros**:
- `address` (number, obrigatório): Endereço (offset) inicial para leitura
- `size` (number, obrigatório): Número de bytes a ler

### Output Schema

```json
{
  "handle": "number",
  "data": "string",
  "data_size": "number"
}
```

**Retorno**:
- `handle` (number): Handle do data source de onde os dados foram lidos
- `data` (string): String codificada em base64 dos dados lidos
- `data_size` (number): Número de bytes efetivamente lidos

### Exemplo de Requisição

```json
{
  "jsonrpc": "2.0",
  "id": 4,
  "method": "tools/call",
  "params": {
    "name": "read_data",
    "arguments": {
      "address": 0,
      "size": 256
    }
  }
}
```

### Exemplo de Resposta

```json
{
  "jsonrpc": "2.0",
  "id": 4,
  "result": {
    "content": [
      {
        "type": "text",
        "text": "Read 256 bytes from address 0x0"
      }
    ],
    "structuredContent": {
      "handle": 1,
      "data": "AQIDBAUG... (base64)",
      "data_size": 256
    }
  }
}
```

### Decodificação Base64

Em Python:
```python
import base64

data_bytes = base64.b64decode(result['data'])
print(data_bytes.hex())
```

Em JavaScript:
```javascript
const dataBytes = Buffer.from(result.data, 'base64');
console.log(dataBytes.toString('hex'));
```

### Limitações

- **Tamanho máximo**: 16 MiB (16777216 bytes) por chamada
- **Endereço inválido**: Se `address >= size_do_arquivo`, retorna dados vazios
- **Leitura parcial**: Se `address + size > size_do_arquivo`, retorna apenas bytes disponíveis

### Casos de Uso

- Ler header de arquivo (primeiros 512 bytes)
- Extrair regiões específicas para análise
- Dump de seções de firmware

---

## 5. execute_pattern_code

**Título**: Execute Pattern Code

**Descrição**: Executa código escrito na Pattern Language no data source atualmente aberto. Isso analisa os dados binários usando os patterns definidos. Você pode então obter a saída do console usando `get_pattern_console_content` e o conteúdo dos patterns gerados usando `get_patterns`. O runtime retorna um `result_code` como inteiro. Se for 0, patterns foram gerados com sucesso. Se for não-zero, ocorreu um erro e você pode verificar a saída do console para detalhes. Este comando pode demorar dependendo da complexidade do código e tamanho do data source.

### Input Schema

```json
{
  "source_code": "string (obrigatório)"
}
```

**Parâmetros**:
- `source_code` (string, obrigatório): Código em Pattern Language a ser executado

### Output Schema

```json
{
  "handle": "number",
  "result_code": "number"
}
```

**Retorno**:
- `handle` (number): Handle do data source usado
- `result_code` (number): Código de retorno (0 = sucesso, não-zero = erro)

### Exemplo de Requisição

```json
{
  "jsonrpc": "2.0",
  "id": 5,
  "method": "tools/call",
  "params": {
    "name": "execute_pattern_code",
    "arguments": {
      "source_code": "struct Header {\n    u32 magic;\n    u32 version;\n    u64 file_size;\n};\n\nHeader header @ 0x00;"
    }
  }
}
```

### Exemplo de Resposta (Sucesso)

```json
{
  "jsonrpc": "2.0",
  "id": 5,
  "result": {
    "content": [
      {
        "type": "text",
        "text": "Pattern executed successfully"
      }
    ],
    "structuredContent": {
      "handle": 1,
      "result_code": 0
    }
  }
}
```

### Exemplo de Resposta (Erro)

```json
{
  "jsonrpc": "2.0",
  "id": 5,
  "result": {
    "content": [
      {
        "type": "text",
        "text": "Pattern execution failed with code 1"
      }
    ],
    "structuredContent": {
      "handle": 1,
      "result_code": 1
    }
  }
}
```

### Pattern Language - Exemplos

**Estrutura Simples**:
```cpp
struct FileHeader {
    char magic[4];
    u32 version;
    u32 entry_count;
};

FileHeader header @ 0x00;
```

**Arrays Dinâmicos**:
```cpp
struct Entry {
    u32 offset;
    u32 size;
};

struct FileTable {
    u32 count;
    Entry entries[count];
};

FileTable table @ 0x10;
```

**Condicionais**:
```cpp
struct Data {
    u32 type;

    if (type == 1) {
        u32 int_value;
    } else if (type == 2) {
        float float_value;
    }
};
```

### Casos de Erro

- **Syntax Error**: `result_code != 0`, verificar console
- **Type Error**: `result_code != 0`, verificar console
- **Runtime Error**: `result_code != 0`, verificar console

### Workflow Típico

1. `execute_pattern_code` → obter `result_code`
2. Se `result_code == 0`: chamar `get_patterns`
3. Se `result_code != 0`: chamar `get_pattern_console_content` para debug

---

## 6. get_patterns

**Título**: Get Patterns

**Descrição**: Recupera os patterns gerados pela última execução de código Pattern Language. Patterns só serão retornados se a última execução foi bem-sucedida (ou seja, `result_code` foi 0). Se a execução falhou, nenhum pattern será retornado.

### Input Schema

```json
{}
```

**Parâmetros**: Nenhum

### Output Schema

```json
{
  "handle": "number",
  "patterns": "object"
}
```

**Retorno**:
- `handle` (number): Handle do data source
- `patterns` (object): Objeto contendo os patterns gerados pela Pattern Language

### Exemplo de Requisição

```json
{
  "jsonrpc": "2.0",
  "id": 6,
  "method": "tools/call",
  "params": {
    "name": "get_patterns",
    "arguments": {}
  }
}
```

### Exemplo de Resposta

```json
{
  "jsonrpc": "2.0",
  "id": 6,
  "result": {
    "content": [
      {
        "type": "text",
        "text": "Retrieved patterns from last execution"
      }
    ],
    "structuredContent": {
      "handle": 1,
      "patterns": {
        "header": {
          "magic": "0x464C457F",
          "version": 1,
          "file_size": 1048576
        }
      }
    }
  }
}
```

### Estrutura de Patterns

Os patterns retornados refletem a estrutura definida no código Pattern Language:

**Código**:
```cpp
struct RGB {
    u8 r;
    u8 g;
    u8 b;
};

RGB color @ 0x00;
```

**Patterns Retornados**:
```json
{
  "color": {
    "r": 255,
    "g": 128,
    "b": 64
  }
}
```

### Notas

- Só funciona após `execute_pattern_code` bem-sucedido
- Patterns são formatados como JSON estruturado
- Arrays e structs aninhados são preservados

---

## 7. get_pattern_console_content

**Título**: Get Pattern Console Content

**Descrição**: Lê a saída do console gerada pela Pattern Language. Você pode ler esta saída após um pattern ter sido executado previamente.

### Input Schema

```json
{}
```

**Parâmetros**: Nenhum

### Output Schema

```json
{
  "handle": "number",
  "content": "string"
}
```

**Retorno**:
- `handle` (number): Handle do data source
- `content` (string): Saída do console gerada pela Pattern Language

### Exemplo de Requisição

```json
{
  "jsonrpc": "2.0",
  "id": 7,
  "method": "tools/call",
  "params": {
    "name": "get_pattern_console_content",
    "arguments": {}
  }
}
```

### Exemplo de Resposta (Sucesso)

```json
{
  "jsonrpc": "2.0",
  "id": 7,
  "result": {
    "content": [
      {
        "type": "text",
        "text": "Console output retrieved"
      }
    ],
    "structuredContent": {
      "handle": 1,
      "content": "Pattern executed successfully\nFound 42 entries\n"
    }
  }
}
```

### Exemplo de Resposta (Erro no Pattern)

```json
{
  "jsonrpc": "2.0",
  "id": 7,
  "result": {
    "content": [
      {
        "type": "text",
        "text": "Console output retrieved"
      }
    ],
    "structuredContent": {
      "handle": 1,
      "content": "Error on line 5: Undefined variable 'magic'\n"
    }
  }
}
```

### Uso com std::print() no Pattern Language

```cpp
struct Header {
    u32 magic;
    u32 version;
};

Header header @ 0x00;

std::print("Magic: 0x{:08X}", header.magic);
std::print("Version: {}", header.version);
```

Console output:
```
Magic: 0x464C457F
Version: 1
```

### Casos de Uso

- Debug de Pattern Language scripts
- Verificar mensagens de erro de execução
- Obter logs gerados por `std::print()` no código

---

## Fluxo de Trabalho Típico

### 1. Análise Básica de Arquivo

```
1. open_file("/path/to/file.bin")
   → Obter handle (ex: 1)

2. read_data(address=0, size=1024)
   → Analisar primeiros 1024 bytes

3. list_open_data_sources()
   → Confirmar arquivo está aberto
```

### 2. Análise com Pattern Language

```
1. open_file("/path/to/firmware.bin")
   → handle = 1

2. execute_pattern_code(source_code="struct Header { ... }")
   → result_code = 0 (sucesso)

3. get_patterns()
   → Obter estruturas parseadas

4. get_pattern_console_content()
   → Ver logs de execução
```

### 3. Múltiplos Arquivos

```
1. open_file("/path/to/file1.bin")
   → handle = 1

2. open_file("/path/to/file2.bin")
   → handle = 2

3. select_data_source(handle=1)
   → Trabalhar com file1.bin

4. read_data(address=0, size=256)
   → Ler de file1.bin

5. select_data_source(handle=2)
   → Mudar para file2.bin

6. read_data(address=0, size=256)
   → Ler de file2.bin
```

---

## Referências

- **ImHex Documentation**: https://docs.werwolv.net/imhex/
- **Pattern Language Guide**: https://docs.werwolv.net/pattern-language/
- **MCP Specification**: https://spec.modelcontextprotocol.io/
- **JSON-RPC 2.0**: https://www.jsonrpc.org/specification

---

## Suporte

Para questões sobre as ferramentas MCP do ImHex:
- GitHub Issues: https://github.com/WerWolv/ImHex/issues
- Discord: https://discord.gg/X63jZ36xBY
