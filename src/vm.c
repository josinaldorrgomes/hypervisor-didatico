/*
 * vm.c - Funções auxiliares do bloco de controle da VM.
 */
#include "vm.h"

const char *vm_nome_estado(int estado)
{
    switch (estado) {
    case VM_LIVRE:      return "LIVRE";
    case VM_PRONTA:     return "PRONTA";
    case VM_EXECUTANDO: return "EXECUTANDO";
    case VM_BLOQUEADA:  return "BLOQUEADA";
    case VM_FINALIZADA: return "FINALIZADA";
    case VM_ABORTADA:   return "ABORTADA";
    }
    return "?";
}
