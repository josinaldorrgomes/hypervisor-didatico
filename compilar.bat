@echo off
REM compilar.bat - Gera o executavel no Windows usando o GCC do MinGW-w64.
REM Uso: compilar.bat
REM Resultado: build\hypervisor.exe (um unico arquivo, sem DLLs extras)

if not exist build mkdir build
gcc -std=c99 -Wall -Wextra -pedantic -O2 -static -o build\hypervisor.exe ^
    src\isa.c src\memoria.c src\montador.c src\vcpu.c src\vm.c ^
    src\dispositivos.c src\vmm.c src\programas.c src\main.c
if errorlevel 1 (
    echo Falha na compilacao. O gcc esta instalado e no PATH?
    exit /b 1
)
echo Executavel gerado: build\hypervisor.exe
echo Execute com: build\hypervisor.exe
