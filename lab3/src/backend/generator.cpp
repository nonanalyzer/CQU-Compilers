#include"backend/generator.h"
#include <cstdint>

#include<assert.h>


#define TODO assert(0 && "todo")

backend::Generator::Generator(ir::Program& p, std::ofstream& f): program(p), fout(f) {}

void backend::Generator::gen() {
    // generate data section
    fout << ".data\n";
    for (const auto& gv : program.globalVal) {
        if (gv.maxlen > 0) {
            // array: allocate space and zero-initialize
            fout << gv.val.name << ": .space " << gv.maxlen * 4 << "\n";
        } else {
            // scalar: check if it has initial value in program
            // for now, default to 0 - could be enhanced to parse initial values
            fout << gv.val.name << ": .word 0\n";
        }
    }
    // generate text section
    fout << ".text\n";
    fout << ".global main\n"; // make sure main is globally visible
    for (const auto& func : program.functions) {
        gen_func(func);
    }
}

void backend::Generator::gen_func(const ir::Function& func) {
    // reset stack map
    svmap = stackVarMap();
    fout << func.name << ":\n";
    
    // pre-allocate a reasonable stack frame (e.g., 1024 bytes for safety)
    int frame = 1024;
    // prologue: allocate stack frame and save return address
    fout << "  addi sp, sp, -" << frame << "\n";
    fout << "  sw ra, " << frame - 4 << "(sp)\n";

    // handle function parameters: save from argument registers to stack
    for (size_t i = 0; i < func.ParameterList.size() && i < 8; i++) {
        std::string argReg = "a" + std::to_string(i);
        int paramOff = svmap.find_operand(func.ParameterList[i]);
        fout << "  sw " << argReg << ", " << paramOff << "(sp)   # save param " << func.ParameterList[i].name << "\n";
    }

    // generate instructions
    for (auto instPtr : func.InstVec) {
        gen_instr(*instPtr);
    }
    // epilogue: restore return address and release frame
    fout << "  lw ra, " << frame - 4 << "(sp)\n";
    fout << "  addi sp, sp, " << frame << "\n";
    fout << "  jr ra\n";
}

