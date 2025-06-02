ORG 0x0100

section .text

start:
    MOV AX, 0xB800
    MOV DS, AX
    MOV ES, AX
    MOV AX, 0x0000
    MOV SS, AX
    MOV word [0x0000], 0x0745
    MOV word [0x0002], 0x074D
    MOV word [0x0004], 0x0755
    MOV word [0x0006], 0x074C
    MOV word [0x0008], 0x0741
    MOV word [0x000A], 0x0754
    MOV word [0x000C], 0x074F
    MOV word [0x000E], 0x0752
    MOV AX, 0x0005
    MOV BX, 0x0003
    MOV CX, 0x0002
    MOV DX, 0x0000
    MOV SI, 0x0010
    MOV DI, 0x0014
    MOV word [SI], 0x0003
    ADD AX, [SI]
    MOV word [SI + 2], 0x0002
    SUB AX, [SI + 2]
    CMP AX, 0x0006
    JE equal_label
    JNE not_equal_label
    JMP end

equal_label:
    MOV word [DI], 0x0745
    JC carry_label
    JMP end

not_equal_label:
    MOV word [DI], 0x074E
    JNC no_carry_label
    JMP end

carry_label:
    MOV word [DI + 2], 0x0743
    JMP end

no_carry_label:
    MOV word [DI + 2], 0x074E

end:
    HLT
