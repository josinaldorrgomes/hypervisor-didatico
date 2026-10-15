# Makefile - Hypervisor didático
#
#   make          compila build/hypervisor
#   make run      compila e executa a demonstração
#   make test     testes unitários + testes de integração
#   make static   executável estático (Linux): roda em qualquer distro, sem dependências
#   make debug    compila com AddressSanitizer/UBSan (para caçar bugs de memória)
#   make clean    apaga a pasta build/

CC      ?= cc
CFLAGS  ?= -std=c99 -Wall -Wextra -pedantic -O2

SRC     = src/isa.c src/memoria.c src/montador.c src/vcpu.c src/vm.c \
          src/dispositivos.c src/vmm.c src/programas.c src/main.c
HDR     = $(wildcard src/*.h)
# tudo menos o main.c: os testes unitários têm o seu próprio main()
MODULOS = $(filter-out src/main.c,$(SRC))

BIN     = build/hypervisor
TESTES  = build/testes_unitarios

.PHONY: all run test static debug clean

all: $(BIN)

$(BIN): $(SRC) $(HDR)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $(SRC)

$(TESTES): tests/testes_unitarios.c $(MODULOS) $(HDR)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ tests/testes_unitarios.c $(MODULOS)

run: $(BIN)
	./$(BIN)

test: $(BIN) $(TESTES)
	./$(TESTES)
	sh tests/testes_integracao.sh ./$(BIN)

static:
	@mkdir -p build
	$(CC) $(CFLAGS) -static -o $(BIN) $(SRC)

debug:
	@mkdir -p build
	$(CC) -std=c99 -Wall -Wextra -pedantic -g -O0 \
	      -fsanitize=address,undefined -o $(BIN) $(SRC)

clean:
	rm -rf build