void backend::Generator::gen_instr(const ir::Instruction& instr) {
    using namespace ir;
    switch (instr.op) {        case Operator::def: {
            // if rhs is literal, use li; otherwise load from its slot
            if (instr.op1.type == ir::Type::IntLiteral) {
                fout << "  li t0, " << instr.op1.name << "\n";
            } else {
                loadOperand(instr.op1, "t0");
            }
            storeOperand(instr.des, "t0");
            break;
        }
        case Operator::mov: {
            loadOperand(instr.op1, "t0");
            storeOperand(instr.des, "t0");
            break;
        }        case Operator::add: {
            loadOperand(instr.op1, "t0");
            loadOperand(instr.op2, "t1");
            fout << "  add t2, t0, t1\n";
            storeOperand(instr.des, "t2");
            break;
        }
        case Operator::addi: {
            loadOperand(instr.op1, "t0");
            int imm = std::stoi(instr.op2.name);
            fout << "  addi t1, t0, " << imm << "\n";
            storeOperand(instr.des, "t1");
            break;
        }
        case Operator::sub: {
            loadOperand(instr.op1, "t0");
            loadOperand(instr.op2, "t1");
            fout << "  sub t2, t0, t1\n";
            storeOperand(instr.des, "t2");
            break;
        }
        case Operator::subi: {
            loadOperand(instr.op1, "t0");
            int imm = std::stoi(instr.op2.name);
            fout << "  addi t1, t0, -" << imm << "\n";
            storeOperand(instr.des, "t1");
            break;
        }        case Operator::load: {
            // load dest, array, index
            if (isGlobalVar(instr.op1)) {
                // loading from global array
                if (instr.op2.type == ir::Type::IntLiteral) {
                    int idx = std::stoi(instr.op2.name) * 4;
                    fout << "  la t2, " << instr.op1.name << "\n";
                    fout << "  lw t0, " << idx << "(t2)\n";
                } else {
                    loadOperand(instr.op2, "t1");
                    fout << "  slli t1, t1, 2\n";  // multiply by 4
                    fout << "  la t2, " << instr.op1.name << "\n";
                    fout << "  add t2, t2, t1\n";
                    fout << "  lw t0, 0(t2)\n";
                }
            } else {
                // loading from local array
                int arrayOff = svmap.find_operand(instr.op1);
                if (instr.op2.type == ir::Type::IntLiteral) {
                    int idx = std::stoi(instr.op2.name) * 4;
                    int addr_off = arrayOff + idx;
                    fout << "  lw t0, " << addr_off << "(sp)\n";
                } else {
                    loadOperand(instr.op2, "t1");
                    fout << "  slli t1, t1, 2\n";
                    fout << "  addi t2, sp, " << arrayOff << "\n";
                    fout << "  add t2, t2, t1\n";
                    fout << "  lw t0, 0(t2)\n";
                }
            }
            storeOperand(instr.des, "t0");
            break;
        }
        case Operator::store: {
            // store value, array, index
            loadOperand(instr.des, "t0");  // value to store
            if (isGlobalVar(instr.op1)) {
                // storing to global array
                if (instr.op2.type == ir::Type::IntLiteral) {
                    int idx = std::stoi(instr.op2.name) * 4;
                    fout << "  la t2, " << instr.op1.name << "\n";
                    fout << "  sw t0, " << idx << "(t2)\n";
                } else {
                    loadOperand(instr.op2, "t1");
                    fout << "  slli t1, t1, 2\n";
                    fout << "  la t2, " << instr.op1.name << "\n";
                    fout << "  add t2, t2, t1\n";
                    fout << "  sw t0, 0(t2)\n";
                }
            } else {
                // storing to local array
                int arrayOff = svmap.find_operand(instr.op1);
                if (instr.op2.type == ir::Type::IntLiteral) {
                    int idx = std::stoi(instr.op2.name) * 4;
                    int addr_off = arrayOff + idx;
                    fout << "  sw t0, " << addr_off << "(sp)\n";
                } else {
                    loadOperand(instr.op2, "t1");
                    fout << "  slli t1, t1, 2\n";
                    fout << "  addi t2, sp, " << arrayOff << "\n";
                    fout << "  add t2, t2, t1\n";
                    fout << "  sw t0, 0(t2)\n";
                }
            }
            break;
        }case Operator::_return: {
            if (instr.op1.name != "null") {
                loadOperand(instr.op1, "a0");
            }
            break;
        }
        case ir::Operator::fdef: {
            int offd = svmap.find_operand(instr.des);
            if (instr.op1.type == ir::Type::FloatLiteral) {
                fout << "  li t0, " << instr.op1.name << "\n";
                fout << "  fmv.w.x ft0, t0\n";
            } else {
                int srcOff = svmap.find_operand(instr.op1);
                fout << "  flw ft0, " << srcOff << "(sp)\n";
            }
            fout << "  fsw ft0, " << offd << "(sp)\n";
            break;
        }
        case ir::Operator::fmov: {
            int off1 = svmap.find_operand(instr.op1);
            int offd = svmap.find_operand(instr.des);
            fout << "  flw ft0, " << off1 << "(sp)\n";
            fout << "  fsw ft0, " << offd << "(sp)\n";
            break;
        }
        case ir::Operator::fadd: case ir::Operator::fsub: case ir::Operator::fmul: case ir::Operator::fdiv: {
            int off1 = svmap.find_operand(instr.op1);
            int off2 = svmap.find_operand(instr.op2);
            int offd = svmap.find_operand(instr.des);
            fout << "  flw ft0, " << off1 << "(sp)\n";
            fout << "  flw ft1, " << off2 << "(sp)\n";
            std::string op = (instr.op == ir::Operator::fadd ? "fadd.s" : instr.op == ir::Operator::fsub ? "fsub.s" : instr.op == ir::Operator::fmul ? "fmul.s" : "fdiv.s");
            fout << "  " << op << " ft2, ft0, ft1\n";
            fout << "  fsw ft2, " << offd << "(sp)\n";
            break;
        }        case ir::Operator::mul: case ir::Operator::div: case ir::Operator::mod: {
            loadOperand(instr.op1, "t0");
            loadOperand(instr.op2, "t1");
            std::string op = (instr.op == ir::Operator::mul ? "mul" : instr.op == ir::Operator::div ? "div" : "rem");
            fout << "  " << op << " t2, t0, t1\n";
            storeOperand(instr.des, "t2");
            break;
        }
        case ir::Operator::lss: case ir::Operator::flss: case ir::Operator::leq: case ir::Operator::fleq:
        case ir::Operator::gtr: case ir::Operator::fgtr: case ir::Operator::geq: case ir::Operator::fgeq: {
            // only implement integer '<' and '>' for now
            int off1 = svmap.find_operand(instr.op1);
            int off2 = svmap.find_operand(instr.op2);
            int offd = svmap.find_operand(instr.des);
            fout << "  lw t0, " << off1 << "(sp)\n";
            fout << "  lw t1, " << off2 << "(sp)\n";
            if (instr.op == ir::Operator::lss || instr.op == ir::Operator::fgtr) {
                fout << "  slt t2, t0, t1\n";
            } else if (instr.op == ir::Operator::gtr || instr.op == ir::Operator::fleq) {
                fout << "  slt t2, t1, t0\n";
            } else {
                fout << "  # TODO complex compare for " << instr.draw() << "\n";
            }
            fout << "  sw t2, " << offd << "(sp)\n";
            break;
        }
        case ir::Operator::eq: case ir::Operator::feq: case ir::Operator::neq: case ir::Operator::fneq: {
            int off1 = svmap.find_operand(instr.op1);
            int off2 = svmap.find_operand(instr.op2);
            int offd = svmap.find_operand(instr.des);
            fout << "  lw t0, " << off1 << "(sp)\n";
            fout << "  lw t1, " << off2 << "(sp)\n";
            fout << "  xor t2, t0, t1\n";
            if (instr.op == ir::Operator::eq || instr.op == ir::Operator::feq) {
                fout << "  sltiu t2, t2, 1\n";
            } else {
                fout << "  sltu t2, x0, t2\n";
            }
            fout << "  sw t2, " << offd << "(sp)\n";
            break;
        }
        case ir::Operator::_not: {
            int off1 = svmap.find_operand(instr.op1);
            int offd = svmap.find_operand(instr.des);
            fout << "  lw t0, " << off1 << "(sp)\n";
            fout << "  seqz t1, t0   # requires RV64 or pseudo, fallback to xor/sltiu if not available\n";
            fout << "  sw t1, " << offd << "(sp)\n";
            break;
        }
        case ir::Operator::_and: case ir::Operator::_or: {
            int off1 = svmap.find_operand(instr.op1);
            int off2 = svmap.find_operand(instr.op2);
            int offd = svmap.find_operand(instr.des);
            fout << "  lw t0, " << off1 << "(sp)\n";
            fout << "  lw t1, " << off2 << "(sp)\n";
            std::string op = (instr.op == ir::Operator::_and ? "and" : "or");
            fout << "  " << op << " t2, t0, t1\n";
            fout << "  sw t2, " << offd << "(sp)\n";
            break;
        }
        case ir::Operator::cvt_i2f: case ir::Operator::cvt_f2i: {
            // simple conversion: int->float or float->int via fmv
            int off1 = svmap.find_operand(instr.op1);
            int offd = svmap.find_operand(instr.des);
            if (instr.op == ir::Operator::cvt_i2f) {
                fout << "  lw t0, " << off1 << "(sp)\n";
                fout << "  fmv.w.x ft0, t0\n";
                fout << "  fsw ft0, " << offd << "(sp)\n";
            } else {
                fout << "  flw ft0, " << off1 << "(sp)\n";
                fout << "  fmv.x.w t0, ft0\n";
                fout << "  sw t0, " << offd << "(sp)\n";
            }
            break;
        }
        case ir::Operator::getptr: {
            int off1 = svmap.find_operand(instr.op1);
            int idx = std::stoi(instr.op2.name) * 4;
            int offd = svmap.add_operand(instr.des, 8);
            fout << "  addi t0, sp, " << off1 << "   # base ptr\n";
            fout << "  addi t1, t0, " << idx << "   # element ptr\n";
            fout << "  sw t1, " << offd << "(sp)   # store new ptr\n";
            break;
        }        case ir::Operator::call: {
            // check if this is a CallInst with arguments
            const ir::CallInst* callInst = dynamic_cast<const ir::CallInst*>(&instr);
            if (callInst && !callInst->argumentList.empty()) {
                // handle function arguments - pass via a0, a1, a2, etc.
                for (size_t i = 0; i < callInst->argumentList.size() && i < 8; i++) {
                    std::string argReg = "a" + std::to_string(i);
                    loadOperand(callInst->argumentList[i], argReg);
                }
                fout << "  jal ra, " << instr.op1.name << "   # call function with args\n";
            } else {
                fout << "  jal ra, " << instr.op1.name << "   # call function\n";
            }
            int offd = svmap.find_operand(instr.des);
            fout << "  sw a0, " << offd << "(sp)   # save return value\n";
            break;
        }
        case ir::Operator::_goto: {
            if (instr.op1.name != "null") {
                fout << "  bnez t0, label_" << instr.des.name << "   # conditional goto\n";
            } else {
                fout << "  j label_" << instr.des.name << "   # unconditional goto\n";
            }
            break;
        }
        case ir::Operator::__unuse__: {
            // no-op
            break;
        }
        case Operator::alloc: {
            // skip alloc for global arrays - they're already allocated in .data section
            if (!isGlobalVar(instr.des)) {
                int size = std::stoi(instr.op1.name) * 4;
                svmap.add_operand(instr.des, size);
                fout << "  # alloc " << instr.des.name << " size=" << size << "\n";
            }
            break;
        }
        default:
            fout << "  # " << instr.draw() << "\n";
    }
}

