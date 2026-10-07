; Compute 5! using a recursive function.
; Calling convention: argument in R0, result in R0.
    LOADI R0, 5
    CALL fact
    PRINT R0
    HALT

fact:
    CMPI R0, 1
    JLE base           ; if n <= 1, return 1
    PUSH R0            ; save n across the recursive call
    DEC R0             ; n - 1
    CALL fact          ; R0 = fact(n - 1)
    POP R1             ; R1 = n
    MUL R0, R1         ; R0 = fact(n - 1) * n
    RET
base:
    LOADI R0, 1
    RET
