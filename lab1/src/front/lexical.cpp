#include"front/lexical.h"

#include<map>
#include<cassert>
#include<string>

#define TODO assert(0 && "todo")

// #define DEBUG_DFA
// #define DEBUG_SCANNER

std::string frontend::toString(State s) {
    switch (s) {
    case State::Empty: return "Empty";
    case State::Ident: return "Ident";
    case State::IntLiteral: return "IntLiteral";
    case State::FloatLiteral: return "FloatLiteral";
    case State::op: return "op";
    default:
        assert(0 && "invalid State");
    }
    return "";
}

std::set<std::string> frontend::keywords= {
    "const", "int", "float", "if", "else", "while", "continue", "break", "return", "void"
};

frontend::DFA::DFA(): cur_state(frontend::State::Empty), cur_str() {}

frontend::DFA::~DFA() {}

bool frontend::DFA::next(char input, Token& buf) {
#ifdef DEBUG_DFA
#include<iostream>
    std::cout << "in state [" << toString(cur_state) << "], input = \'" << input << "\', str = " << cur_str << "\t";
#endif
    switch(cur_state){
        case State::Empty:
            // Empty to op
            if(ispunct(input) && input != '_'){
                cur_state = State::op;
                cur_str = input;
                return false;
            }
            // Empty to Int
            if(isdigit(input)){
                cur_state = State::IntLiteral;
                cur_str = input;
                return false;
            }
            // Empty to Ident
            if(isalpha(input) || input == '_'){
                cur_state = State::Ident;
                cur_str = input;
                return false;
            }
            // Empty to Empty
            if(isspace(input)){
                cur_state = State::Empty;
                return false;
            }
            assert(0 && "illegal state, Empty to other state");
            return false;

        case State::IntLiteral:
            // Int to Empty
            if(isspace(input)){
                buf.value = cur_str;
                buf.type = TokenType::INTLTR;
                reset();
                return true;
            }
            // Int to Int
            if(isdigit(input) || (input == 'x' || input == 'X') || (input >= 'a' && input <= 'f') || (input >= 'A' && input <= 'F')){
                cur_state = State::IntLiteral;
                cur_str += input;
                return false;
            }
            // Int to Float
            if(input == '.'){
                cur_state = State::FloatLiteral;
                cur_str += input;
                return false;
            }
            // Int to op
            if(ispunct(input)){
                buf.value = cur_str;
                buf.type = TokenType::INTLTR;
                cur_state = State::op;
                cur_str = input;
                return true;
            }
            assert(0 && "illegal state, IntLiteral to other state");
            return false;

        case State::FloatLiteral:
            // Float to Empty
            if(isspace(input)){
                buf.value = cur_str;
                buf.type = TokenType::FLOATLTR;
                reset();
                return true;
            }
            // Float to Float
            if(isdigit(input)){
                cur_state = State::FloatLiteral;
                cur_str += input;
                return false;
            }
            // Float to op
            if(ispunct(input)){
                buf.value = cur_str;
                buf.type = TokenType::FLOATLTR;
                cur_state = State::op;
                cur_str = input;
                return true;
            }
            assert(0 && "illegal state, FloatLiteral to other state");
            return false;

        case State::Ident:
            // Ident to Empty
            if(isspace(input)){
                buf.value = cur_str;
                buf.type = TokenType::IDENFR;
                reset();
                return true;
            }
            // Ident to Ident
            if(isalnum(input) || input == '_'){
                cur_state = State::Ident;
                cur_str += input;
                return false;
            }
            // Ident to op
            if(ispunct(input) && input != '_'){
                buf.value = cur_str;
                buf.type = TokenType::IDENFR;
                cur_state = State::op;
                cur_str = input;
                return true;
            }
            assert(0 && "illegal state, Ident to other state");
            return false;

        case State::op:
            auto get_op_type = [](std::string str) -> TokenType {
                if(str == "+") return TokenType::PLUS;
                if(str == "-") return TokenType::MINU;
                if(str == "*") return TokenType::MULT;
                if(str == "/") return TokenType::DIV;
                if(str == "%") return TokenType::MOD;
                if(str == "<") return TokenType::LSS;
                if(str == ">") return TokenType::GTR;
                if(str == ":") return TokenType::COLON;
                if(str == "=") return TokenType::ASSIGN;
                if(str == ";") return TokenType::SEMICN;
                if(str == ",") return TokenType::COMMA;
                if(str == "(") return TokenType::LPARENT;
                if(str == ")") return TokenType::RPARENT;
                if(str == "[") return TokenType::LBRACK;
                if(str == "]") return TokenType::RBRACK;
                if(str == "{") return TokenType::LBRACE;
                if(str == "}") return TokenType::RBRACE;
                if(str == "!") return TokenType::NOT;
                if(str == "<=") return TokenType::LEQ;
                if(str == ">=") return TokenType::GEQ;
                if(str == "==") return TokenType::EQL;
                if(str == "!=") return TokenType::NEQ;
                if(str == "&&") return TokenType::AND;
                if(str == "||") return TokenType::OR;
                #ifdef DEBUG_DFA
                std::cout << "in get_op_type, str = " << str << std::endl;
                #endif
                assert(0 && "illegal op");
            };
            // op to Empty
            if(isspace(input)){
                buf.value = cur_str;
                buf.type = get_op_type(cur_str);
                reset();
                return true;
            }
            // op to Int
            if(isdigit(input)){
                buf.value = cur_str;
                buf.type = get_op_type(cur_str);
                cur_state = State::IntLiteral;
                cur_str = input;
                return true;
            }
            // op to Ident
            if(isalpha(input) || input == '_'){
                buf.value = cur_str;
                buf.type = get_op_type(cur_str);
                cur_state = State::Ident;
                cur_str = input;
                return true;
            }
            // op to op
            if(ispunct(input) && input != '_'){
                if(cur_str.size() <= 1 && (((input == '=') && (cur_str[0] == '<' || cur_str[0] == '>' || cur_str[0] == '=' || cur_str[0] == '!')) || (input == '&' && cur_str[0] == '&') || (input == '|' && cur_str[0] == '|'))){
                    cur_state = State::op;
                    cur_str += input;
                    return false;
                }
                else{
                    buf.value = cur_str;
                    buf.type = get_op_type(cur_str);
                    cur_state = State::op;
                    cur_str = input;
                    return true;
                }
            }
            assert(0 && "illegal state, op to other state");
            return false;
    }
#ifdef DEBUG_DFA
    std::cout << "next state is [" << toString(cur_state) << "], next str = " << cur_str << "\t, ret = " << ret << std::endl;
#endif
}

