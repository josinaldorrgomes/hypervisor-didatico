# Etapa 2 – Memória virtual com paginação

**Arquivos:** `src/memoria.h`, `src/memoria.c`

## Objetivo

Cada VM precisa acreditar que tem **uma memória própria começando no endereço 0**, mas todas compartilham **uma única RAM** do hospedeiro. Nesta etapa implementamos:

- a **RAM do hospedeiro**, dividida em **quadros**;
- uma **tabela de páginas por VM**, que traduz endereços do convidado em endereços reais;
- a **proteção**: uma VM nunca consegue acessar um quadro que não é dela;
- a **paginação sob demanda**: páginas que só recebem quadro quando são usadas.

## Conceitos da aula aplicados

- Paginação: página, quadro, deslocamento e tabela de páginas.
- Proteção de memória e isolamento entre processos.
- Falta de página (*page fault*) e paginação sob demanda.
- Fragmentação: com paginação, os quadros de uma VM **não precisam ser contíguos**.

> **Em hypervisors reais** existem **dois níveis** de tradução. O SO convidado traduz endereço virtual em "físico do convidado", e o hypervisor traduz "físico do convidado" em físico real, com a **EPT** (Intel) ou a **NPT** (AMD). Nosso convidado não tem SO próprio, então implementamos só o nível do hypervisor, que é exatamente o papel da EPT.

## Passo 1 – Estruturas de dados

```c
static int ram[TAM_RAM_HOSPEDEIRO];   /* 64 quadros × 64 palavras = 4096 */
static int dono_quadro[NUM_QUADROS];  /* 0 = livre; senão, id da VM      */

struct TabelaPaginas {
    int quadro[MAX_PAGINAS_VM];  /* quadro[pagina] ou SEM_QUADRO (-1)  */
    int num_paginas;             /* limite do espaço da VM             */
};
```

São só dois vetores globais (`static`, visíveis apenas em `memoria.c`) e um vetor por VM. Não há `malloc`: a "RAM física" é um vetor comum do processo hypervisor.

```
          RAM do hospedeiro (vetor ram[])
          ┌─────────┬─────────┬─────────┬─────────┬─────────┐
 quadro → │    0    │    1    │    2    │    3    │   ...   │
 dono   → │   VM1   │   VM1   │   VM2   │  livre  │         │
          └─────────┴─────────┴─────────┴─────────┴─────────┘
                ▲          ▲         ▲
   VM1: página 0 ┘  página 1┘         │
   VM2: página 0 ─────────────────────┘
```

## Passo 2 – Tradução de endereço

Esta é a função mais importante da etapa:

```c
int memoria_traduzir(const struct TabelaPaginas *tp, int endereco)
{
    int pagina, deslocamento;

    if (endereco < 0)
        return MEMORIA_VIOLACAO;

    pagina = endereco / TAM_PAGINA;
    deslocamento = endereco % TAM_PAGINA;

    if (pagina >= tp->num_paginas)          /* fora do espaço da VM */
        return MEMORIA_VIOLACAO;
    if (tp->quadro[pagina] == SEM_QUADRO)   /* válida, mas ausente  */
        return MEMORIA_AUSENTE;

    return tp->quadro[pagina] * TAM_PAGINA + deslocamento;
}
```

Exemplo com `TAM_PAGINA = 64`, para uma VM cuja página 2 está no quadro 11:

```
endereço do convidado = 191
página       = 191 / 64 = 2
deslocamento = 191 % 64 = 63
físico       = quadro[2] * 64 + 63 = 11 * 64 + 63 = 767   → ram[767]
```

A função tem **três resultados possíveis**, e cada um vira uma situação diferente no hypervisor:

| Resultado | Situação | O que o hypervisor faz ([Etapa 7](07-etapa-vmm.md)) |
|---|---|---|
| `>= 0` | acesso válido | lê ou escreve `ram[físico]` |
| `MEMORIA_AUSENTE` | página válida, ainda sem quadro | **falta de página**: aloca um quadro e repete a instrução |
| `MEMORIA_VIOLACAO` | fora do espaço da VM | **aborta a VM** (proteção) |

> **Isolamento:** uma VM não tem como *nomear* um quadro alheio. Qualquer endereço que ela use passa pela **sua** tabela, que só aponta para quadros **dela**. É isso que o programa `invasor` demonstra.

