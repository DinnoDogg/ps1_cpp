#include <cstdint>
#include <vector>
#include <array>
#include <fstream>
#include <print>
#include <utility>

#include "include/bus.hpp"
#include "include/memory.hpp"
#include "include/r3000.hpp"

#include "json.hpp"

using nlohmann::json;

class TestCpu;
class TestBus;

static void init_test(TestCpu& cpu, TestBus& bus, json& test_json);
static bool check_test(TestCpu& cpu, TestBus& bus, json& test_json);

class TestBus : public Bus {
    private:
        struct Transaction {
            enum class Type {
                NOACTION, READ, WRITE, FETCH = 4
            };

            Type type;
            uint32_t address, data;
            uint8_t size;
        };

        std::vector<Transaction> m_transactions;
        json m_transaction_json;

        friend void init_test(TestCpu& cpu, TestBus& bus, json& test_json);
        friend bool check_test(TestCpu& cpu, TestBus& bus, json& test_json);

    public:
        TestBus();

        uint32_t read(uint32_t address, MemoryWidth width, bool fetch) override;
        void write(uint32_t address, uint32_t data, MemoryWidth width, bool fetch) override;
};

class TestCpu : public R3000 {
    friend void init_test(TestCpu& cpu, TestBus& bus, json& test_json);
    friend bool check_test(TestCpu& cpu, TestBus& bus, json& test_json);

    public:
        TestCpu(Bus& bus);
};

TestCpu::TestCpu(Bus& bus) :
    R3000(bus)
{
    
}

TestBus::TestBus() :
    m_transactions (2)
{

}

uint32_t TestBus::read(uint32_t address, MemoryWidth width, bool fetch) {
    Transaction::Type type = fetch ? Transaction::Type::FETCH : Transaction::Type::READ;

    uint32_t data = 0;
    uint8_t size = std::to_underlying(width); 

    int transaction_count = m_transaction_json.size();

    for (int i = 0; i < transaction_count; i++) {
        if (m_transaction_json[i]["addr"] != address) {
            continue;
        }

        data = m_transaction_json[i]["val"];
    }
    
    Transaction t = {
        type, address, data, size
    };

    m_transactions.push_back(t);
    return data;
}

void TestBus::write(uint32_t address, uint32_t data, MemoryWidth width, bool fetch) {
    uint8_t size = std::to_underlying(width);

    Transaction t = {
        Transaction::Type::WRITE, address, data, size
    };

    m_transactions.push_back(t);
}

void init_test(TestCpu& cpu, TestBus& bus, json& test_json) {
    const json& initial = test_json["initial"];
    
    const json& reg = initial["R"];
    const json& delay = initial["delay"];

    for (int i = 0; i < 0x20; i++) {
        cpu.m_registers.reg[i] = reg[i];
    }

    cpu.m_registers.hi = initial["hi"];
    cpu.m_registers.lo = initial["lo"];
    cpu.m_registers.pc = initial["PC"];

    cpu.m_control_cop.get_reg(TestCpu::SystemControl::Register::CAUSE) = initial["CAUSE"];
    cpu.m_control_cop.get_reg(TestCpu::SystemControl::Register::EPC) = initial["EPC"];
    cpu.m_control_cop.get_reg(TestCpu::SystemControl::Register::TAR) = initial["TAR"];

    cpu.m_load_delay.target = delay["load"]["target"];
    cpu.m_load_delay.value = delay["load"]["val"];
    cpu.m_load_delay.transfer = cpu.m_load_delay.target != -1;

    cpu.m_branch.taken = delay["branch"]["take"];
    cpu.m_branch.in_slot = delay["branch"]["slot"];
    cpu.m_branch.target = delay["branch"]["target"];

    bus.m_transactions.clear();
    bus.m_transaction_json = test_json["cycles"];
}

