/*
 * montador.c - Montador de DOIS PASSOS.
 *
 * Formato de cada linha (tudo é opcional):
 *
 *     rotulo:   INSTRUCAO  operando, operando   ; comentário
 *
 * Diretivas de dados:
 *     .word  1, 2, 3        uma palavra para cada valor
 *     .string "texto\n"     um caractere por palavra + 0 no final
 *     .space 10             reserva 10 palavras com zero
 *
 * Operandos: R0..R7, números (10, -3), caracteres ('A', '\n') e rótulos.
 *
 * Por que dois passos? Um rótulo pode ser usado ANTES de ser definido
 * (ex.: "JEQ fim" com "fim:" mais abaixo). No 1º passo só calculamos o
 * endereço de cada rótulo; no 2º passo geramos a imagem de verdade.
 */
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "montador.h"
#include "isa.h"
#include "config.h"

#define MAX_ROTULOS    64
#define MAX_LINHA     160
#define MAX_OPERANDOS  16
#define TAM_ROTULO     24

struct Rotulo {
    char nome[TAM_ROTULO];
    int endereco;
};

struct Montador {
    struct Rotulo rotulos[MAX_ROTULOS];
    int num_rotulos;
    int *imagem;
    int capacidade;
    int posicao;    /* próximo endereço a ser preenchido */
    int passo;      /* 1 = só calcula rótulos, 2 = gera a imagem */
    int linha;      /* linha atual, para as mensagens de erro */
    char *erro;
    int tam_erro;
};

/* Registra a mensagem de erro e devolve 0 (falha). */
static int falhar(struct Montador *m, const char *mensagem, const char *detalhe)
{
    snprintf(m->erro, m->tam_erro, "linha %d: %s '%s'", m->linha, mensagem, detalhe);
    return 0;
}

/* ---------------------------------------------------------------- texto */

/*
 * Copia a próxima linha de 'fonte' para 'linha' e avança o cursor.
 * Devolve 0 no fim do texto, 1 se leu uma linha, 2 se a linha era longa demais.
 */
static int ler_linha(const char **cursor, char *linha, int tam)
{
    const char *p = *cursor;
    int n = 0, longa = 0;

    if (*p == '\0')
        return 0;
    while (*p != '\0' && *p != '\n') {
        if (n < tam - 1)
            linha[n++] = *p;
        else
            longa = 1;
        p++;
    }
    if (*p == '\n')
        p++;
    linha[n] = '\0';
    *cursor = p;
    return longa ? 2 : 1;
}

/* Corta o comentário (';'), ignorando ';' dentro de '...' ou "...". */
static void remover_comentario(char *s)
{
    char aspas = 0;
    int i;

    for (i = 0; s[i] != '\0'; i++) {
        if (aspas) {
            if (s[i] == '\\' && s[i + 1] != '\0')
                i++;                       /* pula o caractere escapado */
            else if (s[i] == aspas)
                aspas = 0;
        } else if (s[i] == '\'' || s[i] == '"') {
            aspas = s[i];
        } else if (s[i] == ';') {
            s[i] = '\0';
            return;
        }
    }
}

/* Remove espaços do início e do fim. */
static char *aparar(char *s)
{
    int fim;

    while (isspace((unsigned char)*s))
        s++;
    fim = (int)strlen(s);
    while (fim > 0 && isspace((unsigned char)s[fim - 1]))
        s[--fim] = '\0';
    return s;
}

/* Separa a primeira palavra de 's'. O restante (já aparado) vai para 'resto'. */
static char *separar_palavra(char *s, char **resto)
{
    char *p = s;

    while (*p != '\0' && !isspace((unsigned char)*p))
        p++;
    if (*p != '\0') {
        *p = '\0';
        p++;
    }
    *resto = aparar(p);
    return s;
}

/*
 * Divide "a, b, c" em operandos (respeitando vírgulas dentro de aspas).
 * Devolve quantos encontrou, ou -1 se passar de 'max'.
 */
static int dividir_operandos(char *s, char *ops[], int max)
{
    char aspas = 0;
    char *inicio = s;
    int n = 0, i;

    if (*s == '\0')
        return 0;

    for (i = 0; ; i++) {
        char c = s[i];

        if (c == '\0' || (c == ',' && !aspas)) {
            if (n == max)
                return -1;
            s[i] = '\0';
            ops[n++] = aparar(inicio);
            if (c == '\0')
                return n;
            inicio = &s[i + 1];
        } else if (aspas) {
            if (c == '\\' && s[i + 1] != '\0')
                i++;
            else if (c == aspas)
                aspas = 0;
        } else if (c == '\'' || c == '"') {
            aspas = c;
        }
    }
}

