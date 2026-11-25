#include <cstdint>
#include <array>
#include <utility>
#include <print>

#include "include/bus.hpp"
#include "include/r3000.hpp"

void R3000::addiu() {
    ImmediateArgs args(m_opcode);
    store_reg_result(args.m_rt, m_registers.reg[args.m_rs] + args.m_imm_signed);
}

void R3000::addi() {
    ImmediateArgs args(m_opcode);

    uint32_t source = m_registers.reg[args.m_rs];
    uint32_t result = source + args.m_imm_signed;

    bool overflow = (result ^ source) & (result ^ args.m_imm_signed) & 0x80000000;

    if (overflow) {
        enter_exception(Exception::OVF);
        return;
    }

    store_reg_result(args.m_rt, result);
}

void R3000::andi() {
    ImmediateArgs args(m_opcode);
    store_reg_result(args.m_rt, m_registers.reg[args.m_rs] & args.m_imm);
}

void R3000::ori() {
    ImmediateArgs args(m_opcode);
    store_reg_result(args.m_rt, m_registers.reg[args.m_rs] | args.m_imm);
}

void R3000::xori() {
    ImmediateArgs args(m_opcode);
    store_reg_result(args.m_rt, m_registers.reg[args.m_rs] ^ args.m_imm);
}

void R3000::lui() {
    ImmediateArgs args(m_opcode);
    store_reg_result(args.m_rt, args.m_imm << 0x10);
}

void R3000::slti() {
    ImmediateArgs args(m_opcode);
    int32_t source_signed = static_cast<int32_t>(m_registers.reg[args.m_rs]);
    store_reg_result(args.m_rt, source_signed < args.m_imm_signed);
}

void R3000::sltiu() {
    ImmediateArgs args(m_opcode);
    store_reg_result(args.m_rt, m_registers.reg[args.m_rs] < static_cast<uint32_t>(args.m_imm_signed));
}

void R3000::beq() {
    ImmediateArgs args(m_opcode);
    bool taken = m_registers.reg[args.m_rs] == m_registers.reg[args.m_rt];
    immediate_branch(taken, args.m_imm_signed);
}

void R3000::bgtz() {
    ImmediateArgs args(m_opcode);
    bool taken = static_cast<int32_t>(m_registers.reg[args.m_rs]) > 0;
    immediate_branch(taken, args.m_imm_signed);
}

void R3000::blez() {
    ImmediateArgs args(m_opcode);
    bool taken = static_cast<int32_t>(m_registers.reg[args.m_rs]) <= 0;
    immediate_branch(taken, args.m_imm_signed);
}

void R3000::bne() {
    ImmediateArgs args(m_opcode);
    bool taken = m_registers.reg[args.m_rs] != m_registers.reg[args.m_rt];
    immediate_branch(taken, args.m_imm_signed);
}

void R3000::j() {
    immediate_jump(m_opcode);
}

void R3000::jal() {
    link_next_instruction(0x1F);
    immediate_jump(m_opcode);
}

void R3000::lb() {
    ImmediateArgs args(m_opcode);
    uint32_t addr = m_registers.reg[args.m_rs] + args.m_imm_signed;
    int8_t data = static_cast<int8_t>(read(addr, MemoryWidth::Byte));
    start_load(args.m_rt, static_cast<int32_t>(data));
}

void R3000::lh() {
    ImmediateArgs args(m_opcode);
    uint32_t addr = m_registers.reg[args.m_rs] + args.m_imm_signed;
    int16_t data = static_cast<int16_t>(read(addr, MemoryWidth::Halfword));
    start_load(args.m_rt, static_cast<int32_t>(data));
}

void R3000::lbu() {
    ImmediateArgs args(m_opcode);
    uint32_t addr = m_registers.reg[args.m_rs] + args.m_imm_signed;
    start_load(args.m_rt, read(addr, MemoryWidth::Byte));
}

void R3000::lhu() {
    ImmediateArgs args(m_opcode);
    uint32_t addr = m_registers.reg[args.m_rs] + args.m_imm_signed;
    start_load(args.m_rt, read(addr, MemoryWidth::Halfword));
}

