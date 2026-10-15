/*
 * memoria.c - RAM do hospedeiro, alocação de quadros e tradução de endereços.
 */
#include <stdio.h>

#include "memoria.h"

static int ram[TAM_RAM_HOSPEDEIRO];   /* a "RAM física" do hospedeiro          */
static int dono_quadro[NUM_QUADROS];  /* QUADRO_LIVRE ou o id da VM dona       */

void memoria_iniciar(void)
{
    int i;

    for (i = 0; i < TAM_RAM_HOSPEDEIRO; i++)
        ram[i] = 0;
    for (i = 0; i < NUM_QUADROS; i++)
        dono_quadro[i] = QUADRO_LIVRE;
}

/* Cria uma tabela com 'num_paginas' páginas válidas, todas ainda ausentes. */
void memoria_iniciar_tabela(struct TabelaPaginas *tp, int num_paginas)
{
    int p;

    tp->num_paginas = num_paginas;
    for (p = 0; p < MAX_PAGINAS_VM; p++)
        tp->quadro[p] = SEM_QUADRO;
}

/*
 * Dá um quadro livre para a página 'pagina' da VM.
 * Devolve o número do quadro, ou SEM_QUADRO se a RAM acabou.
 */
int memoria_alocar_pagina(struct TabelaPaginas *tp, int pagina, int id_vm)
{
    int q, i;

    if (pagina < 0 || pagina >= tp->num_paginas)
        return SEM_QUADRO;
    if (tp->quadro[pagina] != SEM_QUADRO)
        return tp->quadro[pagina];

    for (q = 0; q < NUM_QUADROS; q++) {
        if (dono_quadro[q] == QUADRO_LIVRE) {
            dono_quadro[q] = id_vm;
            /* Zera o quadro: a VM nova não pode ler lixo deixado por outra VM. */
            for (i = 0; i < TAM_PAGINA; i++)
                ram[q * TAM_PAGINA + i] = 0;
            tp->quadro[pagina] = q;
            return q;
        }
    }
    return SEM_QUADRO;
}

/* Devolve ao hospedeiro todos os quadros usados pela VM. */
void memoria_liberar(struct TabelaPaginas *tp)
{
    int p;

    for (p = 0; p < tp->num_paginas; p++) {
        if (tp->quadro[p] != SEM_QUADRO) {
            dono_quadro[tp->quadro[p]] = QUADRO_LIVRE;
            tp->quadro[p] = SEM_QUADRO;
        }
    }
}

/*
 * O coração da virtualização de memória: converte um endereço do convidado
 * em um índice da RAM do hospedeiro.
 *
 *   página       = endereço / TAM_PAGINA
 *   deslocamento = endereço % TAM_PAGINA
 *   físico       = quadro[página] * TAM_PAGINA + deslocamento
 *
 * Devolve o índice na RAM (>= 0), MEMORIA_VIOLACAO ou MEMORIA_AUSENTE.
 */
int memoria_traduzir(const struct TabelaPaginas *tp, int endereco)
{
    int pagina, deslocamento;

    if (endereco < 0)
        return MEMORIA_VIOLACAO;

    pagina = endereco / TAM_PAGINA;
    deslocamento = endereco % TAM_PAGINA;

    if (pagina >= tp->num_paginas)
        return MEMORIA_VIOLACAO;
    if (tp->quadro[pagina] == SEM_QUADRO)
        return MEMORIA_AUSENTE;

    return tp->quadro[pagina] * TAM_PAGINA + deslocamento;
}

int memoria_ler(const struct TabelaPaginas *tp, int endereco, int *valor)
{
    int fisico = memoria_traduzir(tp, endereco);

    if (fisico < 0)
        return fisico;
    *valor = ram[fisico];
    return MEMORIA_OK;
}

int memoria_escrever(const struct TabelaPaginas *tp, int endereco, int valor)
{
    int fisico = memoria_traduzir(tp, endereco);

    if (fisico < 0)
        return fisico;
    ram[fisico] = valor;
    return MEMORIA_OK;
}

int memoria_quadros_livres(void)
{
    int q, livres = 0;

    for (q = 0; q < NUM_QUADROS; q++) {
        if (dono_quadro[q] == QUADRO_LIVRE)
            livres++;
    }
    return livres;
}

/* Desenha a RAM do hospedeiro: cada posição é um quadro e mostra a VM dona. */
void memoria_imprimir_mapa(void)
{
    int q;

    printf("[VMM] Mapa da RAM do hospedeiro (%d quadros de %d palavras; "
           "numero = VM dona, '.' = livre)\n", NUM_QUADROS, TAM_PAGINA);
    for (q = 0; q < NUM_QUADROS; q++) {
        if (q % 16 == 0)
            printf("      q%02d-q%02d:", q, q + 15);
        if (dono_quadro[q] == QUADRO_LIVRE)
            printf("  .");
        else
            printf(" %2d", dono_quadro[q]);
        if (q % 16 == 15)
            printf("\n");
    }
    printf("      quadros livres: %d de %d\n", memoria_quadros_livres(), NUM_QUADROS);
}

/* Mostra a tabela de páginas de uma VM: página -> quadro. */
void memoria_imprimir_tabela(const struct TabelaPaginas *tp)
{
    int p;

    printf("      tabela de paginas:");
    for (p = 0; p < tp->num_paginas; p++) {
        if (tp->quadro[p] == SEM_QUADRO)
            printf("  p%d->ausente", p);
        else
            printf("  p%d->q%d", p, tp->quadro[p]);
    }
    printf("\n");
}
