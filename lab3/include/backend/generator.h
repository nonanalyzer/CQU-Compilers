#ifndef GENERARATOR_H
#define GENERARATOR_H

#include "ir/ir.h"
#include "backend/rv_def.h"
#include "backend/rv_inst_impl.h"

#include<map>
#include<string>
#include<vector>
#include<fstream>
#include <cstdint>

namespace backend {

// it is a map bewteen variable and its mem addr, the mem addr of a local variable can be identified by ($sp + off)
struct stackVarMap {
    std::map<ir::Operand, int> _table;
    int next_offset = 4;  // Start from offset 4 to avoid conflicts

    /**
     * @brief find the addr of a ir::Operand
     * @return the offset
    */
    int find_operand(ir::Operand);

    /**
     * @brief add a ir::Operand into current map, alloc space for this variable in memory 
     * @param[in] size: the space needed(in byte)
     * @return the offset
    */
    int add_operand(ir::Operand, uint32_t size = 4);
};


struct Generator {
    const ir::Program& program;         // the program to gen
    std::ofstream& fout;                 // output file

    Generator(ir::Program&, std::ofstream&);

    // reg allocate api
    rv::rvREG getRd(ir::Operand);
    rv::rvFREG fgetRd(ir::Operand);
    rv::rvREG getRs1(ir::Operand);
    rv::rvREG getRs2(ir::Operand);
    rv::rvFREG fgetRs1(ir::Operand);
    rv::rvFREG fgetRs2(ir::Operand);

    // generate wrapper function
    void gen();
    void gen_func(const ir::Function&);
    void gen_instr(const ir::Instruction&, int pc = 0, const std::string& funcName = "", const ir::Function* func = nullptr);
    // stack allocation helper
    stackVarMap svmap;
    
    // Helper functions for global/local variable handling
    bool isGlobalVar(const ir::Operand& op);
    void loadOperand(const ir::Operand& op, const std::string& reg);
    void storeOperand(const ir::Operand& op, const std::string& reg);
};

} // namespace backend


#endif