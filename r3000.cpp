#include <cstdint>
#include <array>
#include <utility>
#include <print>

#include "include/bus.hpp"
#include "include/r3000.hpp"

R3000::R3000(Bus& bus) :
    m_cop {&m_control_cop, nullptr, nullptr, nullptr},
    m_bus {bus}
{

}

uint32_t R3000::get_memory_alignment(MemoryWidth width) {
    switch (width) {
        case MemoryWidth::Word:  
            return 3;

        case MemoryWidth::Halfword:  
            return 1;

        default:
            return 0;
    }
}

uint32_t R3000::get_data_mask(MemoryWidth width) {
    switch (width) {
        case MemoryWidth::Byte:  
            return 0xFF;

        case MemoryWidth::Halfword:  
            return 0xFFFF;

        default:
            return 0xFFFFFFFF;
    }
}

bool R3000::invalid_address(uint32_t address, MemoryWidth width, bool read) {
    bool user_mode = (m_control_cop.get_reg(SystemControl::Register::SR) >> 1) & 0x1; 
    uint32_t alignment_mask = get_memory_alignment(width);
    
    bool error = (address & alignment_mask) /*|| (address > 0x7FFFFFFF && user_mode)*/;

    if (error) {
        Exception exception = read ? Exception::ADEL : Exception::ADES;
        m_control_cop.get_reg(SystemControl::Register::BADA) = address;
        enter_exception(exception);
    }

    return error;
}

uint32_t R3000::read_cop(uint8_t cop, uint8_t reg) {
    uint8_t c = cop & 0x3;

    if (m_cop[c] == nullptr) {
        return 0;
    }

    return m_cop[c]->read_reg(reg & 0xF);
}

void R3000::write_cop(uint8_t cop, uint8_t reg, uint32_t data) {
    uint8_t c = cop & 0x3;

    if (m_cop[c] == nullptr) {
        return;
    }

    m_cop[c]->write_reg(reg & 0xF, data);
}

void R3000::store_reg_result(uint8_t reg, uint32_t value) {
    if (m_load_delay.transfer && m_load_delay.target == reg) {
        end_load();
    }

    m_registers.reg[reg] = value;
}

void R3000::start_load(uint8_t reg, uint32_t value) {
    if (m_memory_error) {
        return;
    }

    if (m_load_delay.transfer && m_load_delay.target != reg) {
        m_registers.reg[m_load_delay.target & 0x1F] = m_load_delay.value;
    }

    m_load_delay.target = reg;
    m_load_delay.value = value;
    m_load_delay.transfer = false;
}

void R3000::start_branch(bool taken, uint32_t target) {
    m_branch.taken = taken;
    m_branch.target = target;
    m_next_op_branch = true;
}

void R3000::end_load() {
    m_load_delay.target = -1;
    m_load_delay.value = 0;
    m_load_delay.transfer = false;
}

void R3000::end_branch() {
    m_branch.target = 0;
    m_branch.taken = false;
}

uint32_t R3000::get_physical_address(uint32_t address) {
    return address;
}

uint32_t R3000::read(uint32_t address, MemoryWidth width) {
    m_memory_error = invalid_address(address, width, true);

    if (m_memory_error) {
        return 0;
    }

    return m_bus.read(address, width, false) & get_data_mask(width);
}

void R3000::write(uint32_t address, uint32_t data, MemoryWidth width) {
    m_memory_error = invalid_address(address, width, false);

    if (m_memory_error) {
        return;
    }

    m_bus.write(address, data, width, false);
}

uint32_t R3000::fetch() {
    m_next_pc = m_registers.pc + 4;
    return m_bus.read(m_registers.pc, MemoryWidth::Word, true);
}

void R3000::enter_exception(Exception exception) {
    uint32_t pc = m_registers.pc;
    uint8_t cause = std::to_underlying(exception);
    
    uint32_t& reg_cause = m_control_cop.get_reg(SystemControl::Register::CAUSE);
    uint32_t& reg_sr = m_control_cop.get_reg(SystemControl::Register::SR);

    uint8_t user_irq_enable = reg_sr & 0xF;

    reg_cause &= 0xFF80;
    reg_cause |= (cause << 2);

    if (m_branch.in_slot) {
        pc -= 4;
        reg_cause |= 1 << 31;

        if (m_branch.taken) {
            reg_cause |= 1 << 30;
            m_control_cop.get_reg(SystemControl::Register::TAR) = m_branch.target;
        }
    }

    reg_sr &= ~0x3F;
    reg_sr |= user_irq_enable << 2;

    m_control_cop.get_reg(SystemControl::Register::EPC) = pc;
    m_next_pc = m_exception_vector;
}

void R3000::immediate_branch(bool taken, int32_t imm) {
    start_branch(taken, m_next_pc + (imm << 2));
}

void R3000::immediate_jump(uint32_t opcode) {
    uint32_t target = (opcode & 0x3FFFFFF) << 2;
    target |= (m_next_pc & 0xF0000000);
    start_branch(true, target); 
}

void R3000::link_next_instruction(uint8_t reg) {
    store_reg_result(reg, m_next_pc + 4);
}

