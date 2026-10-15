/*
 * montador.h - Montador (assembler) dos programas convidados.
 *
 * Converte o texto em assembly de um programa convidado na "imagem"
 * numérica que será copiada para a memória da VM (Etapa 5).
 */
#ifndef MONTADOR_H
#define MONTADOR_H

/*
 * Monta 'fonte' em 'imagem' (no máximo 'capacidade' palavras).
 * Devolve o tamanho da imagem em palavras, ou -1 em caso de erro
 * (a mensagem fica em 'erro').
 */
int montar(const char *fonte, int *imagem, int capacidade, char *erro, int tam_erro);

#endif
