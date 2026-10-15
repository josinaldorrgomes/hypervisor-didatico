/*
 * vcpu.c - Ciclo busca -> decodifica -> executa da CPU virtual.
 *
 * Regra de ouro: o convidado NÃO é confiável. Nada que ele faça pode
 * corromper a memória do hospedeiro ou derrubar o hypervisor. Por isso:
 *   - todo acesso à memória passa pela tabela de páginas da VM;
 *   - todo índice de registrador é validado antes de ser usado;
 *   - divisão por zero vira uma VM exit, e não um crash do hospedeiro;
 *   - a aritmética é feita em 'unsigned' (estouro em int é comportamento
 *     indefinido em C e poderia ser explorado pelo convidado).
 */
#include <stdio.h>

#include "vcpu.h"
#include "vm.h"
#include "isa.h"
#include "memoria.h"

static const char *NOMES_SAIDA[NUM_MOTIVOS_SAIDA] = {
    "NENHUMA", "QUANTUM", "IO_OUT", "IO_IN", "HLT", "HYPERCALL",
    "FALTA_PAGINA", "VIOLACAO_MEMORIA", "INSTRUCAO_INVALIDA", "DIVISAO_ZERO"
};

const char *vcpu_nome_saida(int motivo)
{
    if (motivo < 0 || motivo >= NUM_MOTIVOS_SAIDA)
        return "?";
    return NOMES_SAIDA[motivo];
}

/* Equivale ao "reset" de uma CPU real: zera tudo e define pc e sp. */
void vcpu_reiniciar(struct VCpu *cpu, int pc_inicial, int sp_inicial)
{
    int i;

    for (i = 0; i < NUM_REGISTRADORES; i++)
        cpu->reg[i] = 0;
    cpu->pc = pc_inicial;
    cpu->sp = sp_inicial;
    cpu->flag = 0;
    cpu->instrucoes_executadas = 0;
    cpu->saida_porta = 0;
    cpu->saida_reg = 0;
    cpu->saida_valor = 0;
    cpu->endereco_falha = 0;
}

/* ---------- acesso à memória do convidado (passa SEMPRE pela tradução) ---------- */

/* Converte o resultado da tradução em motivo de saída e anota o endereço. */
static int falha_de_memoria(struct VCpu *cpu, int resultado, int endereco)
{
    cpu->endereco_falha = endereco;
    if (resultado == MEMORIA_AUSENTE)
        return SAIDA_FALTA_PAGINA;
    return SAIDA_VIOLACAO_MEMORIA;
}

static int ler(struct MaquinaVirtual *vm, int endereco, int *valor)
{
    int r = memoria_ler(&vm->paginas, endereco, valor);

    if (r != MEMORIA_OK)
        return falha_de_memoria(&vm->cpu, r, endereco);
    return SAIDA_NENHUMA;
}

static int escrever(struct MaquinaVirtual *vm, int endereco, int valor)
{
    int r = memoria_escrever(&vm->paginas, endereco, valor);

    if (r != MEMORIA_OK)
        return falha_de_memoria(&vm->cpu, r, endereco);
    return SAIDA_NENHUMA;
}

/* ---------- aritmética segura (sem comportamento indefinido) ---------- */

static int somar(int x, int y)       { return (int)((unsigned)x + (unsigned)y); }
static int subtrair(int x, int y)    { return (int)((unsigned)x - (unsigned)y); }
static int multiplicar(int x, int y) { return (int)((unsigned)x * (unsigned)y); }

