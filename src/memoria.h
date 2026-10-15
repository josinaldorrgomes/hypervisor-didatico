/*
 * memoria.h - Virtualização de memória com paginação.
 *
 * O hospedeiro possui uma única RAM (um vetor de int) dividida em QUADROS.
 * Cada VM enxerga um espaço de endereços próprio, que começa em 0, dividido
 * em PÁGINAS. A tabela de páginas de cada VM diz em qual quadro da RAM do
 * hospedeiro cada página está:
 *
 *   endereço do convidado ──(tabela de páginas da VM)──> endereço na RAM real
 *
 * É o mesmo papel da EPT (Intel) / NPT (AMD) em hypervisors de verdade.
 */
#ifndef MEMORIA_H
#define MEMORIA_H

#include "config.h"

#define SEM_QUADRO     -1   /* página válida, mas ainda sem quadro (não presente) */
#define QUADRO_LIVRE    0   /* dono de um quadro que ninguém está usando          */

/* Resultados de memoria_ler / memoria_escrever / memoria_traduzir */
#define MEMORIA_OK          0
#define MEMORIA_VIOLACAO   -1   /* endereço fora do espaço da VM        */
#define MEMORIA_AUSENTE    -2   /* página válida, mas ainda não alocada */

struct TabelaPaginas {
    int quadro[MAX_PAGINAS_VM];  /* quadro[pagina] = quadro na RAM, ou SEM_QUADRO */
    int num_paginas;             /* limite: páginas que a VM pode usar             */
};

void memoria_iniciar(void);
void memoria_iniciar_tabela(struct TabelaPaginas *tp, int num_paginas);
int  memoria_alocar_pagina(struct TabelaPaginas *tp, int pagina, int id_vm);
void memoria_liberar(struct TabelaPaginas *tp);

int  memoria_traduzir(const struct TabelaPaginas *tp, int endereco);
int  memoria_ler(const struct TabelaPaginas *tp, int endereco, int *valor);
int  memoria_escrever(const struct TabelaPaginas *tp, int endereco, int valor);

int  memoria_quadros_livres(void);
void memoria_imprimir_mapa(void);
void memoria_imprimir_tabela(const struct TabelaPaginas *tp);

#endif
