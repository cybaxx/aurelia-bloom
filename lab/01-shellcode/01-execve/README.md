# Shellcode: execve /bin/sh

## Objective
Write and test minimal BSD x86-64 shellcode that spawns a shell.

## Key Concepts
- BSD and Linux share syscall number 59 for `execve`, but calling conventions differ
- BSD uses: `rax`=syscall#, `rdi`, `rsi`, `rdx`, `r10`, `r8`, `r9`
- Null bytes must be avoided in shellcode (breaks `strcpy`-based injection)

## Build & Test
```sh
nasm -f elf64 shellcode.asm -o shellcode.o
ld -o shellcode shellcode.o
./shellcode
```

## Extract raw bytes
```sh
objcopy -O binary shellcode.o shellcode.bin
xxd shellcode.bin
```

## Exercises
1. Verify the shellcode contains no null bytes (`\x00`) — modify if needed
2. Reduce the shellcode size as much as possible
3. Inject this shellcode into the Level 1 stack overflow on target-basic
