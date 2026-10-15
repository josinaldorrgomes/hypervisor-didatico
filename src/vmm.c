/*
 * vmm.c - Núcleo do hypervisor: carregador, escalonador e tratador de VM exits.
 *
 * Fluxo geral:
 *
 *   vmm_criar_vm()  -> monta o programa, cria a tabela de páginas e
 *                      copia a imagem para a memória da VM.
 *
 *   vmm_executar()  -> laço principal:
 *        enquanto houver VM viva:
 *            acorda as VMs cujo sono terminou
 *            escolhe a próxima VM PRONTA (Round-Robin)
 *            executa uma fatia de tempo dela:
 *                VM entry  -> vcpu_executar()
 *                VM exit   -> tratar_saida()   (repete até o quantum acabar)
 */
#include <stdio.h>
#include <string.h>

#include "vmm.h"
#include "vm.h"
#include "isa.h"
#include "memoria.h"
#include "montador.h"
#include "dispositivos.h"

#define TAM_MAX_TEXTO 256   /* maior string aceita pela HCALL 0 */

static struct MaquinaVirtual vms[MAX_VMS];   /* tabela de VMs ("tabela de processos") */
static int num_vms;
static struct ConfigVMM config;

static long relogio;          /* tempo global, em instruções executadas ("ticks") */
static long tempo_ocioso;     /* ticks em que todas as VMs estavam bloqueadas     */
static long trocas_contexto;

void vmm_iniciar(const struct ConfigVMM *cfg)
{
    config = *cfg;
    num_vms = 0;
    relogio = 0;
    tempo_ocioso = 0;
    trocas_contexto = 0;
    memoria_iniciar();
}

/* ================================================================== */
/*  CARREGADOR (Etapa 5)                                              */
/* ================================================================== */

int vmm_criar_vm(const char *nome, const char *fonte)
{
    static int imagem[MAX_PALAVRAS_VM];   /* static: vetor grande fora da pilha */
    char erro[120];
    struct MaquinaVirtual *vm;
    int tamanho, paginas_imagem, paginas_total, i;

    if (num_vms == MAX_VMS) {
        printf("[VMM] ERRO: tabela de VMs cheia (maximo %d)\n", MAX_VMS);
        return -1;
    }

    /* 1. Montagem: texto -> imagem numérica (sobra espaço para a pilha) */
    tamanho = montar(fonte, imagem, MAX_PALAVRAS_VM - TAM_PILHA, erro, sizeof erro);
    if (tamanho < 0) {
        printf("[VMM] ERRO ao montar '%s': %s\n", nome, erro);
        return -1;
    }

    /* 2. Quantas páginas a VM pode usar: imagem + pilha */
    paginas_imagem = (tamanho + TAM_PAGINA - 1) / TAM_PAGINA;
    paginas_total = (tamanho + TAM_PILHA + TAM_PAGINA - 1) / TAM_PAGINA;

    /* 3. Preenche o bloco de controle da VM */
    vm = &vms[num_vms];
    memset(vm, 0, sizeof *vm);
    vm->id = num_vms + 1;
    strncpy(vm->nome, nome, MAX_NOME - 1);
    memoria_iniciar_tabela(&vm->paginas, paginas_total);

    /* 4. Aloca quadros só para as páginas da imagem.
     *    As páginas de pilha ficam AUSENTES até serem usadas (paginação sob demanda). */
    for (i = 0; i < paginas_imagem; i++) {
        if (memoria_alocar_pagina(&vm->paginas, i, vm->id) == SEM_QUADRO) {
            printf("[VMM] ERRO: sem memoria no hospedeiro para '%s'\n", nome);
            memoria_liberar(&vm->paginas);
            return -1;
        }
    }

    /* 5. Copia a imagem para a memória da VM (passando pela tradução) */
    for (i = 0; i < tamanho; i++)
        memoria_escrever(&vm->paginas, i, imagem[i]);

    /* 6. "Liga" a vCPU: começa no endereço 0, pilha no topo do espaço */
    vcpu_reiniciar(&vm->cpu, 0, paginas_total * TAM_PAGINA);
    vm->estado = VM_PRONTA;
    num_vms++;

    printf("[VMM] VM%d criada: '%s' - %d palavras, espaco de %d paginas (%d carregadas)\n",
           vm->id, vm->nome, tamanho, paginas_total, paginas_imagem);
    memoria_imprimir_tabela(&vm->paginas);
    return vm->id;
}

