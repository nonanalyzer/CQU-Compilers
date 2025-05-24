/*
Exp -> AddExp
    Exp.v

Number -> IntConst | floatConst

PrimaryExp -> '(' Exp ')' | Number
    PrimaryExp.v

UnaryExp -> PrimaryExp | UnaryOp UnaryExp
    UnaryExp.v

UnaryOp -> '+' | '-'

MulExp -> UnaryExp { ('*' | '/') UnaryExp }
    MulExp.v

AddExp -> MulExp { ('+' | '-') MulExp }
    AddExp.v
*/
#include<map>
#include<cassert>
#include<string>
#include<iostream>
#include<vector>
#include<set>
#include<queue>

#define TODO assert(0 && "TODO")
// #define DEBUG_DFA
// #define DEBUG_PARSER

// enumerate for Status
enum class State {
    Empty,              // space, \n, \r ...
    IntLiteral,         // int literal, like '1' '01900', '0xAB', '0b11001'
    op                  // operators and '(', ')'
};
std::string toString(State s) {
    switch (s) {
    case State::Empty: return "Empty";
    case State::IntLiteral: return "IntLiteral";
    case State::op: return "op";
    default:
        assert(0 && "invalid State");
    }
    return "";
}

// enumerate for Token type
enum class TokenType{
    INTLTR,        // int literal
    PLUS,        // +
    MINU,        // -
    MULT,        // *
    DIV,        // /
    LPARENT,        // (
    RPARENT,        // )
};
std::string toString(TokenType type) {
    switch (type) {
    case TokenType::INTLTR: return "INTLTR";
    case TokenType::PLUS: return "PLUS";
    case TokenType::MINU: return "MINU";
    case TokenType::MULT: return "MULT";
    case TokenType::DIV: return "DIV";
    case TokenType::LPARENT: return "LPARENT";
    case TokenType::RPARENT: return "RPARENT";
    default:
        assert(0 && "invalid token type");
        break;
    }
    return "";
}

// definition of Token
struct Token {
    TokenType type;
    std::string value;
};

// definition of DFA
struct DFA {
    /**
     * @brief constructor, set the init state to State::Empty
     */
    DFA();
    
    /**
     * @brief destructor
     */
    ~DFA();
    
    // the meaning of copy and assignment for a DFA is not clear, so we do not allow them
    DFA(const DFA&) = delete;   // copy constructor
    DFA& operator=(const DFA&) = delete;    // assignment

    /**
     * @brief take a char as input, change state to next state, and output a Token if necessary
     * @param[in] input: the input character
     * @param[out] buf: the output Token buffer
     * @return  return true if a Token is produced, the buf is valid then
     */
    bool next(char input, Token& buf);

    /**
     * @brief reset the DFA state to begin
     */
    void reset();

private:
    State cur_state;    // record current state of the DFA
    std::string cur_str;    // record input characters
};


DFA::DFA(): cur_state(State::Empty), cur_str() {}

DFA::~DFA() {}

// helper function, you are not require to implement these, but they may be helpful
bool isoperator(char c) {
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '(' || c == ')';
}

TokenType get_op_type(std::string s) {
    if(s == "+") return TokenType::PLUS;
    else if(s == "-") return TokenType::MINU;
    else if(s == "*") return TokenType::MULT;
    else if(s == "/") return TokenType::DIV;
    else if(s == "(") return TokenType::LPARENT;
    else if(s == ")") return TokenType::RPARENT;
    else assert(0 && "invalid op");
    return TokenType::INTLTR; // for no warning
}

bool DFA::next(char input, Token& buf) {
    switch(cur_state){
        case State::Empty:
            // Empty to op
            if(isoperator(input)){
                cur_state = State::op;
                cur_str = input;
                return false;
            }
            // Empty to IntLiteral
            if(isdigit(input)){
                cur_state = State::IntLiteral;
                cur_str += input;
                return false;
            }
            // Empty to Empty
                reset();
                return false;

        case State::IntLiteral:
            // Int to op
            if(isoperator(input)){
                buf.value = cur_str;
                buf.type = TokenType::INTLTR;
                cur_state = State::op;
                cur_str = input;
                return true;
            }
            // Int to Int
            if(isdigit(input) || (input == 'x' || input == 'X') || (input >= 'a' && input <= 'f') || (input >= 'A' && input <= 'F')){
                cur_state = State::IntLiteral;
                cur_str += input;
                return false;
            }
            // Int to Empty
                buf.value = cur_str;
                buf.type = TokenType::INTLTR;
                reset();
                return true;

        case State::op:
            // op to op
            if(isoperator(input)){
                buf.value = cur_str;
                buf.type = get_op_type(cur_str);
                cur_state = State::op;
                cur_str = input;
                return true;
            }
            // op to IntLiteral
            if(isdigit(input) || (input == 'x' || input == 'X') || (input >= 'a' && input <= 'f') || (input >= 'A' && input <= 'F')){
                buf.value = cur_str;
                buf.type = get_op_type(cur_str);
                cur_state = State::IntLiteral;
                cur_str = input;
                return true;
            }
            // op to Empty
                buf.value = cur_str;
                buf.type = get_op_type(cur_str);
                reset();
                return true;

        default:
            assert(0 && "illegal state");
    }
    return 0; // for no warning too
}