## Passo 3 – Leitura e escrita sempre pela tabela

```c
int memoria_ler(const struct TabelaPaginas *tp, int endereco, int *valor)
{
    int fisico = memoria_traduzir(tp, endereco);
    if (fisico < 0)
        return fisico;          /* propaga VIOLACAO ou AUSENTE */
    *valor = ram[fisico];
    return MEMORIA_OK;
}
```

`memoria_escrever` é simétrica. **Nenhum outro módulo acessa `ram[]` diretamente**: o vetor é `static` justamente para que o compilador impeça isso.

## Passo 4 – Alocação de quadros

```c
int memoria_alocar_pagina(struct TabelaPaginas *tp, int pagina, int id_vm)
{
    ...
    for (q = 0; q < NUM_QUADROS; q++) {
        if (dono_quadro[q] == QUADRO_LIVRE) {
            dono_quadro[q] = id_vm;
            for (i = 0; i < TAM_PAGINA; i++)    /* zera o quadro! */
                ram[q * TAM_PAGINA + i] = 0;
            tp->quadro[pagina] = q;
            return q;
        }
    }
    return SEM_QUADRO;                          /* RAM esgotada */
}
```

Dois detalhes importantes:

- **Primeiro livre (*first-fit*) quadro a quadro.** Como cada página é alocada separadamente, os quadros de uma VM podem ficar espalhados. No exemplo da demonstração, a pilha da VM3 recebe o quadro 11, longe dos quadros 4 e 5 que guardam o código.
- **Zerar o quadro** antes de entregá-lo. Sem isso, uma VM nova poderia ler dados deixados por uma VM antiga, o que seria um **vazamento de informação** entre VMs. SOs reais fazem o mesmo.

`memoria_liberar` devolve todos os quadros da VM, marcando-os como `QUADRO_LIVRE`.

## Passo 5 – Paginação sob demanda (como será usada)

Na criação da VM ([Etapa 5](05-etapa-montador.md)), o espaço é calculado como **imagem + pilha**. Só as páginas da imagem recebem quadros. As páginas da pilha começam **ausentes**:

```
[VMM] VM3 criada: 'fatorial' - 77 palavras, espaco de 3 paginas (2 carregadas)
      tabela de paginas:  p0->q4  p1->q5  p2->ausente
...
[VMM] t=52     VM3: falta de pagina no endereco 191 -> pagina 2 recebeu o quadro 11
```

Um programa que nunca usa a pilha nunca gasta esse quadro.

## Passo 6 – Visualização

`memoria_imprimir_mapa()` desenha quem é dono de cada quadro, e `memoria_imprimir_tabela()` mostra o mapeamento página → quadro de uma VM. Ao fim da execução o mapa precisa estar **todo livre**, o que prova que nenhuma VM "vazou" memória:

```
      q00-q15:  1  1  1  2  3  3  4  4  5  5  6  .  .  .  .  .
```

## Ponto de verificação

`memoria.c` depende apenas de `config.h`. Um teste rápido:

```c
#include <stdio.h>
#include "src/memoria.h"

int main(void)
{
    struct TabelaPaginas tp;
    int v;

    memoria_iniciar();
    memoria_iniciar_tabela(&tp, 2);
    printf("%d\n", memoria_traduzir(&tp, 10));    /* -2: ausente   */
    memoria_alocar_pagina(&tp, 0, 1);
    memoria_escrever(&tp, 10, 99);
    memoria_ler(&tp, 10, &v);
    printf("%d\n", v);                            /* 99            */
    printf("%d\n", memoria_traduzir(&tp, 500));   /* -1: violação  */
    memoria_imprimir_mapa();
    return 0;
}
```

```bash
cc -std=c99 teste_mem.c src/memoria.c -o /tmp/teste_mem && /tmp/teste_mem
```

No projeto final, os testes da seção **Memoria** de `make test` cobrem esta etapa.

## Para discutir em aula

1. O que muda se `TAM_PAGINA` passar a ser 16? E 256? (Pense em fragmentação interna e no tamanho da tabela.)
2. Por que zerar o quadro é uma questão de **segurança**, e não só de organização?
3. Como você implementaria **compartilhamento de memória** entre duas VMs usando as tabelas de páginas?

⬅️ [Etapa 1](01-etapa-isa.md) · ➡️ [Etapa 3 – CPU virtual e VM exits](03-etapa-vcpu.md)
