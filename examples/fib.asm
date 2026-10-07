; Print the Fibonacci numbers below 1000.
    LOADI R0, 0        ; a
    LOADI R1, 1        ; b
    LOADI R2, 1000     ; limit
loop:
    CMP R0, R2         ; compare a with limit
    JGE done           ; if a >= limit, stop
    PRINT R0
    MOV R3, R0         ; tmp = a
    ADD R3, R1         ; tmp = a + b
    MOV R0, R1         ; a = b
    MOV R1, R3         ; b = tmp
    JMP loop
done:
    HALT