void DFA::reset() {
    cur_state = State::Empty;
    cur_str = "";
}

// hw2
enum class NodeType {
    TERMINAL,       // terminal lexical unit
    EXP,
    NUMBER,
    PRIMARYEXP,
    UNARYEXP,
    UNARYOP,
    MULEXP,
    ADDEXP,
    NONE
};
std::string toString(NodeType nt) {
    switch (nt) {
    case NodeType::TERMINAL: return "Terminal";
    case NodeType::EXP: return "Exp";
    case NodeType::NUMBER: return "Number";
    case NodeType::PRIMARYEXP: return "PrimaryExp";
    case NodeType::UNARYEXP: return "UnaryExp";
    case NodeType::UNARYOP: return "UnaryOp";
    case NodeType::MULEXP: return "MulExp";
    case NodeType::ADDEXP: return "AddExp";
    case NodeType::NONE: return "NONE";
    default:
        assert(0 && "invalid node type");
        break;
    }
    return "";
}

// tree node basic class
struct AstNode{
    int value;
    NodeType type;  // the node type
    AstNode* parent;    // the parent node
    std::vector<AstNode*> children;     // children of node

    /**
     * @brief constructor
     */
    AstNode(NodeType t = NodeType::NONE, AstNode* p = nullptr): type(t), parent(p), value(0) {} 

    /**
     * @brief destructor
     */
    virtual ~AstNode() {
        for(auto child: children) {
            delete child;
        }
    }

    // rejcet copy and assignment
    AstNode(const AstNode&) = delete;
    AstNode& operator=(const AstNode&) = delete;
};

// definition of Parser
// a parser should take a token stream as input, then parsing it, output a AST
struct Parser {
    uint32_t index; // current token index
    const std::vector<Token>& token_stream;

    /**
     * @brief constructor
     * @param tokens: the input token_stream
     */
    Parser(const std::vector<Token>& tokens): index(0), token_stream(tokens) {}

    /**
     * @brief destructor
     */
    ~Parser() {}
    
    /**
     * @brief creat the abstract syntax tree
     * @return the root of abstract syntax tree
     */
    AstNode* get_abstract_syntax_tree() {
        AstNode* root = new Exp;
        parseExp((Exp*)root);
        return root;
    }

// u can define member funcition of Parser here
// for debug, u r not required to use this
// how to use this: in ur local enviroment, defines the macro DEBUG_PARSER and add this function in every parse fuction
void log(AstNode* node){
#ifdef DEBUG_PARSER
        std::cout << "in parse" << toString(node->type) << ", cur_token_type::" << toString(token_stream[index].type) << ", token_val::" << token_stream[index].value << '\n';
#endif
    }

#define CUR_TOKEN_IS(tk_type) (token_stream[index].type == TokenType::tk_type)
#define PARSE_TOKEN(tk_type) root->children.push_back(parseTerm(root, TokenType::tk_type))
#define PARSE(name, type) auto name = new type(root); assert(parse##type(name)); root->children.push_back(name); 

    struct Term: AstNode{
        Token token;
        Term(Token t, AstNode* p = nullptr): token(t), AstNode(NodeType::TERMINAL, p) {}
    };
    struct Exp: AstNode{
        Exp(AstNode* p = nullptr): AstNode(NodeType::EXP, p) {}
    };
    struct Number: AstNode{
        Number(AstNode* p = nullptr): AstNode(NodeType::NUMBER, p) {}
    };
    struct PrimaryExp: AstNode{
        PrimaryExp(AstNode* p = nullptr): AstNode(NodeType::PRIMARYEXP, p) {}
    };
    struct UnaryExp: AstNode{
        UnaryExp(AstNode* p = nullptr): AstNode(NodeType::UNARYEXP, p) {}
    };
    struct UnaryOp: AstNode{
        UnaryOp(AstNode* p = nullptr): AstNode(NodeType::UNARYOP, p) {}
    };
    struct MulExp: AstNode{
        MulExp(AstNode* p = nullptr): AstNode(NodeType::MULEXP, p) {}
    };
    struct AddExp: AstNode{
        AddExp(AstNode* p = nullptr): AstNode(NodeType::ADDEXP, p) {}
    };
    struct None: AstNode{
        None(AstNode* p = nullptr): AstNode(NodeType::NONE, p) {}
    };
    
