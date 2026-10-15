# Etapa 8 – Executável sem instalação, tudo em memória, e versionamento no GitHub

**Arquivos:** `Makefile`, `compilar.sh`, `compilar.bat`, `.github/workflows/build.yml`, `.gitignore`

## Objetivo

1. Gerar **um único arquivo executável** que roda sem nenhuma instalação.
2. Garantir que **toda a execução aconteça em memória**.
3. Versionar o projeto no **GitHub** e publicar executáveis prontos para download.

## Parte A – Por que tudo roda em memória

| Recurso | Onde fica | Disco? |
|---|---|---|
| Código-fonte dos convidados | strings `const char[]` dentro do executável (`programas.c`) | não |
| Montagem | vetor `static int imagem[]` em `vmm.c` | não |
| "RAM física" das VMs | vetor `static int ram[]` em `memoria.c` | não |
| Tabelas de páginas, vCPUs e consoles | vetor `vms[]` em `vmm.c` | não |
| Saída | `printf` no terminal | não |

O programa **não abre nenhum arquivo** (não há `fopen` em `src/`), não cria arquivos temporários, não usa variáveis de ambiente nem arquivos de configuração. Também **não aloca memória dinâmica**: todos os vetores têm tamanho fixo, definido em `config.h`. O consumo total de memória é conhecido em tempo de compilação: menos de 30 KB de dados.

Para conferir:

```bash
grep -n "fopen\|malloc\|fwrite" src/*.c     # nenhum resultado
```

## Parte B – Compilando

### Com `make` (Linux, macOS, WSL)

```bash
make            # build/hypervisor
make run        # compila e executa a demonstração
make test       # testes unitários + integração
make debug      # com AddressSanitizer: detecta acessos inválidos de memória
make static     # Linux: binário estático
make clean
```

### Sem `make`

Um único comando compila tudo, porque o projeto não tem dependências:

```bash
cc -std=c99 -O2 -o hypervisor src/*.c
```

Também há scripts prontos:

```bash
sh compilar.sh          # Linux/macOS (no Linux tenta gerar binário estático)
```

```bat
compilar.bat            :: Windows com MinGW-w64 (gcc no PATH)
```

### Flags utilizadas

| Flag | Por quê |
|---|---|
| `-std=c99` | padrão C99: inicializadores designados, `snprintf`, `long long` |
| `-Wall -Wextra -pedantic` | todos os avisos; o projeto compila **sem nenhum aviso** |
| `-O2` | otimização |
| `-static` | inclui a biblioteca C dentro do executável |

## Parte C – Executável sem instalação

**O que significa "sem instalação"?** O executável é um único arquivo, que funciona se for copiado para outra máquina do **mesmo sistema operacional e arquitetura** e executado diretamente, sem instalador, sem pacotes e sem DLLs ou `.so` adicionais.

| Sistema | Como gerar | Dependências em tempo de execução |
|---|---|---|
| **Linux** | `make static` ou `sh compilar.sh` | **nenhuma** (binário estático) |
| **Windows** | `compilar.bat` (`-static`) | **nenhuma** além do próprio Windows |
| **macOS** | `make` | só a `libSystem`, que todo macOS tem (a Apple não permite binários 100% estáticos) |

Para conferir no Linux:

```bash
make static
file build/hypervisor        # "statically linked"
ldd build/hypervisor         # "not a dynamic executable"
```

Para usar em outra máquina, basta copiar o arquivo:

```bash
scp build/hypervisor aluno@laboratorio:/tmp/
ssh aluno@laboratorio /tmp/hypervisor
```

## Parte D – Versionando no GitHub

### 1. Criar o repositório local

```bash
cd hypervisor-didatico
git init -b main
git add .
git commit -m "feat: hypervisor didatico com vcpu, paginacao e escalonador"
```

O `.gitignore` já exclui a pasta `build/` e os executáveis. **Binários não são versionados**: o GitHub Actions gera os executáveis a partir do código.

### 2. Publicar no GitHub

Pelo site: *New repository* → nome `hypervisor-didatico` → **sem** README (o projeto já tem um). Depois:

```bash
git remote add origin https://github.com/SEU-USUARIO/hypervisor-didatico.git
git push -u origin main
```

Ou, com o GitHub CLI:

```bash
gh repo create hypervisor-didatico --public --source=. --push
```

### 3. Integração contínua (`.github/workflows/build.yml`)

A cada `push`, o GitHub Actions executa em **Linux, macOS e Windows**:

1. compila com `gcc` (estático no Linux e no Windows);
2. roda os testes unitários;
3. roda os testes de integração;
4. guarda o executável como *artifact* (aba **Actions** → execução → *Artifacts*).

### 4. Publicar uma versão com executáveis para download

```bash
git tag v1.0
git push origin v1.0
```

A tag dispara o job `release`, que cria uma **Release** no GitHub com três arquivos:

```
hypervisor-linux-x64
hypervisor-macos-arm64
hypervisor-windows-x64.exe
```

Os alunos baixam o arquivo do seu sistema e executam, **sem compilador e sem instalação**:

```bash
chmod +x hypervisor-linux-x64      # Linux/macOS: dar permissão de execução
./hypervisor-linux-x64
```

> No macOS, o Gatekeeper pode bloquear um binário baixado da internet. Libere com `xattr -d com.apple.quarantine hypervisor-macos-arm64` ou compile localmente com `make`.

### 5. Sugestão de fluxo para a turma

- O professor publica o repositório, com tag `v1.0`, como **template**.
- Cada grupo cria o seu a partir do template (*Use this template*).
- Cada exercício da [Etapa 9](09-testes-e-exercicios.md) vira uma **branch** e um **Pull Request**. O CI mostra automaticamente se os testes continuam passando.

⬅️ [Etapa 7](07-etapa-vmm.md) · ➡️ [Etapa 9 – Testes, exercícios e hypervisors reais](09-testes-e-exercicios.md)
