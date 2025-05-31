#include"front/syntax.h"

#include<iostream>
#include<cassert>

using frontend::Parser;

// #define DEBUG_PARSER
#define TODO assert(0 && "todo")
#define CUR_TOKEN_IS(tk_type) (token_stream[index].type == TokenType::tk_type)
#define PARSE_TOKEN(tk_type) root->children.push_back(parseTerm(root, TokenType::tk_type))
#define PARSE(name, type) auto name = new type(root); assert(parse##type(name)); root->children.push_back(name); 


Parser::Parser(const std::vector<frontend::Token>& tokens): index(0), token_stream(tokens) {}

Parser::~Parser() {}

frontend::CompUnit* Parser::get_abstract_syntax_tree(){
    auto root = new CompUnit;
    parseCompUnit(root);
    return root;
}

void Parser::log(AstNode* node){
#ifdef DEBUG_PARSER
        std::cout << "in parse" << toString(node->type) << ", cur_token_type::" << toString(token_stream[index].type) << ", token_val::" << token_stream[index].value << '\n';
#endif
}

#include"front/abstract_syntax_tree.h"
frontend::Term* Parser::parseTerm(frontend::AstNode* parent, frontend::TokenType expected){
    #ifdef DEBUG_PARSER
    // std::cout << toString(token_stream[index].type) << " # " << toString(expected) << std::endl;
    #endif
    if(token_stream[index].type != expected) assert(0 && "unmatched token type");
    Term* now = new Term(token_stream[index]);
    // parent->children.push_back(now);
    index++;
    return now;
}

// CompUnit -> (Decl | FuncDef) [CompUnit]
bool Parser::parseCompUnit(CompUnit* root) {
    log(root);
    
    // 判断是Decl还是FuncDef
    if ((CUR_TOKEN_IS(CONSTTK)) || 
        ((CUR_TOKEN_IS(INTTK) || CUR_TOKEN_IS(FLOATTK)) && 
         index + 1 < token_stream.size() && 
         token_stream[index + 1].type == TokenType::IDENFR && 
         (index + 2 >= token_stream.size() || token_stream[index + 2].type != TokenType::LPARENT))) {
        // 这是Decl
        PARSE(decl, Decl);
    } else {
        // 这是FuncDef
        PARSE(funcDef, FuncDef);
    }
    
    // 递归解析CompUnit
    if (index < token_stream.size()) {
        PARSE(compUnit, CompUnit);
    }
    
    return true;
}

// Decl -> ConstDecl | VarDecl
bool Parser::parseDecl(Decl* root) {
    log(root);
    
    if (CUR_TOKEN_IS(CONSTTK)) {
        PARSE(constDecl, ConstDecl);
    } else {
        PARSE(varDecl, VarDecl);
    }
    
    return true;
}

// ConstDecl -> 'const' BType ConstDef { ',' ConstDef } ';'
bool Parser::parseConstDecl(ConstDecl* root) {
    log(root);
    
    PARSE_TOKEN(CONSTTK);
    PARSE(bType, BType);
    PARSE(constDef, ConstDef);
    
    while (CUR_TOKEN_IS(COMMA)) {
        PARSE_TOKEN(COMMA);
        PARSE(constDef, ConstDef);
    }
    
    PARSE_TOKEN(SEMICN);
    
    return true;
}

// BType -> 'int' | 'float'
bool Parser::parseBType(BType* root) {
    log(root);
    
    if (CUR_TOKEN_IS(INTTK)) {
        PARSE_TOKEN(INTTK);
    } else if (CUR_TOKEN_IS(FLOATTK)) {
        PARSE_TOKEN(FLOATTK);
    } else {
        assert(0 && "Expected int or float for BType");
    }
    
    return true;
}

// ConstDef -> Ident { '[' ConstExp ']' } '=' ConstInitVal
bool Parser::parseConstDef(ConstDef* root) {
    log(root);
    
    PARSE_TOKEN(IDENFR);
    
    while (CUR_TOKEN_IS(LBRACK)) {
        PARSE_TOKEN(LBRACK);
        PARSE(constExp, ConstExp);
        PARSE_TOKEN(RBRACK);
    }
    
    PARSE_TOKEN(ASSIGN);
    PARSE(constInitVal, ConstInitVal);
    
    return true;
}