    Term* parseTerm(AstNode* parent, TokenType expected){
        if(token_stream[index].type != expected) assert(0 && "unmatched token type");
        Term* now = new Term(token_stream[index]);
        // parent->children.push_back(now);
        index++;
        return now;
    }
    bool parseNumber(Number* root){
        log(root);
        
        std::string num = token_stream[index].value;
        PARSE_TOKEN(INTLTR);
        int base = 10;
        if(num[0] == '0' && num.size() > 1){
            if(num[1] == 'b') base = 2, num = num.substr(2);
            else if(num[1] == 'x') base = 16, num = num.substr(2);
            else if(num[1] == 'o') base = 8, num = num.substr(2);
            else base = 8, num = num.substr(1);
        }
        root->value = std::stoi(num, nullptr, base);

        return true;
    }
    bool parseUnaryOp(UnaryOp* root){
        log(root);

        if(CUR_TOKEN_IS(PLUS)) PARSE_TOKEN(PLUS), root->value = 1;
        else PARSE_TOKEN(MINU), root->value = 2;

        return true;
    }
    bool parsePrimaryExp(PrimaryExp* root){
        log(root);

        if(CUR_TOKEN_IS(LPARENT)){
            PARSE_TOKEN(LPARENT);
            PARSE(exp, Exp);
            PARSE_TOKEN(RPARENT);
            root->value = exp->value;
        }
        else{
            PARSE(number, Number);
            root->value = number->value;
        }

        return true;
    }
    bool parseUnaryExp(UnaryExp* root){
        log(root);

        if(CUR_TOKEN_IS(PLUS) || CUR_TOKEN_IS(MINU)){
            PARSE(unaryop, UnaryOp);
            PARSE(unaryexp, UnaryExp);
            if(unaryop->value == 1) root->value = unaryexp->value;
            else root->value = -unaryexp->value;
        }
        else{
            PARSE(primaryexp, PrimaryExp);
            root->value = primaryexp->value;
        }

        return true;
    }
    bool parseMulExp(MulExp* root){
        log(root);

        PARSE(unaryexp, UnaryExp);
        root->value = unaryexp->value;
        
        while(CUR_TOKEN_IS(MULT) || CUR_TOKEN_IS(DIV)){
            if(CUR_TOKEN_IS(MULT)){
                PARSE_TOKEN(MULT);
                PARSE(unaryexp1, UnaryExp);
                root->value *= unaryexp1->value;
            }
            else if(CUR_TOKEN_IS(DIV)){
                PARSE_TOKEN(DIV);
                PARSE(unaryexp1, UnaryExp);
                root->value /= unaryexp1->value;
            }
        }

        return true;
    }
    bool parseAddExp(AddExp* root){
        log(root);

        PARSE(mulexp, MulExp);
        root->value = mulexp->value;

        while(CUR_TOKEN_IS(PLUS) || CUR_TOKEN_IS(MINU)){
            if(CUR_TOKEN_IS(PLUS)){
                PARSE_TOKEN(PLUS);
                PARSE(mulexp1, MulExp);
                root->value += mulexp1->value;
            }
            else if(CUR_TOKEN_IS(MINU)){
                PARSE_TOKEN(MINU);
                PARSE(mulexp1, MulExp);
                root->value -= mulexp1->value;
            }
        }

        return true;
    }
    bool parseExp(Exp* root){
        log(root);

        PARSE(addexp, AddExp);
        root->value = addexp->value;

        return true;
    }

};

// u can define funcition here


int main(){
    std::string stdin_str;
    std::getline(std::cin, stdin_str);
    stdin_str += "\n";
    DFA dfa;
    Token tk;
    std::vector<Token> tokens;
    for (size_t i = 0; i < stdin_str.size(); i++) {
        if(dfa.next(stdin_str[i], tk)){
            tokens.push_back(tk); 
        }
    }

    // hw2
    Parser parser(tokens);
    auto root = parser.get_abstract_syntax_tree();
    // u may add function here to analysis the AST, or do this in parsing
    // like get_value(root);
    

    std::cout << root->value;

    return 0;
}