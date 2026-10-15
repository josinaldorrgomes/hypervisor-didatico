# Hypervisor Didático em C

Este é um **hypervisor (VMM – Virtual Machine Monitor)** funcional, escrito em C puro, para a disciplina de **Sistemas Operacionais**. Ele foi pensado para a aula da _Semana 11 – Virtualização, Máquinas Virtuais e Contêineres_.

O hypervisor cria várias **máquinas virtuais isoladas**, cada uma com sua **CPU virtual**, sua **memória paginada** e seu **console**. Ele divide a CPU real entre elas com **Round-Robin** e trata cada **VM exit** com a técnica **trap-and-emulate**. Uma VM que tenta invadir a memória alheia, dividir por zero ou monopolizar a CPU é encerrada sozinha, sem afetar as outras.

```
  [VM2 contador  ] 1
[VMM] t=52     VM3: falta de pagina no endereco 191 -> pagina 2 recebeu o quadro 11
  [VM3 fatorial  ] 1! = 1
  [VM4 dorminhoco] Rodada 1, relogio = 82
  [VM5 invasor   ] Vou tentar escrever no endereco 3000...
[VMM] t=91     VM5 (invasor) ABORTADA: violacao de memoria no endereco 3000 (pc=12)
  [VM6 guloso    ] Vou monopolizar a CPU com um laco infinito!
  [VM2 contador  ] 5
  ...
```

## Por que este projeto é simples de ler

- Usa só **C99 e a biblioteca padrão** (`stdio.h`, `string.h`, `stdlib.h`, `ctype.h`), sem bibliotecas externas.
- Usa só **vetores, structs, enums, `switch` e laços**. Não há `malloc`, listas ligadas nem ponteiros para função.
- Cada arquivo corresponde a **um conceito da disciplina**.
- **Tudo roda em memória.** Os programas convidados vêm embutidos no executável, e nada é lido ou gravado em disco.
- Gera **um único executável**, que roda sem instalação.

## Início rápido

```bash
make            # ou: sh compilar.sh   (Windows: compilar.bat)
./build/hypervisor
```

Outras formas de executar:

```bash
./build/hypervisor -l                    # lista os programas convidados
./build/hypervisor contador primos       # cria uma VM para cada programa
./build/hypervisor -v contador contador  # mostra cada VM exit e troca de contexto
./build/hypervisor -q 5 -t fibonacci     # quantum 5 e rastreio de cada instrução
./build/hypervisor -s fatorial           # mostra o assembly de um programa
make test                                # 32 testes unitários + 19 de integração
```

## Roteiro de implementação

A documentação completa está em [`docs/`](docs/). Cada etapa traz o objetivo, os conceitos da aula aplicados, o código explicado, o fluxo e um ponto de verificação.

| Etapa | Documento                                                                | Arquivos                           | Conceitos da disciplina                                |
| ----- | ------------------------------------------------------------------------ | ---------------------------------- | ------------------------------------------------------ |
| 0     | [Visão geral e arquitetura](docs/00-visao-geral.md)                      | –                                  | VMM, tipos de hypervisor, Popek & Goldberg             |
| 1     | [A máquina virtual: ISA e registradores](docs/01-etapa-isa.md)           | `config.h`, `isa.h`, `isa.c`       | arquitetura, instruções privilegiadas/sensíveis        |
| 2     | [Memória virtual com paginação](docs/02-etapa-memoria.md)                | `memoria.h`, `memoria.c`           | paginação, tabela de páginas, proteção                 |
| 3     | [CPU virtual e VM exits](docs/03-etapa-vcpu.md)                          | `vcpu.h`, `vcpu.c`, `vm.h`, `vm.c` | ciclo de instrução, traps, trap-and-emulate            |
| 4     | [Dispositivos e hypercalls](docs/04-etapa-dispositivos.md)               | `dispositivos.h`, `dispositivos.c` | E/S por portas, chamadas de sistema, paravirtualização |
| 5     | [Montador, carregador e programas](docs/05-etapa-montador.md)            | `montador.*`, `programas.*`        | compilação/montagem, carregador                        |
| 6     | [Escalonador de VMs](docs/06-etapa-escalonador.md)                       | `vmm.c`                            | estados, Round-Robin, quantum, troca de contexto       |
| 7     | [Juntando tudo: o laço do hypervisor](docs/07-etapa-vmm.md)              | `vmm.*`, `main.c`                  | tratamento de exceções, isolamento, ciclo de vida      |
| 8     | [Executável sem instalação e GitHub](docs/08-etapa-executavel.md)        | `Makefile`, `compilar.*`, CI       | compilação, ligação estática                           |
| 9     | [Testes, exercícios e hypervisors reais](docs/09-testes-e-exercicios.md) | `tests/`                           | comparação com KVM/VT-x                                |

A **[referência rápida da ISA](docs/referencia-isa.md)** reúne instruções, portas e hypercalls numa única página.

## Estrutura do repositório

```
hypervisor-didatico/
├── src/
│   ├── config.h          constantes (tamanho de página, quantum, ...)
│   ├── isa.h / isa.c     conjunto de instruções da máquina virtual
│   ├── memoria.h / .c    RAM do hospedeiro, quadros, tabelas de páginas
│   ├── vcpu.h / .c       CPU virtual: busca, decodifica, executa, VM exit
│   ├── vm.h / .c         bloco de controle da VM (o "PCB" da VM)
│   ├── dispositivos.h/.c console virtual
│   ├── montador.h / .c   assembler de dois passos
│   ├── programas.h / .c  programas convidados embutidos
│   ├── vmm.h / .c        o hypervisor: carregador, escalonador, exits
│   └── main.c            linha de comando
├── tests/
│   ├── testes_unitarios.c
│   └── testes_integracao.sh
├── docs/                 roteiro passo a passo
├── .github/workflows/    CI: compila, testa e publica executáveis
├── Makefile
├── compilar.sh           compila sem make (Linux/macOS)
└── compilar.bat          compila no Windows (MinGW)
```

## Executável sem instalação

O resultado da compilação é **um único arquivo**. Ele não precisa de instalador, de arquivos de configuração nem de pastas auxiliares. Basta copiá-lo e executá-lo.

- **Linux:** `make static` gera um binário estático que roda em qualquer distribuição.
- **Windows:** `compilar.bat` gera um `.exe` com `-static`, sem DLLs extras.
- **macOS:** depende apenas das bibliotecas do próprio sistema.
- **GitHub:** ao criar uma tag `v*`, o CI publica uma _Release_ com os executáveis dos três sistemas para download.

Os detalhes estão na [Etapa 8](docs/08-etapa-executavel.md).

## Licença

Antes de publicar, escolha uma licença (por exemplo, MIT) e adicione o arquivo `LICENSE` ao repositório.