// ConstInitVal -> ConstExp | '{' [ ConstInitVal { ',' ConstInitVal } ] '}'
bool Parser::parseConstInitVal(ConstInitVal* root) {
    log(root);
    
    if (CUR_TOKEN_IS(LBRACE)) {
        PARSE_TOKEN(LBRACE);
        
        if (!CUR_TOKEN_IS(RBRACE)) {
            PARSE(constInitVal, ConstInitVal);
            
            while (CUR_TOKEN_IS(COMMA)) {
                PARSE_TOKEN(COMMA);
                PARSE(constInitVal, ConstInitVal);
            }
        }
        
        PARSE_TOKEN(RBRACE);
    } else {
        PARSE(constExp, ConstExp);
    }
    
    return true;
}

// VarDecl -> BType VarDef { ',' VarDef } ';'
bool Parser::parseVarDecl(VarDecl* root) {
    log(root);
    
    PARSE(bType, BType);
    PARSE(varDef, VarDef);
    
    while (CUR_TOKEN_IS(COMMA)) {
        PARSE_TOKEN(COMMA);
        PARSE(varDef, VarDef);
    }
    
    PARSE_TOKEN(SEMICN);
    
    return true;
}

// VarDef -> Ident { '[' ConstExp ']' } [ '=' InitVal ]
bool Parser::parseVarDef(VarDef* root) {
    log(root);
    
    PARSE_TOKEN(IDENFR);
    
    while (CUR_TOKEN_IS(LBRACK)) {
        PARSE_TOKEN(LBRACK);
        PARSE(constExp, ConstExp);
        PARSE_TOKEN(RBRACK);
    }
    
    if (CUR_TOKEN_IS(ASSIGN)) {
        PARSE_TOKEN(ASSIGN);
        PARSE(initVal, InitVal);
    }
    
    return true;
}

// InitVal -> Exp | '{' [ InitVal { ',' InitVal } ] '}'
bool Parser::parseInitVal(InitVal* root) {
    log(root);
    
    if (CUR_TOKEN_IS(LBRACE)) {
        PARSE_TOKEN(LBRACE);
        
        if (!CUR_TOKEN_IS(RBRACE)) {
            PARSE(initVal, InitVal);
            
            while (CUR_TOKEN_IS(COMMA)) {
                PARSE_TOKEN(COMMA);
                PARSE(initVal, InitVal);
            }
        }
        
        PARSE_TOKEN(RBRACE);
    } else {
        PARSE(exp, Exp);
    }
    
    return true;
}

// FuncDef -> FuncType Ident '(' [FuncFParams] ')' Block
bool Parser::parseFuncDef(FuncDef* root) {
    log(root);

    PARSE(functype, FuncType);
    PARSE_TOKEN(IDENFR);
    PARSE_TOKEN(LPARENT);
    
    // 判断是否有参数
    if (CUR_TOKEN_IS(RPARENT)) {
        PARSE_TOKEN(RPARENT);
    } else {
        PARSE(node, FuncFParams);
        PARSE_TOKEN(RPARENT);
    }
    
    PARSE(block, Block);
    
    return true;
}

// FuncType -> 'void' | 'int' | 'float'
bool Parser::parseFuncType(FuncType* root) {
    log(root);
    
    if (CUR_TOKEN_IS(VOIDTK)) {
        PARSE_TOKEN(VOIDTK);
    } else if (CUR_TOKEN_IS(INTTK)) {
        PARSE_TOKEN(INTTK);
    } else if (CUR_TOKEN_IS(FLOATTK)) {
        PARSE_TOKEN(FLOATTK);
    } else {
        assert(0 && "Expected void, int, or float for FuncType");
    }
    
    return true;
}

// FuncFParams -> FuncFParam { ',' FuncFParam }
bool Parser::parseFuncFParams(FuncFParams* root) {
    log(root);
    
    PARSE(funcFParam, FuncFParam);
    
    while (CUR_TOKEN_IS(COMMA)) {
        PARSE_TOKEN(COMMA);
        PARSE(funcFParam, FuncFParam);
    }
    
    return true;
}

