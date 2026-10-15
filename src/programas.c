/*
 * programas.c - Código-fonte (assembly) dos programas convidados.
 *
 * Cada programa demonstra um conceito. Para ver o fonte de um deles
 * sem abrir este arquivo:   ./hypervisor -s nome
 */
#include <string.h>

#include "programas.h"

/* ------------------------------------------------------------------ */
static const char FONTE_OLA[] =
    "; ola - imprime texto de duas formas diferentes:\n"
    ";   1) caractere a caractere com OUT: CADA OUT gera uma VM exit\n"
    ";      (virtualizacao total, trap-and-emulate);\n"
    ";   2) a linha inteira com UMA unica HCALL (paravirtualizacao).\n"
    "        LOADI R1, msg          ; R1 = endereco da mensagem\n"
    "laco:   LOADR R0, R1           ; R0 = memoria[R1]\n"
    "        CMPI  R0, 0            ; chegou no 0 do fim da string?\n"
    "        JEQ   fim_laco\n"
    "        OUT   R0, 2            ; porta 2 = console (caractere)\n"
    "        ADDI  R1, 1            ; proximo caractere\n"
    "        JMP   laco\n"
    "fim_laco:\n"
    "        IN    R2, 3            ; porta 3 = numero desta VM\n"
    "        LOADI R0, txt_id\n"
    "        HCALL 0                ; hypercall 0 = imprimir texto em R0\n"
    "        OUT   R2, 1            ; porta 1 = console (numero)\n"
    "        LOADI R0, '\\n'\n"
    "        OUT   R0, 2\n"
    "        HLT\n"
    "msg:    .string \"Ola! Esta linha saiu com OUT, um caractere por vez.\\n\"\n"
    "txt_id: .string \"Esta saiu com uma unica HCALL. Eu sou a VM numero \"\n";

/* ------------------------------------------------------------------ */
static const char FONTE_CONTADOR[] =
    "; contador - imprime os numeros de 1 a 10, um por linha.\n"
    "; Rodando junto com outras VMs, mostra a CPU sendo compartilhada.\n"
    "        LOADI R0, 1            ; R0 = contador\n"
    "        LOADI R1, '\\n'\n"
    "laco:   OUT   R0, 1            ; imprime o numero\n"
    "        OUT   R1, 2            ; quebra de linha\n"
    "        ADDI  R0, 1\n"
    "        CMPI  R0, 10\n"
    "        JGT   fim              ; passou de 10? termina\n"
    "        JMP   laco\n"
    "fim:    HLT\n";

/* ------------------------------------------------------------------ */
static const char FONTE_FATORIAL[] =
    "; fatorial - calcula n! para n = 1..10 com uma sub-rotina RECURSIVA.\n"
    "; Usa CALL/RET/PUSH/POP: a pilha cresce para baixo, numa pagina que o\n"
    "; hypervisor so aloca no primeiro uso (falta de pagina).\n"
    "        LOADI R4, 1            ; n = 1\n"
    "proximo:\n"
    "        MOV   R0, R4\n"
    "        CALL  fatorial         ; R1 = fatorial(R0)\n"
    "        OUT   R4, 1\n"
    "        LOADI R0, txt_igual\n"
    "        HCALL 0\n"
    "        OUT   R1, 1\n"
    "        LOADI R0, '\\n'\n"
    "        OUT   R0, 2\n"
    "        ADDI  R4, 1\n"
    "        CMPI  R4, 10\n"
    "        JGT   fim\n"
    "        JMP   proximo\n"
    "fim:    HLT\n"
    "\n"
    "; fatorial(R0) -> R1\n"
    "fatorial:\n"
    "        CMPI  R0, 1\n"
    "        JGT   recursao         ; n > 1 ?\n"
    "        LOADI R1, 1            ; caso base: 1! = 1\n"
    "        RET\n"
    "recursao:\n"
    "        PUSH  R0               ; guarda n na pilha\n"
    "        ADDI  R0, -1\n"
    "        CALL  fatorial         ; R1 = (n-1)!\n"
    "        POP   R0               ; recupera n\n"
    "        MUL   R1, R0           ; R1 = n * (n-1)!\n"
    "        RET\n"
    "txt_igual: .string \"! = \"\n";

