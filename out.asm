global _start

_start:

    ;; --- let x ---
    mov rax, 6
    push rax

    ;; --- let y ---
    mov rax, 7
    push rax

    ;; --- exit syscall ---
    push QWORD [rsp +0]
    mov rax, 60
    pop rdi
    syscall

    ;; --- program exit (fallback) ---
    mov rax, 60
    mov rdi, 0
    syscall
