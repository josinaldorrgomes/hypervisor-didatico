/*
 * isa.h - Conjunto de instruções (ISA) da máquina que será virtualizada.
 *
 * Toda instrução ocupa TAM_INSTRUCAO (3) palavras na memória do convidado:
 *
 *      endereço:   pc        pc+1        pc+2
 *      conteúdo: [opcode] [operando A] [operando B]
 *
 * As instruções "sensíveis" (IN, OUT, HLT, HCALL) mexem em recursos que
 * pertencem ao hospedeiro. Elas NUNCA são executadas pela vCPU: sempre
 * provocam uma saída para o hypervisor (VM exit), que emula o efeito.
 * Esse é o princípio "trap-and-emulate" de Popek & Goldberg.
 */
#ifndef ISA_H
#define ISA_H

enum Opcode {
    /* ---- instruções comuns (executadas direto pela vCPU) ---- */
    OP_NOP,     /* não faz nada                               */
    OP_LOADI,   /* LOADI  Ra, valor   Ra = valor              */
    OP_LOAD,    /* LOAD   Ra, end     Ra = mem[end]           */
    OP_STORE,   /* STORE  Ra, end     mem[end] = Ra           */
    OP_LOADR,   /* LOADR  Ra, Rb      Ra = mem[Rb]            */
    OP_STORER,  /* STORER Ra, Rb      mem[Rb] = Ra            */
    OP_MOV,     /* MOV    Ra, Rb      Ra = Rb                 */
    OP_ADD,     /* ADD    Ra, Rb      Ra = Ra + Rb            */
    OP_ADDI,    /* ADDI   Ra, valor   Ra = Ra + valor         */
    OP_SUB,     /* SUB    Ra, Rb      Ra = Ra - Rb            */
    OP_MUL,     /* MUL    Ra, Rb      Ra = Ra * Rb            */
    OP_DIV,     /* DIV    Ra, Rb      Ra = Ra / Rb            */
    OP_MOD,     /* MOD    Ra, Rb      Ra = Ra % Rb            */
    OP_CMP,     /* CMP    Ra, Rb      flag = compara(Ra, Rb)  */
    OP_CMPI,    /* CMPI   Ra, valor   flag = compara(Ra, val) */
    OP_JMP,     /* JMP    end         pc = end                */
    OP_JEQ,     /* JEQ    end         se flag == 0: pc = end  */
    OP_JNE,     /* JNE    end         se flag != 0: pc = end  */
    OP_JLT,     /* JLT    end         se flag <  0: pc = end  */
    OP_JGT,     /* JGT    end         se flag >  0: pc = end  */
    OP_PUSH,    /* PUSH   Ra          sp--, mem[sp] = Ra      */
    OP_POP,     /* POP    Ra          Ra = mem[sp], sp++      */
    OP_CALL,    /* CALL   end         PUSH pc; pc = end       */
    OP_RET,     /* RET                POP pc                  */

    /* ---- instruções sensíveis (sempre causam VM exit) ---- */
    OP_IN,      /* IN     Ra, porta   Ra = dispositivo[porta] */
    OP_OUT,     /* OUT    Ra, porta   dispositivo[porta] = Ra */
    OP_HLT,     /* HLT                desliga a VM            */
    OP_HCALL,   /* HCALL  numero      chamada ao hypervisor   */

    NUM_OPCODES
};

/* Que tipo de coisa cada operando guarda */
enum TipoOperando {
    OPERANDO_NENHUM,   /* operando não usado                       */
    OPERANDO_REG,      /* índice de registrador (0..7)             */
    OPERANDO_VALOR     /* número, endereço, porta ou rótulo        */
};

struct InfoInstrucao {
    const char *nome;
    int tipo_a;
    int tipo_b;
    int sensivel;      /* 1 = provoca VM exit                      */
};

extern const struct InfoInstrucao TABELA_INSTRUCOES[NUM_OPCODES];

/* ---- Portas de E/S dos dispositivos virtuais (Etapa 4) ---- */
#define PORTA_CONSOLE_NUMERO  1   /* OUT: escreve um inteiro no console    */
#define PORTA_CONSOLE_CHAR    2   /* OUT: escreve um caractere no console  */
#define PORTA_ID_VM           3   /* IN : lê o número desta VM             */
#define PORTA_RELOGIO         4   /* IN : lê o relógio do hypervisor       */

/* ---- Hypercalls: "chamadas de sistema" do convidado ao hypervisor ---- */
#define HC_IMPRIMIR_TEXTO     0   /* R0 = endereço de uma string (fim = 0) */
#define HC_CEDER_CPU          1   /* devolve a CPU antes do fim do quantum */
#define HC_DORMIR             2   /* R0 = ticks de relógio para dormir     */

int  isa_buscar_opcode(const char *nome);
void isa_desmontar(int op, int a, int b, char *saida, int tam);

#endif
