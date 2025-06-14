#include"backend/generator.h"
#include <cstdint>
#include <set>
#include <cctype>

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
    fout << "  sw ra, " << frame - 4 << "(sp)\n";    // handle function parameters
    // First 8 params come from argument registers a0-a7, save to stack
    for (size_t i = 0; i < func.ParameterList.size() && i < 8; i++) {
        std::string argReg = "a" + std::to_string(i);
        int paramOff = svmap.find_operand(func.ParameterList[i]);
        fout << "  sw " << argReg << ", " << paramOff << "(sp)   # save param " << func.ParameterList[i].name << "\n";
    }
    
    // Params 8+ are already on stack from caller, but we need to map them
    // to our local stack space for consistent access
    if (func.ParameterList.size() > 8) {
        for (size_t i = 8; i < func.ParameterList.size(); i++) {
            // Calculate caller's stack offset for this parameter
            // The caller pushed args in reverse order, so:
            // arg[8] is at caller_sp + 0, arg[9] at caller_sp + 4, etc.
            int callerArgOffset = (i - 8) * 4;
            int paramOff = svmap.find_operand(func.ParameterList[i]);
            
            // Copy from caller's stack to our local stack space
            fout << "  lw t0, " << (frame + callerArgOffset) << "(sp)  # load param " << i << " from caller stack\n";
            fout << "  sw t0, " << paramOff << "(sp)   # save param " << func.ParameterList[i].name << "\n";
        }
    }// first pass: collect all jump targets
    std::set<int> jumpTargets;
    for (size_t i = 0; i < func.InstVec.size(); i++) {
        const auto& instr = *func.InstVec[i];
        if (instr.op == ir::Operator::_goto) {
            // check if des is a number (relative offset)
            bool isNumber = true;
            std::string desName = instr.des.name;
            // handle negative numbers
            size_t startPos = 0;
            if (!desName.empty() && desName[0] == '-') {
                startPos = 1;
            }
            for (size_t j = startPos; j < desName.length(); j++) {
                if (!std::isdigit(desName[j])) {
                    isNumber = false;
                    break;
                }
            }
            if (isNumber && !desName.empty()) {
                int offset = std::stoi(desName);
                int target = static_cast<int>(i) + offset;
                if (target >= 0 && target < static_cast<int>(func.InstVec.size())) {
                    jumpTargets.insert(target);
                }
            }
        }
    }    // generate instructions with labels
    for (size_t i = 0; i < func.InstVec.size(); i++) {
        // insert label if this instruction is a jump target
        if (jumpTargets.find(static_cast<int>(i)) != jumpTargets.end()) {
            fout << func.name << "_label_" << i << ":\n";
        }
        gen_instr(*func.InstVec[i], static_cast<int>(i), func.name, &func);
    }
    // epilogue: restore return address and release frame
    fout << "  lw ra, " << frame - 4 << "(sp)\n";
    fout << "  addi sp, sp, " << frame << "\n";
    fout << "  jr ra\n";
}

