ORG 0x0100

section .text

start:
    MOV AX, 0xB800        ; Set AX to video memory segment
    MOV DS, AX            ; Set DS to video memory
    MOV ES, AX            ; Set ES to video memory
    MOV AX, 0x0000        ; Set AX to 0 for stack segment
    MOV SS, AX            ; Set SS to 0
    MOV word [0x0000], 0x0745  ; 'E' at start of video memory
    MOV word [0x0002], 0x074D  ; 'M'
    MOV word [0x0004], 0x0755  ; 'U'
    MOV word [0x0006], 0x074C  ; 'L'
    MOV word [0x0008], 0x0741  ; 'A'
    MOV word [0x000A], 0x0754  ; 'T'
    MOV word [0x000C], 0x074F  ; 'O'
    MOV word [0x000E], 0x0752  ; 'R'
    MOV AX, 0x0005        ; AX = 5
    MOV BX, 0x0003        ; BX = 3
    MOV CX, 0x0002        ; CX = 2
    MOV DX, 0x0000        ; DX = 0
    MOV SI, 0x0010        ; SI = 0x0010 (memory address for data)
    MOV DI, 0x0014        ; DI = 0x0014 (memory address for result)
    MOV word [SI], 0x0003 ; Store 3 at address in SI (0x0010)
    ADD AX, [SI]          ; AX = AX + [SI] (5 + 3 = 8)
    MOV word [SI + 2], 0x0002 ; Store 2 at address SI + 2 (0x0012)
    SUB AX, [SI + 2]      ; AX = AX - [SI + 2] (8 - 2 = 6)
    CMP AX, 0x0006        ; Compare AX with 6
    JE equal_label        ; Jump if equal to equal_label
    JNE not_equal_label   ; Jump if not equal to not_equal_label
    JMP end               ; Unconditional jump to end

equal_label:
    MOV word [DI], 0x0745 ; Store 'E' at address in DI (0x0014) if equal
    JC carry_label        ; Jump if carry flag is set
    JMP end               ; Jump to end

not_equal_label:
    MOV word [DI], 0x074E ; Store 'N' at address in DI (0x0014) if not equal
    JNC no_carry_label    ; Jump if no carry flag
    JMP end               ; Jump to end

carry_label:
    MOV word [DI + 2], 0x0743 ; Store 'C' at DI + 2 (0x0016) if carry
    JMP end               ; Jump to end

no_carry_label:
    MOV word [DI + 2], 0x074E ; Store 'N' at DI + 2 (0x0016) if no carry

end:
    HLT                   ; Halt the CPU