void frontend::DFA::reset() {
    cur_state = State::Empty;
    cur_str = "";
}

frontend::Scanner::Scanner(std::string filename): fin(filename) {
    if(!fin.is_open()) {
        assert(0 && "in Scanner constructor, input file cannot open");
    }
}

frontend::Scanner::~Scanner() {
    fin.close();
}

std::vector<frontend::Token> frontend::Scanner::run() {
    std::vector<Token> ret;
    DFA dfa;
    Token tk;
    auto preproccess = [](std::ifstream& fin) -> std::string {
        std::string ret, l;
        while(std::getline(fin, l)) ret += l + '\n';
        for(auto start = ret.find("//"); start != std::string::npos; start = ret.find("//")){
            auto end = ret.find('\n', start) + 1;
            if(end == std::string::npos) assert(0 && "comment pairing error 1");
            ret = ret.substr(0, start) + ret.substr(end);
        }
        for(auto start = ret.find("/*"); start != std::string::npos; start = ret.find("/*")){
            auto end = ret.find("*/", start + 2) + 2;
            if(end == std::string::npos) assert(0 && "comment pairing error 2");
            ret = ret.substr(0, start) + ret.substr(end);
        }
        return ret;
    };
    std::string s = preproccess(fin); // delete comments
    for(auto c: s){
        if(dfa.next(c, tk)){
            if(tk.type == TokenType::IDENFR){
                if(keywords.find(tk.value) == keywords.end()) tk.type = TokenType::IDENFR;
                else{
                    auto get_keyword_type = [](std::string str) -> TokenType {
                        if(str == "const") return TokenType::CONSTTK;
                        if(str == "int") return TokenType::INTTK;
                        if(str == "float") return TokenType::FLOATTK;
                        if(str == "if") return TokenType::IFTK;
                        if(str == "else") return TokenType::ELSETK;
                        if(str == "while") return TokenType::WHILETK;
                        if(str == "continue") return TokenType::CONTINUETK;
                        if(str == "break") return TokenType::BREAKTK;
                        if(str == "return") return TokenType::RETURNTK;
                        if(str == "void") return TokenType::VOIDTK;
                        assert(0 && "illegal keyword");
                    };
                    tk.type = get_keyword_type(tk.value);
                }
            }
            ret.push_back(tk);
        }
    }
    return ret;
#ifdef DEBUG_SCANNER
#include<iostream>
            std::cout << "token: " << toString(tk.type) << "\t" << tk.value << std::endl;
#endif
}