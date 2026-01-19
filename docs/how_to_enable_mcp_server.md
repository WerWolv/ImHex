# Como Habilitar o MCP Server no ImHex

Este guia mostra como ativar o servidor MCP do ImHex para permitir conexões de clientes externos (como o cliente Python).

## Pré-requisitos

- ImHex instalado e funcionando
- Versão do ImHex com suporte a MCP (verifique se possui a opção nas configurações)

## Visão Geral

O ImHex possui um servidor MCP integrado que escuta na porta **19743** quando habilitado. Este servidor permite que aplicações externas (como este cliente Python) se conectem e controlem o ImHex programaticamente.

## Método 1: Interface Gráfica (GUI)

### Passo a Passo

1. **Abra o ImHex**
   - Inicie o aplicativo ImHex normalmente

2. **Acesse as Configurações**
   - Clique em **Edit** > **Settings** (ou use `Ctrl+,` / `Cmd+,`)
   - Ou vá no menu superior: **Extras** > **Settings**

3. **Navegue para General Settings**
   - No painel esquerdo, clique em **General**

4. **Encontre a Seção Network**
   - Role até encontrar a seção **Network**
   - Você verá várias opções relacionadas a rede

5. **Habilite MCP Server**
   - Marque a checkbox **"MCP Server"**
   - Descrição da opção:
     > "When enabled, AI clients can interact with ImHex while it's running. After enabling, simply add 'imhex --mcp' as an stdio MCP server to your AI client."

6. **Aplique as Mudanças**
   - As configurações são salvas automaticamente
   - O servidor MCP começará a rodar imediatamente

### Verificação Visual

Após habilitar, você verá um indicador no rodapé (footer) do ImHex:

- **🔴 Vermelho**: MCP Server habilitado, mas sem conexões
  - Texto: "MCP Server enabled but no AI Client is connected"

- **🟢 Verde**: MCP Server habilitado com cliente conectado
  - Texto: "Connected to '{nome_do_cliente}'"
  - Mostra versão do cliente e protocolo ao passar o mouse

## Método 2: Linha de Comando

Você também pode iniciar o ImHex diretamente com MCP habilitado:

### Windows
```cmd
imhex.exe --mcp
```

### Linux/macOS
```bash
./imhex --mcp
```

### Como Funciona

O flag `--mcp` inicia o ImHex em modo MCP stdio, que:
- Lê requisições JSON-RPC 2.0 de stdin
- Escreve respostas para stdout
- Permite integração com AI clients (ex: Claude Desktop)

**Nota**: Este modo é diferente do servidor TCP (porta 19743). O modo `--mcp` é para stdio, enquanto o servidor TCP é habilitado via GUI.

## Método 3: Arquivo de Configuração

As configurações do ImHex são salvas em arquivos JSON. Você pode editar diretamente:

### Localização do Arquivo de Configuração

**Windows:**
```
%APPDATA%\imhex\config\settings.json
```

**Linux:**
```
~/.config/imhex/settings.json
```

**macOS:**
```
~/Library/Application Support/imhex/settings.json
```

### Editar Configuração

1. Feche o ImHex (importante!)

2. Abra o arquivo `settings.json` em um editor de texto

3. Procure pela seção `hex.builtin.setting.general`:

```json
{
  "hex.builtin.setting.general": {
    "hex.builtin.setting.general.network_interface": false,
    "hex.builtin.setting.general.mcp_server": false,
    ...
  }
}
```

4. Altere `mcp_server` para `true`:

```json
{
  "hex.builtin.setting.general": {
    "hex.builtin.setting.general.network_interface": false,
    "hex.builtin.setting.general.mcp_server": true,
    ...
  }
}
```

5. Salve o arquivo

6. Inicie o ImHex - o MCP Server estará habilitado

## Testando a Conexão

Após habilitar o MCP Server, teste a conexão:

### Teste Rápido com Python

```python
from imhex_mcp_client import ImHexMCPClient

client = ImHexMCPClient()
try:
    client.connect()
    print("✓ Conectado com sucesso!")
    client.disconnect()
except ConnectionRefusedError:
    print("✗ Conexão recusada - MCP Server não está habilitado")
```

