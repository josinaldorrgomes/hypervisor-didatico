/*
 * programas.h - Programas convidados embutidos no executável.
 *
 * Os programas ficam como texto dentro do próprio binário: nada é lido do
 * disco. Montagem, carga e execução acontecem inteiramente na memória.
 */
#ifndef PROGRAMAS_H
#define PROGRAMAS_H

struct ProgramaConvidado {
    const char *nome;
    const char *descricao;
    const char *fonte;       /* código em assembly da nossa ISA */
};

extern const struct ProgramaConvidado PROGRAMAS[];
extern const int NUM_PROGRAMAS;

const struct ProgramaConvidado *programa_buscar(const char *nome);

#endif
