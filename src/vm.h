/*
 * vm.h - Bloco de controle de uma máquina virtual.
 *
 * É o "PCB" do hypervisor: assim como o sistema operacional guarda um
 * bloco de controle por processo, o hypervisor guarda um struct destes
 * por VM, com CPU virtual, memória, dispositivos, estado e estatísticas.
 */
#ifndef VM_H
#define VM_H

#include "config.h"
#include "vcpu.h"
#include "memoria.h"
#include "dispositivos.h"

/* Mesmo diagrama de estados de processos visto nas aulas */
enum EstadoVM {
    VM_LIVRE,        /* posição da tabela sem VM                 */
    VM_PRONTA,       /* aguardando a vez de usar a CPU real      */
    VM_EXECUTANDO,   /* com a CPU real neste momento             */
    VM_BLOQUEADA,    /* dormindo (HCALL 2) até 'acordar_em'      */
    VM_FINALIZADA,   /* terminou normalmente (HLT)               */
    VM_ABORTADA      /* encerrada pelo hypervisor (erro)         */
};

struct MaquinaVirtual {
    int id;
    char nome[MAX_NOME];
    int estado;

    struct VCpu cpu;               /* CPU virtual                         */
    struct TabelaPaginas paginas;  /* memória virtual                     */
    struct Console console;        /* dispositivo de saída                */

    long acordar_em;               /* relógio em que uma VM BLOQUEADA acorda */

    /* estatísticas para o relatório final */
    long saidas[NUM_MOTIVOS_SAIDA];  /* quantas VM exits de cada tipo       */
    long fatias;                     /* quantas vezes recebeu a CPU         */
    char motivo_fim[96];
};

const char *vm_nome_estado(int estado);

#endif