void R3000::print_state() {
    for (int i = 0; i < 0x20; i++) {
        std::println("r{}: {}", i, m_registers.reg[i]);
    }
    
    std::println();

    std::println("lo: {}", m_registers.lo);
    std::println("hi: {}", m_registers.hi);

    std::println();
    std::println("pc: {}", m_registers.pc);

    std::println();
    std::println("System control");
    std::println("CAUSE: {}", m_control_cop.get_reg(SystemControl::Register::CAUSE));
    std::println("EPC: {}", m_control_cop.get_reg(SystemControl::Register::EPC));
    std::println("TAR: {}", m_control_cop.get_reg(SystemControl::Register::TAR));

    std::println();
    std::println("Load delay target: {}", m_load_delay.target);
    std::println("Load delay value: {}", m_load_delay.value);

    std::println();
    std::println("Is branch slot: {}", m_branch.in_slot);
    std::println("Branch taken: {}", m_branch.taken);
    std::println("Branch target: {}", m_branch.target);
}

uint32_t R3000::SystemControl::read_reg(uint8_t reg) {
    return m_reg[reg & 0xF];
}

void R3000::SystemControl::write_reg(uint8_t reg, uint32_t data) {
    m_reg[reg & 0xF] = data;
}

uint32_t& R3000::SystemControl::get_reg(SystemControl::Register reg) {
    uint8_t reg_number = std::to_underlying(reg);
    return m_reg[reg_number];
}

R3000::ImmediateArgs::ImmediateArgs(uint32_t opcode) {
    int16_t imm = opcode & 0xFFFF;

    m_rs = (opcode >> 21) & 0x1F; 
    m_rt = (opcode >> 16) & 0x1F; 
    
    m_imm = static_cast<uint16_t>(imm);
    m_imm_signed = static_cast<int32_t>(imm);
}

R3000::RegisterArgs::RegisterArgs(uint32_t opcode) {
    m_rs = (opcode >> 21) & 0x1F; 
    m_rt = (opcode >> 16) & 0x1F;  
    m_rd = (opcode >> 11) & 0x1F; 
    m_shift = (opcode >> 6) & 0x1F; 
}

void R3000::run_instr_reg() {
    uint8_t op = m_opcode & 0x3F;

    switch (op) {
        case 0x20: add(); break;
        case 0x21: addu(); break;
        case 0x24: andr(); break;
        case 0x1A: div(); break;
        case 0x1B: divu(); break;
        case 0x18: mult(); break;
        case 0x19: multu(); break;
        case 0x27: nor(); break;
        case 0x25: orr(); break;
        case 0x0: sll(); break;
        case 0x4: sllv(); break;
        case 0x3: sra(); break;
        case 0x7: srav(); break;
        case 0x2: srl(); break;
        case 0x6: srlv(); break;
        case 0x22: sub(); break;
        case 0x23: subu(); break;
        case 0x26: xorr(); break;
        case 0x2A: slt(); break;
        case 0x2B: sltu(); break;
        case 0x8: jr(); break;
        case 0x9: jalr(); break;
        case 0x10: mfhi(); break;
        case 0x11: mthi(); break;
        case 0x12: mflo(); break;
        case 0x13: mtlo(); break;
        case 0xC: syscall(); break;
        case 0xD: brk(); break;
    }
}

void R3000::run_instr_regimm() {
    uint8_t op = (m_opcode >> 16) & 0x1F;
    bool run_bgez = op & 0x1;

    op >>= 1;

    if (op == 0b1000) {
        run_bgez ? bgezal() : bltzal();
        return;
    }

    run_bgez ? bgez() : bltz();
}

void R3000::run_instruction() {
    m_opcode = fetch();

    if (m_branch.in_slot && m_branch.taken) {
        m_next_pc = m_branch.target;
    }

    uint8_t op = m_opcode >> 26;

    if (op == 0) {
        run_instr_reg();
    }

    else if (op == 1) {
        run_instr_regimm();
    }

    else {
        switch (op) {
            case 0x9: addiu(); break;
            case 0x8: addi(); break;
            case 0xC: andi(); break;
            case 0xD: ori(); break;
            case 0xE: xori(); break;
            case 0xF: lui(); break;
            case 0xA: slti(); break;
            case 0xB: sltiu(); break;
            case 0x4: beq(); break;
            case 0x7: bgtz(); break;
            case 0x6: blez(); break;
            case 0x5: bne(); break;
            case 0x2: j(); break;
            case 0x3: jal(); break;
            case 0x20: lb(); break;
            case 0x21: lh(); break;
            case 0x24: lbu(); break;
            case 0x25: lhu(); break;
            case 0x23: lw(); break;
            case 0x28: sb(); break;
            case 0x29: sh(); break;
            case 0x2B: sw(); break;
        }
    }

    if (m_load_delay.transfer) {
        m_registers.reg[m_load_delay.target & 0x1F] = m_load_delay.value;
        end_load();
    }

    m_registers.reg[0] = 0;
    m_registers.pc = m_next_pc;

    m_branch.in_slot = m_next_op_branch;
    m_next_op_branch = false;

    m_load_delay.transfer = m_load_delay.target != -1;
}