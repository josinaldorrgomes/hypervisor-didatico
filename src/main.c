/*
 * main.c - Ponto de entrada: lê as opções da linha de comando, cria as VMs
 *          e entrega o controle ao hypervisor.
 *
 * Uso: hypervisor [opcoes] [programa ...]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>   /* SetConsoleOutputCP: acentos corretos no console do Windows */
#endif

#include "config.h"
#include "memoria.h"
#include "programas.h"
#include "vmm.h"

/* VMs criadas quando nenhum programa é informado */
static const char *DEMONSTRACAO[] = {
    "ola", "contador", "fatorial", "dorminhoco", "invasor", "guloso"
};
#define NUM_DEMONSTRACAO ((int)(sizeof DEMONSTRACAO / sizeof DEMONSTRACAO[0]))

static void mostrar_ajuda(void)
{
    printf("Hypervisor didatico - Sistemas Operacionais\n\n");
    printf("Uso: hypervisor [opcoes] [programa ...]\n\n");
    printf("Cada programa informado vira uma maquina virtual (o mesmo pode se repetir).\n");
    printf("Sem programas, executa a demonstracao: ");
    {
        int i;
        for (i = 0; i < NUM_DEMONSTRACAO; i++)
            printf("%s ", DEMONSTRACAO[i]);
    }
    printf("\n\nOpcoes:\n");
    printf("  -l          lista os programas convidados embutidos\n");
    printf("  -s nome     mostra o codigo assembly de um programa\n");
    printf("  -q N        quantum: instrucoes por fatia de tempo (padrao %d)\n", QUANTUM_PADRAO);
    printf("  -m N        watchdog: maximo de instrucoes por VM (padrao %d)\n", LIMITE_PADRAO);
    printf("  -v          verboso: mostra cada VM exit e troca de contexto\n");
    printf("  -t          rastreia cada instrucao executada (gera muita saida)\n");
    printf("  -h          mostra esta ajuda\n\n");
    printf("Exemplos:\n");
    printf("  hypervisor\n");
    printf("  hypervisor -v contador contador\n");
    printf("  hypervisor -q 5 -t fibonacci\n");
}

static void listar_programas(void)
{
    int i;

    printf("Programas convidados embutidos:\n");
    for (i = 0; i < NUM_PROGRAMAS; i++)
        printf("  %-11s %s\n", PROGRAMAS[i].nome, PROGRAMAS[i].descricao);
}

/* Converte texto em número positivo. Devolve 0 se for inválido. */
static long ler_numero_positivo(const char *texto)
{
    char *fim;
    long valor = strtol(texto, &fim, 10);

    if (*texto == '\0' || *fim != '\0' || valor <= 0)
        return 0;
    return valor;
}

int main(int argc, char *argv[])
{
    struct ConfigVMM config;
    const char *nomes[MAX_VMS];
    int num_nomes = 0;
    int i;

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    config.quantum = QUANTUM_PADRAO;
    config.limite_instrucoes = LIMITE_PADRAO;
    config.verboso = 0;
    config.rastrear = 0;

    /* ---------- 1. Opções da linha de comando ---------- */
    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            mostrar_ajuda();
            return 0;
        } else if (strcmp(arg, "-l") == 0) {
            listar_programas();
            return 0;
        } else if (strcmp(arg, "-s") == 0 && i + 1 < argc) {
            const struct ProgramaConvidado *p = programa_buscar(argv[++i]);
            if (p == NULL) {
                fprintf(stderr, "Programa desconhecido: %s (use -l)\n", argv[i]);
                return 1;
            }
            printf("%s", p->fonte);
            return 0;
        } else if (strcmp(arg, "-q") == 0 && i + 1 < argc) {
            long q = ler_numero_positivo(argv[++i]);
            if (q == 0 || q > 1000000) {
                fprintf(stderr, "Quantum invalido: %s\n", argv[i]);
                return 1;
            }
            config.quantum = (int)q;
        } else if (strcmp(arg, "-m") == 0 && i + 1 < argc) {
            config.limite_instrucoes = ler_numero_positivo(argv[++i]);
            if (config.limite_instrucoes == 0) {
                fprintf(stderr, "Limite invalido: %s\n", argv[i]);
                return 1;
            }
        } else if (strcmp(arg, "-v") == 0) {
            config.verboso = 1;
        } else if (strcmp(arg, "-t") == 0) {
            config.rastrear = 1;
        } else if (arg[0] == '-') {
            fprintf(stderr, "Opcao invalida ou incompleta: %s (use -h)\n", arg);
            return 1;
        } else {
            if (programa_buscar(arg) == NULL) {
                fprintf(stderr, "Programa desconhecido: %s (use -l)\n", arg);
                return 1;
            }
            if (num_nomes == MAX_VMS) {
                fprintf(stderr, "No maximo %d VMs por execucao\n", MAX_VMS);
                return 1;
            }
            nomes[num_nomes++] = arg;
        }
    }

    if (num_nomes == 0) {
        for (i = 0; i < NUM_DEMONSTRACAO; i++)
            nomes[num_nomes++] = DEMONSTRACAO[i];
    }

    /* ---------- 2. Liga o hypervisor e cria as VMs ---------- */
    printf("============================ HYPERVISOR DIDATICO ============================\n");
    vmm_iniciar(&config);

    for (i = 0; i < num_nomes; i++) {
        if (vmm_criar_vm(nomes[i], programa_buscar(nomes[i])->fonte) < 0)
            return 1;
    }
    memoria_imprimir_mapa();

    /* ---------- 3. Executa até todas as VMs terminarem ---------- */
    printf("\n");
    vmm_executar();

    /* ---------- 4. Resultado ---------- */
    vmm_relatorio();
    memoria_imprimir_mapa();
    return 0;
}
