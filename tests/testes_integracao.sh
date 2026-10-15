#!/bin/sh
# testes_integracao.sh - Executa o hypervisor de verdade e confere a saída.
#
# Uso: sh tests/testes_integracao.sh [caminho/do/executavel]

BIN="${1:-./build/hypervisor}"
TOTAL=0
FALHAS=0

# confere "descrição" "texto esperado" argumentos...
confere() {
    descricao="$1"
    esperado="$2"
    shift 2
    TOTAL=$((TOTAL + 1))
    if "$BIN" "$@" 2>&1 | grep -F -q -- "$esperado"; then
        echo "  ok    $descricao"
    else
        echo "  FALHA $descricao (esperava: $esperado)"
        FALHAS=$((FALHAS + 1))
    fi
}

# nao_contem "descrição" "texto proibido" argumentos...
nao_contem() {
    descricao="$1"
    proibido="$2"
    shift 2
    TOTAL=$((TOTAL + 1))
    if "$BIN" "$@" 2>&1 | grep -F -q -- "$proibido"; then
        echo "  FALHA $descricao (apareceu: $proibido)"
        FALHAS=$((FALHAS + 1))
    else
        echo "  ok    $descricao"
    fi
}

echo "Integracao ($BIN)"

confere "contador chega a 10"            "[VM1 contador  ] 10"             contador
confere "fatorial de 10"                 "10! = 3628800"                   fatorial
confere "primos ate 50"                  "2 3 5 7 11 13 17 19 23 29 31 37 41 43 47" primos
confere "fibonacci"                      "144 233 377"                     fibonacci
confere "bubble sort"                    "Depois: 3 7 8 11 19 25 30 42"    ordena
confere "OUT e HCALL no mesmo programa"  "Eu sou a VM numero 1"            ola
confere "dorminhoco acorda"              "Acordei pela ultima vez"         dorminhoco
confere "pilha alocada sob demanda"      "falta de pagina"                 fatorial
confere "invasor e abortado"             "violacao de memoria no endereco 3000" invasor
nao_contem "isolamento preservado"       "ISOLAMENTO FALHOU"               invasor contador
confere "outra VM sobrevive ao invasor"  "VM2 (contador) finalizada"       invasor contador
confere "divisao por zero abortada"      "divisao por zero"                divzero
confere "watchdog encerra o guloso"      "watchdog: atingiu o limite de 500" -m 500 guloso
confere "guloso nao trava os outros"     "VM2 (contador) finalizada"       -m 5000 guloso contador
confere "memoria toda devolvida"         "quadros livres: 64 de 64"        ordena fatorial
confere "modo verboso mostra VM exits"   "VM exit da VM1: IO_OUT"          -v contador
confere "rastreamento mostra instrucoes" "LOADI  R0, 1"                    -t contador
confere "demonstracao padrao termina"    "Todas as VMs terminaram"
confere "programa desconhecido"          "Programa desconhecido"           nao_existe

echo
echo "$TOTAL testes, $FALHAS falha(s)"
[ "$FALHAS" -eq 0 ]