/* Traduz o caractere depois da barra: \n, \t, \0, \\, \', \" */
static int ler_escape(char c)
{
    switch (c) {
    case 'n': return '\n';
    case 't': return '\t';
    case '0': return 0;
    }
    return (unsigned char)c;
}

/* ------------------------------------------------------------ operandos */

static int nome_valido(const char *s)
{
    int i;

    if (!isalpha((unsigned char)s[0]) && s[0] != '_')
        return 0;
    for (i = 1; s[i] != '\0'; i++) {
        if (!isalnum((unsigned char)s[i]) && s[i] != '_')
            return 0;
    }
    return i < TAM_ROTULO;
}

static int buscar_rotulo(const struct Montador *m, const char *nome)
{
    int i;

    for (i = 0; i < m->num_rotulos; i++) {
        if (strcmp(m->rotulos[i].nome, nome) == 0)
            return i;
    }
    return -1;
}

static int definir_rotulo(struct Montador *m, const char *nome)
{
    if (!nome_valido(nome))
        return falhar(m, "nome de rotulo invalido", nome);
    if (m->passo == 2)
        return 1;                       /* já foi registrado no 1º passo */
    if (buscar_rotulo(m, nome) >= 0)
        return falhar(m, "rotulo definido duas vezes", nome);
    if (m->num_rotulos == MAX_ROTULOS)
        return falhar(m, "rotulos demais", nome);

    strcpy(m->rotulos[m->num_rotulos].nome, nome);
    m->rotulos[m->num_rotulos].endereco = m->posicao;
    m->num_rotulos++;
    return 1;
}

/* "R0".."R7" -> 0..7 */
static int ler_registrador(const char *t, int *reg)
{
    if ((t[0] == 'R' || t[0] == 'r') && isdigit((unsigned char)t[1]) && t[2] == '\0') {
        int r = t[1] - '0';
        if (r < NUM_REGISTRADORES) {
            *reg = r;
            return 1;
        }
    }
    return 0;
}

/* 'A' ou '\n' -> código do caractere */
static int ler_caractere(const char *t, int *valor)
{
    if (t[1] == '\\' && t[2] != '\0' && t[3] == '\'' && t[4] == '\0') {
        *valor = ler_escape(t[2]);
        return 1;
    }
    if (t[1] != '\0' && t[1] != '\\' && t[2] == '\'' && t[3] == '\0') {
        *valor = (unsigned char)t[1];
        return 1;
    }
    return 0;
}

/* Número, caractere ou rótulo -> valor inteiro */
static int ler_valor(struct Montador *m, const char *t, int *valor)
{
    if (t[0] == '\'') {
        if (!ler_caractere(t, valor))
            return falhar(m, "caractere invalido", t);
        return 1;
    }

    if (isdigit((unsigned char)t[0]) ||
        ((t[0] == '-' || t[0] == '+') && isdigit((unsigned char)t[1]))) {
        char *fim;
        long long n = strtoll(t, &fim, 10);
        if (*fim != '\0')
            return falhar(m, "numero invalido", t);
        if (n < INT_MIN || n > INT_MAX)
            return falhar(m, "numero fora do intervalo de int", t);
        *valor = (int)n;
        return 1;
    }

    if (nome_valido(t)) {
        int r;
        if (m->passo == 1) {            /* endereço ainda pode ser desconhecido */
            *valor = 0;
            return 1;
        }
        r = buscar_rotulo(m, t);
        if (r < 0)
            return falhar(m, "rotulo nao definido", t);
        *valor = m->rotulos[r].endereco;
        return 1;
    }

    return falhar(m, "operando invalido", t);
}

static int ler_operando(struct Montador *m, int tipo, const char *t, int *valor)
{
    if (tipo == OPERANDO_REG) {
        if (!ler_registrador(t, valor))
            return falhar(m, "esperava um registrador (R0..R7)", t);
        return 1;
    }
    return ler_valor(m, t, valor);
}

/* ------------------------------------------------------------- geração */

/* Coloca uma palavra na imagem (no 1º passo só conta o espaço). */
static int emitir(struct Montador *m, int valor)
{
    if (m->posicao >= m->capacidade) {
        snprintf(m->erro, m->tam_erro, "linha %d: programa grande demais "
                 "(limite de %d palavras)", m->linha, m->capacidade);
        return 0;
    }
    if (m->passo == 2)
        m->imagem[m->posicao] = valor;
    m->posicao++;
    return 1;
}

