ORG 0x0100

section .text

start:
    MOV AX, 0xB800
    MOV DS, AX
    MOV ES, AX
    MOV AX, 0x0000
    MOV SS, AX
    MOV word [0x0000], 0x0748
    MOV word [0x0002], 0x0765
    MOV word [0x0004], 0x076C
    MOV word [0x0006], 0x076C
    MOV word [0x0008], 0x076F
    MOV AX, 0x0005
    MOV BX, 0x0003
    MOV CX, 0x0002
    MOV DX, 0x0000
    MOV word [0x0010], 0x0003
    ADD AX, [0x0010]
    MOV word [0x0012], 0x0002
    SUB AX, [0x0012]
    CMP AX, 0x0006
    JE equal_label
    MOV word [0x000A], 0x0758
    JMP end
equal_label:
    MOV word [0x000A], 0x0759
end:
    HLT