/* ================================================================== */
/*  CICLO DE VIDA                                                     */
/* ================================================================== */

static int vm_viva(const struct MaquinaVirtual *vm)
{
    return vm->estado == VM_PRONTA || vm->estado == VM_EXECUTANDO
        || vm->estado == VM_BLOQUEADA;
}

/* Termina a VM (normalmente ou por erro) e devolve a memória ao hospedeiro. */
static void encerrar_vm(struct MaquinaVirtual *vm, int estado_final, const char *motivo)
{
    console_descarregar(vm);
    memoria_liberar(&vm->paginas);
    vm->estado = estado_final;
    strncpy(vm->motivo_fim, motivo, sizeof vm->motivo_fim - 1);
    printf("[VMM] t=%-6ld VM%d (%s) %s: %s\n", relogio, vm->id, vm->nome,
           estado_final == VM_FINALIZADA ? "finalizada" : "ABORTADA", motivo);
}

/* ================================================================== */
/*  TRATAMENTO DE VM EXITS (Etapas 3 e 4)                             */
/* ================================================================== */

/* OUT: o convidado "escreveu numa porta". Emulamos o dispositivo. */
static void emular_out(struct MaquinaVirtual *vm)
{
    struct VCpu *cpu = &vm->cpu;

    switch (cpu->saida_porta) {
    case PORTA_CONSOLE_NUMERO:
        console_escrever_numero(vm, cpu->saida_valor);
        break;
    case PORTA_CONSOLE_CHAR:
        console_escrever_char(vm, cpu->saida_valor);
        break;
    default:   /* porta sem dispositivo: como no hardware real, a escrita se perde */
        if (config.verboso)
            printf("[VMM] VM%d: OUT na porta %d (sem dispositivo) ignorado\n",
                   vm->id, cpu->saida_porta);
    }
}

/* IN: o convidado "leu uma porta". O hypervisor decide o valor. */
static void emular_in(struct MaquinaVirtual *vm)
{
    struct VCpu *cpu = &vm->cpu;
    int valor = 0;   /* porta sem dispositivo devolve 0 */

    switch (cpu->saida_porta) {
    case PORTA_ID_VM:   valor = vm->id;       break;
    case PORTA_RELOGIO: valor = (int)relogio; break;
    }
    cpu->reg[cpu->saida_reg] = valor;
}

/* HCALL 0: lê uma string da memória DO CONVIDADO e a imprime no console. */
static void hypercall_imprimir(struct MaquinaVirtual *vm)
{
    int endereco = vm->cpu.reg[0];
    int i, c;

    for (i = 0; i < TAM_MAX_TEXTO; i++) {
        if (memoria_ler(&vm->paginas, endereco + i, &c) != MEMORIA_OK || c == 0)
            break;
        console_escrever_char(vm, c);
    }
}

static void tratar_hypercall(struct MaquinaVirtual *vm)
{
    struct VCpu *cpu = &vm->cpu;

    switch (cpu->saida_valor) {
    case HC_IMPRIMIR_TEXTO:
        hypercall_imprimir(vm);
        break;
    case HC_CEDER_CPU:
        vm->estado = VM_PRONTA;              /* volta para o fim da fila */
        break;
    case HC_DORMIR:
        if (cpu->reg[0] > 0) {
            vm->estado = VM_BLOQUEADA;
            vm->acordar_em = relogio + cpu->reg[0];
        } else {
            vm->estado = VM_PRONTA;
        }
        break;
    default:
        cpu->reg[0] = -1;                    /* hypercall inexistente: devolve erro */
    }
}

