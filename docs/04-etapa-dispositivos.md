# Etapa 4 – Dispositivos e hypercalls

**Arquivos:** `src/dispositivos.h`, `src/dispositivos.c`, mais as funções `emular_out`, `emular_in` e `tratar_hypercall` de `src/vmm.c`

## Objetivo

Dar às VMs um jeito de **conversar com o mundo exterior** sem que elas toquem no hardware real. Isso pode ser feito de duas formas, e ambas foram implementadas:

1. **Virtualização total:** o convidado usa `IN`/`OUT` como se existisse um dispositivo real. Cada acesso gera uma VM exit, e o hypervisor **emula** o dispositivo.
2. **Paravirtualização:** o convidado *sabe* que está numa VM e pede o serviço diretamente ao hypervisor com uma **hypercall** (`HCALL`).

## Conceitos da aula aplicados

- **E/S mapeada em portas** (*port-mapped I/O*): `IN`/`OUT` com número de porta.
- **Drivers e controladores de dispositivo**: o console virtual tem um *buffer* de linha.
- **Chamadas de sistema**: a hypercall está para o hypervisor como a *syscall* está para o kernel.
- **Custo de troca de modo**: cada trap custa caro, e menos traps significam mais desempenho.

## Passo 1 – O console virtual (`dispositivos.c`)

Cada VM tem um console próprio, com um buffer de uma linha:

```c
struct Console {
    char linha[TAM_LINHA_CONSOLE];
    int tamanho;
};
```

Os caracteres se acumulam no buffer. Quando chega um `'\n'`, o hypervisor imprime a linha inteira no terminal do hospedeiro, **identificando a VM**:

```c
void console_escrever_char(struct MaquinaVirtual *vm, int c)
{
    if (c == '\n') {                 /* fim de linha: imprime tudo */
        imprimir_linha(vm);
        return;
    }
    if (c < 32 || c > 126)           /* só ASCII imprimível */
        c = '?';
    vm->console.linha[vm->console.tamanho++] = (char)c;
    if (vm->console.tamanho == TAM_LINHA_CONSOLE - 1)   /* buffer cheio */
        imprimir_linha(vm);
}
```

Por que usar buffer? Sem ele, as saídas de várias VMs se misturariam caractere a caractere, porque a CPU troca de VM no meio da linha. É o mesmo motivo pelo qual o `stdout` do C tem buffer de linha.

O filtro `32..126` também é uma proteção: o convidado não consegue mandar **sequências de controle** (ESC, por exemplo) ao terminal do hospedeiro.

## Passo 2 – Emulando `OUT` e `IN` (*trap-and-emulate*)

Quando a vCPU devolve `SAIDA_IO_OUT`, o hypervisor olha a porta e decide o que fazer:

```c
static void emular_out(struct MaquinaVirtual *vm)
{
    switch (cpu->saida_porta) {
    case PORTA_CONSOLE_NUMERO: console_escrever_numero(vm, cpu->saida_valor); break;
    case PORTA_CONSOLE_CHAR:   console_escrever_char(vm, cpu->saida_valor);   break;
    default: /* porta sem dispositivo: a escrita se perde, como no hardware */ ;
    }
}
```

No caso de `IN`, é o hypervisor quem **inventa** o valor lido e o coloca no registrador do convidado:

```c
static void emular_in(struct MaquinaVirtual *vm)
{
    int valor = 0;
    switch (cpu->saida_porta) {
    case PORTA_ID_VM:   valor = vm->id;       break;
    case PORTA_RELOGIO: valor = (int)relogio; break;
    }
    cpu->reg[cpu->saida_reg] = valor;
}
```

Isso mostra o **poder** do hypervisor sobre o convidado: a VM acha que leu um relógio de hardware, mas recebe o que o VMM quiser, nesse caso o **relógio virtual** (instruções executadas), e não a hora real.

## Passo 3 – Hypercalls (paravirtualização)

`HCALL n` gera `SAIDA_HYPERCALL` com `saida_valor = n`. Os argumentos vão nos registradores, como numa syscall:

| Número | Nome | Argumento | Efeito |
|---|---|---|---|
| 0 | `HC_IMPRIMIR_TEXTO` | `R0` = endereço da string | imprime a string inteira |
| 1 | `HC_CEDER_CPU` | – | devolve a CPU antes do quantum acabar (`yield`) |
| 2 | `HC_DORMIR` | `R0` = ticks | a VM fica **BLOQUEADA** por `R0` ticks |
| outro | – | – | devolve `R0 = -1` (erro, como `ENOSYS`) |

A hypercall 0 mostra o hypervisor **lendo a memória do convidado**, sempre pela tabela de páginas da VM:

```c
static void hypercall_imprimir(struct MaquinaVirtual *vm)
{
    int endereco = vm->cpu.reg[0];
    for (i = 0; i < TAM_MAX_TEXTO; i++) {
        if (memoria_ler(&vm->paginas, endereco + i, &c) != MEMORIA_OK || c == 0)
            break;
        console_escrever_char(vm, c);
    }
}
```

Dois cuidados de segurança aparecem nesse trecho:

- o endereço vem do convidado, então é **traduzido** e nunca usado direto em `ram[]`;
- existe um **limite** (`TAM_MAX_TEXTO`), para que uma string sem `0` no final não prenda o hypervisor.

## Passo 4 – Comparando as duas abordagens

O programa `ola` imprime uma linha com `OUT`, caractere a caractere, e outra com uma única `HCALL 0`. O relatório final mostra a diferença:

```
VM  Nome        Estado      Instrucoes Fatias  E/S  HCALL ...
1   ola         FINALIZADA         323     13    55     1 ...
```

São **55 VM exits** para a linha impressa com `OUT` e **1** para a linha (mais longa) impressa com `HCALL`. Num hypervisor real cada VM exit custa centenas ou milhares de ciclos. É por isso que existem os **drivers paravirtualizados**, como o *virtio* no KVM e as *VMware Tools*: eles trocam muitos traps por poucas hypercalls.

| | Virtualização total (`OUT`) | Paravirtualização (`HCALL`) |
|---|---|---|
| Convidado precisa ser modificado? | não | sim, precisa conhecer as hypercalls |
| Número de VM exits | um por acesso | um por operação |
| Desempenho | menor | maior |
| Exemplo real | emulação de placa de rede e1000 | virtio-net |

## Ponto de verificação

Depois da [Etapa 7](07-etapa-vmm.md):

```bash
./build/hypervisor ola
./build/hypervisor -v ola | grep -c "IO_OUT"      # conta as VM exits de OUT
```

## Para discutir em aula

1. Como seria um dispositivo de **entrada** (teclado virtual)? O que a VM deveria fazer enquanto não há tecla: ficar BLOQUEADA ou fazer *polling*?
2. Por que o hypervisor não pode confiar no endereço passado em `R0`?
3. Qual é a relação entre *hypercall* e *syscall*? E entre VM exit e *trap*?

⬅️ [Etapa 3](03-etapa-vcpu.md) · ➡️ [Etapa 5 – Montador, carregador e programas](05-etapa-montador.md)
