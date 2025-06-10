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

void frontend::SymbolTable::add_scope(Block* node) {
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

    // 添加运行时库到符号表
    symbol_table.functions = *get_lib_funcs();
    // 进入全局作用域
    symbol_table.add_scope(nullptr);

    analyzeCompUnit(root);

    // 将全局变量添加到globalVal中
    for(auto it: symbol_table.scope_stack[0].table){
        if(it.second.operand.type != Type::null){
            program.globalVal.push_back(
                ir::GlobalVal(
                    it.second.operand,
                    it.second.dimension.size() > 0 ? it.second.dimension[0] : 0
                )
            );
            // 疑问：const变量是否需要特殊处理？暂时假设不需要
        }
    }
    // 处理全局变量的初始化（使用global函数）
    Function global_func("global", Type::null);
    for(auto& inst: g_init_inst) {
        global_func.addInst(inst);
    }
    global_func.addInst(new Instruction({}, {}, {}, Operator::_return));
    program.addFunction(global_func);

    // 删除运行时库
    for(auto libFunc: *get_lib_funcs()){
        symbol_table.functions.erase(libFunc.first);
    }
    for(auto func: symbol_table.functions){
        if(func.first == "main") func.second->addInst(new ir::CallInst(Operand("global", Type::null), {})); // 在main开头调用global
        program.addFunction(*func.second);
    }

    // 退出全局作用域
    symbol_table.exit_scope();

    return program;
}

std::string frontend::Analyzer::getTmpName(){
    return "t" + std::to_string(tmp_cnt++);
}

