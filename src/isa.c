/*
 * isa.c - Tabela de instruções: nome, tipo dos operandos e se é sensível.
 *
 * A mesma tabela é usada por três partes do sistema:
 *   - o montador (texto -> número),
 *   - a vCPU (validar operandos antes de executar),
 *   - o modo de rastreamento (número -> texto).
 */
#include <stdio.h>
#include <string.h>

#include "isa.h"

const struct InfoInstrucao TABELA_INSTRUCOES[NUM_OPCODES] = {
    /*              nome      operando A      operando B      sensível */
    [OP_NOP]    = { "NOP",    OPERANDO_NENHUM, OPERANDO_NENHUM, 0 },
    [OP_LOADI]  = { "LOADI",  OPERANDO_REG,    OPERANDO_VALOR,  0 },
    [OP_LOAD]   = { "LOAD",   OPERANDO_REG,    OPERANDO_VALOR,  0 },
    [OP_STORE]  = { "STORE",  OPERANDO_REG,    OPERANDO_VALOR,  0 },
    [OP_LOADR]  = { "LOADR",  OPERANDO_REG,    OPERANDO_REG,    0 },
    [OP_STORER] = { "STORER", OPERANDO_REG,    OPERANDO_REG,    0 },
    [OP_MOV]    = { "MOV",    OPERANDO_REG,    OPERANDO_REG,    0 },
    [OP_ADD]    = { "ADD",    OPERANDO_REG,    OPERANDO_REG,    0 },
    [OP_ADDI]   = { "ADDI",   OPERANDO_REG,    OPERANDO_VALOR,  0 },
    [OP_SUB]    = { "SUB",    OPERANDO_REG,    OPERANDO_REG,    0 },
    [OP_MUL]    = { "MUL",    OPERANDO_REG,    OPERANDO_REG,    0 },
    [OP_DIV]    = { "DIV",    OPERANDO_REG,    OPERANDO_REG,    0 },
    [OP_MOD]    = { "MOD",    OPERANDO_REG,    OPERANDO_REG,    0 },
    [OP_CMP]    = { "CMP",    OPERANDO_REG,    OPERANDO_REG,    0 },
    [OP_CMPI]   = { "CMPI",   OPERANDO_REG,    OPERANDO_VALOR,  0 },
    [OP_JMP]    = { "JMP",    OPERANDO_VALOR,  OPERANDO_NENHUM, 0 },
    [OP_JEQ]    = { "JEQ",    OPERANDO_VALOR,  OPERANDO_NENHUM, 0 },
    [OP_JNE]    = { "JNE",    OPERANDO_VALOR,  OPERANDO_NENHUM, 0 },
    [OP_JLT]    = { "JLT",    OPERANDO_VALOR,  OPERANDO_NENHUM, 0 },
    [OP_JGT]    = { "JGT",    OPERANDO_VALOR,  OPERANDO_NENHUM, 0 },
    [OP_PUSH]   = { "PUSH",   OPERANDO_REG,    OPERANDO_NENHUM, 0 },
    [OP_POP]    = { "POP",    OPERANDO_REG,    OPERANDO_NENHUM, 0 },
    [OP_CALL]   = { "CALL",   OPERANDO_VALOR,  OPERANDO_NENHUM, 0 },
    [OP_RET]    = { "RET",    OPERANDO_NENHUM, OPERANDO_NENHUM, 0 },
    [OP_IN]     = { "IN",     OPERANDO_REG,    OPERANDO_VALOR,  1 },
    [OP_OUT]    = { "OUT",    OPERANDO_REG,    OPERANDO_VALOR,  1 },
    [OP_HLT]    = { "HLT",    OPERANDO_NENHUM, OPERANDO_NENHUM, 1 },
    [OP_HCALL]  = { "HCALL",  OPERANDO_VALOR,  OPERANDO_NENHUM, 1 },
};

/* Procura o opcode pelo nome (ex.: "ADDI" -> OP_ADDI). Devolve -1 se não existir. */
int isa_buscar_opcode(const char *nome)
{
    int op;

    for (op = 0; op < NUM_OPCODES; op++) {
        if (strcmp(TABELA_INSTRUCOES[op].nome, nome) == 0)
            return op;
    }
    return -1;
}

static void formatar_operando(int tipo, int valor, char *saida, int tam)
{
    if (tipo == OPERANDO_REG)
        snprintf(saida, tam, "R%d", valor);
    else
        snprintf(saida, tam, "%d", valor);
}

/* Converte uma instrução numérica em texto (ex.: 8 0 1 -> "ADDI R0, 1"). */
void isa_desmontar(int op, int a, int b, char *saida, int tam)
{
    const struct InfoInstrucao *info;
    char texto_a[16];
    char texto_b[16];

    if (op < 0 || op >= NUM_OPCODES) {
        snprintf(saida, tam, "??? (opcode %d)", op);
        return;
    }

    info = &TABELA_INSTRUCOES[op];
    formatar_operando(info->tipo_a, a, texto_a, sizeof texto_a);
    formatar_operando(info->tipo_b, b, texto_b, sizeof texto_b);

    if (info->tipo_a == OPERANDO_NENHUM)
        snprintf(saida, tam, "%s", info->nome);
    else if (info->tipo_b == OPERANDO_NENHUM)
        snprintf(saida, tam, "%-6s %s", info->nome, texto_a);
    else
        snprintf(saida, tam, "%-6s %s, %s", info->nome, texto_a, texto_b);
}