/* Falta de página: a página é válida, só falta dar um quadro para ela. */
static void tratar_falta_pagina(struct MaquinaVirtual *vm)
{
    int pagina = vm->cpu.endereco_falha / TAM_PAGINA;
    int quadro = memoria_alocar_pagina(&vm->paginas, pagina, vm->id);
    char motivo[96];

    if (quadro == SEM_QUADRO) {
        snprintf(motivo, sizeof motivo, "sem memoria no hospedeiro para a pagina %d", pagina);
        encerrar_vm(vm, VM_ABORTADA, motivo);
        return;
    }
    printf("[VMM] t=%-6ld VM%d: falta de pagina no endereco %d -> pagina %d "
           "recebeu o quadro %d\n", relogio, vm->id, vm->cpu.endereco_falha, pagina, quadro);
    /* a vCPU não avançou o pc: a instrução será reexecutada e agora vai funcionar */
}

static void tratar_saida(struct MaquinaVirtual *vm, int motivo)
{
    struct VCpu *cpu = &vm->cpu;
    char texto[96];

    vm->saidas[motivo]++;
    if (config.verboso)
        printf("[VMM] t=%-6ld <- VM exit da VM%d: %s\n", relogio, vm->id,
               vcpu_nome_saida(motivo));

    switch (motivo) {
    case SAIDA_QUANTUM:
        vm->estado = VM_PRONTA;   /* preempção: volta para a fila de prontas */
        break;
    case SAIDA_IO_OUT:
        emular_out(vm);
        break;
    case SAIDA_IO_IN:
        emular_in(vm);
        break;
    case SAIDA_HYPERCALL:
        tratar_hypercall(vm);
        break;
    case SAIDA_FALTA_PAGINA:
        tratar_falta_pagina(vm);
        break;
    case SAIDA_HLT:
        encerrar_vm(vm, VM_FINALIZADA, "executou HLT");
        break;
    case SAIDA_VIOLACAO_MEMORIA:
        snprintf(texto, sizeof texto, "violacao de memoria no endereco %d (pc=%d)",
                 cpu->endereco_falha, cpu->pc);
        encerrar_vm(vm, VM_ABORTADA, texto);
        break;
    case SAIDA_INSTRUCAO_INVALIDA:
        snprintf(texto, sizeof texto, "instrucao invalida (pc=%d)", cpu->pc);
        encerrar_vm(vm, VM_ABORTADA, texto);
        break;
    case SAIDA_DIVISAO_ZERO:
        snprintf(texto, sizeof texto, "divisao por zero (pc=%d)",
                 cpu->pc - TAM_INSTRUCAO);
        encerrar_vm(vm, VM_ABORTADA, texto);
        break;
    }
}

/* ================================================================== */
/*  ESCALONADOR (Etapa 6)                                             */
/* ================================================================== */

/* Dá a CPU real para a VM durante um quantum. */
static void executar_fatia(struct MaquinaVirtual *vm)
{
    int restante = config.quantum;
    char texto[96];

    vm->estado = VM_EXECUTANDO;
    vm->fatias++;

    while (vm->estado == VM_EXECUTANDO) {
        long antes = vm->cpu.instrucoes_executadas;
        long ate_limite = config.limite_instrucoes - antes;
        int maximo = (restante < ate_limite) ? restante : (int)ate_limite;
        int motivo;

        motivo = vcpu_executar(vm, maximo, config.rastrear);     /* VM entry */

        relogio += vm->cpu.instrucoes_executadas - antes;
        restante -= (int)(vm->cpu.instrucoes_executadas - antes);

        tratar_saida(vm, motivo);                                /* VM exit  */

        /* watchdog: impede que uma VM rode para sempre */
        if (vm_viva(vm) && vm->cpu.instrucoes_executadas >= config.limite_instrucoes) {
            snprintf(texto, sizeof texto, "watchdog: atingiu o limite de %ld instrucoes",
                     config.limite_instrucoes);
            encerrar_vm(vm, VM_ABORTADA, texto);
        }

        /* quantum acabou exatamente numa VM exit de E/S? também é preempção */
        if (vm->estado == VM_EXECUTANDO && restante <= 0)
            vm->estado = VM_PRONTA;
    }
}

