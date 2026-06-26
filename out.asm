global _start
_start:

    mov rax, 10
    push rax
    mov rax, 3
    push rax
    push QWORD [rsp + 0]
    push QWORD [rsp + 16]
    pop rax
    pop rbx
    sub rax, rbx
    push rax
    mov rax, 2
    push rax
    push QWORD [rsp + 8]
    pop rax
    pop rbx
    mul rbx
    push rax
    push QWORD [rsp + 0]
    pop rax
    test rax, rax
    jz label0
    push QWORD [rsp + 24]
    push QWORD [rsp + 8]
    pop rax
    pop rbx
    add rax, rbx
    push rax
    push QWORD [rsp + 0]
    mov rax, 60
    pop rdi
    syscall
    add rsp, 8
label0:
    push QWORD [rsp + 16]
    mov rax, 60
    pop rdi
    syscall
    mov rax, 60
    mov rdi, 0
    syscall

