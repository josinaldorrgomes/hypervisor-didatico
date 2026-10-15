# Etapa 1 – A máquina virtual: ISA e registradores

**Arquivos:** `src/config.h`, `src/isa.h`, `src/isa.c`

## Objetivo

Antes de virtualizar uma máquina, é preciso definir **qual máquina** será virtualizada. Nesta etapa definimos a **ISA** (*Instruction Set Architecture*): os registradores, as instruções, a forma como elas ficam na memória e, principalmente, **quais delas são sensíveis**.

## Conceitos da aula aplicados

- **Arquitetura de computadores:** registradores, PC, SP, flags e o ciclo de instrução.
- **Instruções privilegiadas e sensíveis:** o critério de Popek & Goldberg diz que uma arquitetura é virtualizável quando **toda instrução sensível é também privilegiada**, ou seja, causa um *trap* quando executada fora do modo privilegiado.
- **Modo dual:** o convidado roda "desprivilegiado" e o hypervisor tem o controle.

## Passo 1 – Constantes globais (`config.h`)

Todos os tamanhos ficam num único lugar. Assim os alunos podem experimentar mudando um número e recompilando.

```c
#define TAM_PAGINA          64   /* palavras por página (e por quadro)  */
#define NUM_QUADROS         64   /* quadros na RAM do hospedeiro         */
#define MAX_PAGINAS_VM       8   /* tamanho máximo do espaço de uma VM   */
#define TAM_PILHA           64   /* palavras reservadas para a pilha     */
#define NUM_REGISTRADORES    8   /* R0 .. R7                             */
#define TAM_INSTRUCAO        3   /* op, A, B                             */
#define MAX_VMS             10
#define QUANTUM_PADRAO      25   /* instruções por fatia de tempo        */
#define LIMITE_PADRAO   100000   /* watchdog                             */
```

> **Decisão de projeto:** a unidade de memória é a **palavra** (`int`), e não o byte. Isso evita conversões, *endianness* e alinhamento, e cada posição da memória guarda um número inteiro.

## Passo 2 – O estado da CPU

A máquina tem:

| Registrador | Uso |
|---|---|
| `R0` a `R7` | uso geral (`R0` também passa argumentos para as hypercalls) |
| `pc` | endereço da próxima instrução |
| `sp` | topo da pilha (a pilha cresce para baixo) |
| `flag` | resultado do último `CMP`: `-1` (menor), `0` (igual) ou `1` (maior) |

Esse estado é declarado como struct na [Etapa 3](03-etapa-vcpu.md).

## Passo 3 – Formato das instruções

**Toda instrução ocupa 3 palavras.** Esse tamanho fixo deixa a busca (*fetch*) trivial:

```
endereço:   pc        pc+1          pc+2
conteúdo: [opcode] [operando A] [operando B]
```

Exemplo: `ADDI R0, 5` é guardada como `[8, 0, 5]`, porque `OP_ADDI` vale 8.

## Passo 4 – Os opcodes (`isa.h`)

Um `enum` numera as instruções automaticamente:

```c
enum Opcode {
    OP_NOP, OP_LOADI, OP_LOAD, OP_STORE, OP_LOADR, OP_STORER, OP_MOV,
    OP_ADD, OP_ADDI, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
    OP_CMP, OP_CMPI, OP_JMP, OP_JEQ, OP_JNE, OP_JLT, OP_JGT,
    OP_PUSH, OP_POP, OP_CALL, OP_RET,
    /* sensíveis: sempre causam VM exit */
    OP_IN, OP_OUT, OP_HLT, OP_HCALL,
    NUM_OPCODES
};
```

O último item, `NUM_OPCODES`, vale exatamente a quantidade de instruções. É um truque comum em C para declarar vetores do tamanho certo.

### Instruções comuns × sensíveis

| Grupo | Instruções | Quem executa |
|---|---|---|
| Comuns | aritmética, memória, desvios, pilha | a **vCPU**, direto e sem intervenção |
| **Sensíveis** | `IN`, `OUT`, `HLT`, `HCALL` | **o hypervisor**: a vCPU para e gera uma *VM exit* |

Por que essas quatro são sensíveis?