/* Muda para PRONTA toda VM bloqueada cujo tempo de sono já passou. */
static void acordar_vms(void)
{
    int i;

    for (i = 0; i < num_vms; i++) {
        if (vms[i].estado == VM_BLOQUEADA && relogio >= vms[i].acordar_em) {
            vms[i].estado = VM_PRONTA;
            if (config.verboso)
                printf("[VMM] t=%-6ld VM%d acordou\n", relogio, vms[i].id);
        }
    }
}

/* Round-Robin: procura a próxima VM PRONTA depois da 'atual', em círculo. */
static int escolher_proxima(int atual)
{
    int k, i;

    for (k = 1; k <= num_vms; k++) {
        i = (atual + k) % num_vms;
        if (vms[i].estado == VM_PRONTA)
            return i;
    }
    return -1;
}

/* Ninguém pronto, mas há VMs dormindo: adianta o relógio até a primeira acordar. */
static void ficar_ocioso(void)
{
    long menor = -1;
    int i;

    for (i = 0; i < num_vms; i++) {
        if (vms[i].estado == VM_BLOQUEADA && (menor < 0 || vms[i].acordar_em < menor))
            menor = vms[i].acordar_em;
    }
    if (menor > relogio) {
        if (config.verboso)
            printf("[VMM] t=%-6ld todas as VMs bloqueadas: CPU ociosa ate t=%ld\n",
                   relogio, menor);
        tempo_ocioso += menor - relogio;
        relogio = menor;
    }
}

static int existe_vm_viva(void)
{
    int i;

    for (i = 0; i < num_vms; i++) {
        if (vm_viva(&vms[i]))
            return 1;
    }
    return 0;
}

void vmm_executar(void)
{
    int atual = -1, proxima;

    printf("[VMM] Iniciando %d VM(s) | quantum = %d instrucoes | watchdog = %ld instrucoes\n",
           num_vms, config.quantum, config.limite_instrucoes);

    while (existe_vm_viva()) {
        acordar_vms();
        proxima = escolher_proxima(atual);

        if (proxima < 0) {
            ficar_ocioso();
            continue;
        }

        if (proxima != atual) {
            trocas_contexto++;
            if (config.verboso)
                printf("[VMM] t=%-6ld troca de contexto -> VM%d (%s)\n",
                       relogio, vms[proxima].id, vms[proxima].nome);
        }
        atual = proxima;
        executar_fatia(&vms[atual]);
    }
    printf("[VMM] Todas as VMs terminaram.\n");
}

/* ================================================================== */
/*  RELATÓRIO                                                         */
/* ================================================================== */

void vmm_relatorio(void)
{
    long total_saidas = 0;
    int i, m;

    printf("\n================================ RELATORIO DO HYPERVISOR "
           "================================\n");
    printf("VM  Nome        Estado      Instrucoes Fatias  E/S  HCALL F.Pag  Motivo do fim\n");
    printf("--  ----------  ----------  ---------- ------ ----- ----- -----  -------------\n");

    for (i = 0; i < num_vms; i++) {
        struct MaquinaVirtual *vm = &vms[i];

        for (m = 0; m < NUM_MOTIVOS_SAIDA; m++)
            total_saidas += vm->saidas[m];

        printf("%-3d %-10s  %-10s  %10ld %6ld %5ld %5ld %5ld  %s\n",
               vm->id, vm->nome, vm_nome_estado(vm->estado),
               vm->cpu.instrucoes_executadas, vm->fatias,
               vm->saidas[SAIDA_IO_OUT] + vm->saidas[SAIDA_IO_IN],
               vm->saidas[SAIDA_HYPERCALL], vm->saidas[SAIDA_FALTA_PAGINA],
               vm->motivo_fim);
    }

    printf("\nRelogio final: %ld ticks | ocioso: %ld | trocas de contexto: %ld | "
           "VM exits: %ld\n", relogio, tempo_ocioso, trocas_contexto, total_saidas);
}
