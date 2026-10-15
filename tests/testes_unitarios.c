/*
 * testes_unitarios.c - Testes do montador, da memória e da vCPU.
 *
 * Compila junto com os módulos do hypervisor (menos o main.c):
 *     make test
 */
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "../src/isa.h"
#include "../src/memoria.h"
#include "../src/montador.h"
#include "../src/vcpu.h"
#include "../src/vm.h"

static int total = 0;
static int falhas = 0;

static void verificar(int condicao, const char *descricao)
{
    total++;
    if (condicao) {
        printf("  ok    %s\n", descricao);
    } else {
        printf("  FALHA %s\n", descricao);
        falhas++;
    }
}

/* ------------------------------------------------------------ montador */

static int imagem[MAX_PALAVRAS_VM];
static char erro[120];

static int monta(const char *fonte)
{
    erro[0] = '\0';
    return montar(fonte, imagem, MAX_PALAVRAS_VM, erro, sizeof erro);
}

static void testar_montador(void)
{
    printf("Montador\n");

    verificar(monta("LOADI R1, 5\nHLT\n") == 6, "duas instrucoes ocupam 6 palavras");
    verificar(imagem[0] == OP_LOADI && imagem[1] == 1 && imagem[2] == 5,
              "LOADI R1, 5 vira [OP_LOADI, 1, 5]");

    verificar(monta("JMP fim\nNOP\nfim: HLT\n") == 9 && imagem[1] == 6,
              "rotulo usado antes de definido recebe o endereco certo (2 passos)");

    verificar(monta("x: .word 1, -2, 'A'\n") == 3 &&
              imagem[0] == 1 && imagem[1] == -2 && imagem[2] == 'A',
              ".word aceita numeros negativos e caracteres");

    verificar(monta("s: .string \"a;b\\n\"\n") == 5 &&
              imagem[1] == ';' && imagem[3] == '\n' && imagem[4] == 0,
              ".string com ';' e escape \\n, terminada em 0");

    verificar(monta("LOADI R3, ','\n") == 3 && imagem[2] == ',',
              "caractere ',' como operando");

    verificar(monta(".space 4\n") == 4, ".space reserva palavras");
    verificar(monta("loadi r0, 1 ; minusculas\n") == 3, "aceita minusculas");

    verificar(monta("JMP nada\n") < 0 && strstr(erro, "rotulo nao definido") != NULL,
              "erro: rotulo nao definido");
    verificar(monta("FOO R1\n") < 0 && strstr(erro, "instrucao desconhecida") != NULL,
              "erro: instrucao desconhecida");
    verificar(monta("ADD R1\n") < 0 && strstr(erro, "espera 2 operando") != NULL,
              "erro: numero de operandos");
    verificar(monta("MOV R1, R9\n") < 0 && strstr(erro, "registrador") != NULL,
              "erro: registrador inexistente");
    verificar(monta("a: NOP\na: NOP\n") < 0 && strstr(erro, "duas vezes") != NULL,
              "erro: rotulo repetido");
    verificar(monta("NOP\n\nLOADI R1, 12x\n") < 0 && strstr(erro, "linha 3") != NULL,
              "erro informa o numero da linha");
    verificar(monta("; so comentario\n") < 0, "erro: programa vazio");
}

/* ------------------------------------------------------------- memória */

static void testar_memoria(void)
{
    struct TabelaPaginas tp;
    int valor = 0;

    printf("Memoria\n");
    memoria_iniciar();
    memoria_iniciar_tabela(&tp, 2);

    verificar(memoria_traduzir(&tp, 10) == MEMORIA_AUSENTE, "pagina valida sem quadro = AUSENTE");
    verificar(memoria_traduzir(&tp, 2 * TAM_PAGINA) == MEMORIA_VIOLACAO, "alem do limite = VIOLACAO");
    verificar(memoria_traduzir(&tp, -1) == MEMORIA_VIOLACAO, "endereco negativo = VIOLACAO");

    verificar(memoria_alocar_pagina(&tp, 1, 7) == 0, "pagina 1 recebe o primeiro quadro livre");
    verificar(memoria_traduzir(&tp, TAM_PAGINA + 5) == 5, "traducao: pagina 1 + 5 -> quadro 0 + 5");
    verificar(memoria_escrever(&tp, TAM_PAGINA + 5, 42) == MEMORIA_OK &&
              memoria_ler(&tp, TAM_PAGINA + 5, &valor) == MEMORIA_OK && valor == 42,
              "escrita e leitura pela tabela de paginas");

    memoria_liberar(&tp);
    verificar(memoria_quadros_livres() == NUM_QUADROS, "liberar devolve todos os quadros");
}

