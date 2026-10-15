/*
 * config.h - Constantes que definem o "tamanho" do nosso mundo virtual.
 *
 * Tudo é estático (vetores de tamanho fixo): não há malloc/free no projeto.
 * Mudar um valor aqui e recompilar é a forma de experimentar.
 */
#ifndef CONFIG_H
#define CONFIG_H

/* ---------------- Memória (Etapa 2) ---------------- */
#define TAM_PAGINA          64   /* palavras por página (e por quadro)           */
#define NUM_QUADROS         64   /* quantos quadros a RAM do hospedeiro possui    */
#define MAX_PAGINAS_VM       8   /* tamanho máximo do espaço de uma VM (páginas)  */
#define TAM_PILHA           64   /* palavras reservadas para a pilha do convidado */

#define TAM_RAM_HOSPEDEIRO  (NUM_QUADROS * TAM_PAGINA)
#define MAX_PALAVRAS_VM     (MAX_PAGINAS_VM * TAM_PAGINA)

/* ---------------- CPU virtual (Etapas 1 e 3) ---------------- */
#define NUM_REGISTRADORES    8   /* R0 .. R7                                      */
#define TAM_INSTRUCAO        3   /* toda instrução ocupa 3 palavras: op, A, B     */

/* ---------------- Hypervisor (Etapas 4 a 7) ---------------- */
#define MAX_VMS             10   /* tamanho da tabela de máquinas virtuais        */
#define QUANTUM_PADRAO      25   /* instruções por fatia de tempo (timer)         */
#define LIMITE_PADRAO   100000   /* watchdog: instruções máximas por VM           */
#define MAX_NOME            16   /* tamanho máximo do nome de uma VM              */
#define TAM_LINHA_CONSOLE  128   /* buffer de linha do console virtual            */

#endif