/* ------------------------------------------------------------------ */
static const char FONTE_PRIMOS[] =
    "; primos - imprime os numeros primos ate 50 (divisoes sucessivas).\n"
    "        LOADI R0, 2            ; R0 = candidato\n"
    "candidato:\n"
    "        CMPI  R0, 50\n"
    "        JGT   fim\n"
    "        LOADI R1, 2            ; R1 = divisor\n"
    "divisor:\n"
    "        MOV   R2, R1\n"
    "        MUL   R2, R1           ; R2 = divisor * divisor\n"
    "        CMP   R2, R0\n"
    "        JGT   primo            ; divisor^2 > candidato: e primo\n"
    "        MOV   R3, R0\n"
    "        MOD   R3, R1           ; R3 = candidato % divisor\n"
    "        CMPI  R3, 0\n"
    "        JEQ   nao_primo\n"
    "        ADDI  R1, 1\n"
    "        JMP   divisor\n"
    "primo:  OUT   R0, 1\n"
    "        LOADI R3, ' '\n"
    "        OUT   R3, 2\n"
    "nao_primo:\n"
    "        ADDI  R0, 1\n"
    "        JMP   candidato\n"
    "fim:    LOADI R3, '\\n'\n"
    "        OUT   R3, 2\n"
    "        HLT\n";

/* ------------------------------------------------------------------ */
static const char FONTE_FIBONACCI[] =
    "; fibonacci - imprime os 15 primeiros termos usando duas variaveis\n"
    "; guardadas na memoria da VM (LOAD/STORE com enderecamento direto).\n"
    "        LOADI R2, 15           ; termos que faltam\n"
    "        LOADI R3, ' '\n"
    "laco:   LOAD  R0, a            ; R0 = a\n"
    "        LOAD  R1, b            ; R1 = b\n"
    "        OUT   R0, 1\n"
    "        OUT   R3, 2\n"
    "        MOV   R4, R0\n"
    "        ADD   R4, R1           ; R4 = a + b\n"
    "        STORE R1, a            ; a = b\n"
    "        STORE R4, b            ; b = a + b\n"
    "        ADDI  R2, -1\n"
    "        CMPI  R2, 0\n"
    "        JGT   laco\n"
    "        LOADI R3, '\\n'\n"
    "        OUT   R3, 2\n"
    "        HLT\n"
    "a:      .word 0\n"
    "b:      .word 1\n";

/* ------------------------------------------------------------------ */
static const char FONTE_ORDENA[] =
    "; ordena - Bubble Sort de um vetor guardado na memoria da VM.\n"
    "; Usa enderecamento indireto (LOADR/STORER) e sub-rotina (CALL/RET).\n"
    "        LOADI R0, txt_antes\n"
    "        HCALL 0\n"
    "        CALL  imprimir\n"
    "passada:\n"
    "        LOADI R6, 0            ; trocou = 0\n"
    "        LOADI R1, vetor        ; R1 = &vetor[i-1]\n"
    "        LOADI R4, 1            ; i = 1\n"
    "interno:\n"
    "        LOAD  R5, tamanho\n"
    "        CMP   R4, R5\n"
    "        JEQ   fim_passada      ; i == tamanho: fim da passada\n"
    "        MOV   R7, R1\n"
    "        ADDI  R7, 1            ; R7 = &vetor[i]\n"
    "        LOADR R2, R1           ; x = vetor[i-1]\n"
    "        LOADR R3, R7           ; y = vetor[i]\n"
    "        CMP   R2, R3\n"
    "        JGT   trocar           ; x > y: fora de ordem\n"
    "        JMP   avancar\n"
    "trocar: STORER R3, R1          ; vetor[i-1] = y\n"
    "        STORER R2, R7          ; vetor[i]   = x\n"
    "        LOADI R6, 1            ; trocou = 1\n"
    "avancar:\n"
    "        ADDI  R1, 1\n"
    "        ADDI  R4, 1\n"
    "        JMP   interno\n"
    "fim_passada:\n"
    "        CMPI  R6, 1\n"
    "        JEQ   passada          ; houve troca: faz outra passada\n"
    "        LOADI R0, txt_depois\n"
    "        HCALL 0\n"
    "        CALL  imprimir\n"
    "        HLT\n"
    "\n"
    "; imprimir: escreve o vetor inteiro numa linha (usa R1..R5)\n"
    "imprimir:\n"
    "        LOADI R1, vetor\n"
    "        LOADI R4, 0\n"
    "        LOAD  R5, tamanho\n"
    "        LOADI R3, ' '\n"
    "imp_laco:\n"
    "        CMP   R4, R5\n"
    "        JEQ   imp_fim\n"
    "        LOADR R2, R1\n"
    "        OUT   R2, 1\n"
    "        OUT   R3, 2\n"
    "        ADDI  R1, 1\n"
    "        ADDI  R4, 1\n"
    "        JMP   imp_laco\n"
    "imp_fim:\n"
    "        LOADI R3, '\\n'\n"
    "        OUT   R3, 2\n"
    "        RET\n"
    "tamanho:    .word 8\n"
    "vetor:      .word 42, 7, 19, 3, 25, 11, 8, 30\n"
    "txt_antes:  .string \"Antes:  \"\n"
    "txt_depois: .string \"Depois: \"\n";

