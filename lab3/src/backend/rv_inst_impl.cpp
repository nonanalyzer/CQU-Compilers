#include "backend/rv_inst_impl.h"
#include "backend/rv_def.h"
#include <sstream>

namespace rv {

std::string rv_inst::draw() const {
    std::ostringstream oss;
    oss << toString(op) << " ";
    switch (op) {
        case rvOPCODE::ADD: case rvOPCODE::SUB: case rvOPCODE::SLT: case rvOPCODE::SLTU:
        case rvOPCODE::SLL: case rvOPCODE::SRL: case rvOPCODE::SRA:
        case rvOPCODE::AND: case rvOPCODE::OR:  case rvOPCODE::XOR:
            oss << toString(rd) << ", " << toString(rs1) << ", " << toString(rs2);
            break;
        case rvOPCODE::ADDI: case rvOPCODE::SLTI: case rvOPCODE::SLTIU:
        case rvOPCODE::SLLI: case rvOPCODE::SRLI: case rvOPCODE::SRAI:
        case rvOPCODE::XORI: case rvOPCODE::ORI: case rvOPCODE::ANDI:
        case rvOPCODE::LW:
            oss << toString(rd) << ", " << toString(rs1) << ", " << imm;
            break;
        case rvOPCODE::SW:
            oss << toString(rs2) << ", " << imm << "(" << toString(rs1) << ")";
            break;
        case rvOPCODE::JAL: case rvOPCODE::JALR:
            oss << toString(rd) << ", " << label;
            break;
        case rvOPCODE::BEQ: case rvOPCODE::BNE: case rvOPCODE::BLT: case rvOPCODE::BGE:
        case rvOPCODE::BLTU: case rvOPCODE::BGEU:
            oss << toString(rs1) << ", " << toString(rs2) << ", " << label;
            break;
        case rvOPCODE::LA: case rvOPCODE::LI:
            oss << toString(rd) << ", " << label;
            break;
        case rvOPCODE::MOV:
            oss << toString(rd) << ", " << toString(rs1);
            break;
        case rvOPCODE::J:
            oss << label;
            break;
        default:
            oss << "";
    }
    return oss.str();
}

} // namespace rv
