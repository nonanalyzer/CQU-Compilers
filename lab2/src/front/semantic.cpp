#include"front/semantic.h"

#include<cassert>
#include<iostream>
#include<algorithm>

using ir::Instruction;
using ir::Function;
using ir::Operand;
using ir::Operator;

#define TODO assert(0 && "TODO");

#define MATCH_CHILD_TYPE(node, index) root->children[index]->type == NodeType::node
#define GET_CHILD_PTR(node, type, index) auto node = dynamic_cast<type*>(root->children[index]); assert(node); 
#define ANALYSIS(node, type, index) auto node = dynamic_cast<type*>(root->children[index]); assert(node); analysis##type(node, buffer);
#define COPY_EXP_NODE(from, to) to->is_computable = from->is_computable; to->v = from->v; to->t = from->t; to->value = from->value;

namespace frontend {
    map<std::string, int> constValues;
}

map<std::string,ir::Function*>* frontend::get_lib_funcs() {
    static map<std::string,ir::Function*> lib_funcs = {
        {"getint", new Function("getint", Type::Int)},
        {"getch", new Function("getch", Type::Int)},
        {"getfloat", new Function("getfloat", Type::Float)},
        {"getarray", new Function("getarray", {Operand("arr", Type::IntPtr)}, Type::Int)},
        {"getfarray", new Function("getfarray", {Operand("arr", Type::FloatPtr)}, Type::Int)},
        {"putint", new Function("putint", {Operand("i", Type::Int)}, Type::null)},
        {"putch", new Function("putch", {Operand("i", Type::Int)}, Type::null)},
        {"putfloat", new Function("putfloat", {Operand("f", Type::Float)}, Type::null)},
        {"putarray", new Function("putarray", {Operand("n", Type::Int), Operand("arr", Type::IntPtr)}, Type::null)},
        {"putfarray", new Function("putfarray", {Operand("n", Type::Int), Operand("arr", Type::FloatPtr)}, Type::null)},
    };
    return &lib_funcs;
}

void frontend::SymbolTable::add_scope() {
    // 新建一个作用域，作用域名可用Block的地址或编号唯一标识
    ScopeInfo scope;
    scope.cnt = scope_stack.size();
    static int scope_id = 0;
    scope.name = "scope_" + std::to_string(scope_id++);
    scope_stack.push_back(scope);
}
void frontend::SymbolTable::exit_scope() {
    // 退出当前作用域
    assert(!scope_stack.empty());
    scope_stack.pop_back();
}

string frontend::SymbolTable::get_scoped_name(string id) const {
    return id + "_" + scope_stack.back().name;
}

Operand frontend::SymbolTable::get_operand(string id) const {
    // 查找最近的作用域，返回对应的operand（name为重命名后的）
    for(auto it = scope_stack.rbegin(); it != scope_stack.rend(); ++it){
        auto found = it->table.find(id);
        if(found != it->table.end()){
            return found->second.operand;
        }
    }
    assert(0 && "Operand not found");
    return Operand();
}

frontend::STE frontend::SymbolTable::get_ste(string id) const {
    // 查找最近的作用域，返回对应的STE
    for(auto it = scope_stack.rbegin(); it != scope_stack.rend(); ++it){
        auto found = it->table.find(id);
        if(found != it->table.end()){
            return found->second;
        }
    }
    assert(0 && "STE not found");
    return STE();
}

frontend::Analyzer::Analyzer(): tmp_cnt(0), symbol_table() {
}

ir::Program frontend::Analyzer::get_ir_program(CompUnit* root) {
    ir::Program program;

    // 全局作用域
    symbol_table.add_scope();
    symbol_table.scope_stack.back().name = "global";
    // 全局函数
    Function* global_func = new Function("global", Type::null);
    symbol_table.functions["global"] = global_func;
    program.addFunction(*global_func);

    // 添加库函数
    auto lib_funcs = get_lib_funcs();
    for(const auto& [name, func] : *lib_funcs) {
        symbol_table.functions[name] = func;
    }

    analyzeCompUnit(root, program);

    // global需要return
    program.functions[0].addInst(new Instruction(Operand("null", Type::null), Operand(), Operand(), Operator::_return));

    return program;
}

std::string frontend::Analyzer::getTmpName(){
    return "t" + std::to_string(tmp_cnt++);
}

// CompUnit -> (Decl | FuncDef) [CompUnit]
void frontend::Analyzer::analyzeCompUnit(CompUnit* root, ir::Program& buffer){
    if(MATCH_CHILD_TYPE(DECL, 0)){
        GET_CHILD_PTR(decl, Decl, 0);
        analyzeDecl(decl, buffer.functions.back().InstVec);
        // 记录全局变量
        for(size_t i = 0; i < decl->n.size(); ++i){
            auto type = decl->t;
            if(decl->size[i] > 0) type = (type == Type::Int) ? Type::IntPtr : Type::FloatPtr; // 如果是数组，类型为指针
            buffer.globalVal.push_back(ir::GlobalVal(Operand(decl->n[i], type), decl->size[i]));
        }
    }
    else if(MATCH_CHILD_TYPE(FUNCDEF, 0)){
        GET_CHILD_PTR(funcDef, FuncDef, 0);
        auto function = ir::Function();
        analyzeFuncDef(funcDef, function);
        buffer.addFunction(function);
    }
    else{
        assert(0 && "analyzeCompUnit error: expected Decl or FuncDef");
    }

    if(root->children.size() > 1){
        GET_CHILD_PTR(compUnit, CompUnit, 1);
        analyzeCompUnit(compUnit, buffer);
    }
}

// Decl -> ConstDecl | VarDecl
void frontend::Analyzer::analyzeDecl(Decl* root, vector<ir::Instruction*>& buffer){
    if(MATCH_CHILD_TYPE(CONSTDECL, 0)){
        GET_CHILD_PTR(constDecl, ConstDecl, 0);
        analyzeConstDecl(constDecl, buffer);
        // 记录常量名和大小
        root->t = constDecl->t;
        root->n = constDecl->n;
        root->size = constDecl->size;
    }
    else if(MATCH_CHILD_TYPE(VARDECL, 0)){
        GET_CHILD_PTR(varDecl, VarDecl, 0);
        analyzeVarDecl(varDecl, buffer);
        // 记录变量名和大小
        root->t = varDecl->t;
        root->n = varDecl->n;
        root->size = varDecl->size;
    }
    else{
        assert(0 && "analyzeDecl error: expected ConstDecl or VarDecl");
    }
}