static int comparar(int x, int y)
{
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

/* ---------- decodificação ---------- */

static int operando_valido(int tipo, int valor)
{
    if (tipo == OPERANDO_REG)
        return valor >= 0 && valor < NUM_REGISTRADORES;
    return 1;
}

static int instrucao_valida(int op, int a, int b)
{
    if (op < 0 || op >= NUM_OPCODES)
        return 0;
    return operando_valido(TABELA_INSTRUCOES[op].tipo_a, a)
        && operando_valido(TABELA_INSTRUCOES[op].tipo_b, b);
}

/* Busca as 3 palavras da instrução que está em 'pc'. */
static int buscar(struct MaquinaVirtual *vm, int pc, int *op, int *a, int *b)
{
    int r;

    r = ler(vm, pc, op);
    if (r != SAIDA_NENHUMA) return r;
    r = ler(vm, pc + 1, a);
    if (r != SAIDA_NENHUMA) return r;
    return ler(vm, pc + 2, b);
}

/* ---------- execução ---------- */

/*
 * Executa UMA instrução já decodificada. O pc já aponta para a próxima.
 *
 * Importante: se houver falha de memória, NADA do estado pode ter sido
 * alterado, porque a instrução será reexecutada depois que o hypervisor
 * resolver a falta de página. Por isso a escrita na memória sempre vem
 * ANTES de mexer em registradores, sp ou pc.
 */
static int executar(struct MaquinaVirtual *vm, int op, int a, int b)
{
    struct VCpu *cpu = &vm->cpu;
    int *reg = cpu->reg;
    int valor, r;

    switch (op) {
    case OP_NOP:
        return SAIDA_NENHUMA;

    case OP_LOADI:
        reg[a] = b;
        return SAIDA_NENHUMA;

    case OP_LOAD:
        r = ler(vm, b, &valor);
        if (r != SAIDA_NENHUMA) return r;
        reg[a] = valor;
        return SAIDA_NENHUMA;

    case OP_STORE:
        return escrever(vm, b, reg[a]);

    case OP_LOADR:
        r = ler(vm, reg[b], &valor);
        if (r != SAIDA_NENHUMA) return r;
        reg[a] = valor;
        return SAIDA_NENHUMA;

    case OP_STORER:
        return escrever(vm, reg[b], reg[a]);

    case OP_MOV:  reg[a] = reg[b];                    return SAIDA_NENHUMA;
    case OP_ADD:  reg[a] = somar(reg[a], reg[b]);     return SAIDA_NENHUMA;
    case OP_ADDI: reg[a] = somar(reg[a], b);          return SAIDA_NENHUMA;
    case OP_SUB:  reg[a] = subtrair(reg[a], reg[b]);  return SAIDA_NENHUMA;
    case OP_MUL:  reg[a] = multiplicar(reg[a], reg[b]); return SAIDA_NENHUMA;

    case OP_DIV:
    case OP_MOD:
        if (reg[b] == 0)
            return SAIDA_DIVISAO_ZERO;
        if (reg[b] == -1)   /* evita o único estouro possível: INT_MIN / -1 */
            reg[a] = (op == OP_DIV) ? subtrair(0, reg[a]) : 0;
        else
            reg[a] = (op == OP_DIV) ? reg[a] / reg[b] : reg[a] % reg[b];
        return SAIDA_NENHUMA;

    case OP_CMP:  cpu->flag = comparar(reg[a], reg[b]); return SAIDA_NENHUMA;
    case OP_CMPI: cpu->flag = comparar(reg[a], b);      return SAIDA_NENHUMA;

    case OP_JMP:                       cpu->pc = a; return SAIDA_NENHUMA;
    case OP_JEQ: if (cpu->flag == 0)   cpu->pc = a; return SAIDA_NENHUMA;
    case OP_JNE: if (cpu->flag != 0)   cpu->pc = a; return SAIDA_NENHUMA;
    case OP_JLT: if (cpu->flag < 0)    cpu->pc = a; return SAIDA_NENHUMA;
    case OP_JGT: if (cpu->flag > 0)    cpu->pc = a; return SAIDA_NENHUMA;

    case OP_PUSH:
        r = escrever(vm, cpu->sp - 1, reg[a]);
        if (r != SAIDA_NENHUMA) return r;
        cpu->sp--;
        return SAIDA_NENHUMA;

    case OP_POP:
        r = ler(vm, cpu->sp, &valor);
        if (r != SAIDA_NENHUMA) return r;
        reg[a] = valor;
        cpu->sp++;
        return SAIDA_NENHUMA;

    case OP_CALL:
        r = escrever(vm, cpu->sp - 1, cpu->pc);  /* guarda o endereço de retorno */
        if (r != SAIDA_NENHUMA) return r;
        cpu->sp--;
        cpu->pc = a;
        return SAIDA_NENHUMA;

    case OP_RET:
        r = ler(vm, cpu->sp, &valor);
        if (r != SAIDA_NENHUMA) return r;
        cpu->sp++;
        cpu->pc = valor;
        return SAIDA_NENHUMA;

    /* ---- Instruções sensíveis: a vCPU não executa, só avisa o hypervisor ---- */
    case OP_OUT:
        cpu->saida_porta = b;
        cpu->saida_reg = a;
        cpu->saida_valor = reg[a];
        return SAIDA_IO_OUT;

    case OP_IN:
        cpu->saida_porta = b;
        cpu->saida_reg = a;
        return SAIDA_IO_IN;

    case OP_HLT:
        return SAIDA_HLT;

    case OP_HCALL:
        cpu->saida_valor = a;
        return SAIDA_HYPERCALL;
    }
    return SAIDA_INSTRUCAO_INVALIDA;
}

/*
 * "Entra" na VM e executa até 'max_instrucoes' instruções.
 * Devolve o motivo pelo qual parou (VM exit).
 */
int vcpu_executar(struct MaquinaVirtual *vm, int max_instrucoes, int rastrear)
{
    struct VCpu *cpu = &vm->cpu;
    int i, pc, op, a, b, motivo;
    char texto[48];

    for (i = 0; i < max_instrucoes; i++) {
        pc = cpu->pc;

        /* 1. BUSCA */
        motivo = buscar(vm, pc, &op, &a, &b);
        if (motivo != SAIDA_NENHUMA)
            return motivo;

        /* 2. DECODIFICA (e valida: o convidado pode ter escrito lixo como código) */
        if (!instrucao_valida(op, a, b))
            return SAIDA_INSTRUCAO_INVALIDA;

        if (rastrear) {
            isa_desmontar(op, a, b, texto, sizeof texto);
            printf("      . VM%d pc=%04d  %-18s%s\n", vm->id, pc, texto,
                   TABELA_INSTRUCOES[op].sensivel ? "<- sensivel: VM exit" : "");
        }

        /* 3. EXECUTA */
        cpu->pc = pc + TAM_INSTRUCAO;
        motivo = executar(vm, op, a, b);

        if (motivo == SAIDA_FALTA_PAGINA || motivo == SAIDA_VIOLACAO_MEMORIA) {
            cpu->pc = pc;   /* instrução não concluída: será reexecutada */
            return motivo;
        }

        cpu->instrucoes_executadas++;
        if (motivo != SAIDA_NENHUMA)
            return motivo;
    }
    return SAIDA_QUANTUM;   /* o "timer" disparou: acabou a fatia de tempo */
}
