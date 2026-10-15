#!/bin/sh
# compilar.sh - Gera o executável sem precisar de make (Linux e macOS).
#
# Uso: sh compilar.sh
# Resultado: build/hypervisor  (um único arquivo, pronto para rodar)

set -e
mkdir -p build

FLAGS="-std=c99 -Wall -Wextra -pedantic -O2"

# No Linux tentamos gerar um binário ESTÁTICO: ele não depende de nenhuma
# biblioteca instalada e roda copiado para qualquer outra máquina Linux.
if [ "$(uname -s)" = "Linux" ] && cc $FLAGS -static -o build/hypervisor src/*.c 2>/dev/null; then
    echo "Executavel estatico gerado: build/hypervisor"
else
    cc $FLAGS -o build/hypervisor src/*.c
    echo "Executavel gerado: build/hypervisor"
fi

echo "Execute com: ./build/hypervisor"