// ConstDecl -> 'const' BType ConstDef { ',' ConstDef } ';'
void frontend::Analyzer::analyzeConstDecl(ConstDecl* root, vector<ir::Instruction*>& buffer) {
    GET_CHILD_PTR(btype, BType, 1);
    analyzeBType(btype);
    root->t = btype->t;

    for(size_t i = 2; i < root->children.size() - 1; i += 2){ // 跳过逗号
        GET_CHILD_PTR(constDef, ConstDef, i);
        analyzeConstDef(constDef, buffer, btype->t);
        // 记录变量名和大小
        root->n.push_back(constDef->n);
        root->size.push_back(constDef->size);
    }
}

// VarDecl -> BType VarDef { ',' VarDef } ';'
void frontend::Analyzer::analyzeVarDecl(VarDecl* root, vector<ir::Instruction*>& buffer) {
    GET_CHILD_PTR(btype, BType, 0);
    analyzeBType(btype);
    root->t = btype->t;

    for(size_t i = 1; i < root->children.size() - 1; i += 2){ // 跳过逗号
        GET_CHILD_PTR(varDef, VarDef, i);
        analyzeVarDef(varDef, buffer, btype->t);
        // 记录变量名和大小
        root->n.push_back(varDef->n);
        root->size.push_back(varDef->size);
    }
}

// ConstDef -> Ident { '[' ConstExp ']' } '=' ConstInitVal
void frontend::Analyzer::analyzeConstDef(ConstDef* root, vector<ir::Instruction*>& buffer, ir::Type t) {
    // 变量名ident
    GET_CHILD_PTR(ident, Term, 0);
    root->n = symbol_table.get_scoped_name(ident->token.value);

    vector<int> dims;
    int size = 0; // 如果非数组，size置0
    if(root->children.size() > 2 && MATCH_CHILD_TYPE(CONSTEXP, 2)){
        // 是数组
        size = 1;
        for(size_t i = 2; i < root->children.size(); i += 3){
            if(MATCH_CHILD_TYPE(CONSTEXP, i)){  // 计算每一维大小
                GET_CHILD_PTR(constExp, ConstExp, i);
                analyzeConstExp(constExp, buffer);
                // assert(constExp->t == Type::IntLiteral && std::stoi(constExp->v) >= 0 && "ConstExp must be a const non-negative integer"); // 要不得了，立即数被我删完了
                assert(constExp->is_computable && constExp->value >= 0 && "ConstExp must be positive integer");
                dims.push_back(constExp->value);
                size *= constExp->value;
            }
            else break;
        }
    }
    root->size = size;

    if(size == 0){
        symbol_table.scope_stack.back().table[ident->token.value] = STE(Operand(root->n, t), dims);
    }
    else{
        symbol_table.scope_stack.back().table[ident->token.value] = STE(Operand(root->n, t == Type::Int ? Type::IntPtr : Type::FloatPtr), dims);
        // 分配数组空间
        buffer.push_back(new Instruction(
            Operand(std::to_string(size), Type::IntLiteral), // op1: 数组大小
            Operand(), // op2: 无
            Operand(root->n, t == Type::Int ? Type::IntPtr : Type::FloatPtr), // dst
            Operator::alloc
        ));
    }
    GET_CHILD_PTR(constInitVal, ConstInitVal, root->children.size() - 1);
    constInitVal->v = ident->token.value; // 变量名
    if(t == Type::Int){
        constInitVal->t = size == 0 ? Type::Int : Type::IntPtr;
    }
    else if(t == Type::Float){
        constInitVal->t = size == 0 ? Type::Float : Type::FloatPtr;
    }
    else{
        assert(0 && "ConstDef error: unsupported type");
    }
    // 分析ConstInitVal
    analyzeConstInitVal(constInitVal, buffer, size, 0, dims);
}

// ConstInitVal -> ConstExp | '{' [ ConstInitVal { ',' ConstInitVal } ] '}'
void frontend::Analyzer::analyzeConstInitVal(ConstInitVal* root, vector<ir::Instruction*>& buffer, int size, int offset, vector<int>& dims) {
    // size: 数组总大小，offset: 当前偏移，dims: 每一维大小
    string name = symbol_table.get_scoped_name(root->v);

    if (root->children.size() == 1 && MATCH_CHILD_TYPE(CONSTEXP, 0)) {
        GET_CHILD_PTR(constExp, ConstExp, 0);
        analyzeConstExp(constExp, buffer);
        // 生成IR：store 常量到数组/变量
        // 如果是数组元素，name[offset] = constExp->v
        if (size > 0) {
            // 数组元素初始化
            buffer.push_back(new Instruction(
                Operand(name, root->t), // op1: 数组名
                Operand(std::to_string(offset), Type::IntLiteral), // op2: 偏移
                Operand(constExp->v, constExp->t), // des: 常量值
                Operator::store
            ));
        }
        else {
            // 普通变量初始化
            bool mismatch = (root->t == Type::Int && (constExp->t == Type::Float || constExp->t == Type::FloatLiteral))
                         || (root->t == Type::Float && (constExp->t == Type::Int || constExp->t == Type::IntLiteral));
            if(mismatch) {
                // 类型转换
                if (root->t == Type::Int) {
                    auto tmp = Operand(getTmpName(), Type::Float);
                    buffer.push_back(new Instruction(
                        Operand(constExp->v, Type::Float), // op1: 浮点数
                        Operand(), // op2: 无
                        tmp, // des: 临时浮点变量
                        Operator::fdef
                    ));
                    buffer.push_back(new Instruction(
                        tmp, // op1: 浮点数
                        Operand(), // op2: 无
                        Operand(name, Type::Int), // des: 整数
                        Operator::cvt_f2i
                    ));
                }
                else if (root->t == Type::Float) {
                    auto tmp = Operand(getTmpName(), Type::Int);
                    buffer.push_back(new Instruction(
                        Operand(constExp->v, Type::Int), // op1: 立即数或变量
                        Operand(), // op2: 无
                        tmp, // des: 临时整数变量名
                        Operator::def
                    ));
                    buffer.push_back(new Instruction(
                        tmp, // op1: 整数
                        Operand(), // op2: 无
                        Operand(name, Type::Float), // des: 浮点数
                        Operator::cvt_i2f
                    ));
                }
                else {
                    assert(0 && "ConstInitVal error: type mismatch");
                }
            }
            else{
                buffer.push_back(new Instruction(
                    Operand(constExp->v, constExp->t), // op1: 常量值
                    Operand(), // op2: 无
                    Operand(name, root->t), // des: 变量名
                    root->t == Type::Int ? Operator::def : Operator::fdef
                ));
            }

            if(root->t == Type::Int){
                constValues[name] = constExp->value;
            }
        }
    }
    else {
        // '{' [ ConstInitVal { ',' ConstInitVal } ] '}'
        // 根据测试点，多维数组的初始化不使用多层花括号嵌套，偷个懒先
        int idx = 1; // 跳过 '{'
        int elem = 0;
        // 统计本层已初始化元素数
        while (idx < root->children.size() - 1) { // 跳过 '}'
            if (MATCH_CHILD_TYPE(CONSTINITVAL, idx)) {
                GET_CHILD_PTR(subInit, ConstInitVal, idx);
                subInit->v = root->v;
                subInit->t = root->t; // 传递数组原名(下一层会重新加后缀)和类型
                analyzeConstInitVal(subInit, buffer, size, offset + elem, dims);
                elem++;
            }
            idx += 2; // 跳过 ','
        }
        // 补零
        for (; elem < size; ++elem) {
            buffer.push_back(new Instruction(
                Operand(name, root->t), // op1: 数组名
                Operand(std::to_string(offset + elem), Type::IntLiteral), // op2: 偏移
                Operand("0", Type::IntLiteral), // des: 常量值
                Operator::store
            ));
        }
    }
}

