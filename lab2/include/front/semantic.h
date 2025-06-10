/**
 * @file semantic.h
 * @author Yuntao Dai (d1581209858@live.com)
 * @brief 
 * @version 0.1
 * @date 2023-01-06
 * 
 * a Analyzer should 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef SEMANTIC_H
#define SEMANTIC_H

#include"ir/ir.h"
#include"front/abstract_syntax_tree.h"

#include<map>
#include<string>
#include<vector>
using std::map;
using std::string;
using std::vector;

namespace frontend
{

// definition of symbol table entry
struct STE {
    ir::Operand operand;
    vector<int> dimension;
    STE() = default;
    STE(const ir::Operand& op, const vector<int>& dim = {})
        : operand(op), dimension(dim) {}
};

using map_str_ste = map<string, STE>;
// definition of scope infomation
struct ScopeInfo {
    int cnt;
    string name;
    map_str_ste table;
};

// surpport lib functions
map<std::string,ir::Function*>* get_lib_funcs();

// definition of symbol table
struct SymbolTable{
    vector<ScopeInfo> scope_stack;
    map<std::string,ir::Function*> functions;

    /**
     * @brief enter a new scope, record the infomation in scope stacks
     * @param node: a Block node, entering a new Block means a new name scope
     */
    void add_scope();

    /**
     * @brief exit a scope, pop out infomations
     */
    void exit_scope();

    /**
     * @brief Get the scoped name, to deal the same name in different scopes, we change origin id to a new one with scope infomation,
     * for example, we have these code:
     * "     
     * int a;
     * {
     *      int a; ....
     * }
     * "
     * in this case, we have two variable both name 'a', after change they will be 'a' and 'a_block'
     * @param id: origin id 
     * @return string: new name with scope infomations
     */
    string get_scoped_name(string id) const;

    /**
     * @brief get the right operand with the input name
     * @param id identifier name
     * @return Operand 
     */
    ir::Operand get_operand(string id) const;

    /**
     * @brief get the right ste with the input name
     * @param id identifier name
     * @return STE 
     */
    STE get_ste(string id) const;
};


// singleton class
struct Analyzer {
    int tmp_cnt;
    vector<ir::Instruction*> g_init_inst;
    SymbolTable symbol_table;

    /**
     * @brief constructor
     */
    Analyzer();

    // analysis functions
    ir::Program get_ir_program(CompUnit*);

    // reject copy & assignment
    Analyzer(const Analyzer&) = delete;
    Analyzer& operator=(const Analyzer&) = delete;

    // 语义分析函数
    void analyzeCompUnit(CompUnit*, ir::Program&);
    void analyzeDecl(Decl*, vector<ir::Instruction*>&);
    void analyzeFuncDef(FuncDef*);
    void analyzeFuncType(FuncType*);
    void analyzeTerm(Term*);
    void analyzeFuncFParams(FuncFParams*, vector<ir::Operand>&);
    void analyzeFuncFParam(FuncFParam*, vector<ir::Operand>&);
    void analyzeBlock(Block*, vector<ir::Instruction*>&);
    void analyzeBlockItem(BlockItem*, vector<ir::Instruction*>&);
    void analyzeStmt(Stmt*, vector<ir::Instruction*>&);
    void analyzeExp(Exp*, vector<ir::Instruction*>&);
    void analyzeAddExp(AddExp*, vector<ir::Instruction*>&);
    void analyzeMulExp(MulExp*, vector<ir::Instruction*>&);
    void analyzeUnaryExp(UnaryExp*, vector<ir::Instruction*>&);
    void analyzePrimaryExp(PrimaryExp*, vector<ir::Instruction*>&);
    void analyzeNumber(Number*, vector<ir::Instruction*>&);
    void analyzeVarDecl(VarDecl*, vector<ir::Instruction*>&);
    void analyzeBType(BType*);
    void analyzeVarDef(VarDef*, vector<ir::Instruction*>&, ir::Type);
    void analyzeConstExp(ConstExp*, vector<ir::Instruction*>&);
    void analyzeInitVal(InitVal*, vector<ir::Instruction*>&, int, int, vector<int>&);
    void analyzeLVal(LVal*, vector<ir::Instruction*>&);
    void analyzeConstDecl(ConstDecl*, vector<ir::Instruction*>&);
    void analyzeConstDef(ConstDef*, vector<ir::Instruction*>&, ir::Type);
    void analyzeConstInitVal(ConstInitVal*, vector<ir::Instruction*>&, int, int, vector<int>&);
    void analyzeFuncRParams(FuncRParams*, vector<ir::Instruction*>&, vector<ir::Operand>&, vector<ir::Operand>&);
    void analyzeCond(Cond*, vector<ir::Instruction*>&);
    void analyzeLOrExp(LOrExp*, vector<ir::Instruction*>&);
    void analyzeLAndExp(LAndExp*, vector<ir::Instruction*>&);
    void analyzeEqExp(EqExp*, vector<ir::Instruction*>&);
    void analyzeRelExp(RelExp*, vector<ir::Instruction*>&);
    void analyzeUnaryOp(UnaryOp*);

    string getTmpName();
};

} // namespace frontend

#endif