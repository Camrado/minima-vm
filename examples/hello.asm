; Walk a zero-terminated string in memory and print it.
    LOADI R0, msg      ; R0 = address of the first character
print_loop:
    LOAD R1, R0        ; R1 = mem[R0]
    CMPI R1, 0         ; reached the terminator?
    JE done
    PRINTC R1
    INC R0
    JMP print_loop
done:
    LOADI R1, 10       ; newline
    PRINTC R1
    HALT

msg:
    .string "Hello from Minima!"