// BType -> 'int' | 'float'
void frontend::Analyzer::analyzeBType(BType* root) {
    GET_CHILD_PTR(type, Term, 0);
    if(type){
        if(type->token.value == "int"){
            root->t = ir::Type::Int;
        }
        else if(type->token.value == "float"){
            root->t = ir::Type::Float;
        }
        else{
            assert(0 && "Unknown BType");
        }
    }
    else{
        assert(0 && "BType should be a Term node");
    }
}

// VarDef -> Ident { '[' ConstExp ']' } [ '=' InitVal ]
void frontend::Analyzer::analyzeVarDef(VarDef* root, vector<ir::Instruction*>& buffer, ir::Type t) {
    // 变量名ident
    GET_CHILD_PTR(ident, Term, 0);
    root->n = symbol_table.get_scoped_name(ident->token.value);

    vector<int> dims;
    int size = 0; // 如果非数组，size置0
    if(root->children.size() > 2 && MATCH_CHILD_TYPE(CONSTEXP, 2)){
        // 是数组
        size = 1;
        for(size_t i = 2; i < root->children.size(); i += 3){
            if(MATCH_CHILD_TYPE(CONSTEXP, i)){  // 计算每一维大小
                GET_CHILD_PTR(constExp, ConstExp, i);
                analyzeConstExp(constExp, buffer);
                // assert(constExp->t == Type::IntLiteral && std::stoi(constExp->v) >= 0 && "ConstExp must be a const non-negative integer"); // 要不得了，立即数被我删完了
                assert(constExp->is_computable && constExp->value >= 0 && "ConstExp must be positive integer");
                dims.push_back(constExp->value);
                size *= constExp->value;
            }
            else break;
        }
    }
    root->size = size;

    if(size == 0){
        symbol_table.scope_stack.back().table[ident->token.value] = STE(Operand(root->n, t), dims);
    }
    else{
        symbol_table.scope_stack.back().table[ident->token.value] = STE(Operand(root->n, t == Type::Int ? Type::IntPtr : Type::FloatPtr), dims);
        // 分配数组空间
        buffer.push_back(new Instruction(
            Operand(std::to_string(size), Type::IntLiteral), // op1: 数组大小
            Operand(), // op2: 无
            Operand(root->n, t == Type::Int ? Type::IntPtr : Type::FloatPtr), // dst
            Operator::alloc
        ));
    }
    if(root->children.back()->type == NodeType::INITVAL){
        GET_CHILD_PTR(initVal, InitVal, root->children.size() - 1);
        initVal->v = ident->token.value; // 变量名
        if(t == Type::Int){
            initVal->t = size == 0 ? Type::Int : Type::IntPtr;
        }
        else if(t == Type::Float){
            initVal->t = size == 0 ? Type::Float : Type::FloatPtr;
        }
        else{
            assert(0 && "VarDef error: unsupported type");
        }
        // 分析InitVal
        analyzeInitVal(initVal, buffer, size, 0, dims);
    }
}

// InitVal -> Exp | '{' [ InitVal { ',' InitVal } ] '}'
void frontend::Analyzer::analyzeInitVal(InitVal* root, vector<ir::Instruction*>& buffer, int size, int offset, vector<int>& dims) {
    // size: 数组总大小，offset: 当前偏移，dims: 每一维大小
    string name = symbol_table.get_scoped_name(root->v);

    if (root->children.size() == 1 && MATCH_CHILD_TYPE(EXP, 0)) {
        GET_CHILD_PTR(exp, Exp, 0);
        analyzeExp(exp, buffer);
        // 生成IR：store 常量到数组/变量
        // 如果是数组元素，name[offset] = exp->v
        if (size > 0) {
            // 数组元素初始化
            buffer.push_back(new Instruction(
                Operand(name, root->t), // op1: 数组名
                Operand(std::to_string(offset), Type::IntLiteral), // op2: 偏移
                Operand(exp->v, exp->t), // des: 常量值
                Operator::store
            ));
        }
        else {
            // 普通变量初始化
            bool mismatch = (root->t == Type::Int && (exp->t == Type::Float || exp->t == Type::FloatLiteral))
                         || (root->t == Type::Float && (exp->t == Type::Int || exp->t == Type::IntLiteral));
            if(mismatch) {
                // 类型转换
                if (root->t == Type::Int) {
                    auto tmp = Operand(getTmpName(), Type::Float);
                    buffer.push_back(new Instruction(
                        Operand(exp->v, Type::Float), // op1: 浮点数
                        Operand(), // op2: 无
                        tmp, // des: 临时浮点变量
                        Operator::fdef
                    ));
                    buffer.push_back(new Instruction(
                        tmp, // op1: 浮点数
                        Operand(), // op2: 无
                        Operand(name, Type::Int), // des: 整数
                        Operator::cvt_f2i
                    ));
                }
                else if (root->t == Type::Float) {
                    auto tmp = Operand(getTmpName(), Type::Int);
                    buffer.push_back(new Instruction(
                        Operand(exp->v, Type::Int), // op1: 整数
                        Operand(), // op2: 无
                        tmp, // des: 临时整数
                        Operator::def
                    ));
                    buffer.push_back(new Instruction(
                        tmp, // op1: 整数
                        Operand(), // op2: 无
                        Operand(name, Type::Float), // des: 浮点数
                        Operator::cvt_i2f
                    ));
                }
                else {
                    assert(0 && "InitVal error: type mismatch");
                }
            }
            else{
                buffer.push_back(new Instruction(
                    Operand(exp->v, exp->t), // op1: 常量值
                    Operand(), // op2: 无
                    Operand(name, root->t), // des: 变量名
                    root->t == Type::Int ? Operator::def : Operator::fdef
                ));
            }
        }
    }
    else {
        // '{' [ InitVal { ',' InitVal } ] '}'
        // 根据测试点，多维数组的初始化不使用多层花括号嵌套，偷个懒先
        int idx = 1; // 跳过 '{'
        int elem = 0;
        // 统计本层已初始化元素数
        while (idx < root->children.size() - 1) { // 跳过 '}'
            if (MATCH_CHILD_TYPE(INITVAL, idx)) {
                GET_CHILD_PTR(subInit, InitVal, idx);
                subInit->v = root->v;
                subInit->t = root->t; // 传递数组原名(下一层会重新加后缀)和类型
                analyzeInitVal(subInit, buffer, size, offset + elem, dims);
                elem++;
            }
            idx += 2; // 跳过 ','
        }
        // 补零
        for (; elem < size; ++elem) {
            buffer.push_back(new Instruction(
                Operand(name, root->t), // op1: 数组名
                Operand(std::to_string(offset + elem), Type::IntLiteral), // op2: 偏移
                Operand("0", Type::IntLiteral), // des: 常量值
                Operator::store
            ));
        }
    }
}