void R3000::lw() {
    ImmediateArgs args(m_opcode);
    uint32_t addr = m_registers.reg[args.m_rs] + args.m_imm_signed;
    start_load(args.m_rt, read(addr, MemoryWidth::Word));
}

void R3000::sb() {
    ImmediateArgs args(m_opcode);
    uint32_t addr = m_registers.reg[args.m_rs] + args.m_imm_signed;
    write(addr, m_registers.reg[args.m_rt], MemoryWidth::Byte);
}

void R3000::sh() {
    ImmediateArgs args(m_opcode);
    uint32_t addr = m_registers.reg[args.m_rs] + args.m_imm_signed;
    write(addr, m_registers.reg[args.m_rt], MemoryWidth::Halfword);
}

void R3000::sw() {
    ImmediateArgs args(m_opcode);
    uint32_t addr = m_registers.reg[args.m_rs] + args.m_imm_signed;
    write(addr, m_registers.reg[args.m_rt], MemoryWidth::Word);
}

void R3000::add() {
    RegisterArgs args(m_opcode);
    uint32_t a = m_registers.reg[args.m_rs], b = m_registers.reg[args.m_rt];

    uint32_t result = a + b;
    bool overflow = (result ^ a) & (result ^ b) & 0x80000000;

    if (overflow) {
        enter_exception(Exception::OVF);
        return;
    }

    store_reg_result(args.m_rd, result);
}

void R3000::addu() {
    RegisterArgs args(m_opcode);
    store_reg_result(args.m_rd, m_registers.reg[args.m_rs] + m_registers.reg[args.m_rt]);
}

void R3000::andr() {
    RegisterArgs args(m_opcode);
    store_reg_result(args.m_rd, m_registers.reg[args.m_rs] & m_registers.reg[args.m_rt]);
}

void R3000::div() {
    RegisterArgs args(m_opcode);

    int32_t a = static_cast<int32_t>(m_registers.reg[args.m_rs]);
    int32_t b = static_cast<int32_t>(m_registers.reg[args.m_rt]);

    int64_t dividend = static_cast<int64_t>(a);
    int64_t divisor = static_cast<int64_t>(b);

    int64_t result = -0x80000000;
    int64_t remainder = dividend;

    if (divisor == 0) {
        if (dividend >= 0) {
            result = -1;
        }

        else if (dividend < 0) {
            result = 1;
        }
    }

    else {
        result = dividend / divisor;
        remainder = dividend % divisor;
    }

    m_registers.lo = result & 0xFFFFFFFF;
    m_registers.hi = remainder & 0xFFFFFFFF;
}

void R3000::divu() {
    RegisterArgs args(m_opcode);

    uint32_t dividend = m_registers.reg[args.m_rs];
    uint32_t divisor = m_registers.reg[args.m_rt];

    uint32_t result = 0xFFFFFFFF;
    uint32_t remainder = dividend;

    if (divisor != 0) {
        result = dividend / divisor;
        remainder = dividend % divisor;
    }

    m_registers.lo = result;
    m_registers.hi = remainder;
}

void R3000::mult() {
    RegisterArgs args(m_opcode);

    int32_t a = static_cast<int32_t>(m_registers.reg[args.m_rs]);
    int32_t b = static_cast<int32_t>(m_registers.reg[args.m_rt]);

    int64_t multiplier = static_cast<int64_t>(a);
    int64_t multiplicand = static_cast<int64_t>(b);

    int64_t result = multiplier * multiplicand;

    m_registers.hi = result >> 0x20;
    m_registers.lo = result & 0xFFFFFFFF;
}

void R3000::multu() {
    RegisterArgs args(m_opcode);

    uint64_t multiplier = static_cast<uint64_t>(m_registers.reg[args.m_rs]);
    uint64_t multiplicand = static_cast<uint64_t>(m_registers.reg[args.m_rt]);

    uint64_t result = multiplier * multiplicand;
    
    m_registers.hi = result >> 0x20;
    m_registers.lo = result & 0xFFFFFFFF;
}

void R3000::orr() {
    RegisterArgs args(m_opcode);
    store_reg_result(args.m_rd, m_registers.reg[args.m_rs] | m_registers.reg[args.m_rt]);
}

void R3000::nor() {
    RegisterArgs args(m_opcode);
    uint32_t result = m_registers.reg[args.m_rs] | m_registers.reg[args.m_rt];
    store_reg_result(args.m_rd, ~result);
}

