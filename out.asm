global _start
_start:

    ;; program start

    ;; gives
    ;; mul expr
    ;; add expr
    ;; int lit: 5
    mov rax, 5
    push rax
    ;; int lit: 2
    mov rax, 2
    push rax
    pop rax
    pop rbx
    add rax, rbx
    push rax
    ;; add expr
    ;; int lit: 4
    mov rax, 4
    push rax
    ;; int lit: 3
    mov rax, 3
    push rax
    pop rax
    pop rbx
    add rax, rbx
    push rax
    pop rax
    pop rbx
    mul rbx
    push rax
    mov rax, 60
    pop rdi
    syscall
    ;; /gives

    ;; program end
    mov rax, 60
    mov rdi, 0
    syscall

