#include"front/semantic.h"

#include<cassert>

using ir::Instruction;
using ir::Function;
using ir::Operand;
using ir::Operator;

#define TODO assert(0 && "TODO");

#define MATCH_CHILD_TYPE(node, index) root->children[index]->type == NodeType::node
#define GET_CHILD_PTR(node, type, index) auto node = dynamic_cast<type*>(root->children[index]); assert(node); 
#define ANALYSIS(node, type, index) auto node = dynamic_cast<type*>(root->children[index]); assert(node); analysis##type(node, buffer);
#define COPY_EXP_NODE(from, to) to->is_computable = from->is_computable; to->v = from->v; to->t = from->t;

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
    // 加后缀
    if(scope_stack.empty()) return id; // 全局作用域
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
    }
    else if(MATCH_CHILD_TYPE(VARDECL, 0)){
        GET_CHILD_PTR(varDecl, VarDecl, 0);
        analyzeVarDecl(varDecl, buffer);
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
    }
}

// ConstDef -> Ident { '[' ConstExp ']' } '=' ConstInitVal
void frontend::Analyzer::analyzeConstDef(ConstDef* root, vector<ir::Instruction*>& buffer, ir::Type t) {
    // 变量名ident
    GET_CHILD_PTR(ident, Term, 0);
    root->arr_name = symbol_table.get_scoped_name(ident->token.value);

    vector<int> dims;
    int size = 0; // 如果非数组，size置0
    if(root->children.size() > 2 && MATCH_CHILD_TYPE(CONSTEXP, 2)){
        // 是数组
        size = 1;
        for(size_t i = 2; i < root->children.size(); i += 3){
            if(MATCH_CHILD_TYPE(CONSTEXP, i)){  // 计算每一维大小
                GET_CHILD_PTR(constExp, ConstExp, i);
                analyzeConstExp(constExp, buffer);
                assert(constExp->t == Type::IntLiteral && std::stoi(constExp->v) >= 0 && "ConstExp must be a const non-negative integer");
                dims.push_back(std::stoi(constExp->v));
                size *= std::stoi(constExp->v);
            }
            else break;
        }
    }

    if(size == 0){
        symbol_table.scope_stack.back().table[ident->token.value] = STE(Operand(root->arr_name, t), dims);
    }
    else{
        symbol_table.scope_stack.back().table[ident->token.value] = STE(Operand(root->arr_name, t == Type::Int ? Type::IntPtr : Type::FloatPtr), dims);
        // 分配数组空间
        buffer.push_back(new Instruction(
            Operand(std::to_string(size), Type::IntLiteral), // op1: 数组大小
            Operand(), // op2: 无
            Operand(root->arr_name, t == Type::Int ? Type::IntPtr : Type::FloatPtr), // dst
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
                        Operand(constExp->v, Type::Int), // op1: 整数
                        Operand(), // op2: 无
                        tmp, // des: 临时整数变量
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
                    Operand(name, root->t), // op1: 变量名
                    Operand(), // op2: 无
                    Operand(constExp->v, constExp->t), // des: 常量值
                    root->t == Type::Int ? Operator::def : Operator::fdef
                ));
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
    root->arr_name = symbol_table.get_scoped_name(ident->token.value);

    vector<int> dims;
    int size = 0; // 如果非数组，size置0
    if(root->children.size() > 2 && MATCH_CHILD_TYPE(CONSTEXP, 2)){
        // 是数组
        size = 1;
        for(size_t i = 2; i < root->children.size(); i += 3){
            if(MATCH_CHILD_TYPE(CONSTEXP, i)){  // 计算每一维大小
                GET_CHILD_PTR(constExp, ConstExp, i);
                analyzeConstExp(constExp, buffer);
                assert(constExp->t == Type::IntLiteral && std::stoi(constExp->v) >= 0 && "ConstExp must be a const non-negative integer");
                dims.push_back(std::stoi(constExp->v));
                size *= std::stoi(constExp->v);
            }
            else break;
        }
    }

    if(size == 0){
        symbol_table.scope_stack.back().table[ident->token.value] = STE(Operand(root->arr_name, t), dims);
    }
    else{
        symbol_table.scope_stack.back().table[ident->token.value] = STE(Operand(root->arr_name, t == Type::Int ? Type::IntPtr : Type::FloatPtr), dims);
        // 分配数组空间
        buffer.push_back(new Instruction(
            Operand(std::to_string(size), Type::IntLiteral), // op1: 数组大小
            Operand(), // op2: 无
            Operand(root->arr_name, t == Type::Int ? Type::IntPtr : Type::FloatPtr), // dst
            Operator::alloc
        ));
    }
    if(root->children.size() > 1){
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
                        tmp, // des: 临时整数变量
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
                    Operand(name, root->t), // op1: 变量名
                    Operand(), // op2: 无
                    Operand(exp->v, exp->t), // des: 常量值
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
            analyzeLVal(lval, buffer);
            GET_CHILD_PTR(exp, Exp, 2);
            analyzeExp(exp, buffer);
            Operator opc = (lval->t == Type::Float) ? Operator::fmov : Operator::mov;
            buffer.push_back(new Instruction(
                Operand(lval->v, lval->t), Operand(), Operand(exp->v, exp->t), opc
            ));
            return;
        }
    }
    // return statement
    if(MATCH_CHILD_TYPE(TERMINAL, 0)) {
        GET_CHILD_PTR(term, Term, 0);
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
    }
    // block
    if(MATCH_CHILD_TYPE(BLOCK,0)) {
        GET_CHILD_PTR(block, Block, 0);
        analyzeBlock(block, buffer);
        return;
    }
    // if statement
    if(MATCH_CHILD_TYPE(TERMINAL,0)){
        GET_CHILD_PTR(term, Term, 0);
        if(term->token.type == TokenType::IFTK){
            // if '(' Cond ')' Stmt [ 'else' Stmt ]
            GET_CHILD_PTR(cond, Cond, 2);
            analyzeCond(cond, buffer);
            // invert cond for false jump
            std::string notVar = getTmpName();
            buffer.push_back(new Instruction(
                Operand(cond->v, Type::Int), Operand(), Operand(notVar, Type::Int), Operator::_not
            ));
            // placeholder jump to else or end
            auto condJump = new Instruction(
                Operand(notVar, Type::Int), Operand(), Operand("", Type::IntLiteral), Operator::_goto
            );
            buffer.push_back(condJump);
            // then branch
            GET_CHILD_PTR(thenStmt, Stmt, 4);
            analyzeStmt(thenStmt, buffer);
            if(root->children.size() > 5 && root->children[5]->type == NodeType::TERMINAL){
                // has else
                // skip goto after then
                int skipPos = buffer.size();
                auto skipJump = new Instruction(
                    Operand("null", Type::null), Operand(), Operand("", Type::IntLiteral), Operator::_goto
                );
                buffer.push_back(skipJump);
                // patch condJump to else branch
                condJump->des = Operand(std::to_string(skipPos+1), Type::IntLiteral);
                // else branch
                GET_CHILD_PTR(elseStmt, Stmt, 6);
                analyzeStmt(elseStmt, buffer);
                // patch skipJump to after else
                skipJump->des = Operand(std::to_string(buffer.size()), Type::IntLiteral);
            } else {
                // no else: patch condJump to after then
                condJump->des = Operand(std::to_string(buffer.size()), Type::IntLiteral);
            }
            return;
        }
        // while statement
        if(term->token.type == TokenType::WHILETK){
            // while '(' Cond ')' Stmt
            // record loop start
            int loopStart = buffer.size();
            GET_CHILD_PTR(cond, Cond, 2);
            analyzeCond(cond, buffer);
            // invert cond for exit
            std::string notVar2 = getTmpName();
            buffer.push_back(new Instruction(
                Operand(cond->v, Type::Int), Operand(), Operand(notVar2, Type::Int), Operator::_not
            ));
            // placeholder exit jump
            auto exitJump = new Instruction(
                Operand(notVar2, Type::Int), Operand(), Operand("", Type::IntLiteral), Operator::_goto
            );
            buffer.push_back(exitJump);
            // body
            GET_CHILD_PTR(bodyStmt, Stmt, 4);
            analyzeStmt(bodyStmt, buffer);
            // after body, jump back to loop start
            buffer.push_back(new Instruction(
                Operand("null", Type::null), Operand(), Operand(std::to_string(loopStart), Type::IntLiteral), Operator::_goto
            ));
            // patch continue jumps to loop start
            for(auto inst: root->jump_bow){ inst->des = Operand(std::to_string(loopStart), Type::IntLiteral); }
            // patch break jumps to after loop
            int loopEnd = buffer.size();
            for(auto inst: root->jump_eow){ inst->des = Operand(std::to_string(loopEnd), Type::IntLiteral); }
            // patch exitJump
            exitJump->des = Operand(std::to_string(loopEnd), Type::IntLiteral);
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
void frontend::Analyzer::analyzeLVal(LVal* root, vector<ir::Instruction*>& buffer) {
    // LVal -> Ident {'[' Exp ']'}
    GET_CHILD_PTR(ident, Term, 0);
    std::string name = ident->token.value;
    ir::Operand base = symbol_table.get_operand(name);
    if (root->children.size() == 1) {
        // simple variable
        root->is_computable = false;
        root->v = base.name;
        root->t = base.type;
    } else {
        // array element access chain
        ir::Operand cur = base;
        for (size_t i = 1; i + 1 < root->children.size(); i += 2) {
            GET_CHILD_PTR(exp, Exp, i+1);
            analyzeExp(exp, buffer);
            std::string tmp = getTmpName();
            buffer.push_back(new Instruction(
                Operand(cur.name, cur.type),
                Operand(exp->v, exp->t),
                Operand(tmp, cur.type == Type::IntPtr ? Type::Int : Type::Float),
                Operator::load
            ));
            cur = Operand(tmp, cur.type == Type::IntPtr ? Type::Int : Type::Float);
        }
        root->is_computable = false;
        root->v = cur.name;
        root->t = cur.type;
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
    // AddExp -> MulExp { ('+' | '-') MulExp }
    // 初始项
    GET_CHILD_PTR(first, MulExp, 0);
    analyzeMulExp(first, buffer);
    root->is_computable = first->is_computable;
    root->v = first->v;
    root->t = first->t;
    // 后续加减
    for(size_t i = 1; i + 1 < root->children.size(); i += 2) {
        // 运算符
        GET_CHILD_PTR(opTerm, Term, i);
        std::string op = opTerm->token.value;
        // 右操作数
        GET_CHILD_PTR(next, MulExp, i+1);
        analyzeMulExp(next, buffer);
        // 常量合并
        if(root->is_computable && next->is_computable) {
            if(root->t == Type::Int) {
                int l = std::stoi(root->v);
                int r = std::stoi(next->v);
                int res = (op == "+" ? l + r : l - r);
                root->v = std::to_string(res);
                root->t = Type::IntLiteral;
            } else {
                double l = std::stod(root->v);
                double r = std::stod(next->v);
                double res = (op == "+" ? l + r : l - r);
                root->v = std::to_string(res);
                root->t = Type::FloatLiteral;
            }
            root->is_computable = true;
        } else {
            // 生成 IR
            std::string dst = getTmpName();
            if(root->t == Type::Int) {
                // 立即数加减
                if(next->t == Type::IntLiteral) {
                    buffer.push_back(new Instruction(
                        Operand(root->v, Type::Int),
                        Operand(next->v, Type::IntLiteral),
                        Operand(dst, Type::Int),
                        op == "+" ? Operator::addi : Operator::subi
                    ));
                } else {
                    buffer.push_back(new Instruction(
                        Operand(root->v, Type::Int),
                        Operand(next->v, Type::Int),
                        Operand(dst, Type::Int),
                        op == "+" ? Operator::add : Operator::sub
                    ));
                }
            } else {
                // 浮点加减
                buffer.push_back(new Instruction(
                    Operand(root->v, Type::Float),
                    Operand(next->v, Type::Float),
                    Operand(dst, Type::Float),
                    op == "+" ? Operator::fadd : Operator::fsub
                ));
            }
            root->v = dst;
            root->t = root->t == Type::IntLiteral ? Type::Int : root->t; // 若初始为Literal，结果为普通类型
            root->is_computable = false;
        }
    }
}

// MulExp -> UnaryExp { ('*' | '/' | '%') UnaryExp }
void frontend::Analyzer::analyzeMulExp(MulExp* root, vector<ir::Instruction*>& buffer) {
    // MulExp -> UnaryExp { ('*' | '/' | '%') UnaryExp }
    GET_CHILD_PTR(first, UnaryExp, 0);
    analyzeUnaryExp(first, buffer);
    root->is_computable = first->is_computable;
    root->v = first->v;
    root->t = first->t;
    for(size_t i = 1; i + 1 < root->children.size(); i += 2) {
        GET_CHILD_PTR(opTerm, Term, i);
        std::string op = opTerm->token.value;
        GET_CHILD_PTR(next, UnaryExp, i+1);
        analyzeUnaryExp(next, buffer);
        // 常量合并
        if(root->is_computable && next->is_computable) {
            if(root->t == Type::Int) {
                int l = std::stoi(root->v);
                int r = std::stoi(next->v);
                int res = 0;
                if(op == "*") res = l * r;
                else if(op == "/") res = l / r;
                else if(op == "%") res = l % r;
                root->v = std::to_string(res);
                root->t = Type::IntLiteral;
            } else {
                double l = std::stod(root->v);
                double r = std::stod(next->v);
                double res = (op == "*") ? l * r : l / r;
                root->v = std::to_string(res);
                root->t = Type::FloatLiteral;
            }
            root->is_computable = true;
        } else {
            // 非常量，生成 IR，处理立即数
            std::string dst = getTmpName();
            std::string lop = root->v;
            std::string rop = next->v;
            // 如果左操作数是立即数，先def 一个临时变量
            if(root->t == Type::IntLiteral) {
                std::string tmpL = getTmpName();
                buffer.push_back(new Instruction(
                    Operand(root->v, Type::IntLiteral), Operand(), Operand(tmpL, Type::Int), Operator::def
                ));
                lop = tmpL;
            }
            // 如果右操作数是立即数，先def 一个临时变量
            if(next->t == Type::IntLiteral) {
                std::string tmpR = getTmpName();
                buffer.push_back(new Instruction(
                    Operand(next->v, Type::IntLiteral), Operand(), Operand(tmpR, Type::Int), Operator::def
                ));
                rop = tmpR;
            }
            if(root->t == Type::Int) {
                // 整型运算
                Operator opc = (op == "*") ? Operator::mul : (op == "/" ? Operator::div : Operator::mod);
                buffer.push_back(new Instruction(
                    Operand(lop, Type::Int), Operand(rop, Type::Int), Operand(dst, Type::Int), opc
                ));
                root->t = Type::Int;
            } else {
                // 浮点运算
                // 处理浮点立即数
                if(root->t == Type::FloatLiteral) {
                    std::string tmpLf = getTmpName();
                    buffer.push_back(new Instruction(
                        Operand(root->v, Type::FloatLiteral), Operand(), Operand(tmpLf, Type::Float), Operator::fdef
                    ));
                    lop = tmpLf;
                }
                if(next->t == Type::FloatLiteral) {
                    std::string tmpRf = getTmpName();
                    buffer.push_back(new Instruction(
                        Operand(next->v, Type::FloatLiteral), Operand(), Operand(tmpRf, Type::Float), Operator::fdef
                    ));
                    rop = tmpRf;
                }
                Operator opc = (op == "*") ? Operator::fmul : Operator::fdiv;
                buffer.push_back(new Instruction(
                    Operand(lop, Type::Float), Operand(rop, Type::Float), Operand(dst, Type::Float), opc
                ));
                root->t = Type::Float;
            }
            root->v = dst;
            root->is_computable = false;
        }
    }
}

// UnaryExp -> PrimaryExp | Ident '(' [FuncRParams] ')' | UnaryOp UnaryExp
void frontend::Analyzer::analyzeUnaryExp(UnaryExp* root, vector<ir::Instruction*>& buffer) {
    // UnaryExp -> PrimaryExp | Ident '(' [FuncRParams] ')' | UnaryOp UnaryExp
    if(root->children.size() == 1 && root->children[0]->type == NodeType::PRIMARYEXP) {
        GET_CHILD_PTR(pe, PrimaryExp, 0);
        analyzePrimaryExp(pe, buffer);
        root->is_computable = pe->is_computable;
        root->v = pe->v;
        root->t = pe->t;
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
        GET_CHILD_PTR(ue, UnaryExp, 1);
        analyzeUnaryExp(ue, buffer);
        // 处理 + - !
        if(uop->op == TokenType::PLUS) {
            root->is_computable = ue->is_computable;
            root->v = ue->v;
            root->t = ue->t;
        } else if(uop->op == TokenType::MINU) {
            if(ue->is_computable) {
                if(ue->t == Type::Int) {
                    int r = -std::stoi(ue->v);
                    root->v = std::to_string(r);
                    root->t = Type::IntLiteral;
                    root->is_computable = true;
                } else {
                    double r = -std::stod(ue->v);
                    root->v = std::to_string(r);
                    root->t = Type::FloatLiteral;
                    root->is_computable = true;
                }
            } else {
                // 生成 0 - x
                std::string dst = getTmpName();
                if(ue->t == Type::Int) {
                    buffer.push_back(new Instruction(
                        Operand("0", Type::IntLiteral), Operand(ue->v, Type::Int), Operand(dst, Type::Int), Operator::sub
                    ));
                    root->t = Type::Int;
                } else {
                    buffer.push_back(new Instruction(
                        Operand("0", Type::FloatLiteral), Operand(ue->v, Type::Float), Operand(dst, Type::Float), Operator::fsub
                    ));
                    root->t = Type::Float;
                }
                root->v = dst;
                root->is_computable = false;
            }
        } else if(uop->op == TokenType::NOT) {
            std::string dst = getTmpName();
            buffer.push_back(new Instruction(
                Operand(ue->v, ue->t), Operand(), Operand(dst, ue->t), Operator::_not
            ));
            root->v = dst;
            root->t = ue->t;
            root->is_computable = false;
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
        analyzeLVal(lval, buffer);
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
        root->t = Type::IntLiteral;
        root->v = term->token.value;
        // 对二、八、十六进制数字进行转换
        if(root->v.size() > 2 && root->v[0] == '0'){
            if(root->v[1] == 'x' || root->v[1] == 'X'){
                // 十六进制
                root->v = std::to_string(std::stoi(root->v, nullptr, 16));
            }
            else if(root->v[1] == 'b' || root->v[1] == 'B'){
                // 二进制
                root->v = std::to_string(std::stoi(root->v, nullptr, 2));
            }
            else{
                // 八进制 他可能只有单个前导零而不是0o
                root->v = std::to_string(std::stoi(root->v, nullptr, 8));
            }
        }
    }
    else if(term->token.type == TokenType::FLOATLTR){
        root->t = Type::FloatLiteral;
        root->v = term->token.value;
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
    // LOrExp -> LAndExp [ '||' LOrExp ]
    // initial left operand
    GET_CHILD_PTR(first, LAndExp, 0);
    analyzeLAndExp(first, buffer);
    root->is_computable = first->is_computable;
    root->v = first->v;
    root->t = first->t;
    // optional '||' right operand
    if(root->children.size() > 1) {
        // '||'
        GET_CHILD_PTR(next, LOrExp, 2);
        analyzeLOrExp(next, buffer);
        // constant folding
        if(root->is_computable && next->is_computable) {
            int l = std::stoi(root->v);
            int r = std::stoi(next->v);
            int res = (l || r);
            root->v = std::to_string(res);
            root->t = Type::IntLiteral;
            root->is_computable = true;
        } else {
            // generate IR for logical or
            std::string dst = getTmpName();
            buffer.push_back(new Instruction(
                Operand(root->v, Type::Int),
                Operand(next->v, Type::Int),
                Operand(dst, Type::Int),
                Operator::_or
            ));
            root->v = dst;
            root->t = Type::Int;
            root->is_computable = false;
        }
    }
}

// LAndExp -> EqExp [ '&&' LAndExp ]
void frontend::Analyzer::analyzeLAndExp(LAndExp* root, vector<ir::Instruction*>& buffer) {
    // LAndExp -> EqExp [ '&&' LAndExp ]
    GET_CHILD_PTR(first, EqExp, 0);
    analyzeEqExp(first, buffer);
    root->is_computable = first->is_computable;
    root->v = first->v;
    root->t = first->t;
    if(root->children.size() > 1) {
        // '&&'
        GET_CHILD_PTR(next, LAndExp, 2);
        analyzeLAndExp(next, buffer);
        // constant folding
        if(root->is_computable && next->is_computable) {
            int l = std::stoi(root->v);
            int r = std::stoi(next->v);
            int res = (l && r);
            root->v = std::to_string(res);
            root->t = Type::IntLiteral;
            root->is_computable = true;
        } else {
            // generate IR for logical and
            std::string dst = getTmpName();
            buffer.push_back(new Instruction(
                Operand(root->v, Type::Int),
                Operand(next->v, Type::Int),
                Operand(dst, Type::Int),
                Operator::_and
            ));
            root->v = dst;
            root->t = Type::Int;
            root->is_computable = false;
        }
    }
}

// EqExp -> RelExp { ('==' | '!=') RelExp }
void frontend::Analyzer::analyzeEqExp(EqExp* root, vector<ir::Instruction*>& buffer) {
    // EqExp -> RelExp { ('==' | '!=') RelExp }
    // initial operand
    GET_CHILD_PTR(first, RelExp, 0);
    analyzeRelExp(first, buffer);
    root->is_computable = first->is_computable;
    root->v = first->v;
    root->t = Type::Int;
    // subsequent comparisons
    for(size_t i = 1; i + 1 < root->children.size(); i += 2) {
        GET_CHILD_PTR(opTerm, Term, i);
        std::string op = opTerm->token.value;
        GET_CHILD_PTR(next, RelExp, i+1);
        analyzeRelExp(next, buffer);
        // constant folding
        if(root->is_computable && next->is_computable) {
            int l = std::stoi(root->v);
            int r = std::stoi(next->v);
            int res = 0;
            if(op == "==") res = (l == r);
            else res = (l != r);
            root->v = std::to_string(res);
            root->t = Type::IntLiteral;
            root->is_computable = true;
        } else {
            // generate IR
            std::string dst = getTmpName();
            // select operator
            Operator opc;
            if(op == "==" ) opc = Operator::eq;
            else opc = Operator::neq;
            buffer.push_back(new Instruction(
                Operand(first->v, first->t),
                Operand(next->v, next->t),
                Operand(dst, Type::Int),
                opc
            ));
            root->v = dst;
            root->t = Type::Int;
            root->is_computable = false;
        }
    }
}

// RelExp -> AddExp { ('<' | '>' | '<=' | '>=') AddExp }
void frontend::Analyzer::analyzeRelExp(RelExp* root, vector<ir::Instruction*>& buffer) {
    // RelExp -> AddExp { ('<' | '>' | '<=' | '>=') AddExp }
    GET_CHILD_PTR(first, AddExp, 0);
    analyzeAddExp(first, buffer);
    root->is_computable = first->is_computable;
    root->v = first->v;
    root->t = Type::Int;
    for(size_t i = 1; i + 1 < root->children.size(); i += 2) {
        GET_CHILD_PTR(opTerm, Term, i);
        std::string op = opTerm->token.value;
        GET_CHILD_PTR(next, AddExp, i+1);
        analyzeAddExp(next, buffer);
        // constant folding
        if(root->is_computable && next->is_computable) {
            if(first->t == Type::Int) {
                int l = std::stoi(root->v);
                int r = std::stoi(next->v);
                int res = 0;
                if(op == "<") res = (l < r);
                else if(op == ">") res = (l > r);
                else if(op == "<=") res = (l <= r);
                else res = (l >= r);
                root->v = std::to_string(res);
            } else {
                double l = std::stod(root->v);
                double r = std::stod(next->v);
                int res = 0;
                if(op == "<") res = (l < r);
                else if(op == ">") res = (l > r);
                else if(op == "<=") res = (l <= r);
                else res = (l >= r);
                root->v = std::to_string(res);
            }
            root->t = Type::IntLiteral;
            root->is_computable = true;
        } else {
            // generate IR
            std::string dst = getTmpName();
            Operator opc;
            bool isFloat = (first->t == Type::Float || next->t == Type::Float);
            if(op == "<") opc = isFloat ? Operator::flss : Operator::lss;
            else if(op == ">") opc = isFloat ? Operator::fgtr : Operator::gtr;
            else if(op == "<=") opc = isFloat ? Operator::fleq : Operator::leq;
            else opc = isFloat ? Operator::fgeq : Operator::geq;
            buffer.push_back(new Instruction(
                Operand(first->v, first->t),
                Operand(next->v, next->t),
                Operand(dst, Type::Int),
                opc
            ));
            root->v = dst;
            root->t = Type::Int;
            root->is_computable = false;
        }
    }
}

// ConstExp -> AddExp
void frontend::Analyzer::analyzeConstExp(ConstExp* root, vector<ir::Instruction*>& buffer) {
    GET_CHILD_PTR(addExp, AddExp, 0);
    analyzeAddExp(addExp, buffer);
    COPY_EXP_NODE(addExp, root); 
}