/* ---------------------------------------------------------------- vCPU */

static struct MaquinaVirtual vm;

/* Monta 'fonte' e prepara uma VM com todas as páginas presentes. */
static void preparar_vm(const char *fonte, int paginas)
{
    int tamanho = monta(fonte), i;

    memoria_iniciar();
    memset(&vm, 0, sizeof vm);
    vm.id = 1;
    memoria_iniciar_tabela(&vm.paginas, paginas);
    for (i = 0; i < paginas; i++)
        memoria_alocar_pagina(&vm.paginas, i, vm.id);
    for (i = 0; i < tamanho; i++)
        memoria_escrever(&vm.paginas, i, imagem[i]);
    vcpu_reiniciar(&vm.cpu, 0, paginas * TAM_PAGINA);
}

static void testar_vcpu(void)
{
    int motivo;

    printf("vCPU\n");

    preparar_vm("LOADI R0, 6\nLOADI R1, 7\nMUL R0, R1\nHLT\n", 1);
    motivo = vcpu_executar(&vm, 100, 0);
    verificar(motivo == SAIDA_HLT && vm.cpu.reg[0] == 42, "6 * 7 = 42 e para no HLT");

    preparar_vm("laco: JMP laco\n", 1);
    motivo = vcpu_executar(&vm, 10, 0);
    verificar(motivo == SAIDA_QUANTUM && vm.cpu.instrucoes_executadas == 10,
              "laco infinito devolve a CPU ao fim do quantum");

    preparar_vm("LOADI R0, 65\nOUT R0, 2\nHLT\n", 1);
    motivo = vcpu_executar(&vm, 100, 0);
    verificar(motivo == SAIDA_IO_OUT && vm.cpu.saida_porta == 2 && vm.cpu.saida_valor == 65,
              "OUT gera VM exit com porta e valor");

    preparar_vm("LOADI R1, 0\nDIV R0, R1\n", 1);
    verificar(vcpu_executar(&vm, 100, 0) == SAIDA_DIVISAO_ZERO, "divisao por zero vira VM exit");

    preparar_vm("LOAD R0, 64\n", 1);
    verificar(vcpu_executar(&vm, 100, 0) == SAIDA_VIOLACAO_MEMORIA &&
              vm.cpu.endereco_falha == 64 && vm.cpu.pc == 0,
              "acesso fora do limite: VIOLACAO, pc nao avanca");

    /* pilha numa página ausente: falta de página e a instrução é refeita */
    preparar_vm("LOADI R0, 9\nPUSH R0\nHLT\n", 1);
    vm.paginas.num_paginas = 2;             /* página 1 válida, mas ausente */
    vm.cpu.sp = 2 * TAM_PAGINA;             /* pilha no topo da página 1    */
    motivo = vcpu_executar(&vm, 100, 0);
    verificar(motivo == SAIDA_FALTA_PAGINA && vm.cpu.pc == 3 && vm.cpu.sp == 2 * TAM_PAGINA,
              "PUSH em pagina ausente: FALTA_PAGINA sem alterar sp nem pc");
    memoria_alocar_pagina(&vm.paginas, 1, vm.id);
    motivo = vcpu_executar(&vm, 100, 0);
    verificar(motivo == SAIDA_HLT && vm.cpu.sp == 2 * TAM_PAGINA - 1,
              "depois de alocar o quadro o PUSH e reexecutado com sucesso");

    /* convidado malicioso escreve código inválido e salta para ele */
    preparar_vm("LOADI R0, 999\nSTORE R0, 30\nJMP 30\n", 1);
    verificar(vcpu_executar(&vm, 100, 0) == SAIDA_INSTRUCAO_INVALIDA,
              "opcode inexistente criado em tempo de execucao e barrado");

    preparar_vm("LOADI R0, 9\nSTORE R0, 31\nJMP 30\n", 1);   /* MOV R9, ... */
    memoria_escrever(&vm.paginas, 30, OP_MOV);
    verificar(vcpu_executar(&vm, 100, 0) == SAIDA_INSTRUCAO_INVALIDA,
              "registrador R9 forjado na memoria e barrado");

    preparar_vm("LOADI R0, -2147483648\nLOADI R1, -1\nDIV R0, R1\nHLT\n", 1);
    verificar(vcpu_executar(&vm, 100, 0) == SAIDA_HLT && vm.cpu.reg[0] == INT_MIN,
              "INT_MIN / -1 nao derruba o hospedeiro");
}

int main(void)
{
    testar_montador();
    testar_memoria();
    testar_vcpu();

    printf("\n%d testes, %d falha(s)\n", total, falhas);
    return falhas == 0 ? 0 : 1;
}