static int montar_instrucao(struct Montador *m, int op, char *operandos)
{
    const struct InfoInstrucao *info = &TABELA_INSTRUCOES[op];
    char *ops[MAX_OPERANDOS];
    int esperados = 0, n, a = 0, b = 0;

    if (info->tipo_a != OPERANDO_NENHUM) esperados++;
    if (info->tipo_b != OPERANDO_NENHUM) esperados++;

    n = dividir_operandos(operandos, ops, MAX_OPERANDOS);
    if (n != esperados) {
        snprintf(m->erro, m->tam_erro, "linha %d: %s espera %d operando(s)",
                 m->linha, info->nome, esperados);
        return 0;
    }

    if (esperados >= 1 && !ler_operando(m, info->tipo_a, ops[0], &a))
        return 0;
    if (esperados == 2 && !ler_operando(m, info->tipo_b, ops[1], &b))
        return 0;

    return emitir(m, op) && emitir(m, a) && emitir(m, b);
}

static int montar_word(struct Montador *m, char *operandos)
{
    char *ops[MAX_OPERANDOS];
    int n, i, valor;

    n = dividir_operandos(operandos, ops, MAX_OPERANDOS);
    if (n <= 0)
        return falhar(m, "use de 1 a 16 valores em", ".word");
    for (i = 0; i < n; i++) {
        if (!ler_valor(m, ops[i], &valor) || !emitir(m, valor))
            return 0;
    }
    return 1;
}

static int montar_space(struct Montador *m, char *operandos)
{
    char *ops[MAX_OPERANDOS];
    int n, i, quantidade;

    n = dividir_operandos(operandos, ops, MAX_OPERANDOS);
    if (n != 1)
        return falhar(m, "use exatamente um valor em", ".space");
    if (!ler_valor(m, ops[0], &quantidade))
        return 0;
    if (quantidade < 0)
        return falhar(m, "quantidade negativa em", ".space");
    for (i = 0; i < quantidade; i++) {
        if (!emitir(m, 0))
            return 0;
    }
    return 1;
}

static int montar_string(struct Montador *m, const char *texto)
{
    int tam = (int)strlen(texto);
    int i, c;

    if (tam < 2 || texto[0] != '"' || texto[tam - 1] != '"')
        return falhar(m, "a string deve estar entre aspas", texto);

    for (i = 1; i < tam - 1; i++) {
        c = (unsigned char)texto[i];
        if (c == '\\' && i + 1 < tam - 1) {
            i++;
            c = ler_escape(texto[i]);
        }
        if (!emitir(m, c))
            return 0;
    }
    return emitir(m, 0);              /* terminador, como em C */
}

static int montar_linha(struct Montador *m, char *linha)
{
    char *texto, *palavra, *resto;
    char mnemonico[16];
    int tam, i, op;

    remover_comentario(linha);
    texto = aparar(linha);
    if (*texto == '\0')
        return 1;                     /* linha vazia ou só comentário */

    palavra = separar_palavra(texto, &resto);

    /* rótulo? */
    tam = (int)strlen(palavra);
    if (palavra[tam - 1] == ':') {
        palavra[tam - 1] = '\0';
        if (!definir_rotulo(m, palavra))
            return 0;
        if (*resto == '\0')
            return 1;                 /* linha só com o rótulo */
        palavra = separar_palavra(resto, &resto);
    }

    /* diretiva? */
    if (palavra[0] == '.') {
        if (strcmp(palavra, ".word") == 0)   return montar_word(m, resto);
        if (strcmp(palavra, ".space") == 0)  return montar_space(m, resto);
        if (strcmp(palavra, ".string") == 0) return montar_string(m, resto);
        return falhar(m, "diretiva desconhecida", palavra);
    }

    /* instrução: aceita maiúsculas ou minúsculas */
    for (i = 0; palavra[i] != '\0' && i < (int)sizeof mnemonico - 1; i++)
        mnemonico[i] = (char)toupper((unsigned char)palavra[i]);
    mnemonico[i] = '\0';

    op = isa_buscar_opcode(mnemonico);
    if (op < 0)
        return falhar(m, "instrucao desconhecida", palavra);
    return montar_instrucao(m, op, resto);
}

int montar(const char *fonte, int *imagem, int capacidade, char *erro, int tam_erro)
{
    struct Montador m;
    char linha[MAX_LINHA];
    const char *cursor;
    int lida;

    m.num_rotulos = 0;
    m.imagem = imagem;
    m.capacidade = capacidade;
    m.erro = erro;
    m.tam_erro = tam_erro;

    for (m.passo = 1; m.passo <= 2; m.passo++) {
        m.posicao = 0;
        m.linha = 0;
        cursor = fonte;

        while ((lida = ler_linha(&cursor, linha, sizeof linha)) != 0) {
            m.linha++;
            if (lida == 2) {
                snprintf(erro, tam_erro, "linha %d: linha longa demais", m.linha);
                return -1;
            }
            if (!montar_linha(&m, linha))
                return -1;
        }
    }

    if (m.posicao == 0) {
        snprintf(erro, tam_erro, "programa vazio");
        return -1;
    }
    return m.posicao;
}