// FuncFParam -> BType Ident ['[' ']' { '[' ConstExp ']' }]
bool Parser::parseFuncFParam(FuncFParam* root) {
    log(root);
    
    PARSE(bType, BType);
    PARSE_TOKEN(IDENFR);
    
    if (CUR_TOKEN_IS(LBRACK)) {
        PARSE_TOKEN(LBRACK);
        PARSE_TOKEN(RBRACK);
        
        while (CUR_TOKEN_IS(LBRACK)) {
            PARSE_TOKEN(LBRACK);
            PARSE(constExp, ConstExp);
            PARSE_TOKEN(RBRACK);
        }
    }
    
    return true;
}

// Block -> '{' { BlockItem } '}'
bool Parser::parseBlock(Block* root) {
    log(root);
    
    PARSE_TOKEN(LBRACE);
    
    while (!CUR_TOKEN_IS(RBRACE)) {
        PARSE(blockItem, BlockItem);
    }
    
    PARSE_TOKEN(RBRACE);
    
    return true;
}

// BlockItem -> Decl | Stmt
bool Parser::parseBlockItem(BlockItem* root) {
    log(root);
    
    if (CUR_TOKEN_IS(CONSTTK) || 
        ((CUR_TOKEN_IS(INTTK) || CUR_TOKEN_IS(FLOATTK)) && 
         index + 1 < token_stream.size() && 
         token_stream[index + 1].type == TokenType::IDENFR &&
         (index + 2 >= token_stream.size() || token_stream[index + 2].type != TokenType::LPARENT))) {
        PARSE(decl, Decl);
    } else {
        PARSE(stmt, Stmt);
    }
    
    return true;
}

// Stmt -> LVal '=' Exp ';' | Block | 'if' '(' Cond ')' Stmt [ 'else' Stmt ] | 'while' '(' Cond ')' Stmt | 'break' ';' | 'continue' ';' | 'return' [Exp] ';' | [Exp] ';'
bool Parser::parseStmt(Stmt* root) {
    log(root);
    
    if (CUR_TOKEN_IS(LBRACE)) {
        // Block
        PARSE(block, Block);
    } else if (CUR_TOKEN_IS(IFTK)) {
        // if (Cond) Stmt [else Stmt]
        PARSE_TOKEN(IFTK);
        PARSE_TOKEN(LPARENT);
        PARSE(cond, Cond);
        PARSE_TOKEN(RPARENT);
        PARSE(stmt, Stmt);
        
        if (CUR_TOKEN_IS(ELSETK)) {
            PARSE_TOKEN(ELSETK);
            PARSE(elseStmt, Stmt);
        }
    } else if (CUR_TOKEN_IS(WHILETK)) {
        // while (Cond) Stmt
        PARSE_TOKEN(WHILETK);
        PARSE_TOKEN(LPARENT);
        PARSE(cond, Cond);
        PARSE_TOKEN(RPARENT);
        PARSE(stmt, Stmt);
    } else if (CUR_TOKEN_IS(BREAKTK)) {
        // break ;
        PARSE_TOKEN(BREAKTK);
        PARSE_TOKEN(SEMICN);
    } else if (CUR_TOKEN_IS(CONTINUETK)) {
        // continue ;
        PARSE_TOKEN(CONTINUETK);
        PARSE_TOKEN(SEMICN);
    } else if (CUR_TOKEN_IS(RETURNTK)) {
        // return [Exp] ;
        PARSE_TOKEN(RETURNTK);
        
        if (!CUR_TOKEN_IS(SEMICN)) {
            PARSE(exp, Exp);
        }
        
        PARSE_TOKEN(SEMICN);
    } else if (CUR_TOKEN_IS(SEMICN)) {
        // 空语句 ;
        PARSE_TOKEN(SEMICN);
    } else {
        // 需要判断是 LVal = Exp ; 还是 Exp ;
        // 备份当前索引以便回溯
        int backup_index = index;
        bool is_lval_assign = false;
        
        // 尝试解析 LVal
        LVal* lval = new LVal(root);
        try {
            if (parseLVal(lval)) {
                if (CUR_TOKEN_IS(ASSIGN)) {
                    // LVal = Exp ;
                    is_lval_assign = true;
                    root->children.push_back(lval);
                    PARSE_TOKEN(ASSIGN);
                    PARSE(exp, Exp);
                    PARSE_TOKEN(SEMICN);
                }
            }
        } catch (...) { }
        
        if (!is_lval_assign) {
            // 恢复索引并解析 Exp ;
            delete lval;
            index = backup_index;
            
            if (!CUR_TOKEN_IS(SEMICN)) {
                PARSE(exp, Exp);
            }
            PARSE_TOKEN(SEMICN);
        }
    }
    
    return true;
}