// Helper function to check if operand is global variable
bool backend::Generator::isGlobalVar(const ir::Operand& op) {
    for (const auto& gv : program.globalVal) {
        if (gv.val.name == op.name) {
            return true;
        }
    }
    return false;
}

// Helper function to load operand (global or local)
void backend::Generator::loadOperand(const ir::Operand& op, const std::string& reg) {
    if (isGlobalVar(op)) {
        fout << "  lw " << reg << ", " << op.name << "\n";
    } else {
        int off = svmap.find_operand(op);
        fout << "  lw " << reg << ", " << off << "(sp)\n";
    }
}

// Helper function to store to operand (global or local)
void backend::Generator::storeOperand(const ir::Operand& op, const std::string& reg) {
    if (isGlobalVar(op)) {
        fout << "  la t3, " << op.name << "\n";
        fout << "  sw " << reg << ", 0(t3)\n";
    } else {
        int off = svmap.find_operand(op);
        fout << "  sw " << reg << ", " << off << "(sp)\n";
    }
}

// stackVarMap implementation
int backend::stackVarMap::find_operand(ir::Operand op) {
    auto it = _table.find(op);
    if (it == _table.end()) {
        // allocate default 4-byte slot for new variable (e.g., temp)
        int offset = add_operand(op, 4);
        return offset;
    }
    return it->second;
}

int backend::stackVarMap::add_operand(ir::Operand op, uint32_t size) {
    next_offset += static_cast<int>(size);
    _table[op] = next_offset;
    return next_offset;
}

// register allocation helpers

rv::rvREG backend::Generator::getRd(ir::Operand op) {
    // TODO: map destination operand to a register
    TODO;
}

rv::rvFREG backend::Generator::fgetRd(ir::Operand op) {
    // TODO: map floating-point destination
    TODO;
}

rv::rvREG backend::Generator::getRs1(ir::Operand op) {
    // TODO: map first source operand to register
    TODO;
}

rv::rvREG backend::Generator::getRs2(ir::Operand op) {
    // TODO: map second source operand to register
    TODO;
}

rv::rvFREG backend::Generator::fgetRs1(ir::Operand op) {
    // TODO: map first FP source operand
    TODO;
}

rv::rvFREG backend::Generator::fgetRs2(ir::Operand op) {
    // TODO: map second FP source operand
    TODO;
}