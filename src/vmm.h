/*
 * vmm.h - O hypervisor (Virtual Machine Monitor) propriamente dito.
 *
 * Responsabilidades:
 *   - carregar programas em VMs novas (montador + tabela de páginas);
 *   - escalonar as vCPUs na CPU real (Round-Robin com quantum);
 *   - tratar cada VM exit (trap-and-emulate, hypercalls, faltas de página);
 *   - isolar as VMs: um erro em uma VM nunca afeta as outras.
 */
#ifndef VMM_H
#define VMM_H

struct ConfigVMM {
    int  quantum;              /* instruções por fatia de tempo        */
    long limite_instrucoes;    /* watchdog: máximo de instruções / VM  */
    int  verboso;              /* 1 = mostra cada VM exit e troca      */
    int  rastrear;             /* 1 = mostra cada instrução executada  */
};

void vmm_iniciar(const struct ConfigVMM *config);
int  vmm_criar_vm(const char *nome, const char *fonte);
void vmm_executar(void);
void vmm_relatorio(void);

#endif
