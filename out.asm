global _start
_start:

    ;; program start

    ;; we_haves a
    ;; int lit: 48
    mov rax, 48
    push rax
    ;; /we_haves
    ;; we_haves b
    ;; int lit: 18
    mov rax, 18
    push rax
    ;; /we_haves
    ;; we_haves temp
    ;; int lit: 0
    mov rax, 0
    push rax
    ;; /we_haves
    ;; if
    ;; less than expr
    ;; variable: b
    push QWORD [rsp + 8]
    ;; variable: a
    push QWORD [rsp + 24]
    pop rax
    pop rbx
    cmp rax, rbx
    setl al
    movzx rax, al
    push rax
    pop rax
    test rax, rax
    jz label0
    ;; temp =
    ;; variable: a
    push QWORD [rsp + 16]
    pop rax
    mov QWORD [rsp + 0], rax
    ;; a =
    ;; variable: b
    push QWORD [rsp + 8]
    pop rax
    mov QWORD [rsp + 16], rax
    ;; b =
    ;; variable: temp
    push QWORD [rsp + 0]
    pop rax
    mov QWORD [rsp + 8], rax
label0:
    ;; /if
    ;; we_haves done
    ;; int lit: 0
    mov rax, 0
    push rax
    ;; /we_haves
    ;; if
    ;; eq expr
    ;; int lit: 0
    mov rax, 0
    push rax
    ;; variable: b
    push QWORD [rsp + 24]
    pop rax
    pop rbx
    cmp rax, rbx
    sete al
    movzx rax, al
    push rax
    pop rax
    test rax, rax
    jz label1
    ;; done =
    ;; int lit: 1
    mov rax, 1
    push rax
    pop rax
    mov QWORD [rsp + 0], rax
label1:
    ;; /if
    ;; if
    ;; not expr
    ;; variable: done
    push QWORD [rsp + 0]
    pop rax
    test rax, rax
    setz al
    movzx rax, al
    push rax
    pop rax
    test rax, rax
    jz label2
    ;; we_haves remainder
    ;; int lit: 0
    mov rax, 0
    push rax
    ;; /we_haves
    ;; we_haves counter
    ;; int lit: 0
    mov rax, 0
    push rax
    ;; /we_haves
    ;; if
  ;; and expr
    ;; not expr
    ;; less than expr
    ;; variable: b
    push QWORD [rsp + 32]
    ;; variable: a
    push QWORD [rsp + 48]
    pop rax
    pop rbx
    cmp rax, rbx
    setl al
    movzx rax, al
    push rax
    pop rax
    test rax, rax
    setz al
    movzx rax, al
    push rax
    ;; greater than or equal expr
    ;; variable: b
    push QWORD [rsp + 40]
    ;; variable: a
    push QWORD [rsp + 56]
    pop rax
    pop rbx
    cmp rax, rbx
    setge al
    movzx rax, al
    push rax
    pop rax
    pop rbx
    test rax, rax
    setnz al
    test rbx, rbx
    setnz bl
    and al, bl
    movzx rax, al
    push rax
    pop rax
    test rax, rax
    jz label3
    ;; counter =
    ;; int lit: 1
    mov rax, 1
    push rax
    pop rax
    mov QWORD [rsp + 0], rax
label3:
    ;; /if
    ;; if
  ;; and expr
    ;; greater than expr
    ;; int lit: 0
    mov rax, 0
    push rax
    ;; variable: b
    push QWORD [rsp + 40]
    pop rax
    pop rbx
    cmp rax, rbx
    setg al
    movzx rax, al
    push rax
    ;; greater than expr
    ;; variable: b
    push QWORD [rsp + 40]
    ;; variable: a
    push QWORD [rsp + 56]
    pop rax
    pop rbx
    cmp rax, rbx
    setg al
    movzx rax, al
    push rax
    pop rax
    pop rbx
    test rax, rax
    setnz al
    test rbx, rbx
    setnz bl
    and al, bl
    movzx rax, al
    push rax
    pop rax
    test rax, rax
    jz label4
    ;; counter =
    ;; add expr
    ;; int lit: 1
    mov rax, 1
    push rax
    ;; variable: counter
    push QWORD [rsp + 8]
    pop rax
    pop rbx
    add rax, rbx
    push rax
    pop rax
    mov QWORD [rsp + 0], rax
label4:
    ;; /if
    ;; if
  ;; and expr
    ;; not eq expr
    ;; int lit: 0
    mov rax, 0
    push rax
    ;; variable: b
    push QWORD [rsp + 40]
    pop rax
    pop rbx
    cmp rax, rbx
    setne al
    movzx rax, al
    push rax
    ;; not eq expr
    ;; variable: b
    push QWORD [rsp + 40]
    ;; variable: a
    push QWORD [rsp + 56]
    pop rax
    pop rbx
    cmp rax, rbx
    setne al
    movzx rax, al
    push rax
    pop rax
    pop rbx
    test rax, rax
    setnz al
    test rbx, rbx
    setnz bl
    and al, bl
    movzx rax, al
    push rax
    pop rax
    test rax, rax
    jz label5
    ;; counter =
    ;; add expr
    ;; int lit: 1
    mov rax, 1
    push rax
    ;; variable: counter
    push QWORD [rsp + 8]
    pop rax
    pop rbx
    add rax, rbx
    push rax
    pop rax
    mov QWORD [rsp + 0], rax
label5:
    ;; /if
    ;; we_haves check
    ;; int lit: 0
    mov rax, 0
    push rax
    ;; /we_haves
    ;; if
    ;; eq expr
    ;; int lit: 3
    mov rax, 3
    push rax
    ;; variable: counter
    push QWORD [rsp + 16]
    pop rax
    pop rbx
    cmp rax, rbx
    sete al
    movzx rax, al
    push rax
    pop rax
    test rax, rax
    jz label6
    ;; check =
    ;; int lit: 100
    mov rax, 100
    push rax
    pop rax
    mov QWORD [rsp + 0], rax
label6:
    ;; /if
    ;; if
  ;; or expr
    ;; eq expr
    ;; variable: b
    push QWORD [rsp + 40]
    ;; variable: a
    push QWORD [rsp + 56]
    pop rax
    pop rbx
    cmp rax, rbx
    sete al
    movzx rax, al
    push rax
    ;; not eq expr
    ;; int lit: 3
    mov rax, 3
    push rax
    ;; variable: counter
    push QWORD [rsp + 24]
    pop rax
    pop rbx
    cmp rax, rbx
    setne al
    movzx rax, al
    push rax
    pop rax
    pop rbx
    test rax, rax
    setnz al
    test rbx, rbx
    setnz bl
    or al, bl
    movzx rax, al
    push rax
    pop rax
    test rax, rax
    jz label7
    ;; check =
    ;; add expr
    ;; int lit: 1
    mov rax, 1
    push rax
    ;; variable: check
    push QWORD [rsp + 8]
    pop rax
    pop rbx
    add rax, rbx
    push rax
    pop rax
    mov QWORD [rsp + 0], rax
label7:
    ;; /if
    ;; gives
    ;; variable: check
    push QWORD [rsp + 0]
    mov rax, 60
    pop rdi
    syscall
    ;; /gives
    add rsp, 24
label2:
    ;; /if
    ;; gives
    ;; int lit: 99
    mov rax, 99
    push rax
    mov rax, 60
    pop rdi
    syscall
    ;; /gives

    ;; program end
    mov rax, 60
    mov rdi, 0
    syscall