void R3000::sll() {
    RegisterArgs args(m_opcode);
    store_reg_result(args.m_rd, m_registers.reg[args.m_rt] << args.m_shift);
}

void R3000::sllv() {
    RegisterArgs args(m_opcode);
    store_reg_result(args.m_rd, m_registers.reg[args.m_rt] << m_registers.reg[args.m_rs]);
}

void R3000::sra() {
    RegisterArgs args(m_opcode);
    int32_t result = static_cast<int32_t>(m_registers.reg[args.m_rt]) >> args.m_shift;
    store_reg_result(args.m_rd, static_cast<uint32_t>(result));
}

void R3000::srav() {
    RegisterArgs args(m_opcode);
    int32_t result = static_cast<int32_t>(m_registers.reg[args.m_rt]) >> m_registers.reg[args.m_rs];
    store_reg_result(args.m_rd, static_cast<uint32_t>(result));
}

void R3000::srl() {
    RegisterArgs args(m_opcode);
    store_reg_result(args.m_rd, m_registers.reg[args.m_rt] >> args.m_shift);
}

void R3000::srlv() {
    RegisterArgs args(m_opcode);
    store_reg_result(args.m_rd, m_registers.reg[args.m_rt] >> m_registers.reg[args.m_rs]);
}

void R3000::sub() {
    RegisterArgs args(m_opcode);
    uint32_t a = m_registers.reg[args.m_rs], b = m_registers.reg[args.m_rt];

    uint32_t result = a - b;
    bool overflow = (result ^ a) & (result ^ ~b) & 0x80000000;

    if (overflow) {
        enter_exception(Exception::OVF);
        return;
    }

    store_reg_result(args.m_rd, result);
}

void R3000::subu() {
    RegisterArgs args(m_opcode);
    store_reg_result(args.m_rd, m_registers.reg[args.m_rs] - m_registers.reg[args.m_rt]);
}

void R3000::xorr() {
    RegisterArgs args(m_opcode);
    store_reg_result(args.m_rd, m_registers.reg[args.m_rs] ^ m_registers.reg[args.m_rt]);
}

void R3000::slt() {
    RegisterArgs args(m_opcode);
    int32_t a = static_cast<int32_t>(m_registers.reg[args.m_rs]);
    int32_t b = static_cast<int32_t>(m_registers.reg[args.m_rt]);
    store_reg_result(args.m_rd, a < b);
}

void R3000::sltu() {
    RegisterArgs args(m_opcode);
    store_reg_result(args.m_rd, m_registers.reg[args.m_rs] < m_registers.reg[args.m_rt]);
}

void R3000::jr() {
    RegisterArgs args(m_opcode);
    start_branch(true, m_registers.reg[args.m_rs]);
}

void R3000::jalr() {
    RegisterArgs args(m_opcode);
    uint32_t target = m_registers.reg[args.m_rs];
    link_next_instruction(args.m_rd);
    start_branch(true, target);
}

void R3000::mfhi() {
    RegisterArgs args(m_opcode);
    store_reg_result(args.m_rd, m_registers.hi);
}

void R3000::mflo() {
    RegisterArgs args(m_opcode);
    store_reg_result(args.m_rd, m_registers.lo);
}

void R3000::mthi() {
    RegisterArgs args(m_opcode);
    m_registers.hi = m_registers.reg[args.m_rs];
}

void R3000::mtlo() {
    RegisterArgs args(m_opcode);
    m_registers.lo = m_registers.reg[args.m_rs];
}

void R3000::syscall() {
    enter_exception(Exception::SYS);
}

void R3000::brk() {
    enter_exception(Exception::BP);
}

void R3000::bgez() {
    ImmediateArgs args(m_opcode);
    immediate_branch(!(m_registers.reg[args.m_rs] >> 31), args.m_imm_signed);
}

void R3000::bltz() {
    ImmediateArgs args(m_opcode);
    immediate_branch(m_registers.reg[args.m_rs] >> 31, args.m_imm_signed);
}

void R3000::bgezal() {
    bgez();
    link_next_instruction(0x1F);
}

void R3000::bltzal() {
    bltz();
    link_next_instruction(0x1F);
}