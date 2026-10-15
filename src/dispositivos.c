/*
 * dispositivos.c - Console virtual (um por VM).
 */
#include <stdio.h>

#include "dispositivos.h"
#include "vm.h"

static void imprimir_linha(struct MaquinaVirtual *vm)
{
    vm->console.linha[vm->console.tamanho] = '\0';
    printf("  [VM%d %-10s] %s\n", vm->id, vm->nome, vm->console.linha);
    vm->console.tamanho = 0;
}

void console_escrever_char(struct MaquinaVirtual *vm, int c)
{
    if (c == '\n') {
        imprimir_linha(vm);
        return;
    }
    if (c == '\t')
        c = ' ';
    if (c < 32 || c > 126)   /* aceita apenas ASCII imprimível */
        c = '?';

    vm->console.linha[vm->console.tamanho] = (char)c;
    vm->console.tamanho++;

    if (vm->console.tamanho == TAM_LINHA_CONSOLE - 1)   /* buffer cheio */
        imprimir_linha(vm);
}

void console_escrever_numero(struct MaquinaVirtual *vm, int n)
{
    char texto[16];
    int i;

    snprintf(texto, sizeof texto, "%d", n);
    for (i = 0; texto[i] != '\0'; i++)
        console_escrever_char(vm, texto[i]);
}

/* Imprime o que sobrou no buffer (usado quando a VM termina no meio de uma linha). */
void console_descarregar(struct MaquinaVirtual *vm)
{
    if (vm->console.tamanho > 0)
        imprimir_linha(vm);
}
