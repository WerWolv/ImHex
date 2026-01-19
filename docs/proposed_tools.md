

# Ferramentas MCP Propostas para Expansão

Este documento descreve ferramentas adicionais que podem ser implementadas no plugin opcional do ImHex para expandir as capacidades do MCP connector.

## Categorias de Ferramentas

1. [Análise de Dados](#categoria-análise-de-dados)
2. [Manipulação de Dados](#categoria-manipulação-de-dados)
3. [Visualização](#categoria-visualização)
4. [Gerenciamento de Projeto](#categoria-gerenciamento-de-projeto)
5. [Hashing e Criptografia](#categoria-hashing-e-criptografia)
6. [Disassembly e Análise de Código](#categoria-disassembly-e-análise-de-código)

---

## Categoria: Análise de Dados

### get_entropy

**Descrição**: Calcula a entropia de Shannon de uma região de bytes para identificar dados comprimidos, encriptados ou aleatórios.

**API Base**: `ContentRegistry::DataVisualizers`

**Input Schema**:
```json
{
  "address": "number",
  "size": "number",
  "block_size": "number (opcional, padrão: 256)"
}
```

**Output Schema**:
```json
{
  "handle": "number",
  "entropy": "number (0.0 - 8.0)",
  "block_entropies": "array<number> (opcional)",
  "visualization_data": "array<{offset: number, entropy: number}>"
}
```

**Casos de Uso**:
- Identificar seções criptografadas em firmware
- Detectar compressão de dados
- Encontrar regiões de código vs dados

**Complexidade**: Média

---

### find_patterns

**Descrição**: Busca sequências de bytes ou padrões regex em todo o data source ou região específica.

**API Base**: Provider read APIs + custom search logic

**Input Schema**:
```json
{
  "pattern": "string (hex ou regex)",
  "pattern_type": "string (hex | regex | string)",
  "start_offset": "number (opcional)",
  "end_offset": "number (opcional)",
  "max_results": "number (opcional, padrão: 100)"
}
```

**Output Schema**:
```json
{
  "handle": "number",
  "matches": "array<{offset: number, length: number, context_before: string, context_after: string}>",
  "total_matches": "number"
}
```

**Casos de Uso**:
- Buscar magic numbers em arquivo
- Encontrar strings específicas em binários
- Localizar padrões de vulnerabilidades

**Complexidade**: Alta

---

### get_data_information

**Descrição**: Retorna informações contextuais sobre uma região de dados (tipo detectado, possível formato, etc.).

**API Base**: `ContentRegistry::DataInformation`

**Input Schema**:
```json
{
  "address": "number",
  "size": "number"
}
```

**Output Schema**:
```json
{
  "handle": "number",
  "detected_type": "string",
  "mime_type": "string (opcional)",
  "magic_bytes": "string",
  "confidence": "number (0.0 - 1.0)",
  "suggestions": "array<string>"
}
```

**Casos de Uso**:
- Identificar tipo de arquivo embedded
- Detectar estruturas conhecidas
- Sugerir patterns apropriados

**Complexidade**: Média

---

### calculate_checksum

**Descrição**: Calcula checksums comuns (CRC32, Adler32, Fletcher) para validação de integridade.

**API Base**: Custom implementation

**Input Schema**:
```json
{
  "address": "number",
  "size": "number",
  "algorithm": "string (crc32 | adler32 | fletcher16 | fletcher32)"
}
```

**Output Schema**:
```json
{
  "handle": "number",
  "checksum": "string (hex)",
  "algorithm": "string"
}
```

**Casos de Uso**:
- Verificar integridade de blocos de dados
- Validar firmware headers
- Comparar checksums

**Complexidade**: Baixa

---

## Categoria: Manipulação de Dados

### write_data

**Descrição**: Escreve bytes em offset específico do data source (se for writable).

**API Base**: `Provider::writeRaw()`

**Input Schema**:
```json
{
  "address": "number",
  "data": "string (base64)"
}
```

**Output Schema**:
```json
{
  "handle": "number",
  "bytes_written": "number",
  "success": "boolean"
}
```

**Casos de Uso**:
- Patchear binários
- Modificar valores em estruturas
- Aplicar fixes automatizados

**Complexidade**: Baixa

**Segurança**: ⚠️ Requer confirmação do usuário antes de escrita

---

### resize_provider

**Descrição**: Redimensiona o data source atual (aumenta ou diminui tamanho).

**API Base**: `Provider::resize()`

**Input Schema**:
```json
{
  "new_size": "number"
}
```

**Output Schema**:
```json
{
  "handle": "number",
  "old_size": "number",
  "new_size": "number",
  "success": "boolean"
}
```

**Casos de Uso**:
- Truncar arquivos
- Estender arquivo para adicionar dados
- Preparar buffer para escrita

**Complexidade**: Baixa

**Segurança**: ⚠️ Operação destrutiva, requer confirmação

---

### apply_patch

**Descrição**: Aplica patch IPS/IPS32/UPS em data source.

**API Base**: ImHex patch system

**Input Schema**:
```json
{
  "patch_file": "string (path)",
  "patch_format": "string (ips | ips32 | ups | auto)"
}
```

**Output Schema**:
```json
{
  "handle": "number",
  "patches_applied": "number",
  "success": "boolean",
  "errors": "array<string> (opcional)"
}
```

**Casos de Uso**:
- Aplicar patches de ROM hacking
- Atualizar firmware com patches
- Reverter modificações

**Complexidade**: Média

---

### copy_region

**Descrição**: Copia região de bytes de um offset para outro.

**API Base**: Provider read/write APIs

**Input Schema**:
```json
{
  "source_address": "number",
  "dest_address": "number",
  "size": "number"
}
```

**Output Schema**:
```json
{
  "handle": "number",
  "bytes_copied": "number",
  "success": "boolean"
}
```

**Casos de Uso**:
- Duplicar estruturas
- Reorganizar dados
- Backup de regiões antes de modificação

**Complexidade**: Baixa

---

## Categoria: Visualização

### get_hex_dump

**Descrição**: Retorna dump hexadecimal formatado de uma região.

**API Base**: Custom formatting

**Input Schema**:
```json
{
  "address": "number",
  "size": "number",
  "columns": "number (opcional, padrão: 16)",
  "show_ascii": "boolean (opcional, padrão: true)",
  "show_offset": "boolean (opcional, padrão: true)",
  "uppercase_hex": "boolean (opcional, padrão: false)"
}
```

**Output Schema**:
```json
{
  "handle": "number",
  "formatted_text": "string",
  "lines": "array<{offset: number, hex: string, ascii: string}>"
}
```

**Casos de Uso**:
- Gerar relatórios de análise
- Debug visual de dados
- Comparação de regiões

**Complexidade**: Baixa

---

### get_disassembly

**Descrição**: Disassembla código de máquina para assembly.

**API Base**: `ContentRegistry::Disassemblers` (Capstone integration)

**Input Schema**:
```json
{
  "address": "number",
  "size": "number",
  "architecture": "string (x86_64 | arm | arm64 | mips | ...)",
  "mode": "string (opcional: 16 | 32 | 64 | thumb | ...)"
}
```

**Output Schema**:
```json
{
  "handle": "number",
  "instructions": "array<{offset: number, bytes: string, mnemonic: string, operands: string}>",
  "formatted_text": "string"
}
```

**Casos de Uso**:
- Análise de firmware
- Reverse engineering de código
- Identificação de funções

**Complexidade**: Alta

---

### render_3d_model

**Descrição**: Renderiza modelo 3D de Pattern Language visualizer (ex: STL files).

**API Base**: Pattern Language 3D visualizers

**Input Schema**:
```json
{
  "format": "string (opcional: depende do pattern executado)"
}
```

**Output Schema**:
```json
{
  "handle": "number",
  "model_data": "string (base64 ou URL)",
  "format": "string (stl | obj | ...)",
  "vertices_count": "number"
}
```

**Casos de Uso**:
- Visualizar modelos 3D embedded em dados
- Análise de arquivos CAD
- Verificação de geometria

**Complexidade**: Alta

---

### get_data_graph

**Descrição**: Gera grafo de relacionamentos entre estruturas de dados baseado em patterns.

**API Base**: Pattern Language runtime

**Input Schema**:
```json
{
  "format": "string (dot | json | mermaid)"
}
```

**Output Schema**:
```json
{
  "handle": "number",
  "graph_data": "string",
  "format": "string",
  "nodes_count": "number",
  "edges_count": "number"
}
```

**Casos de Uso**:
- Visualizar estruturas complexas
- Entender relações de ponteiros
- Documentar formato de arquivo

**Complexidade**: Alta

---

## Categoria: Gerenciamento de Projeto

### save_project

**Descrição**: Salva o projeto atual do ImHex (providers, patterns, bookmarks, etc.).

**API Base**: `ProjectFileManager`

**Input Schema**:
```json
{
  "project_path": "string"
}
```

**Output Schema**:
```json
{
  "success": "boolean",
  "project_path": "string",
  "providers_saved": "number",
  "bookmarks_saved": "number"
}
```

**Casos de Uso**:
- Salvar estado de análise
- Criar checkpoints
- Compartilhar análise com equipe

**Complexidade**: Média

---

### load_project

**Descrição**: Carrega um projeto salvo do ImHex.

**API Base**: `ProjectFileManager`

**Input Schema**:
```json
{
  "project_path": "string"
}
```

**Output Schema**:
```json
{
  "success": "boolean",
  "providers_loaded": "number",
  "bookmarks_loaded": "number",
  "patterns_loaded": "number"
}
```

**Casos de Uso**:
- Restaurar sessão de análise
- Carregar configurações salvas
- Trabalho colaborativo

**Complexidade**: Média

---

### export_bookmarks

**Descrição**: Exporta bookmarks/anotações do projeto.

**API Base**: `ImHexApi::Bookmarks`

**Input Schema**:
```json
{
  "format": "string (json | csv | markdown)"
}
```

**Output Schema**:
```json
{
  "handle": "number",
  "bookmarks": "array<{address: number, size: number, name: string, color: string, comment: string}>",
  "formatted_data": "string",
  "count": "number"
}
```

**Casos de Uso**:
- Documentar regiões importantes
- Exportar anotações para relatórios
- Compartilhar descobertas

**Complexidade**: Baixa

---

### create_bookmark

**Descrição**: Cria um bookmark em região específica.

**API Base**: `ImHexApi::Bookmarks::add()`

**Input Schema**:
```json
{
  "address": "number",
  "size": "number",
  "name": "string",
  "comment": "string (opcional)",
  "color": "string (hex color, opcional)"
}
```

**Output Schema**:
```json
{
  "success": "boolean",
  "bookmark_id": "number"
}
```

**Casos de Uso**:
- Marcar regiões de interesse automaticamente
- Anotar descobertas durante análise
- Criar índice de estruturas

**Complexidade**: Baixa

---

## Categoria: Hashing e Criptografia

### calculate_hash

**Descrição**: Calcula hashes criptográficos de uma região (MD5, SHA1, SHA256, etc.).

**API Base**: `ContentRegistry::Hashes`

**Input Schema**:
```json
{
  "address": "number",
  "size": "number",
  "algorithm": "string (md5 | sha1 | sha256 | sha512 | blake2b | ...)"
}
```

**Output Schema**:
```json
{
  "handle": "number",
  "hash": "string (hex)",
  "algorithm": "string"
}
```

**Casos de Uso**:
- Verificar integridade de firmware
- Identificar arquivos conhecidos
- Comparar versões de binários

**Complexidade**: Baixa

---

### hash_file_regions

**Descrição**: Calcula hashes de múltiplas regiões em uma operação.

**API Base**: `ContentRegistry::Hashes`

**Input Schema**:
```json
{
  "regions": "array<{address: number, size: number}>",
  "algorithm": "string"
}
```

**Output Schema**:
```json
{
  "handle": "number",
  "hashes": "array<{address: number, size: number, hash: string}>",
  "algorithm": "string"
}
```

**Casos de Uso**:
- Hashear seções de ELF/PE
- Criar índice de blocos
- Verificação de integridade parcial

**Complexidade**: Média

---

### decrypt_region

**Descrição**: Tenta decriptar região usando algoritmos comuns (XOR, AES, etc.).

**API Base**: Custom implementation

**Input Schema**:
```json
{
  "address": "number",
  "size": "number",
  "algorithm": "string (xor | aes128 | aes256 | ...)",
  "key": "string (base64)",
  "iv": "string (base64, opcional)"
}
```

**Output Schema**:
```json
{
  "handle": "number",
  "decrypted_data": "string (base64)",
  "success": "boolean"
}
```

**Casos de Uso**:
- Decriptar firmware com chaves conhecidas
- Testar algoritmos de criptografia
- Análise de malware

**Complexidade**: Alta

**Segurança**: ⚠️ Não armazena chaves permanentemente

---

## Categoria: Disassembly e Análise de Código

### find_functions

**Descrição**: Identifica funções em código binário usando análise de fluxo.

**API Base**: Capstone + custom analysis

**Input Schema**:
```json
{
  "address": "number",
  "size": "number",
  "architecture": "string"
}
```

**Output Schema**:
```json
{
  "handle": "number",
  "functions": "array<{address: number, size: number, name: string (opcional)}>",
  "count": "number"
}
```

**Casos de Uso**:
- Análise de firmware
- Mapeamento de funções em binários
- Reverse engineering

**Complexidade**: Muito Alta

---

### analyze_control_flow

**Descrição**: Analisa fluxo de controle (branches, calls, returns) em código.

**API Base**: Capstone + custom CFG analysis

**Input Schema**:
```json
{
  "address": "number",
  "architecture": "string"
}
```

**Output Schema**:
```json
{
  "handle": "number",
  "basic_blocks": "array<{address: number, size: number}>",
  "edges": "array<{from: number, to: number, type: string}>",
  "entry_point": "number"
}
```

**Casos de Uso**:
- Entender lógica de funções
- Identificar loops e condicionais
- Análise de vulnerabilidades

**Complexidade**: Muito Alta

---

## Priorização de Implementação

### Fase 1: Ferramentas Essenciais (Baixa Complexidade)
1. **get_hex_dump** - Visualização básica
2. **calculate_hash** - Hashing comum
3. **calculate_checksum** - Validação de integridade
4. **create_bookmark** - Anotações
5. **export_bookmarks** - Documentação

### Fase 2: Manipulação de Dados (Média Complexidade)
1. **write_data** - Edição básica
2. **copy_region** - Operações de dados
3. **save_project / load_project** - Persistência
4. **get_data_information** - Detecção de tipos

### Fase 3: Análise Avançada (Alta Complexidade)
1. **get_entropy** - Análise estatística
2. **find_patterns** - Busca avançada
3. **get_disassembly** - Disassembly
4. **apply_patch** - Patching
5. **hash_file_regions** - Hashing em lote

### Fase 4: Recursos Premium (Muito Alta Complexidade)
1. **find_functions** - Análise de código
2. **analyze_control_flow** - CFG analysis
3. **render_3d_model** - Visualização 3D
4. **get_data_graph** - Grafos de dados
5. **decrypt_region** - Criptografia

---

## Notas de Implementação

### Segurança
- Ferramentas de escrita (write_data, resize_provider, apply_patch) devem:
  - Validar permissões de escrita
  - Confirmar operações destrutivas
  - Manter backups automáticos
  - Logar todas as modificações

### Performance
- Ferramentas que processam grandes volumes (find_patterns, hash_file_regions):
  - Implementar processamento em chunks
  - Suportar cancelamento
  - Reportar progresso
  - Usar threading quando apropriado

### Compatibilidade
- Todas as ferramentas devem:
  - Validar handles de data sources
  - Verificar limites de endereços
  - Retornar erros descritivos
  - Seguir schema JSON consistente

---

## Referências de APIs ImHex

- **ContentRegistry**: `/lib/libimhex/include/hex/api/content_registry/`
- **Provider APIs**: `/lib/libimhex/include/hex/api/imhex_api/provider.hpp`
- **Hashes**: `/plugins/hashes/`
- **Disassemblers**: `/lib/libimhex/include/hex/api/content_registry/disassemblers.hpp`
- **Pattern Language**: `/lib/external/pattern_language/`
