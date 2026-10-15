/*
 * dispositivos.h - Dispositivos virtuais emulados pelo hypervisor.
 *
 * Cada VM tem o seu próprio console. O convidado acha que escreve num
 * dispositivo de verdade (via OUT ou HCALL); na prática o hypervisor
 * guarda os caracteres num buffer e, a cada linha completa, imprime no
 * terminal do hospedeiro com o nome da VM na frente.
 */
#ifndef DISPOSITIVOS_H
#define DISPOSITIVOS_H

#include "config.h"

struct Console {
    char linha[TAM_LINHA_CONSOLE];
    int tamanho;
};

struct MaquinaVirtual;

void console_escrever_char(struct MaquinaVirtual *vm, int c);
void console_escrever_numero(struct MaquinaVirtual *vm, int n);
void console_descarregar(struct MaquinaVirtual *vm);

#endif