// Exp -> AddExp
bool Parser::parseExp(Exp* root) {
    log(root);
    
    PARSE(addExp, AddExp);
    
    return true;
}

// Cond -> LOrExp
bool Parser::parseCond(Cond* root) {
    log(root);
    
    PARSE(lOrExp, LOrExp);
    
    return true;
}

// LVal -> Ident {'[' Exp ']'}
bool Parser::parseLVal(LVal* root) {
    log(root);
    
    PARSE_TOKEN(IDENFR);
    
    while (CUR_TOKEN_IS(LBRACK)) {
        PARSE_TOKEN(LBRACK);
        PARSE(exp, Exp);
        PARSE_TOKEN(RBRACK);
    }
    
    return true;
}

// PrimaryExp -> '(' Exp ')' | LVal | Number
bool Parser::parsePrimaryExp(PrimaryExp* root) {
    log(root);
    
    if (CUR_TOKEN_IS(LPARENT)) {
        // '(' Exp ')'
        PARSE_TOKEN(LPARENT);
        PARSE(exp, Exp);
        PARSE_TOKEN(RPARENT);
    } else if (CUR_TOKEN_IS(IDENFR)) {
        // LVal
        PARSE(lval, LVal);
    } else {
        // Number
        PARSE(number, Number);
    }
    
    return true;
}

// Number -> IntConst | floatConst
bool Parser::parseNumber(Number* root) {
    log(root);
    
    if (CUR_TOKEN_IS(INTLTR)) {
        PARSE_TOKEN(INTLTR);
    } else if (CUR_TOKEN_IS(FLOATLTR)) {
        PARSE_TOKEN(FLOATLTR);
    } else {
        assert(0 && "Expected integer or float literal");
    }
    
    return true;
}

// UnaryExp -> PrimaryExp | Ident '(' [FuncRParams] ')' | UnaryOp UnaryExp
bool Parser::parseUnaryExp(UnaryExp* root) {
    log(root);
    
    if (CUR_TOKEN_IS(PLUS) || CUR_TOKEN_IS(MINU) || CUR_TOKEN_IS(NOT)) {
        // UnaryOp UnaryExp
        PARSE(unaryOp, UnaryOp);
        PARSE(unaryExp, UnaryExp);
    } else if (CUR_TOKEN_IS(IDENFR) && 
               index + 1 < token_stream.size() && 
               token_stream[index + 1].type == TokenType::LPARENT) {
        // Ident '(' [FuncRParams] ')'
        PARSE_TOKEN(IDENFR);
        PARSE_TOKEN(LPARENT);
        
        if (!CUR_TOKEN_IS(RPARENT)) {
            PARSE(funcRParams, FuncRParams);
        }
        
        PARSE_TOKEN(RPARENT);
    } else {
        // PrimaryExp
        PARSE(primaryExp, PrimaryExp);
    }
    
    return true;
}

// UnaryOp -> '+' | '-' | '!'
bool Parser::parseUnaryOp(UnaryOp* root) {
    log(root);
    
    if (CUR_TOKEN_IS(PLUS)) {
        PARSE_TOKEN(PLUS);
    } else if (CUR_TOKEN_IS(MINU)) {
        PARSE_TOKEN(MINU);
    } else if (CUR_TOKEN_IS(NOT)) {
        PARSE_TOKEN(NOT);
    } else {
        assert(0 && "Expected +, -, or ! operator");
    }
    
    return true;
}

// FuncRParams -> Exp { ',' Exp }
bool Parser::parseFuncRParams(FuncRParams* root) {
    log(root);
    
    PARSE(exp, Exp);
    
    while (CUR_TOKEN_IS(COMMA)) {
        PARSE_TOKEN(COMMA);
        PARSE(exp, Exp);
    }
    
    return true;
}

