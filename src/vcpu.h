/*
 * vcpu.h - CPU virtual (vCPU).
 *
 * A vCPU executa as instruções do convidado até que algo exija o
 * hypervisor. Nesse momento ela PARA e devolve o motivo da saída
 * (VM exit). É o equivalente ao retorno de ioctl(KVM_RUN) no Linux.
 */
#ifndef VCPU_H
#define VCPU_H

#include "config.h"

/* Por que a vCPU devolveu o controle ao hypervisor? */
enum MotivoSaida {
    SAIDA_NENHUMA,             /* uso interno: instrução terminou normalmente */
    SAIDA_QUANTUM,             /* fim da fatia de tempo (interrupção de timer) */
    SAIDA_IO_OUT,              /* convidado executou OUT                       */
    SAIDA_IO_IN,               /* convidado executou IN                        */
    SAIDA_HLT,                 /* convidado executou HLT                       */
    SAIDA_HYPERCALL,           /* convidado executou HCALL                     */
    SAIDA_FALTA_PAGINA,        /* acesso a página válida ainda não alocada     */
    SAIDA_VIOLACAO_MEMORIA,    /* acesso fora do espaço de endereços da VM     */
    SAIDA_INSTRUCAO_INVALIDA,  /* opcode ou registrador inexistente            */
    SAIDA_DIVISAO_ZERO,        /* DIV ou MOD por zero                          */
    NUM_MOTIVOS_SAIDA
};

/*
 * Estado completo da CPU de uma VM. Salvar/restaurar este struct é tudo
 * o que uma "troca de contexto" entre VMs precisa fazer.
 */
struct VCpu {
    int reg[NUM_REGISTRADORES];   /* R0..R7                                     */
    int pc;                       /* contador de programa                       */
    int sp;                       /* ponteiro de pilha (cresce para baixo)      */
    int flag;                     /* resultado do último CMP: -1, 0 ou 1        */
    long instrucoes_executadas;

    /* Detalhes da última VM exit, preenchidos pela vCPU para o hypervisor */
    int saida_porta;              /* IN/OUT: número da porta                    */
    int saida_reg;                /* IN/OUT: registrador envolvido              */
    int saida_valor;              /* OUT: valor escrito / HCALL: número         */
    int endereco_falha;           /* falta de página / violação: endereço       */
};

struct MaquinaVirtual;

void        vcpu_reiniciar(struct VCpu *cpu, int pc_inicial, int sp_inicial);
int         vcpu_executar(struct MaquinaVirtual *vm, int max_instrucoes, int rastrear);
const char *vcpu_nome_saida(int motivo);

#endif
