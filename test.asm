global _start       ; Make _start visible to the linker as the entry point
_start:
    mov rax, 60     ; Load syscall number 60 (exit) into rax
    mov rdi, 69     ; Load exit code 69 into rdi (first argument)
    syscall          ; Invoke the kernel to execute the exit syscall