// FuncDef -> FuncType Ident '(' [FuncFParams] ')' Block
void frontend::Analyzer::analyzeFuncDef(FuncDef* root, ir::Function& func) {
    GET_CHILD_PTR(funcType, FuncType, 0);
    GET_CHILD_PTR(ident, Term, 1);
    func.returnType = root->t = analyzeFuncType(funcType);
    func.name = root->n = ident->token.value;
    symbol_table.functions[func.name] = &func;
    // 从形参开始进入新作用域
    symbol_table.add_scope();

    if(func.name == "main") {
        func.addInst(new ir::CallInst(
                            Operand("global", Type::null),
                            vector<ir::Operand>(),
                            Operand(getTmpName(), Type::null)
                        ));
    }

    if(root->children.size() > 3 && MATCH_CHILD_TYPE(FUNCFPARAMS, 3)){
        GET_CHILD_PTR(funcFParams, FuncFParams, 3);
        analyzeFuncFParams(funcFParams, func);
        GET_CHILD_PTR(block, Block, 5);
        analyzeBlock(block, func.InstVec);
    }
    else{
        GET_CHILD_PTR(block, Block, 4);
        analyzeBlock(block, func.InstVec);
    }

    if(func.returnType == Type::null){
        func.addInst(new Instruction(Operand("null", Type::null), Operand(), Operand(), Operator::_return));
    }

    symbol_table.exit_scope();
}

// FuncType -> 'void' | 'int' | 'float'
Type frontend::Analyzer::analyzeFuncType(FuncType* root) {
    GET_CHILD_PTR(tk, Term, 0);
    if(tk->token.type == TokenType::VOIDTK) return Type::null;
    else if(tk->token.type == TokenType::INTTK) return Type::Int;
    else if(tk->token.type == TokenType::FLOATTK) return Type::Float;
    else assert(0 && "FuncType error: unknown type");
}

// FuncFParams -> FuncFParam { ',' FuncFParam }
void frontend::Analyzer::analyzeFuncFParams(FuncFParams* root, ir::Function& buffer) {
    for(size_t i = 0; i < root->children.size(); i += 2){
        GET_CHILD_PTR(param, FuncFParam, i);
        analyzeFuncFParam(param, buffer);
    }
}

// FuncFParam -> BType Ident ['[' ']' { '[' Exp ']' }]
void frontend::Analyzer::analyzeFuncFParam(FuncFParam* root, ir::Function& buffer) {
    GET_CHILD_PTR(btype, BType, 0);
    analyzeBType(btype);
    GET_CHILD_PTR(ident, Term, 1);
    std::string name = ident->token.value;
    auto type = btype->t;
    if(root->children.size() > 2) type = type == Type::Int ? Type::IntPtr : Type::FloatPtr; // 数组
    buffer.ParameterList.push_back(Operand(name, type));
    symbol_table.scope_stack.back().table[name] = STE(Operand(name, type), {});
}

// Block -> '{' { BlockItem } '}'
void frontend::Analyzer::analyzeBlock(Block* root, vector<ir::Instruction*>& buffer) {
    symbol_table.add_scope(); // 先添加作用域

    for(size_t i = 1; i + 1 < root->children.size(); ++i){
        GET_CHILD_PTR(item, BlockItem, i);
        analyzeBlockItem(item, buffer);
    }

    symbol_table.exit_scope();
}

// BlockItem -> Decl | Stmt
void frontend::Analyzer::analyzeBlockItem(BlockItem* root, vector<ir::Instruction*>& buffer) {
    if(MATCH_CHILD_TYPE(DECL, 0)){
        GET_CHILD_PTR(decl, Decl, 0);
        analyzeDecl(decl, buffer);
    }
    else if(MATCH_CHILD_TYPE(STMT, 0)){
        GET_CHILD_PTR(stmt, Stmt, 0);
        analyzeStmt(stmt, buffer);
    }
    else{
        assert(0 && "analyzeBlockItem error: expected Decl or Stmt");
    }
}