// CompUnit -> (Decl | FuncDef) [CompUnit]
void frontend::Analyzer::analyzeCompUnit(CompUnit* root){
    if(MATCH_CHILD_TYPE(DECL, 0)){
        GET_CHILD_PTR(decl, Decl, 0);
        analyzeDecl(decl, g_init_inst);
    }
    else if(MATCH_CHILD_TYPE(FUNCDEF, 0)){
        GET_CHILD_PTR(funcDef, FuncDef, 0);
        analyzeFuncDef(funcDef);
    }
    else{
        assert(0 && "analyzeCompUnit error: expected Decl or FuncDef");
    }

    if(root->children.size() > 1){
        GET_CHILD_PTR(compUnit, CompUnit, 1);
        analyzeCompUnit(compUnit);
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
    for(size_t i = 2; i < root->children.size() - 1; i += 2){ // 跳过const、BType、分号
        GET_CHILD_PTR(constDef, ConstDef, i);
        analyzeConstDef(constDef, buffer, btype->t);
    }
}

// VarDecl -> BType VarDef { ',' VarDef } ';'
void frontend::Analyzer::analyzeVarDecl(VarDecl* root, vector<ir::Instruction*>& buffer) {
    GET_CHILD_PTR(btype, BType, 0);
    analyzeBType(btype);

    root->t = btype->t;

    for(size_t i = 1; i < root->children.size() - 1; i += 2){ // 跳过BType、分号
        GET_CHILD_PTR(varDef, VarDef, i);
        analyzeVarDef(varDef, buffer, btype->t);
    }
}

// ConstDef -> Ident { '[' ConstExp ']' } '=' ConstInitVal
void frontend::Analyzer::analyzeConstDef(ConstDef* root, vector<ir::Instruction*>& buffer, ir::Type t) {
    GET_CHILD_PTR(term, Term, 0);
    analyzeTerm(term);
    root->arr_name = symbol_table.get_scoped_name(term->token.value);

    vector<int> dims;
    int size = 0; // 如果非数组，size置0
    if(root->children.size() > 1 && MATCH_CHILD_TYPE(CONSTEXP, 2)){
        // 是数组
        size = 1;
        for(size_t i = 2; i < root->children.size(); i += 3)
            if(MATCH_CHILD_TYPE(CONSTEXP, i)){  // 计算每一维大小
                GET_CHILD_PTR(constExp, ConstExp, i);
                analyzeConstExp(constExp);
                assert(constExp->t == Type::IntLiteral && std::stoi(constExp->v) >= 0 && "ConstExp must be a const non-negative integer");
                dims.push_back(std::stoi(constExp->v));
                size *= std::stoi(constExp->v);
            }
            else break;
    }

    GET_CHILD_PTR(constInitVal, ConstInitVal, root->children.size() - 1);
    constInitVal->v = term->token.value; // 变量名
    if(t == Type::Int){
        if(size == 0){
            constInitVal->t = Type::Int;
            symbol_table.scope_stack.back().table[constInitVal->v] = STE(Operand(root->arr_name, Type::Int), dims);
        }
        else{
            constInitVal->t = Type::IntPtr;
            symbol_table.scope_stack.back().table[constInitVal->v] = STE(Operand(root->arr_name, Type::IntPtr), dims);
            // 分配数组空间
            buffer.push_back(new Instruction(
                Operand(std::to_string(size), Type::IntLiteral), // op1: 数组大小
                {}, // op2: 无
                Operand(constInitVal->v, Type::IntPtr), // dst
                Operator::alloc
            ));
        }
    }
    else if(t == Type::Float){
        if(size == 0){
            constInitVal->t = Type::Float;
            symbol_table.scope_stack.back().table[constInitVal->v] = STE(Operand(root->arr_name, Type::Float), dims);
        }
        else{
            constInitVal->t = Type::FloatPtr;
            symbol_table.scope_stack.back().table[constInitVal->v] = STE(Operand(root->arr_name, Type::FloatPtr), dims);
            // 分配数组空间
            buffer.push_back(new Instruction(
                Operand(std::to_string(size), Type::IntLiteral), // op1: 数组大小
                {}, // op2: 无
                Operand(constInitVal->v, Type::FloatPtr), // dst
                Operator::alloc
            ));
        }
    }
    else{
        assert(0 && "ConstDef error: unsupported type");
    }
    // 分析ConstInitVal
    analyzeConstInitVal(constInitVal, buffer, size, 0, 0, dims);
}

// ConstInitVal -> ConstExp | '{' [ ConstInitVal { ',' ConstInitVal } ] '}'
void frontend::Analyzer::analyzeConstInitVal(ConstInitVal* root, vector<ir::Instruction*>& buffer, int size, int now, int offset, vector<int>& dims) {
    // size: 总元素数（数组大小），now: 当前递归层数，offset: 当前偏移，dims: 每一维长度
    // 假设 root->v 已经存储了变量名（带作用域）
    string arr_name = root->v;
    ir::Type arr_type = Type::Int; // 默认int，实际应由外层传入或root->t
    if (root->t == Type::Float || root->t == Type::FloatPtr) arr_type = Type::Float;

    if (root->children.size() == 1 && MATCH_CHILD_TYPE(CONSTEXP, 0)) {
        // 叶子节点，单个常量
        GET_CHILD_PTR(constExp, ConstExp, 0);
        analyzeConstExp(constExp);
        root->t = constExp->t;
        root->v = constExp->v;
        // 生成IR：store 常量到数组/变量
        // 如果是数组元素，arr_name[offset] = constExp->v
        if (size > 0) {
            // 数组元素初始化
            buffer.push_back(new Instruction(
                Operand(constExp->v, constExp->t), // op1: 常量值
                Operand(std::to_string(offset), Type::IntLiteral), // op2: 偏移
                Operand(arr_name, arr_type == Type::Int ? Type::IntPtr : Type::FloatPtr), // dst: 数组名
                Operator::store
            ));
        } else {
            // 普通变量初始化
            buffer.push_back(new Instruction(
                Operand(constExp->v, constExp->t), // op1: 常量值
                {}, // op2: 无
                Operand(arr_name, arr_type), // dst: 变量名
                Operator::mov
            ));
        }
    } else {
        // '{' [ ConstInitVal { ',' ConstInitVal } ] '}'
        int idx = 1; // 跳过 '{'
        int elem = 0;
        int dim_size = (now < dims.size()) ? dims[now] : 1;
        // 统计本层已初始化元素数
        while (idx < root->children.size() - 1) { // 跳过 '}'
            if (MATCH_CHILD_TYPE(CONSTINITVAL, idx)) {
                GET_CHILD_PTR(subInit, ConstInitVal, idx);
                analyzeConstInitVal(subInit, buffer, d, now + 1, offset + elem * (d / dim_size), dims);
                elem++;
            }
            idx += 2; // 跳过 ','
        }
        // 补零：如果本层未填满，补0
        for (; elem < dim_size; ++elem) {
            int sub_offset = offset + elem * (d / dim_size);
            if (now + 1 == dims.size()) {
                // 最后一维，直接补0
                buffer.push_back(new Instruction(
                    Operand("0", arr_type), // op1: 0
                    Operand(std::to_string(sub_offset), Type::IntLiteral), // op2: 偏移
                    Operand(arr_name, arr_type == Type::Int ? Type::IntPtr : Type::FloatPtr), // dst
                    Operator::store
                ));
            } else {
                // 递归补零
                vector<int> sub_dims(dims.begin() + now + 1, dims.end());
                int sub_d = 1;
                for (int x : sub_dims) sub_d *= x;
                for (int k = 0; k < sub_d; ++k) {
                    buffer.push_back(new Instruction(
                        Operand("0", arr_type),
                        Operand(std::to_string(sub_offset + k), Type::IntLiteral),
                        Operand(arr_name, arr_type == Type::Int ? Type::IntPtr : Type::FloatPtr),
                        Operator::store
                    ));
                }
            }
        }
    }
}

// BType -> 'int' | 'float'
void frontend::Analyzer::analyzeBType(BType* root) {
    TODO;
}

// VarDef -> Ident { '[' ConstExp ']' } [ '=' InitVal ]
void frontend::Analyzer::analyzeVarDef(VarDef* root, vector<ir::Instruction*>& buffer, ir::Type t) {
    TODO;
}

// InitVal -> Exp | '{' [ InitVal { ',' InitVal } ] '}'
void frontend::Analyzer::analyzeInitVal(InitVal* root, vector<ir::Instruction*>& buffer, int d, int now, int tot, vector<int>& dims) {
    TODO;
}

// FuncDef -> FuncType Ident '(' [FuncFParams] ')' Block
void frontend::Analyzer::analyzeFuncDef(FuncDef* root) {
    TODO;
}

// FuncType -> 'void' | 'int' | 'float'
void frontend::Analyzer::analyzeFuncType(FuncType* root) {
    TODO;
}

// FuncFParams -> FuncFParam { ',' FuncFParam }
void frontend::Analyzer::analyzeFuncFParams(FuncFParams* root, vector<ir::Operand>& params) {
    for(size_t i = 0; i < root->children.size(); i += 2){
        GET_CHILD_PTR(param, FuncFParam, i);
        analyzeFuncFParam(param, params);
    }
}

// FuncFParam -> BType Ident ['[' ']' { '[' Exp ']' }]
void frontend::Analyzer::analyzeFuncFParam(FuncFParam* root, vector<ir::Operand>& params) {
    TODO;
}

// Block -> '{' { BlockItem } '}'
void frontend::Analyzer::analyzeBlock(Block* root, vector<ir::Instruction*>& buffer) {
    for(size_t i = 1; i + 1 < root->children.size(); ++i){
        GET_CHILD_PTR(item, BlockItem, i);
        analyzeBlockItem(item, buffer);
    }
}

void frontend::Analyzer::analyzeBlockItem(BlockItem* root, vector<ir::Instruction*>& buffer) {
    // BlockItem -> Decl | Stmt
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
    TODO;
}

// LVal -> Ident {'[' Exp ']'
void frontend::Analyzer::analyzeLVal(LVal* root, vector<ir::Instruction*>& buffer) {
    TODO;
}

// Exp -> AddExp
void frontend::Analyzer::analyzeExp(Exp* root, vector<ir::Instruction*>& buffer) {
    GET_CHILD_PTR(addExp, AddExp, 0);
    analyzeAddExp(addExp, buffer);
    COPY_EXP_NODE(addExp, root);
}

// AddExp -> MulExp { ('+' | '-') MulExp }
void frontend::Analyzer::analyzeAddExp(AddExp* root, vector<ir::Instruction*>& buffer) {
    TODO;
}

// MulExp -> UnaryExp { ('*' | '/' | '%') UnaryExp }
void frontend::Analyzer::analyzeMulExp(MulExp* root, vector<ir::Instruction*>& buffer) {
    TODO;
}

// UnaryExp -> PrimaryExp | Ident '(' [FuncRParams] ')' | UnaryOp UnaryExp
void frontend::Analyzer::analyzeUnaryExp(UnaryExp* root, vector<ir::Instruction*>& buffer) {
    TODO;
}

// PrimaryExp -> '(' Exp ')' | LVal | Number
void frontend::Analyzer::analyzePrimaryExp(PrimaryExp* root, vector<ir::Instruction*>& buffer) {
    TODO;
}

// Number -> IntConst | floatConst
void frontend::Analyzer::analyzeNumber(Number* root, vector<ir::Instruction*>& buffer) {
    TODO;
}

// UnaryOp -> '+' | '-' | '!'
void frontend::Analyzer::analyzeUnaryOp(UnaryOp* root) {
    TODO;
}

// FuncRParams -> Exp { ',' Exp }
void frontend::Analyzer::analyzeFuncRParams(FuncRParams* root, vector<ir::Instruction*>& buffer, vector<ir::Operand>& params, vector<ir::Operand>& types) {
    for(size_t i = 0; i < root->children.size(); i += 2){
        GET_CHILD_PTR(exp, Exp, i);
        analyzeExp(exp, buffer);
        params.push_back(ir::Operand(exp->v, exp->t));
        types.push_back(ir::Operand(exp->v, exp->t));
    }
    // 似乎浮点数需要特殊处理，等会儿补充
    TODO;
}

// Cond -> LOrExp
void frontend::Analyzer::analyzeCond(Cond* root, vector<ir::Instruction*>& buffer) {
    GET_CHILD_PTR(lOrExp, LOrExp, 0);
    analyzeLOrExp(lOrExp, buffer);
    COPY_EXP_NODE(lOrExp, root);
}

// LOrExp -> LAndExp [ '||' LOrExp ]
void frontend::Analyzer::analyzeLOrExp(LOrExp* root, vector<ir::Instruction*>& buffer) {
    TODO;
}

// LAndExp -> EqExp [ '&&' LAndExp ]
void frontend::Analyzer::analyzeLAndExp(LAndExp* root, vector<ir::Instruction*>& buffer) {
    TODO;
}

// EqExp -> RelExp { ('==' | '!=') RelExp }
void frontend::Analyzer::analyzeEqExp(EqExp* root, vector<ir::Instruction*>& buffer) {
    TODO;
}

// RelExp -> AddExp { ('<' | '>' | '<=' | '>=') AddExp }
void frontend::Analyzer::analyzeRelExp(RelExp* root, vector<ir::Instruction*>& buffer) {
    TODO;
}

// ConstExp -> AddExp
void frontend::Analyzer::analyzeConstExp(ConstExp* root) {
    TODO;
}

void frontend::Analyzer::analyzeTerm(Term* root) {
    TODO;
}