/* ------------------------------------------------------------------ */
static const char FONTE_DORMINHOCO[] =
    "; dorminhoco - imprime e depois DORME (hypercall 2), tres vezes.\n"
    "; Enquanto dorme a VM fica BLOQUEADA e nao recebe CPU.\n"
    "        LOADI R4, 1            ; rodada\n"
    "laco:   LOADI R0, txt_rodada\n"
    "        HCALL 0\n"
    "        OUT   R4, 1\n"
    "        LOADI R0, txt_relogio\n"
    "        HCALL 0\n"
    "        IN    R5, 4            ; porta 4 = relogio do hypervisor\n"
    "        OUT   R5, 1\n"
    "        LOADI R0, '\\n'\n"
    "        OUT   R0, 2\n"
    "        LOADI R0, 400          ; dormir 400 ticks\n"
    "        HCALL 2\n"
    "        ADDI  R4, 1\n"
    "        CMPI  R4, 3\n"
    "        JGT   fim\n"
    "        JMP   laco\n"
    "fim:    LOADI R0, txt_fim\n"
    "        HCALL 0\n"
    "        HLT\n"
    "txt_rodada:  .string \"Rodada \"\n"
    "txt_relogio: .string \", relogio = \"\n"
    "txt_fim:     .string \"Acordei pela ultima vez. Tchau!\\n\"\n";

/* ------------------------------------------------------------------ */
static const char FONTE_INVASOR[] =
    "; invasor - tenta escrever FORA do proprio espaco de enderecamento.\n"
    "; A traducao de endereco falha, o hypervisor aborta SO esta VM e as\n"
    "; demais continuam executando normalmente (isolamento).\n"
    "        LOADI R0, txt_aviso\n"
    "        HCALL 0\n"
    "        LOADI R1, 666\n"
    "        LOADI R2, 3000         ; endereco muito alem do limite da VM\n"
    "        STORER R1, R2          ; memoria[3000] = 666 -> VIOLACAO\n"
    "        LOADI R0, txt_falhou\n"
    "        HCALL 0\n"
    "        HLT\n"
    "txt_aviso:  .string \"Vou tentar escrever no endereco 3000...\\n\"\n"
    "txt_falhou: .string \"ISOLAMENTO FALHOU: escrevi fora da minha memoria!\\n\"\n";

/* ------------------------------------------------------------------ */
static const char FONTE_DIVZERO[] =
    "; divzero - provoca uma excecao de divisao por zero.\n"
    "; Num processo comum isso seria um sinal (SIGFPE); aqui vira VM exit.\n"
    "        LOADI R0, txt\n"
    "        HCALL 0\n"
    "        LOADI R1, 10\n"
    "        LOADI R2, 0\n"
    "        DIV   R1, R2           ; excecao!\n"
    "        HLT\n"
    "txt:    .string \"Vou dividir 10 por 0...\\n\"\n";

/* ------------------------------------------------------------------ */
static const char FONTE_GULOSO[] =
    "; guloso - laco infinito tentando monopolizar a CPU.\n"
    "; A preempcao por quantum impede que ele trave as outras VMs e o\n"
    "; watchdog (limite de instrucoes) acaba encerrando-o.\n"
    "        LOADI R0, txt\n"
    "        HCALL 0\n"
    "laco:   ADDI  R1, 1\n"
    "        JMP   laco\n"
    "txt:    .string \"Vou monopolizar a CPU com um laco infinito!\\n\"\n";

/* ------------------------------------------------------------------ */
const struct ProgramaConvidado PROGRAMAS[] = {
    { "ola",        "E/S por OUT (trap-and-emulate) x HCALL (paravirtualizacao)", FONTE_OLA },
    { "contador",   "conta de 1 a 10 (compartilhamento da CPU)",                 FONTE_CONTADOR },
    { "fatorial",   "recursao: pilha + falta de pagina sob demanda",             FONTE_FATORIAL },
    { "primos",     "primos ate 50 (aritmetica e desvios)",                       FONTE_PRIMOS },
    { "fibonacci",  "variaveis na memoria (LOAD/STORE)",                          FONTE_FIBONACCI },
    { "ordena",     "Bubble Sort (enderecamento indireto)",                       FONTE_ORDENA },
    { "dorminhoco", "HCALL de sono: estado BLOQUEADA",                            FONTE_DORMINHOCO },
    { "invasor",    "acesso fora da memoria: isolamento entre VMs",               FONTE_INVASOR },
    { "divzero",    "excecao de divisao por zero",                                FONTE_DIVZERO },
    { "guloso",     "laco infinito: preempcao e watchdog",                        FONTE_GULOSO },
};

const int NUM_PROGRAMAS = (int)(sizeof PROGRAMAS / sizeof PROGRAMAS[0]);

const struct ProgramaConvidado *programa_buscar(const char *nome)
{
    int i;

    for (i = 0; i < NUM_PROGRAMAS; i++) {
        if (strcmp(PROGRAMAS[i].nome, nome) == 0)
            return &PROGRAMAS[i];
    }
    return NULL;
}