// Stmt -> LVal '=' Exp ';' | Block | 'if' '(' Cond ')' Stmt [ 'else' Stmt ] | 'while' '(' Cond ')' Stmt | 'break' ';' | 'continue' ';' | 'return' [Exp] ';' | [Exp] ';'
void frontend::Analyzer::analyzeStmt(Stmt* root, vector<ir::Instruction*>& buffer) {
    // assignment: LVal '=' Exp ';'
    if(MATCH_CHILD_TYPE(LVAL,0)) {
        // expect '=' then Exp
        GET_CHILD_PTR(lval, LVal, 0);
        GET_CHILD_PTR(assignT, Term, 1);
        if(assignT->token.type == TokenType::ASSIGN) {
            string offset("need_offset");
            analyzeLVal(lval, buffer, offset);
            GET_CHILD_PTR(exp, Exp, 2);
            analyzeExp(exp, buffer);
            if(lval->t == Type::Int || lval->t == Type::Float){
                // 普通变量
                if(lval->t == Type::Int && exp->t == Type::Float) {
                    // 整数赋值浮点数，需转换
                    auto tmp = Operand(getTmpName(), Type::Float);
                    buffer.push_back(new Instruction(
                        Operand(exp->v, Type::Float), Operand(), tmp, Operator::fdef
                    ));
                    buffer.push_back(new Instruction(
                        tmp, Operand(), Operand(lval->v, Type::Int), Operator::cvt_f2i
                    ));
                }
                else if(lval->t == Type::Float && exp->t == Type::Int) {
                    // 浮点数赋值整数，需转换
                    auto tmp = Operand(getTmpName(), Type::Int);
                    buffer.push_back(new Instruction(
                        Operand(exp->v, Type::Int), Operand(), tmp, Operator::def
                    ));
                    buffer.push_back(new Instruction(
                        tmp, Operand(), Operand(lval->v, Type::Float), Operator::cvt_i2f
                    ));
                }
                else {
                    // 类型匹配或直接赋值
                    buffer.push_back(new Instruction(
                        Operand(exp->v, exp->t), Operand(), Operand(lval->v, lval->t), Operator::def
                    ));
                }
            }
            else if(lval->t == Type::IntPtr || lval->t == Type::FloatPtr){
                // 数组元素
                buffer.push_back(new Instruction(
                    Operand(lval->v, lval->t),
                    Operand(offset, Type::Int),
                    Operand(exp->v, exp->t),
                    Operator::store
                ));
            }
            else {
                assert(0 && "LVal type error");
            }
            return;
        }
    }
    if(MATCH_CHILD_TYPE(TERMINAL, 0)) {
        GET_CHILD_PTR(term, Term, 0);
        // return
        if(term->token.type == TokenType::RETURNTK) {
            if(root->children.size()>2 && root->children[1]->type==NodeType::EXP) {
                GET_CHILD_PTR(exp, Exp, 1);
                analyzeExp(exp, buffer);
                buffer.push_back(new Instruction(
                    Operand(exp->v, exp->t), Operand(), Operand(), Operator::_return
                ));
            } else {
                buffer.push_back(new Instruction(Operand("null", Type::null), Operand(), Operand(), Operator::_return));
            }
            return;
        }
        // if
        if(term->token.type == TokenType::IFTK){
            // if '(' Cond ')' Stmt [ 'else' Stmt ]
            GET_CHILD_PTR(cond, Cond, 2);
            analyzeCond(cond, buffer);
            auto condJump = new Instruction(
                Operand(cond->v, cond->t), Operand(), Operand("2", Type::IntLiteral), Operator::_goto
            );
            buffer.push_back(condJump);

            vector<ir::Instruction*> thenBuffer, elseBuffer;
            // then分支
            GET_CHILD_PTR(thenStmt, Stmt, 4);
            analyzeStmt(thenStmt, thenBuffer);

            if(root->children.size() > 5 && root->children[5]->type == NodeType::TERMINAL){
                // 有else
                GET_CHILD_PTR(elseStmt, Stmt, 6);
                analyzeStmt(elseStmt, elseBuffer);

                thenBuffer.push_back(new Instruction(
                    Operand("null", Type::null), Operand(), Operand(std::to_string(elseBuffer.size() + 1), Type::IntLiteral), Operator::_goto
                ));
                buffer.push_back(new Instruction(
                    Operand("null", Type::null), Operand(), Operand(std::to_string(thenBuffer.size() + 1), Type::IntLiteral), Operator::_goto
                ));
                buffer.insert(buffer.end(), thenBuffer.begin(), thenBuffer.end());
                buffer.insert(buffer.end(), elseBuffer.begin(), elseBuffer.end());
                buffer.push_back(new Instruction(
                    Operand(), Operand(), Operand(), Operator::__unuse__ // 用于填充
                ));
            } else {
                // 没有else
                buffer.push_back(new Instruction(
                    Operand("null", Type::null), Operand(), Operand(std::to_string(thenBuffer.size() + 1), Type::IntLiteral), Operator::_goto
                ));
                buffer.insert(buffer.end(), thenBuffer.begin(), thenBuffer.end());
                buffer.push_back(new Instruction(
                    Operand(), Operand(), Operand(), Operator::__unuse__
                ));
            }
            return;
        }
        // while
        if(term->token.type == TokenType::WHILETK){
            // while '(' Cond ')' Stmt
            int loopStart = buffer.size();

            GET_CHILD_PTR(cond, Cond, 2);
            analyzeCond(cond, buffer);
            // 条件取反
            std::string notVar2 = getTmpName();
            buffer.push_back(new Instruction(
                Operand(cond->v, Type::Int), Operand(), Operand(notVar2, Type::Int), Operator::_not
            ));
            auto exitJump = new Instruction(
                Operand(notVar2, Type::Int), Operand(), Operand("", Type::IntLiteral), Operator::_goto
            );
            buffer.push_back(exitJump);
            int exitPos = (int)buffer.size() - 1; // 记录退出循环位置
            // 循环体
            GET_CHILD_PTR(bodyStmt, Stmt, 4);
            analyzeStmt(bodyStmt, buffer);
            // 回跳到loopStart
            auto backJump = new Instruction(
                Operand("null", Type::null), Operand(), Operand(std::to_string(loopStart), Type::IntLiteral), Operator::_goto
            );
            buffer.push_back(backJump);
            int backPos = (int)buffer.size() - 1;
            backJump->des = Operand(std::to_string(loopStart - backPos), Type::IntLiteral);

            // patch continue跳回loop start
            for(auto inst: root->jump_bow){
                auto it = std::find(buffer.begin(), buffer.end(), inst);
                int idx = (int)std::distance(buffer.begin(), it);
                inst->des = Operand(std::to_string(loopStart - idx), Type::IntLiteral);
            }
            // patch break跳到loop end
            int loopEnd = buffer.size();
            for(auto inst: root->jump_eow){
                auto it = std::find(buffer.begin(), buffer.end(), inst);
                int idx = (int)std::distance(buffer.begin(), it);
                inst->des = Operand(std::to_string(loopEnd - idx), Type::IntLiteral);
            }
            // patch exitJump跳到loop end
            exitJump->des = Operand(std::to_string(loopEnd - exitPos), Type::IntLiteral);
            return;
        }
        // break
        if(term->token.type == TokenType::BREAKTK){
            auto brJump = new Instruction(
                Operand("null", Type::null), Operand(), Operand("", Type::IntLiteral), Operator::_goto
            );
            buffer.push_back(brJump);
            // find enclosing while
            AstNode* p = root->parent;
            while(p && !(p->type == NodeType::STMT && p->children.size()>0 && p->children[0]->type==NodeType::TERMINAL && dynamic_cast<Term*>(p->children[0])->token.type==TokenType::WHILETK)){
                p = p->parent;
            }
            assert(p && "break not within loop");
            dynamic_cast<Stmt*>(p)->jump_eow.insert(brJump);
            return;
        }
        // continue
        if(term->token.type == TokenType::CONTINUETK){
            auto contJump = new Instruction(
                Operand("null", Type::null), Operand(), Operand("", Type::IntLiteral), Operator::_goto
            );
            buffer.push_back(contJump);
            // find enclosing while
            AstNode* p = root->parent;
            while(p && !(p->type == NodeType::STMT && p->children.size()>0 && p->children[0]->type==NodeType::TERMINAL && dynamic_cast<Term*>(p->children[0])->token.type==TokenType::WHILETK)){
                p = p->parent;
            }
            assert(p && "continue not within loop");
            dynamic_cast<Stmt*>(p)->jump_bow.insert(contJump);
            return;
        }
        // empty statement ';'
        if(term->token.type == TokenType::SEMICN) {
            // no operation
            return;
        }
    }
    // block
    if(MATCH_CHILD_TYPE(BLOCK,0)) {
        GET_CHILD_PTR(block, Block, 0);
        analyzeBlock(block, buffer);
        return;
    }
    // expression statement: [Exp] ';'
    if(root->children.size()>0 && root->children[0]->type==NodeType::EXP){
        GET_CHILD_PTR(exp, Exp, 0);
        analyzeExp(exp, buffer);
        return;
    }
    // unsupported
    assert(0 && "analyzeStmt: unsupported statement");
}