- `IN` e `OUT` acessam **dispositivos**. O convidado não pode tocar no terminal real, e quem decide o que acontece é o hypervisor.
- `HLT` **desliga a CPU**. Se o convidado desligasse a CPU real, todas as VMs parariam.
- `HCALL` é um **pedido explícito** ao hypervisor, o equivalente a uma *syscall*.

Nossa ISA obedece ao critério de Popek & Goldberg: **toda instrução sensível causa trap**. A x86 original **não** obedecia, porque algumas instruções sensíveis (como `POPF`) falhavam em silêncio. Por isso a VMware precisou de tradução binária até a Intel criar o VT-x.

## Passo 5 – A tabela de instruções (`isa.c`)

Em vez de espalhar `if`s pelo código, descrevemos cada instrução numa **tabela**:

```c
struct InfoInstrucao {
    const char *nome;   /* "ADDI"                              */
    int tipo_a;         /* OPERANDO_NENHUM, _REG ou _VALOR      */
    int tipo_b;
    int sensivel;       /* 1 = provoca VM exit                  */
};

const struct InfoInstrucao TABELA_INSTRUCOES[NUM_OPCODES] = {
    [OP_NOP]   = { "NOP",   OPERANDO_NENHUM, OPERANDO_NENHUM, 0 },
    [OP_LOADI] = { "LOADI", OPERANDO_REG,    OPERANDO_VALOR,  0 },
    ...
    [OP_OUT]   = { "OUT",   OPERANDO_REG,    OPERANDO_VALOR,  1 },
};
```

A sintaxe `[OP_LOADI] = {...}` (inicializador designado do C99) amarra cada linha ao seu opcode, mesmo que a ordem do `enum` mude.

Essa **mesma tabela** é usada em três lugares:

1. no **montador**, para converter `"ADDI"` em `OP_ADDI` (`isa_buscar_opcode`) e saber quantos operandos esperar;
2. na **vCPU**, para validar se os operandos do tipo registrador estão entre 0 e 7;
3. no **rastreamento** (`-t`), para converter números em texto (`isa_desmontar`).

## Passo 6 – Portas de E/S e hypercalls

Também em `isa.h` ficam os "endereços" dos dispositivos e os números das hypercalls:

```c
#define PORTA_CONSOLE_NUMERO  1   /* OUT: escreve um inteiro      */
#define PORTA_CONSOLE_CHAR    2   /* OUT: escreve um caractere    */
#define PORTA_ID_VM           3   /* IN : número desta VM         */
#define PORTA_RELOGIO         4   /* IN : relógio do hypervisor   */

#define HC_IMPRIMIR_TEXTO     0   /* R0 = endereço da string      */
#define HC_CEDER_CPU          1
#define HC_DORMIR             2   /* R0 = ticks                   */
```

A lista completa está na [referência rápida da ISA](referencia-isa.md).

## Ponto de verificação

O módulo `isa.c` não depende de nenhum outro e pode ser compilado sozinho:

```bash
cc -std=c99 -Wall -Wextra -c src/isa.c -o /tmp/isa.o
```

Um teste rápido (arquivo temporário `teste_isa.c` na raiz do projeto):

```c
#include <stdio.h>
#include "src/isa.h"

int main(void)
{
    char texto[48];
    isa_desmontar(OP_ADDI, 0, 5, texto, sizeof texto);
    printf("%s\n", texto);                         /* ADDI   R0, 5 */
    printf("%d\n", isa_buscar_opcode("HLT"));      /* 26          */
    return 0;
}
```

```bash
cc -std=c99 teste_isa.c src/isa.c -o /tmp/teste_isa && /tmp/teste_isa
```

## Para discutir em aula

1. O que aconteceria se `OUT` **não** fosse sensível e a vCPU escrevesse direto no `stdout`?
2. Por que `HLT` não pode simplesmente encerrar o processo do hypervisor?
3. Que outra instrução você classificaria como sensível numa CPU real? (Dica: alterar a tabela de páginas, desabilitar interrupções.)

⬅️ [Etapa 0](00-visao-geral.md) · ➡️ [Etapa 2 – Memória virtual com paginação](02-etapa-memoria.md)
