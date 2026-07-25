global _start
_start:

    ;; program start

    ;; we_haves i
    ;; int lit: 0
    mov rax, 0
    push rax
    ;; /we_haves
    ;; while
label0:
    ;; less than expr
    ;; int lit: 5
    mov rax, 5
    push rax
    ;; variable: i
    push QWORD [rsp + 8]
    pop rax
    pop rbx
    cmp rax, rbx
    setl al
    movzx rax, al
    push rax
    pop rax
    test rax, rax
    jz label1
    ;; i =
    ;; add expr
    ;; int lit: 1
    mov rax, 1
    push rax
    ;; variable: i
    push QWORD [rsp + 8]
    pop rax
    pop rbx
    add rax, rbx
    push rax
    pop rax
    mov QWORD [rsp + 0], rax
    jmp label0
label1:
    ;; /while
    ;; gives
    ;; variable: i
    push QWORD [rsp + 0]
    mov rax, 60
    pop rdi
    syscall
    ;; /gives

    ;; program end
    mov rax, 60
    mov rdi, 0
    syscall

