#ifndef R3000_HPP
#define R3000_HPP

#include <cstdint>
#include <array>

#include "bus.hpp"
#include "memory.hpp"

class R3000 {
    protected:
        class Coprocessor {
            public:
                virtual uint32_t read_reg(uint8_t reg) = 0;
                virtual void write_reg(uint8_t reg, uint32_t data) = 0;

                virtual ~Coprocessor() = default;
        };

        class SystemControl : public Coprocessor {
            private:
                std::array<uint32_t, 0x10> m_reg = {};

            public:
                enum class Register {
                    BPC = 0x3, 
                    BDA = 0x5, TAR, DCIC, BADA, BDAM,
                    BPCM = 0xB, SR, CAUSE, EPC, PRID
                };

                uint32_t read_reg(uint8_t reg) override;
                void write_reg(uint8_t reg, uint32_t data) override;

                uint32_t& get_reg(SystemControl::Register reg);
        };

        class ImmediateArgs {
            public:
                ImmediateArgs(uint32_t opcode);
                uint8_t m_rs, m_rt;
                uint16_t m_imm;
                int32_t m_imm_signed;
        };

        class RegisterArgs {
            public:
                RegisterArgs(uint32_t opcode);
                uint8_t m_rs, m_rt, m_rd, m_shift;
        };

        struct {
            std::array<uint32_t, 0x20> reg = {};
            uint32_t pc = 0, hi = 0, lo = 0;
        } m_registers;

        struct {
            bool transfer = false;
            int target = 0;
            uint32_t value = 0;
        } m_load_delay;

        struct {
            bool in_slot = false, taken = false;
            uint32_t target = 0;
        } m_branch;

        enum class Exception {
            INT, MOD, TLBL, TLBS, ADEL, ADES, 
            IBE, DBE, SYS, BP, RI, CPU, OVF
        };

        static constexpr uint32_t m_exception_vector = 0x80000080;

        static uint32_t get_memory_alignment(MemoryWidth width);
        static uint32_t get_data_mask(MemoryWidth width);

        uint32_t m_next_pc = 0;
        uint32_t m_opcode = 0;

        bool m_memory_error = false;
        bool m_next_op_branch = false;

        SystemControl m_control_cop;
        std::array<Coprocessor*, 0x4> m_cop;

        Bus& m_bus;

        bool invalid_address(uint32_t address, MemoryWidth width, bool read);

        uint32_t read_cop(uint8_t cop, uint8_t reg);
        void write_cop(uint8_t cop, uint8_t reg, uint32_t data);

        void store_reg_result(uint8_t reg, uint32_t value);

        void start_load(uint8_t reg, uint32_t value);
        void start_branch(bool taken, uint32_t target);

        void end_load();
        void end_branch();

        uint32_t fetch();

        void enter_exception(Exception exception);

        void immediate_branch(bool taken, int32_t imm);
        void immediate_jump(uint32_t opcode);

        void link_next_instruction(uint8_t reg);

        void run_instr_reg();
        void run_instr_imm();
        void run_instr_regimm();

        virtual uint32_t get_physical_address(uint32_t address);

        virtual uint32_t read(uint32_t address, MemoryWidth width);
        virtual void write(uint32_t address, uint32_t data, MemoryWidth width);

        //immediate instruction
        void addiu();
        void addi();
        void andi();
        void ori();
        void xori();
        void lui();
        void slti();
        void sltiu();
        void beq();
        void bgtz();
        void blez();
        void bne();
        void j();
        void jal();
        void lb();
        void lh();
        void lbu();
        void lhu();
        void lw();
        void sb();
        void sh();
        void sw();

        //register instructions
        void add();
        void addu();
        void andr();
        void div();
        void divu();
        void mult();
        void multu();
        void nor();
        void orr();
        void sll();
        void sllv();
        void sra();
        void srav();
        void srl();
        void srlv();
        void sub();
        void subu();
        void xorr();
        void slt();
        void sltu();
        void jr();
        void jalr();
        void mfhi();
        void mflo();
        void mthi();
        void mtlo();
        void syscall();
        void brk();

        //regimm instructions
        void bgezal();
        void bltzal();
        void bgez();
        void bltz();

    public:
        R3000(Bus& bus);
        ~R3000() = default;
        
        void print_state();

        void run_instruction();
        void set_bus_error();

        uint8_t irq;
};

#endif