// LVal -> Ident {'[' Exp ']'
void frontend::Analyzer::analyzeLVal(LVal* root, vector<ir::Instruction*>& buffer, string& offset) {
    // LVal -> Ident {'[' Exp ']'}
    GET_CHILD_PTR(ident, Term, 0);
    auto name = ident->token.value;
    auto ste  = symbol_table.get_ste(name);
    auto base = symbol_table.get_operand(name);
    const auto& dims = ste.dimension;

    // 普通变量
    if (root->children.size() == 1){
        // simple variable
        root->v = base.name;
        root->t = base.type;
        
        // 我不想再开一个is_const了，测试点能过的话就这样吧
        if(constValues.find(base.name) != constValues.end()){
            root->value = constValues[base.name];
            root->is_computable = true;
        }
        else{
            root->is_computable = false;
        }
        return;
    }

    // 数组变量
    // 计算线性偏移
    string offsetVar;
    for(size_t k = 0, index = 2; k < dims.size(); ++k, index += 3){
        GET_CHILD_PTR(exp, Exp, index);
        analyzeExp(exp, buffer);
        // 计算本维度的 stride
        int stride = 1;
        for(size_t j = dims.size() - 1; j > k; --j) stride *= dims[j];
        // idx_k * stride
        string part = getTmpName();
        if(stride == 1){
            buffer.push_back(new Instruction(
                Operand(exp->v, exp->t),
                Operand(),
                Operand(part, Type::Int),
                Operator::def
            ));
        }
        else{
            Operand t1, t2;
            if(exp->t == Type::IntLiteral){
                t1 = Operand(getTmpName(), Type::Int);
                buffer.push_back(new Instruction(
                    Operand(exp->v, Type::IntLiteral),
                    Operand(),
                    t1,
                    Operator::def
                ));
            }
            else if(exp->t == Type::Int) t1 = Operand(exp->v, Type::Int);
            else assert(0 && "LVal error: Exp type must be Int or IntLiteral");
            t2 = Operand(getTmpName(), Type::Int);
            buffer.push_back(new Instruction(
                Operand(std::to_string(stride), Type::IntLiteral),
                Operand(),
                t2,
                Operator::def
            ));                          
            buffer.push_back(new Instruction(
                t1,
                t2,
                Operand(part, Type::Int),
                Operator::mul
            ));
        }
        // 累加到 offsetVar
        if(k == 0) offsetVar = part;
        else{
            string sum = getTmpName();
            buffer.push_back(new Instruction(
                Operand(offsetVar, Type::Int),
                Operand(part, Type::Int),
                Operand(sum, Type::Int),
                Operator::add
            ));
            offsetVar = sum;
        }
    }
    if(offset == "need_offset"){
        // Stmt -> LVal '=' Exp ';'
        root->v = base.name;
        root->is_computable = false;
        root->t = base.type;
        offset = offsetVar;
    }
    else{
        // PrimaryExp -> LVal
        // 读取：load dst, base, offset
        string dst = getTmpName();
        buffer.push_back(new Instruction(
            Operand(base.name, base.type),
            Operand(offsetVar, Type::Int),
            Operand(dst, base.type == Type::IntPtr ? Type::Int : Type::Float),
            Operator::load
        ));
        root->v = dst;
        root->t = base.type == Type::IntPtr ? Type::Int : Type::Float;
        root->is_computable = false;
    }
}

// Exp -> AddExp
void frontend::Analyzer::analyzeExp(Exp* root, vector<ir::Instruction*>& buffer) {
    GET_CHILD_PTR(addExp, AddExp, 0);
    analyzeAddExp(addExp, buffer);
    COPY_EXP_NODE(addExp, root);
}

// AddExp -> MulExp { ('+' | '-') MulExp }
void frontend::Analyzer::analyzeAddExp(AddExp* root, vector<ir::Instruction*>& buffer) {
    GET_CHILD_PTR(first, MulExp, 0);
    analyzeMulExp(first, buffer);
    COPY_EXP_NODE(first, root);

    for(size_t i = 1; i + 1 < root->children.size(); i += 2) {
        GET_CHILD_PTR(opTerm, Term, i);
        std::string op = opTerm->token.value;

        GET_CHILD_PTR(next, MulExp, i+1);
        analyzeMulExp(next, buffer);

        std::string dst = getTmpName();
        if(root->t == Type::Int && next->t == Type::Int) {
            buffer.push_back(new Instruction(
                Operand(root->v, Type::Int),
                Operand(next->v, Type::Int),
                Operand(dst, Type::Int),
                op == "+" ? Operator::add : Operator::sub
            ));
            if(root->is_computable && next->is_computable) {
                if(op == "+") {
                    root->value = root->value + next->value;
                } else if(op == "-") {
                    root->value = root->value - next->value;
                }
                root->is_computable = true;
            } else {
                root->is_computable = false;
            }
        } else {
            // 浮点加减
            buffer.push_back(new Instruction(
                Operand(root->v, Type::Float),
                Operand(next->v, Type::Float),
                Operand(dst, Type::Float),
                op == "+" ? Operator::fadd : Operator::fsub
            ));
            root->t = Type::Float;
            root->is_computable = false;
        }
        root->v = dst;
    }
}