void backend::Generator::gen_instr(const ir::Instruction& instr, int pc, const std::string& funcName, const ir::Function* func) {
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
                // check if this is a pointer parameter (not a local array)
                bool isLocalArray = false;
                if (func) {
                    for (const auto& instPtr : func->InstVec) {
                        if (instPtr->op == ir::Operator::alloc && instPtr->des.name == instr.op1.name) {
                            isLocalArray = true;
                            break;
                        }
                    }
                }
                
                if (!isLocalArray && instr.op1.name.find("_scope_") != std::string::npos) {
                    // This is likely a pointer parameter
                    loadOperand(instr.op1, "t2");  // load pointer address
                    if (instr.op2.type == ir::Type::IntLiteral) {
                        int idx = std::stoi(instr.op2.name) * 4;
                        fout << "  lw t0, " << idx << "(t2)\n";
                    } else {
                        loadOperand(instr.op2, "t1");
                        fout << "  slli t1, t1, 2\n";
                        fout << "  add t2, t2, t1\n";
                        fout << "  lw t0, 0(t2)\n";
                    }                } else {
                    // loading from local array
                    int arrayOff = svmap.find_operand(instr.op1);
                    if (instr.op2.type == ir::Type::IntLiteral) {
                        int idx = std::stoi(instr.op2.name);
                        fout << "  li t1, " << idx << "\n";
                        fout << "  slli t1, t1, 2\n";
                        fout << "  addi t2, sp, " << arrayOff << "\n";
                        fout << "  add t2, t2, t1\n";
                        fout << "  lw t0, 0(t2)\n";
                    } else {
                        loadOperand(instr.op2, "t1");
                        fout << "  slli t1, t1, 2\n";
                        fout << "  addi t2, sp, " << arrayOff << "\n";
                        fout << "  add t2, t2, t1\n";
                        fout << "  lw t0, 0(t2)\n";
                    }
                }
            }
            storeOperand(instr.des, "t0");
            break;
        }        case Operator::store: {
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
                // check if this is a pointer parameter (not a local array)
                bool isLocalArray = false;
                if (func) {
                    for (const auto& instPtr : func->InstVec) {
                        if (instPtr->op == ir::Operator::alloc && instPtr->des.name == instr.op1.name) {
                            isLocalArray = true;
                            break;
                        }
                    }
                }
                
                if (!isLocalArray && instr.op1.name.find("_scope_") != std::string::npos) {
                    // This is likely a pointer parameter
                    loadOperand(instr.op1, "t2");  // load pointer address
                    if (instr.op2.type == ir::Type::IntLiteral) {
                        int idx = std::stoi(instr.op2.name) * 4;
                        fout << "  sw t0, " << idx << "(t2)\n";
                    } else {
                        loadOperand(instr.op2, "t1");
                        fout << "  slli t1, t1, 2\n";
                        fout << "  add t2, t2, t1\n";
                        fout << "  sw t0, 0(t2)\n";
                    }                } else {
                    // storing to local array
                    int arrayOff = svmap.find_operand(instr.op1);
                    if (instr.op2.type == ir::Type::IntLiteral) {
                        int idx = std::stoi(instr.op2.name);
                        fout << "  li t1, " << idx << "\n";
                        fout << "  slli t1, t1, 2\n";
                        fout << "  addi t2, sp, " << arrayOff << "\n";
                        fout << "  add t2, t2, t1\n";
                        fout << "  sw t0, 0(t2)\n";
                    } else {
                        loadOperand(instr.op2, "t1");
                        fout << "  slli t1, t1, 2\n";
                        fout << "  addi t2, sp, " << arrayOff << "\n";
                        fout << "  add t2, t2, t1\n";
                        fout << "  sw t0, 0(t2)\n";
                    }
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
        }        case ir::Operator::lss: case ir::Operator::flss: case ir::Operator::leq: case ir::Operator::fleq:
        case ir::Operator::gtr: case ir::Operator::fgtr: case ir::Operator::geq: case ir::Operator::fgeq: {
            loadOperand(instr.op1, "t0");
            loadOperand(instr.op2, "t1");
            if (instr.op == ir::Operator::lss || instr.op == ir::Operator::flss) {
                // t0 < t1
                fout << "  slt t2, t0, t1\n";
            } else if (instr.op == ir::Operator::gtr || instr.op == ir::Operator::fgtr) {
                // t0 > t1  equivalent to  t1 < t0
                fout << "  slt t2, t1, t0\n";
            } else if (instr.op == ir::Operator::leq || instr.op == ir::Operator::fleq) {
                // t0 <= t1  equivalent to  !(t0 > t1)  equivalent to  !(t1 < t0)
                fout << "  slt t2, t1, t0\n";
                fout << "  seqz t2, t2\n";
            } else if (instr.op == ir::Operator::geq || instr.op == ir::Operator::fgeq) {
                // t0 >= t1  equivalent to  !(t0 < t1)
                fout << "  slt t2, t0, t1\n";
                fout << "  seqz t2, t2\n";
            }
            storeOperand(instr.des, "t2");
            break;
        }
        case ir::Operator::eq: case ir::Operator::feq: case ir::Operator::neq: case ir::Operator::fneq: {
            loadOperand(instr.op1, "t0");
            loadOperand(instr.op2, "t1");
            fout << "  xor t2, t0, t1\n";
            if (instr.op == ir::Operator::eq || instr.op == ir::Operator::feq) {
                fout << "  sltiu t2, t2, 1\n";
            } else {
                fout << "  sltu t2, x0, t2\n";
            }
            storeOperand(instr.des, "t2");
            break;
        }
        case ir::Operator::_not: {
            loadOperand(instr.op1, "t0");
            fout << "  seqz t1, t0   # requires RV64 or pseudo, fallback to xor/sltiu if not available\n";
            storeOperand(instr.des, "t1");
            break;
        }
        case ir::Operator::_and: case ir::Operator::_or: {
            loadOperand(instr.op1, "t0");
            loadOperand(instr.op2, "t1");
            std::string op = (instr.op == ir::Operator::_and ? "and" : "or");
            fout << "  " << op << " t2, t0, t1\n";
            storeOperand(instr.des, "t2");
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
                // handle function arguments
                // First 8 args go to registers a0-a7
                // Additional args go on stack (pushed in reverse order)
                
                // First, push args 8+ onto stack (in reverse order for correct access)
                if (callInst->argumentList.size() > 8) {
                    for (int i = callInst->argumentList.size() - 1; i >= 8; i--) {
                        const auto& arg = callInst->argumentList[i];
                        
                        // check if this argument is a local array
                        bool isLocalArray = false;
                        if (func) {
                            for (const auto& instPtr : func->InstVec) {
                                if (instPtr->op == ir::Operator::alloc && instPtr->des.name == arg.name) {
                                    isLocalArray = true;
                                    break;
                                }
                            }
                        }
                        
                        if (isLocalArray && !isGlobalVar(arg)) {
                            // this is a local array - push its address
                            int arrayOff = svmap.find_operand(arg);
                            fout << "  addi t0, sp, " << arrayOff << "   # get array address\n";
                            fout << "  addi sp, sp, -4\n";  // allocate stack space
                            fout << "  sw t0, 0(sp)        # push array address\n";
                        } else {
                            // regular argument - push by value
                            loadOperand(arg, "t0");
                            fout << "  addi sp, sp, -4\n";  // allocate stack space
                            fout << "  sw t0, 0(sp)        # push arg " << i << "\n";
                        }
                    }
                }
                
                // Then, load first 8 args into registers a0-a7
                for (size_t i = 0; i < callInst->argumentList.size() && i < 8; i++) {
                    std::string argReg = "a" + std::to_string(i);
                    const auto& arg = callInst->argumentList[i];
                    
                    // check if this argument is a local array
                    bool isLocalArray = false;
                    if (func) {
                        for (const auto& instPtr : func->InstVec) {
                            if (instPtr->op == ir::Operator::alloc && instPtr->des.name == arg.name) {
                                isLocalArray = true;
                                break;
                            }
                        }
                    }
                    
                    if (isLocalArray && !isGlobalVar(arg)) {
                        // this is a local array - pass its address
                        int arrayOff = svmap.find_operand(arg);
                        fout << "  addi " << argReg << ", sp, " << arrayOff << "   # pass array address\n";
                    } else {
                        // regular argument - pass by value
                        loadOperand(arg, argReg);
                    }
                }
                
                fout << "  jal ra, " << instr.op1.name << "   # call function with args\n";
                
                // Clean up stack space used for args 8+
                if (callInst->argumentList.size() > 8) {
                    int stackArgsCount = callInst->argumentList.size() - 8;
                    fout << "  addi sp, sp, " << (stackArgsCount * 4) << "  # cleanup stack args\n";
                }
            } else {
                fout << "  jal ra, " << instr.op1.name << "   # call function\n";
            }
            int offd = svmap.find_operand(instr.des);
            fout << "  sw a0, " << offd << "(sp)   # save return value\n";
            break;
        }case ir::Operator::_goto: {
            if (instr.op1.name != "null") {
                loadOperand(instr.op1, "t0");
                // check if des is a number (relative offset)
                bool isNumber = true;
                std::string desName = instr.des.name;
                // handle negative numbers
                size_t startPos = 0;
                if (!desName.empty() && desName[0] == '-') {
                    startPos = 1;
                }
                for (size_t i = startPos; i < desName.length(); i++) {
                    if (!std::isdigit(desName[i])) {
                        isNumber = false;
                        break;
                    }
                }
                if (isNumber && !desName.empty()) {
                    int offset = std::stoi(desName);
                    int target = pc + offset;  // relative offset (can be negative)
                    fout << "  bnez t0, " << funcName << "_label_" << target << "   # conditional goto\n";
                } else {
                    fout << "  bnez t0, " << desName << "   # conditional goto\n";
                }
            } else {
                // check if des is a number (relative offset)
                bool isNumber = true;
                std::string desName = instr.des.name;
                // handle negative numbers
                size_t startPos = 0;
                if (!desName.empty() && desName[0] == '-') {
                    startPos = 1;
                }
                for (size_t i = startPos; i < desName.length(); i++) {
                    if (!std::isdigit(desName[i])) {
                        isNumber = false;
                        break;
                    }
                }
                if (isNumber && !desName.empty()) {
                    int offset = std::stoi(desName);
                    int target = pc + offset;  // relative offset (can be negative)
                    fout << "  j " << funcName << "_label_" << target << "   # unconditional goto\n";
                } else {
                    fout << "  j " << desName << "   # unconditional goto\n";
                }
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
    if (op.type == ir::Type::IntLiteral) {
        // Load integer constant
        fout << "  li " << reg << ", " << op.name << "\n";
    } else if (isGlobalVar(op)) {
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
    int current_offset = next_offset;
    next_offset += static_cast<int>(size);
    _table[op] = current_offset;
    return current_offset;
}
