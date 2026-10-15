# Etapa 9 – Testes, exercícios e hypervisors reais

**Arquivos:** `tests/testes_unitarios.c`, `tests/testes_integracao.sh`

## Parte A – Testes automatizados

```bash
make test
```

### Testes unitários (`tests/testes_unitarios.c`, 32 testes)

O arquivo é compilado junto com todos os módulos, menos o `main.c`, e usa uma função simples de verificação:

```c
static void verificar(int condicao, const char *descricao)
{
    total++;
    if (condicao) printf("  ok    %s\n", descricao);
    else        { printf("  FALHA %s\n", descricao); falhas++; }
}
```

| Seção | O que garante |
|---|---|
| **Montador** (15) | codificação das instruções, rótulos usados antes de definidos, `.word`/`.string`/`.space`, escapes, todos os erros de sintaxe com o número da linha |
| **Memória** (7) | tradução página → quadro, `AUSENTE` × `VIOLACAO`, devolução dos quadros |
| **vCPU** (10) | execução correta, preempção por quantum, VM exits, **reexecução após falta de página** e **ataques**: opcode forjado, registrador `R9` forjado, `INT_MIN / -1` |

### Testes de integração (`tests/testes_integracao.sh`, 19 testes)

Executam o binário de verdade e procuram textos na saída com `grep`. Cobrem o resultado de cada programa, o isolamento (`ISOLAMENTO FALHOU` **nunca** pode aparecer), o watchdog, a devolução da memória e as opções `-v` e `-t`.

### Caçando bugs de memória no próprio hypervisor

```bash
make debug                # compila com -fsanitize=address,undefined
./build/hypervisor
```

O AddressSanitizer interrompe o programa no primeiro acesso inválido do **hypervisor**, não do convidado. Uma boa prática é rodar a demonstração assim após cada mudança.

## Parte B – Exercícios propostos

Os exercícios estão em ordem crescente de dificuldade. Cada um indica os arquivos que devem ser alterados.

### Nível 1 – Programas convidados (`programas.c`)

1. **Tabuada:** imprima a tabuada do número lido de `IN R0, 3` (o id da VM).
2. **Contagem regressiva com sono:** imprima de 5 até 1, dormindo 100 ticks entre os números.
3. **Soma de vetor:** use `.word` e `LOADR` para somar 10 números.
4. **Cooperativo:** escreva duas versões de um laço longo, uma que chama `HCALL 1` a cada iteração e outra que não chama. Compare o número de fatias no relatório.

### Nível 2 – Hypervisor (`vmm.c`, `isa.h`)

5. **Nova porta de E/S:** a porta 5 (`IN`) deve devolver um número pseudoaleatório (gerador linear congruente).
6. **Nova hypercall:** `HCALL 3` devolve em `R0` quantas VMs estão vivas.
7. **Escalonamento por prioridade:** adicione `prioridade` em `struct MaquinaVirtual` e altere `escolher_proxima`.
8. **Quantum por VM:** VMs que usam muita E/S ganham quantum maior. Discuta: isso é justo?

### Nível 3 – Memória (`memoria.c`, `vmm.c`)

9. **Página de guarda:** deixe uma página inválida entre o código e a pilha para detectar *stack overflow*. Teste com uma recursão infinita.
10. **Estatísticas de memória:** mostre no relatório quantos quadros cada VM chegou a usar (pico).
11. **Memória compartilhada:** faça duas VMs mapearem a mesma página num quadro comum (cuidado ao liberar!) e implemente um produtor/consumidor. *Ponte para o tópico de sincronização.*

### Nível 4 – Desafios

12. **Snapshot e restauração:** copie a `struct VCpu` e as páginas de uma VM e restaure depois. É a base da *migração ao vivo*.
13. **Injeção de exceção:** em vez de abortar na divisão por zero, desvie o convidado para um tratador registrado por ele (`HCALL 4`, com `R0` = endereço do tratador).
14. **Carregar programas de arquivo:** adicione `-f arquivo.asm`. Discuta o que muda no requisito "tudo em memória".

## Parte C – Do hypervisor didático ao KVM real

Em CPUs com Intel VT-x ou AMD-V, o laço de um hypervisor real usando o **KVM** do Linux tem a mesma forma do nosso `executar_fatia`:

```c
/* Esboço simplificado de um VMM sobre KVM (Linux, x86) */
int kvm  = open("/dev/kvm", O_RDWR);
int vmfd = ioctl(kvm, KVM_CREATE_VM, 0);              /* ≈ vmm_criar_vm         */
/* registra a memória do convidado (≈ tabela de páginas / EPT) */
ioctl(vmfd, KVM_SET_USER_MEMORY_REGION, &regiao);
int vcpufd = ioctl(vmfd, KVM_CREATE_VCPU, 0);         /* ≈ struct VCpu          */
struct kvm_run *run = mmap(..., vcpufd, 0);

for (;;) {
    ioctl(vcpufd, KVM_RUN, 0);                        /* ≈ vcpu_executar (VM entry) */
    switch (run->exit_reason) {                       /* ≈ tratar_saida  (VM exit)  */
    case KVM_EXIT_IO:   /* ≈ SAIDA_IO_OUT / SAIDA_IO_IN */  break;
    case KVM_EXIT_HLT:  /* ≈ SAIDA_HLT                  */  return 0;
    case KVM_EXIT_MMIO: /* acesso a dispositivo mapeado em memória */ break;
    }
}
```

| Neste projeto | Em um hypervisor com VT-x/KVM |
|---|---|
| `vcpu_executar()` interpreta instrução por instrução | a **CPU real** executa o código do convidado em *VMX non-root mode* |
| `return SAIDA_IO_OUT` | o hardware gera uma **VM exit** e salva o estado na **VMCS** |
| `struct VCpu` | **VMCS** (Intel) / **VMCB** (AMD) |
| `struct TabelaPaginas` + `memoria_traduzir` | **EPT** / **NPT**, percorridas pelo hardware |
| `SAIDA_FALTA_PAGINA` | **EPT violation** |
| `SAIDA_QUANTUM` (contador de instruções) | **VMX preemption timer** ou interrupção de timer externa |
| `HCALL` | instrução `VMCALL` / `VMMCALL` |
| console virtual + `OUT` | dispositivos emulados (QEMU) ou paravirtualizados (virtio) |
| escalonador Round-Robin próprio | as vCPUs são *threads* escalonadas pelo próprio Linux (CFS) |

Em ambos os casos, **o convidado nunca toca diretamente no que é sensível**: ou o software intercepta (nosso caso), ou o hardware intercepta (VT-x). Esse é o princípio de Popek & Goldberg.

## Parte D – E os contêineres?

A aula da Semana 11 também trata de contêineres. Vale fechar com o contraste:

| | Máquina virtual (este projeto) | Contêiner (Docker, Podman) |
|---|---|---|
| O que é virtualizado | **a máquina**: CPU, memória, dispositivos | **o sistema operacional**: visão de processos, rede, arquivos |
| Kernel | cada VM tem o seu (aqui, o próprio programa convidado) | **compartilhado** com o hospedeiro |
| Isolamento | VMM + tradução de memória | *namespaces* + *cgroups* do kernel |
| Custo de criação | alto (alocar memória, "dar boot") | baixo (é um processo) |
| Equivalente aqui | `struct MaquinaVirtual` | seria um processo comum com uma "visão" restrita do sistema |

⬅️ [Etapa 8](08-etapa-executavel.md) · 🏠 [Voltar ao início](../README.md)