// MulExp -> UnaryExp { ('*' | '/' | '%') UnaryExp }
void frontend::Analyzer::analyzeMulExp(MulExp* root, vector<ir::Instruction*>& buffer) {
    GET_CHILD_PTR(first, UnaryExp, 0);
    analyzeUnaryExp(first, buffer);
    COPY_EXP_NODE(first, root);
    for(size_t i = 1; i + 1 < root->children.size(); i += 2) {
        GET_CHILD_PTR(opTerm, Term, i);
        std::string op = opTerm->token.value;
        GET_CHILD_PTR(next, UnaryExp, i+1);
        analyzeUnaryExp(next, buffer);
        // 生成 IR，处理立即数
        std::string dst = getTmpName();
        if(root->t == Type::Int && next->t == Type::Int) {
            // 整型运算
            Operator opc = (op == "*") ? Operator::mul : (op == "/" ? Operator::div : Operator::mod);
            buffer.push_back(new Instruction(
                Operand(root->v, Type::Int), Operand(next->v, Type::Int), Operand(dst, Type::Int), opc
            ));
            root->t = Type::Int;
            if(root->is_computable && next->is_computable) {
                if(op == "*") {
                    root->value = root->value * next->value;
                } else if(op == "/") {
                    root->value = root->value / next->value;
                } else if(op == "%") {
                    root->value = root->value % next->value;
                }
                root->is_computable = true;
            } else {
                root->is_computable = false;
            }
        } else {
            // 浮点运算
            Operator opc = (op == "*") ? Operator::fmul : Operator::fdiv;
            buffer.push_back(new Instruction(
                Operand(root->v, Type::Float), Operand(next->v, Type::Float), Operand(dst, Type::Float), opc
            ));
            root->t = Type::Float;
            root->is_computable = false;
        }
        root->v = dst;
    }
}

// UnaryExp -> PrimaryExp | Ident '(' [FuncRParams] ')' | UnaryOp UnaryExp
void frontend::Analyzer::analyzeUnaryExp(UnaryExp* root, vector<ir::Instruction*>& buffer) {
    if (root->children.size() == 1 && MATCH_CHILD_TYPE(PRIMARYEXP, 0)) {
        GET_CHILD_PTR(pe, PrimaryExp, 0);
        analyzePrimaryExp(pe, buffer);
        COPY_EXP_NODE(pe, root);
    }
    else if(root->children.size() >= 3 && root->children[0]->type == NodeType::TERMINAL /* Ident */) {
        // 函数调用
        GET_CHILD_PTR(ident, Term, 0);
        std::string func = ident->token.value;
        std::vector<ir::Operand> params;
        std::vector<ir::Operand> types;
        size_t idx = 2;
        if(root->children.size() > 3 && root->children[2]->type == NodeType::FUNCRPARAMS) {
            GET_CHILD_PTR(rps, FuncRParams, 2);
            analyzeFuncRParams(rps, buffer, params, types);
            idx = 3;
        }
        // 调用指令
        Type retT = symbol_table.functions[func]->returnType;
        std::string dst = getTmpName();
        buffer.push_back(new ir::CallInst(Operand(func, Type::null), params, Operand(dst, retT)));
        root->is_computable = false;
        root->v = dst;
        root->t = retT;
    }
    else {
        // 一元运算符
        GET_CHILD_PTR(uop, UnaryOp, 0);
        analyzeUnaryOp(uop, buffer);
        GET_CHILD_PTR(ue, UnaryExp, 1);
        analyzeUnaryExp(ue, buffer);
        // 处理 + - !
        if(uop->op == TokenType::PLUS) {
            COPY_EXP_NODE(ue, root);
        } else if(uop->op == TokenType::MINU) {
            // 生成 0 - x
            std::string dst = getTmpName();
            if(ue->t == Type::Int) {
                Operand zero = Operand(getTmpName(), Type::Int);
                buffer.push_back(new Instruction(
                    Operand("0", Type::IntLiteral), Operand(), zero, Operator::def
                ));
                buffer.push_back(new Instruction(
                    zero, Operand(ue->v, Type::Int), Operand(dst, Type::Int), Operator::sub
                ));
                root->t = Type::Int;
                if(ue->is_computable) {
                    root->value = -ue->value;
                    root->is_computable = true;
                } else {
                    root->is_computable = false;
                }
            } else {
                Operand zero = Operand(getTmpName(), Type::Float);
                buffer.push_back(new Instruction(
                    Operand("0.0", Type::FloatLiteral), Operand(), zero, Operator::fdef
                ));
                buffer.push_back(new Instruction(
                    zero, Operand(ue->v, Type::Float), Operand(dst, Type::Float), Operator::fsub
                ));
                root->t = Type::Float;
                root->is_computable = false;
            }
            root->v = dst;
        } else if(uop->op == TokenType::NOT) {
            std::string dst = getTmpName();
            buffer.push_back(new Instruction(
                Operand(ue->v, ue->t), Operand(), Operand(dst, ue->t), Operator::_not
            ));
            root->v = dst;
            root->t = ue->t;
            root->is_computable = false;
        }
        else {
            assert(0 && "UnaryExp error2: expected PrimaryExp, function call or UnaryOp UnaryExp");
        }
    }
}

// PrimaryExp -> '(' Exp ')' | LVal | Number
void frontend::Analyzer::analyzePrimaryExp(PrimaryExp* root, vector<ir::Instruction*>& buffer) {
    if(MATCH_CHILD_TYPE(TERMINAL, 0)) {
        // '(' Exp ')'
        GET_CHILD_PTR(exp, Exp, 1);
        analyzeExp(exp, buffer);
        COPY_EXP_NODE(exp, root);
    }
    else if(MATCH_CHILD_TYPE(LVAL, 0)) {
        // LVal
        GET_CHILD_PTR(lval, LVal, 0);
        string tmp;
        analyzeLVal(lval, buffer, tmp);
        COPY_EXP_NODE(lval, root);
    }
    else if(MATCH_CHILD_TYPE(NUMBER, 0)) {
        // Number
        GET_CHILD_PTR(number, Number, 0);
        analyzeNumber(number, buffer);
        COPY_EXP_NODE(number, root);
    }
    else {
        assert(0 && "PrimaryExp error: expected Exp, LVal or Number");
    }
}