bool check_test(TestCpu& cpu, TestBus& bus, json& test_json) {
    bool success = true;

    const json& final = test_json["final"];
    const json& transactions = test_json["cycles"];
    
    const json& reg = final["R"];
    const json& delay = final["delay"];

    for (int i = 0; i < 0x20; i++) {
        uint32_t expected = reg[i], got = cpu.m_registers.reg[i];
        
        if (expected != got) {
            std::println("r{} bad. Expected: {}, got: {}.", i, expected, got);
            success = false;
        } 
    }
    
    uint32_t expected_hi = final["hi"], expected_lo = final["lo"], expected_pc = final["PC"];
    uint32_t got_hi = cpu.m_registers.hi, got_lo = cpu.m_registers.lo, got_pc = cpu.m_registers.pc;

    if (expected_hi != got_hi) {
        std::println("Register HI bad. Expected: {}, got: {}.", expected_hi, got_hi);
        success = false;
    }

    if (expected_lo != got_lo) {
        std::println("Register LO bad. Expected: {}, got: {}.", expected_lo, got_lo);
        success = false;
    }

    if (expected_pc != got_pc) {
        std::println("PC bad. Expected: {}, got: {}.", expected_pc, got_pc);
        success = false;
    }

    uint32_t expected_cause = final["CAUSE"], expected_epc = final["EPC"], expected_tar = final["TAR"];

    uint32_t got_cause = cpu.m_control_cop.get_reg(TestCpu::SystemControl::Register::CAUSE) & 0xCFFFFFFF;
    uint32_t got_epc = cpu.m_control_cop.get_reg(TestCpu::SystemControl::Register::EPC);
    uint32_t got_tar = cpu.m_control_cop.get_reg(TestCpu::SystemControl::Register::TAR);

    expected_cause &= 0xCFFFFFFF;

    if (expected_cause != got_cause) {
        std::println("CAUSE bad. Expected: {}, got: {}.", expected_cause, got_cause);
        success = false;
    }

    if (expected_epc != got_epc) {
        std::println("EPC bad. Expected: {}, got: {}.", expected_epc, got_epc);
        success = false;
    }

    if (expected_tar != got_tar) {
        std::println("TAR bad. Expected: {}, got: {}.", expected_tar, got_tar);
        success = false;
    }

    uint32_t expected_load_target = delay["load"]["target"], expected_load_value = delay["load"]["val"];
    uint32_t got_load_target = cpu.m_load_delay.target, got_load_value = cpu.m_load_delay.value; 

    if (expected_load_target != got_load_target) {
        std::println("Load target bad. Expected: {}, got: {}.", expected_load_target, got_load_target);
        success = false;
    }

    if (expected_load_value != got_load_value) {
        std::println("Load value bad. Expected: {}, got: {}.", expected_load_value, got_load_value);
        success = false;
    }

    uint32_t expected_branch_target = delay["branch"]["target"];
    bool expected_branch_slot = delay["branch"]["slot"], expected_branch_taken = delay["branch"]["take"];

    uint32_t got_branch_target = cpu.m_branch.target;
    bool got_branch_slot = cpu.m_branch.in_slot, got_branch_taken = cpu.m_branch.taken;

    if (got_branch_slot != expected_branch_slot) {
        std::println("In branch slot bad. Expected: {}, got: {}.", expected_branch_slot, got_branch_slot);
        success = false;
    }

    if (expected_branch_slot) {
        if (got_branch_taken != expected_branch_taken) {
            std::println("Branch taken bad. Expected: {}, got: {}.", expected_branch_taken, got_branch_taken);
            success = false;
        }

        if (expected_branch_target != got_branch_target) {
            std::println("Branch target bad. Expected: {}, got: {}.", expected_branch_target, got_branch_target);
            success = false;
        }
    }

    unsigned int transaction_count = transactions.size();

    for (unsigned int i = 0; i < transaction_count; i++) {
        TestBus::Transaction::Type expected_type = static_cast<TestBus::Transaction::Type>(transactions[i]["actions"]);
        uint8_t expected_size = transactions[i]["sz"];
        uint32_t expected_addr = transactions[i]["addr"], expected_data = transactions[i]["val"];

        TestBus::Transaction& t = bus.m_transactions[i];

        if (expected_type != t.type) {
            std::println("Transaction {} type bad. Expected: {}, got: {}.", i, std::to_underlying(expected_type), std::to_underlying(t.type));
            success = false;
        }

        if (expected_size != t.size) {
            std::println("Transaction {} size bad. Expected: {}, got: {}.", i, expected_size, t.size);
            success = false;
        }

        if (expected_addr != t.address) {
            std::println("Transaction {} address bad. Expected: {}, got: {}.", i, expected_addr, t.address);
            success = false;
        }

        if (expected_data != t.data) {
            std::println("Transaction {} data bad. Expected: {}, got: {}.", i, expected_data, t.data);
            success = false;
        }
    }

    return success;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::println("Requires test file provided,");
        return 1;
    }

    std::ifstream test_file(argv[1]);

    if (!test_file) {
        std::println("Failed to open test file.");
        return 1;
    }

    json test_json = json::parse(test_file);

    TestBus test_bus;
    TestCpu test_cpu(test_bus);

    unsigned int test_count = test_json.size();
    bool success = true;

    for (unsigned int i = 0; i < test_count; i++) {
        std::print("\rRunning test {} - {}/{}", argv[1], (i + 1), test_count);

        json current_test = test_json[i];
        init_test(test_cpu, test_bus, current_test);

        test_cpu.run_instruction();
        success = check_test(test_cpu, test_bus, current_test);

        if (!success) {
            test_cpu.print_state();
            std::println("\r\033[2KFailed test {} - {}/{}", argv[1], (i + 1), test_count);
            break;
        }
    }

    if (success) {
        std::println("\r\033[2KPassed test {}", argv[1]);
    }

    return 0;
}