### Teste Completo

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
  ...
============================================================
✓ Todos os testes passaram!
============================================================
```

## Solução de Problemas

### Problema: Checkbox não aparece

**Possível causa**: Versão antiga do ImHex

**Solução**:
- Atualize para a versão mais recente do ImHex
- Verifique se você tem a versão com suporte MCP (build recente)

### Problema: Conexão recusada (127.0.0.1:19743)

**Possível causa 1**: MCP Server não está habilitado

**Solução**:
1. Abra Settings > General
2. Verifique se "MCP Server" está marcado
3. Se não estiver, marque e aguarde alguns segundos

**Possível causa 2**: Firewall bloqueando porta 19743

**Solução**:
- Windows: Adicione exceção no Windows Firewall
- Linux: Verifique `iptables` ou `ufw`
- macOS: Verifique Firewall nas System Preferences

**Possível causa 3**: Outra aplicação usando porta 19743

**Solução**:
```bash
# Windows
netstat -ano | findstr :19743

# Linux/macOS
lsof -i :19743
```

Se outra aplicação estiver usando, termine-a ou configure-a para usar outra porta.

### Problema: Servidor desconecta aleatoriamente

**Possível causa**: Timeout de inatividade

**Solução**:
- O servidor MCP mantém conexão persistente
- Implemente keep-alive no cliente se necessário
- Aumente timeout do socket:
  ```python
  client = ImHexMCPClient(timeout=60.0)
  ```

### Problema: Não vejo indicador no rodapé

**Possível causa**: Layout customizado

**Solução**:
- Resetar layout: **View** > **Reset Layout**
- Ou verificar se o footer está visível: **View** > **Footer**

## Verificação de Status

### Indicadores Visuais

**Rodapé do ImHex:**
- Canto inferior direito mostra status do MCP
- Ícone de rede indica se há conexão

**Tooltip (passar mouse):**
- Nome do cliente conectado
- Versão do cliente
- Versão do protocolo

### Via Logs

O ImHex registra atividade MCP nos logs:

**Windows:**
```
%APPDATA%\imhex\logs\
```

**Linux/macOS:**
```
~/.config/imhex/logs/
```

Procure por linhas contendo "MCP" para debug.

## Segurança

### Considerações de Segurança

- **Localhost Only**: O servidor MCP só aceita conexões de 127.0.0.1
- **Sem Autenticação**: Não há autenticação - qualquer processo local pode conectar
- **Privilégios**: O MCP Server tem acesso total às funcionalidades do ImHex

### Boas Práticas

1. **Habilite apenas quando necessário**
   - Desligue quando não estiver usando clientes AI/Python

2. **Monitore conexões**
   - Verifique o indicador no rodapé
   - Se ver conexão não esperada, desabilite o servidor

3. **Firewall**
   - Configure firewall para bloquear porta 19743 de acesso externo
   - Permita apenas conexões de localhost

## Referências

- **Código Fonte**: `plugins/builtin/source/content/settings_entries.cpp:765`
- **Background Service**: `plugins/builtin/source/content/background_services.cpp:121`
- **Servidor MCP**: `lib/libimhex/source/mcp/server.cpp`
- **Porta**: 19743 (hardcoded)
- **Protocolo**: JSON-RPC 2.0
- **Versão**: 2025-06-18

## Próximos Passos

Após habilitar o MCP Server:

1. ✅ Execute o teste de conexão: `python tests/mcp_connection_test.py`
2. 📖 Leia a documentação: `docs/mcp_tools_reference.md`
3. 🚀 Experimente os exemplos: `python examples/basic_usage.py /path/to/file.bin`
4. 💡 Explore as ferramentas disponíveis: `MCP_CONNECTOR_README.md`

---

**Dica**: Para integração com Claude Desktop ou outros AI clients, você precisa usar o modo stdio (`imhex --mcp`), não o servidor TCP. Este cliente Python usa o servidor TCP na porta 19743.
