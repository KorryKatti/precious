global _start
_start:

    ;; program start

    ;; we_haves y
    ;; div expr
    ;; int lit: 2
    mov rax, 2
    push rax
    ;; sub expr
    ;; mul expr
    ;; int lit: 3
    mov rax, 3
    push rax
    ;; int lit: 2
    mov rax, 2
    push rax
    pop rax
    pop rbx
    mul rbx
    push rax
    ;; int lit: 10
    mov rax, 10
    push rax
    pop rax
    pop rbx
    sub rax, rbx
    push rax
    pop rax
    pop rbx
    div rbx
    push rax
    ;; /we_haves
    ;; we_haves x
    ;; int lit: 7
    mov rax, 7
    push rax
    ;; /we_haves
    ;; if
    ;; int lit: 1
    mov rax, 1
    push rax
    pop rax
    test rax, rax
    jz label0
    ;; x =
    ;; add expr
    ;; int lit: 10
    mov rax, 10
    push rax
    ;; int lit: 5
    mov rax, 5
    push rax
    pop rax
    pop rbx
    add rax, rbx
    push rax
    pop rax
    mov QWORD [rsp + 0], rax
    ;; gives
    ;; variable: x
    push QWORD [rsp + 0]
    mov rax, 60
    pop rdi
    syscall
    ;; /gives
    jmp label1
label0:
    ;; elif
    ;; int lit: 1
    mov rax, 1
    push rax
    pop rax
    test rax, rax
    jz label2
    ;; gives
    ;; int lit: 68
    mov rax, 68
    push rax
    mov rax, 60
    pop rdi
    syscall
    ;; /gives
    jmp label1
label2:
    ;; else
    ;; gives
    ;; int lit: 67
    mov rax, 67
    push rax
    mov rax, 60
    pop rdi
    syscall
    ;; /gives
label1:
    ;; /if

    ;; program end
    mov rax, 60
    mov rdi, 0
    syscall

