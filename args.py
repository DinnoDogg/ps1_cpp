import sys 

arg_len = len(sys.argv)

if (arg_len < 2):
    print("Provide opcode")
else:
    opcode = int(sys.argv[1])

    imm_rs = (opcode >> 21) & 0x1F
    imm_rt = (opcode >> 16) & 0x1F
    imm_value = opcode & 0xFFFF

    print("Immediate args")
    print("rs", imm_rs)
    print("rt", imm_rt)
    print("imm", imm_value)