// MulExp -> UnaryExp { ('*' | '/' | '%') UnaryExp }
bool Parser::parseMulExp(MulExp* root) {
    log(root);
    
    PARSE(unaryExp, UnaryExp);
    
    while (CUR_TOKEN_IS(MULT) || CUR_TOKEN_IS(DIV) || CUR_TOKEN_IS(MOD)) {
        if (CUR_TOKEN_IS(MULT)) {
            PARSE_TOKEN(MULT);
        } else if (CUR_TOKEN_IS(DIV)) {
            PARSE_TOKEN(DIV);
        } else {
            PARSE_TOKEN(MOD);
        }
        
        PARSE(unaryExp, UnaryExp);
    }
    
    return true;
}

// AddExp -> MulExp { ('+' | '-') MulExp }
bool Parser::parseAddExp(AddExp* root) {
    log(root);
    
    PARSE(mulExp, MulExp);
    
    while (CUR_TOKEN_IS(PLUS) || CUR_TOKEN_IS(MINU)) {
        if (CUR_TOKEN_IS(PLUS)) {
            PARSE_TOKEN(PLUS);
        } else {
            PARSE_TOKEN(MINU);
        }
        
        PARSE(mulExp, MulExp);
    }
    
    return true;
}

// RelExp -> AddExp { ('<' | '>' | '<=' | '>=') AddExp }
bool Parser::parseRelExp(RelExp* root) {
    log(root);
    
    PARSE(addExp, AddExp);
    
    while (CUR_TOKEN_IS(LSS) || CUR_TOKEN_IS(GTR) || CUR_TOKEN_IS(LEQ) || CUR_TOKEN_IS(GEQ)) {
        if (CUR_TOKEN_IS(LSS)) {
            PARSE_TOKEN(LSS);
        } else if (CUR_TOKEN_IS(GTR)) {
            PARSE_TOKEN(GTR);
        } else if (CUR_TOKEN_IS(LEQ)) {
            PARSE_TOKEN(LEQ);
        } else {
            PARSE_TOKEN(GEQ);
        }
        
        PARSE(addExp, AddExp);
    }
    
    return true;
}

// EqExp -> RelExp { ('==' | '!=') RelExp }
bool Parser::parseEqExp(EqExp* root) {
    log(root);
    
    PARSE(relExp, RelExp);
    
    while (CUR_TOKEN_IS(EQL) || CUR_TOKEN_IS(NEQ)) {
        if (CUR_TOKEN_IS(EQL)) {
            PARSE_TOKEN(EQL);
        } else {
            PARSE_TOKEN(NEQ);
        }
        
        PARSE(relExp, RelExp);
    }
    
    return true;
}

// LAndExp -> EqExp [ '&&' LAndExp ]
bool Parser::parseLAndExp(LAndExp* root) {
    log(root);
    
    PARSE(eqExp, EqExp);
    
    if (CUR_TOKEN_IS(AND)) {
        PARSE_TOKEN(AND);
        PARSE(lAndExp, LAndExp);
    }
    
    return true;
}

// LOrExp -> LAndExp [ '||' LOrExp ]
bool Parser::parseLOrExp(LOrExp* root) {
    log(root);
    
    PARSE(lAndExp, LAndExp);
    
    if (CUR_TOKEN_IS(OR)) {
        PARSE_TOKEN(OR);
        PARSE(lOrExp, LOrExp);
    }
    
    return true;
}

// ConstExp -> AddExp
bool Parser::parseConstExp(ConstExp* root) {
    log(root);
    
    PARSE(addExp, AddExp);
    
    return true;
}
/*
bool Parser::parseFuncDef(FuncDef* root){
    log(root);
    
    PARSE(functype, FuncType);
    PARSE_TOKEN(IDENFR);
    PARSE_TOKEN(LPARENT);
    // no [FuncFParams], FuncType Ident '(' ')' Block
    if(CUR_TOKEN_IS(RPARENT)){
        PARSE_TOKEN(RPARENT);
    }
    // FuncType	Ident '(' FuncFParams ')' Block
    else {
        PARSE(node,	FuncFParams);
        PARSE_TOKEN(RPARENT);
    }
}
*/