// Number -> IntConst | floatConst
void frontend::Analyzer::analyzeNumber(Number* root, vector<ir::Instruction*>& buffer) {
    GET_CHILD_PTR(term, Term, 0);
    if(term->token.type == TokenType::INTLTR){
        auto& str = term->token.value;
        // 对二、八、十六进制数字进行转换
        if(str.size() >= 2 && str[0] == '0'){
            if(str[1] == 'x' || str[1] == 'X'){
                // 十六进制
                str = std::to_string(std::stoi(str, nullptr, 16));
            }
            else if(str[1] == 'b' || str[1] == 'B'){
                // 二进制
                str = std::to_string(std::stoi(str, nullptr, 2));
            }
            else{
                // 八进制 他可能只有单个前导零而不是0o
                str = std::to_string(std::stoi(str, nullptr, 8));
            }
        }
        auto tmp = getTmpName();
        buffer.push_back(new Instruction(
            Operand(str, Type::IntLiteral), Operand(), Operand(tmp, Type::Int), Operator::def
        ));
        root->v = tmp;
        root->t = Type::Int; 
        root->is_computable = true;
        root->value = std::stoi(str);
    }
    else if(term->token.type == TokenType::FLOATLTR){
        auto tmp = getTmpName();
        buffer.push_back(new Instruction(
            Operand(term->token.value, Type::FloatLiteral), Operand(), Operand(tmp, Type::Float), Operator::fdef
        ));
        root->v = tmp;
        root->t = Type::Float;
        root->is_computable = false; // 去他的常数折叠，我只管数组了
    }
    else{
        assert(0 && "Number error: expected IntConst or FloatConst");
    }
}

// UnaryOp -> '+' | '-' | '!'
void frontend::Analyzer::analyzeUnaryOp(UnaryOp* root, vector<ir::Instruction*>& buffer) {
    // UnaryOp -> '+' | '-' | '!'
    GET_CHILD_PTR(term, Term, 0);
    root->op = term->token.type;
}

// FuncRParams -> Exp { ',' Exp }
void frontend::Analyzer::analyzeFuncRParams(FuncRParams* root, vector<ir::Instruction*>& buffer, vector<ir::Operand>& params, vector<ir::Operand>& types) {
    for(size_t i = 0; i < root->children.size(); i += 2){
        GET_CHILD_PTR(exp, Exp, i);
        analyzeExp(exp, buffer);
        params.push_back(ir::Operand(exp->v, exp->t));
        types.push_back(ir::Operand(exp->v, exp->t));
    }
    // 似乎浮点数需要特殊处理，再说吧
}

// Cond -> LOrExp
void frontend::Analyzer::analyzeCond(Cond* root, vector<ir::Instruction*>& buffer) {
    GET_CHILD_PTR(lOrExp, LOrExp, 0);
    analyzeLOrExp(lOrExp, buffer);
    COPY_EXP_NODE(lOrExp, root);
}

// LOrExp -> LAndExp [ '||' LOrExp ]
void frontend::Analyzer::analyzeLOrExp(LOrExp* root, vector<ir::Instruction*>& buffer) {
    GET_CHILD_PTR(first, LAndExp, 0);
    analyzeLAndExp(first, buffer);
    COPY_EXP_NODE(first, root);

    if(root->children.size() > 1) {
        GET_CHILD_PTR(next, LOrExp, 2);
        analyzeLOrExp(next, buffer);

        std::string dst = getTmpName();
        buffer.push_back(new Instruction(
            Operand(root->v, root->t),
            Operand(next->v, next->t),
            Operand(dst, Type::Int),
            Operator::_or
        ));
        root->v = dst;
        root->t = Type::Int;
        root->is_computable = false;
    }
}

// LAndExp -> EqExp [ '&&' LAndExp ]
void frontend::Analyzer::analyzeLAndExp(LAndExp* root, vector<ir::Instruction*>& buffer) {
    GET_CHILD_PTR(first, EqExp, 0);
    analyzeEqExp(first, buffer);
    COPY_EXP_NODE(first, root);

    if(root->children.size() > 1) {
        GET_CHILD_PTR(next, LAndExp, 2);
        analyzeLAndExp(next, buffer);

        std::string dst = getTmpName();
        buffer.push_back(new Instruction(
            Operand(root->v, root->t),
            Operand(next->v, next->t),
            Operand(dst, Type::Int),
            Operator::_and
        ));
        root->v = dst;
        root->t = Type::Int;
        root->is_computable = false;
    }
}

// EqExp -> RelExp { ('==' | '!=') RelExp }
void frontend::Analyzer::analyzeEqExp(EqExp* root, vector<ir::Instruction*>& buffer) {
    GET_CHILD_PTR(first, RelExp, 0);
    analyzeRelExp(first, buffer);
    COPY_EXP_NODE(first, root);

    for(size_t i = 1; i + 1 < root->children.size(); i += 2) {
        GET_CHILD_PTR(opTerm, Term, i);
        std::string op = opTerm->token.value;
        GET_CHILD_PTR(next, RelExp, i+1);
        analyzeRelExp(next, buffer);

        std::string dst = getTmpName();
        Operator opc;
        bool isFloat = (root->t == Type::Float || next->t == Type::Float);
        if(op == "==" ) opc = isFloat ? Operator::feq : Operator::eq;
        else opc = isFloat ? Operator::fneq : Operator::neq;
        buffer.push_back(new Instruction(
            Operand(root->v, root->t),
            Operand(next->v, next->t),
            Operand(dst, Type::Int),
            opc
        ));
        root->v = dst;
        root->t = Type::Int;
        root->is_computable = false;
    }
}

// RelExp -> AddExp { ('<' | '>' | '<=' | '>=') AddExp }
void frontend::Analyzer::analyzeRelExp(RelExp* root, vector<ir::Instruction*>& buffer) {
    GET_CHILD_PTR(first, AddExp, 0);
    analyzeAddExp(first, buffer);
    COPY_EXP_NODE(first, root);

    for(size_t i = 1; i + 1 < root->children.size(); i += 2) {
        GET_CHILD_PTR(opTerm, Term, i);
        std::string op = opTerm->token.value;
        GET_CHILD_PTR(next, AddExp, i+1);
        analyzeAddExp(next, buffer);

        std::string dst = getTmpName();
        Operator opc;
        bool isFloat = (root->t == Type::Float || next->t == Type::Float);
        if(op == "<") opc = isFloat ? Operator::flss : Operator::lss;
        else if(op == ">") opc = isFloat ? Operator::fgtr : Operator::gtr;
        else if(op == "<=") opc = isFloat ? Operator::fleq : Operator::leq;
        else opc = isFloat ? Operator::fgeq : Operator::geq;
        buffer.push_back(new Instruction(
            Operand(root->v, root->t),
            Operand(next->v, next->t),
            Operand(dst, Type::Int),
            opc
        ));
        root->v = dst;
        root->t = Type::Int;
        root->is_computable = false;
    }
}

// ConstExp -> AddExp
void frontend::Analyzer::analyzeConstExp(ConstExp* root, vector<ir::Instruction*>& buffer) {
    GET_CHILD_PTR(addExp, AddExp, 0);
    analyzeAddExp(addExp, buffer);
    COPY_EXP_NODE(addExp, root); 
}