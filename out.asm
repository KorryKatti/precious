global _start
_start:
    mov rax, 6
    push rax
    mov rax, 7
    push rax
    pop rax
    pop rbx
    add rax, rbx
    push rax
    mov rax, 7
    push rax
    push QWORD [rsp + 8]

    mov rax, 60
    pop rdi
    syscall
    mov rax, 60
    mov rdi, 0
    syscall
