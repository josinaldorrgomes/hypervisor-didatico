# Referência rápida da ISA

## Máquina

- **Registradores:** `R0`..`R7` (inteiros), `pc`, `sp` e `flag`.
- **Memória:** palavras (`int`), endereçadas a partir de 0. Páginas de 64 palavras.
- **Instrução:** 3 palavras, `[opcode, A, B]`.
- **Início:** `pc = 0`, `sp = topo do espaço` (a pilha cresce para baixo).

## Instruções

| Opcode | Instrução | Efeito | Observação |
|---|---|---|---|
| 0 | `NOP` | nada | |
| 1 | `LOADI Ra, v` | `Ra = v` | `v`: número, caractere ou rótulo |
| 2 | `LOAD Ra, end` | `Ra = mem[end]` | endereçamento direto |
| 3 | `STORE Ra, end` | `mem[end] = Ra` | |
| 4 | `LOADR Ra, Rb` | `Ra = mem[Rb]` | endereçamento indireto |
| 5 | `STORER Ra, Rb` | `mem[Rb] = Ra` | |
| 6 | `MOV Ra, Rb` | `Ra = Rb` | |
| 7 | `ADD Ra, Rb` | `Ra = Ra + Rb` | |
| 8 | `ADDI Ra, v` | `Ra = Ra + v` | use `v` negativo para subtrair |
| 9 | `SUB Ra, Rb` | `Ra = Ra - Rb` | |
| 10 | `MUL Ra, Rb` | `Ra = Ra * Rb` | |
| 11 | `DIV Ra, Rb` | `Ra = Ra / Rb` | `Rb = 0` → VM exit `DIVISAO_ZERO` |
| 12 | `MOD Ra, Rb` | `Ra = Ra % Rb` | idem |
| 13 | `CMP Ra, Rb` | `flag = -1, 0 ou 1` | compara `Ra` com `Rb` |
| 14 | `CMPI Ra, v` | `flag = -1, 0 ou 1` | compara `Ra` com `v` |
| 15 | `JMP end` | `pc = end` | |
| 16 | `JEQ end` | se `flag == 0` | igual |
| 17 | `JNE end` | se `flag != 0` | diferente |
| 18 | `JLT end` | se `flag < 0` | menor |
| 19 | `JGT end` | se `flag > 0` | maior |
| 20 | `PUSH Ra` | `sp--; mem[sp] = Ra` | |
| 21 | `POP Ra` | `Ra = mem[sp]; sp++` | |
| 22 | `CALL end` | `PUSH pc; pc = end` | |
| 23 | `RET` | `POP pc` | |
| 24 | **`IN Ra, porta`** | `Ra = dispositivo` | **sensível** → VM exit |
| 25 | **`OUT Ra, porta`** | `dispositivo = Ra` | **sensível** → VM exit |
| 26 | **`HLT`** | desliga a VM | **sensível** → VM exit |
| 27 | **`HCALL n`** | chamada ao hypervisor | **sensível** → VM exit |

## Portas de E/S

| Porta | Direção | Dispositivo |
|---|---|---|
| 1 | `OUT` | console: escreve `Ra` como número |
| 2 | `OUT` | console: escreve `Ra` como caractere (`'\n'` imprime a linha) |
| 3 | `IN` | número (id) desta VM |
| 4 | `IN` | relógio do hypervisor (instruções executadas no total) |
| outras | – | `OUT` é ignorado; `IN` devolve 0 |

## Hypercalls

| `n` | Nome | Entrada | Efeito |
|---|---|---|---|
| 0 | imprimir texto | `R0` = endereço da string terminada em 0 | imprime no console |
| 1 | ceder CPU | – | volta para a fila de prontas |
| 2 | dormir | `R0` = ticks | fica BLOQUEADA até `relógio + R0` |
| outro | – | – | `R0 = -1` |

## Sintaxe do montador

```asm
; comentário
rotulo:  INSTRUCAO A, B        ; maiúsculas ou minúsculas
         .word   10, -3, 'x'   ; palavras
         .string "Ola\n"       ; caracteres + 0 final (escapes: \n \t \0 \\ \" \')
         .space  8             ; 8 palavras zeradas
```

## VM exits

| Motivo | Causa | Ação do hypervisor |
|---|---|---|
| `QUANTUM` | fatia de tempo esgotada | VM → PRONTA |
| `IO_OUT` / `IO_IN` | `OUT` / `IN` | emula o dispositivo e continua |
| `HYPERCALL` | `HCALL` | executa o serviço |
| `FALTA_PAGINA` | página válida sem quadro | aloca o quadro e reexecuta |
| `HLT` | `HLT` | VM → FINALIZADA |
| `VIOLACAO_MEMORIA` | endereço fora do espaço | VM → ABORTADA |
| `INSTRUCAO_INVALIDA` | opcode ou registrador inexistente | VM → ABORTADA |
| `DIVISAO_ZERO` | `DIV`/`MOD` por 0 | VM → ABORTADA |
