#include "parser.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>
#include <stack>
#include <functional>

static inline void print_token(const Token& t, std::ostream& out);

Reader::Reader() {

}

Reader::~Reader() {

}

int Reader::read(const std::string& file) {
    _file_path = file;
    std::ifstream in(file, std::ios::in);
    if (in.is_open()) {
        std::stringstream ss;
        ss << in.rdbuf();
        _file_str = ss.str();   
        in.close();
        _cur_line = 0;
        _cur_loc = 0;
        return 0;
    } else {
        return -1;
    }
}

const std::string& Reader::get_file_path() {
    return _file_path;
}

char Reader::cur_char() {
    return _file_str[_cur_loc];
}

char Reader::next_char() {
    if (this->eof()) {
        return '\n';
    }

    char t = _file_str[_cur_loc++];
    if (t == '\n') {
        ++_cur_line;
    }
    return t;
}

char Reader::pre_char() {
    if (_cur_loc == 0) {
        return '\n';
    }

    char t = _file_str[--_cur_loc];
    if (t == '\n') {
        --_cur_line;
    }
    return t;
}

int Reader::get_cur_line() const {
    return _cur_line;
}

int Reader::get_cur_loc() const {
    return _cur_loc;
}

void Reader::next_line() {
    while(_file_str[++_cur_loc] != '\n' && !this->eof()) {}
}

bool Reader::eof() const {
    const int len = (int)_file_str.length()-1;
    return _cur_loc > len;
}

std::string Reader::get_string(int loc, int len) {
    assert(loc+len < _file_str.length());
    return _file_str.substr(loc, len);
}

void Reader::skip_white() {
    if (eof()) {
        return;
    }

    char c = _file_str[_cur_loc];
    if (c==' ' || c=='\t' || c=='\f' || c=='\v' || c=='\0') {
        ++_cur_loc;
        skip_white();
    }
}

static inline bool is_num(char c) {
    return c >= '0' && c <='9';
}

//十进制和八进制
static inline Token lex_number(char c, Reader* cpp_reader, Token& token,  int per_status) {
    // *.*E(e)*
    //-------------------DFA-----------------//
    //      | 0 | 1 |(2)| 3 |(4)| 5 | 6 |(7)|
    //  +/- | 1 |   |   |   |   | 6 |   |   |
    // digit| 2 | 2 | 2 | 4 | 4 | 7 | 7 | 7 |
    //  E/e |   |   | 5 |   | 5 |   |   |   |
    //   .  |   |   | 3 |   |   |   |   |   |
    const static char DFA[4][8] = {
        1,-1,-1,-1,-1,6,-1,-1,
        2,2,2,4,4,7,7,7,
        -1,-1,5,-1,5,-1,-1,-1,
        -1,-1,3,-1,5,-1,-1,-1
    };
    //-------------------DFA-----------------//

    int input = -1;
    if (is_num(c)) {
        input = 1;
    } else if (c == '+' || c == '-') {
        input = 0;
    } else if (c== '.') {
        input = 3;
    } else if (c=='e' || c== 'E') {
        input = 2;
    }

    if (-1 == input) {
        if (!cpp_reader->eof()) {
            cpp_reader->pre_char();
        }
        if (per_status == 2 || per_status == 4 || per_status == 7) {
            if (c == 'f' || c == 'L' || c == 'U') {
                //f的尾注
                token.val += c;
            }
            return token;
        } else {
            token.type = CPP_OTHER;
            return token;
        }
    }
    token.val += c;

    int next_status = DFA[input][per_status];
    return lex_number(cpp_reader->next_char(), cpp_reader, token, next_status);
}

//十进制和八进制
static inline Token lex_hex(char c, Reader* cpp_reader, Token& token,  int per_status) {
    // 0x(0~9|a~f|A-F)
    //-------------------DFA-----------------//
    //             | 0 | 1 | 2 |(3)|
    //     0       | 1 |   |   |
    //    x/X      |   | 2 |   |
    // 0~9 a~f A~F |   |   | 3 | 3
    const static char DFA[3][4] = {
        1,-1,-1,-1,
        -1,2,-1,-1,
        -1,-1,3,3
    };
    //-------------------DFA-----------------//

    int input = -1;
    if (per_status == 0 && '0' ==  c) {
        input = 0;
    } else if (c == 'x' || c == 'X') {
        input = 1;
    } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <='F') || (c >= 'a' && c <='f')) {
        input = 2;
    }

    if (-1 == input) {
        if (!cpp_reader->eof()) {
            cpp_reader->pre_char();
        }
        if (per_status == 3) {
            return token;
        } else {
            token.type = CPP_OTHER;
            return token;
        }
    }
    token.val += c;

    int next_status = DFA[input][per_status];
    return lex_hex(cpp_reader->next_char(), cpp_reader, token, next_status);
} 

static inline bool is_letter(char c) {
    return (c>='a' && c <='z') || (c>='A' && c <='Z');
}

static inline Token lex_identifier(char c, Reader* cpp_reader, Token& token,  int per_status) {
    //-------------------DFA-----------------//
    //       | 0 | 1 |(2)|
    //  _    | 1 | 1 | 2 |
    // letter| 2 | 2 | 2 |
    // number|-1 | 2 | 2 |
    const static char DFA[3][3] = {
        1,1,2,
        2,2,2,
        -1,2,2
    };
    //-------------------DFA-----------------//
    int input = -1;
    if (is_letter(c)) {
        input = 1;
    } else if (c == '_') {
        input = 0;
    } else if (is_num(c)) {
        input = 2;
    }

    if (-1 == input) {
        if (!cpp_reader->eof()) {
            cpp_reader->pre_char();
        }
        if (per_status == 2) {
            for (int i=0; i<sizeof(keywords)/sizeof(char*); ++i) {
                if (token.val == keywords[i]) {
                    token.type = CPP_KEYWORD;
                    break;
                }
            }
            return token;
        } else {
            token.type = CPP_OTHER;
            return token;
        }
    }
    token.val += c;

    int next_status = DFA[input][per_status];
    return lex_identifier(cpp_reader->next_char(), cpp_reader, token, next_status);
}

static inline Token lex_comment(char c, Reader* cpp_reader, Token& token, int per_status) {
    //-------------------DFA-----------------//
    //       | 0 | 1 | 2 | 3 |(4)|
    //  /    | 1 |-1 | 2 | 4 |-1 |
    //  *    |-1 | 2 | 3 | 3 |-1 |
    // other |-1 |-1 | 2 | 2 |-1 |
    const static char DFA[3]/*  */[5] = {
        1,-1,2,4,-1,
        -1,2,3,3,-1,
        -1,-1,2,2,-1
    };
    //-------------------DFA-----------------//

    int input = -1;
    if (c == '/') {
        input = 0;
    } else if (c == '*') {
        input = 1;
    } else {
        input = 2;
    }


    token.val += c;

    int next_status = DFA[input][per_status];
    if (next_status == 4) {
        return token;
    } else {
        return lex_comment(cpp_reader->next_char(), cpp_reader, token, next_status);
    }
}

static inline Token lex_string(char c , Reader* cpp_reader, Token& token, int per_status) {
    //-------------------DFA-----------------//
    //       | 0 | 1 | 2 
    //  "    | 1 | 2 | 
    // other |-1 | 1 | 
    const static char DFA[2][3] = {
        1,2,-1,
        -1,1,-1
    };
    //-------------------DFA-----------------//

    int input = -1;
    if (c == '\"') {
        if (token.val[token.val.length()-1] == '\\') {
            if (token.val.length() > 2 && token.val[token.val.length()-2] == '\\') {
                input = 0;//出现了 \\"
            } else {
                input = 1; //出现了 \"
            }
        } else {
            input = 0;
        }
    } else {
        input = 1;
    }

    token.val += c;

    int next_status = DFA[input][per_status];
    if (next_status == 2) {
        return token;
    } else {
        return lex_string(cpp_reader->next_char(), cpp_reader, token, next_status);
    }
}

Parser::Parser(bool to_be_confuse):_to_be_confuse(to_be_confuse) {

}

Parser::~Parser() {

}

Token Parser::lex(Reader* cpp_reader) {
    if (cpp_reader->eof()) {
        return {CPP_EOF,"",0};
    }

    char c = cpp_reader->next_char();
    switch(c) {
        case '\n': {
            return {CPP_BR, "br", cpp_reader->get_cur_loc()};
            //return lex(cpp_reader);
        }
        case ';': {
            return {CPP_SEMICOLON, ";", cpp_reader->get_cur_loc()};
        }
        //brace
        case ' ': case '\t': case '\f': case '\v': case '\0': 
        {
            cpp_reader->skip_white();
            return lex(cpp_reader);
        }
        case '\\': {
            return {CPP_CONNECTOR, ";", cpp_reader->get_cur_loc()};
        }
        case '=': {
            char nc = cpp_reader->next_char();
            if (nc=='=') {
                return {CPP_EQ_EQ, "==", cpp_reader->get_cur_loc()-1};
            } else {
                cpp_reader->pre_char();
                return {CPP_EQ, "=", cpp_reader->get_cur_loc()};
            }
        }

        case '!': {
            char nc = cpp_reader->next_char();
            if (nc == '=') {
                return {CPP_NOT_EQ, "!=", cpp_reader->get_cur_loc()-1};
            } else {
                cpp_reader->pre_char();
                return {CPP_NOT, "!", cpp_reader->get_cur_loc()};
            }
        }

        case '>': {
            char nc = cpp_reader->next_char();
            //LABEL 运算符右移 和 模板有冲突 在语义分析的时候再解决
            // if (nc == '>') {
            //     return {CPP_RSHIFT, ">>", cpp_reader->get_cur_loc()-1};
            // } else 
            if (nc == '=')  {
                return {CPP_GREATER_EQ, ">=", cpp_reader->get_cur_loc()-1};
            } else {
                cpp_reader->pre_char();
                return {CPP_GREATER, ">", cpp_reader->get_cur_loc()};
            }
        }

        case '<': {
            char nc = cpp_reader->next_char();
            if (nc == '<') {
                return {CPP_LSHIFT, "<<", cpp_reader->get_cur_loc()-1};
            } else if (nc == '=')  {
                return {CPP_LESS_EQ, "<=", cpp_reader->get_cur_loc()-1};
            } else {
                cpp_reader->pre_char();
                return {CPP_LESS, "<", cpp_reader->get_cur_loc()};
            }
        }
        //indentifer
        case '_':
        case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
        case 'g': case 'h': case 'i': case 'j': case 'k': case 'l':
        case 'm': case 'n': case 'o': case 'p': case 'q': case 'r':
        case 's': case 't': case 'u': case 'v': case 'w': case 'x':
        case 'y': case 'z':
        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
        case 'G': case 'H': case 'I': case 'J': case 'K': case 'L':
        case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R':
        case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
        case 'Y': case 'Z':
        {
            Token t  ={ CPP_NAME, "", cpp_reader->get_cur_loc()};
            return lex_identifier(c, cpp_reader, t, 0);
            break;
        }

        //number
        case '0': 
        {
            //hex
            char nc = cpp_reader->next_char();
            if (nc == 'x' || nc == 'X') {
                Token t = {CPP_NUMBER, "0x", cpp_reader->get_cur_loc()};
                return lex_hex(cpp_reader->next_char(), cpp_reader, t, 2);    
            } else {
                //other number
                cpp_reader->pre_char();
                Token t  ={ CPP_NUMBER, "", cpp_reader->get_cur_loc()};
                return lex_number(c, cpp_reader, t, 2);
            }
        }
        case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
        {
            Token t  ={ CPP_NUMBER, "", cpp_reader->get_cur_loc()};
            return lex_number(c, cpp_reader, t, 2);
        }

        //char
        case '\'': 
        {
            char nc = cpp_reader->next_char();
            char nnc = cpp_reader->next_char();
            char nnnc = cpp_reader->next_char();
            if (nnc == '\'' && nc != '\'') {
                cpp_reader->pre_char();
                return {CPP_CHAR, "\'" + std::string(1,nc) + "\'", cpp_reader->get_cur_loc()-2};
            } else if (nc == '\\' && nnnc == '\'' && nnc != '\'') {
                return {CPP_CHAR, "\'\\" + std::string(1,nnc) + "\'", cpp_reader->get_cur_loc()-3};
            }

            cpp_reader->pre_char();
            cpp_reader->pre_char();
            cpp_reader->pre_char();
            return {CPP_OTHER, "", 0};
        }

        //string
        case '\"': 
        { 
            Token t = {CPP_STRING, "\"", cpp_reader->get_cur_loc()};
            return lex_string(cpp_reader->next_char(), cpp_reader, t, 1);
        }

        case '-':
        {
            char nc = cpp_reader->next_char();
            if (nc == '=') {
                return {CPP_MINUS_EQ, "-=", cpp_reader->get_cur_loc()-1};
            } else if (nc == '-') {
                return {CPP_MINUS_MINUS, "--", cpp_reader->get_cur_loc()-1};
            } else if (nc == '>') {
                return {CPP_POINTER, "->", cpp_reader->get_cur_loc()-1};
            } else {
                cpp_reader->pre_char();
                return {CPP_MINUS, "-", cpp_reader->get_cur_loc()};
            }
            //错误
            cpp_reader->pre_char();
            return {CPP_OTHER, "", 0};
        }
        case '+':
        {
            char nc = cpp_reader->next_char();
            if (nc == '=') {
                return {CPP_PLUS_EQ, "+=", cpp_reader->get_cur_loc()-1};
            } else if (nc == '+') {
                return {CPP_PLUS_PLUS, "++", cpp_reader->get_cur_loc()-1};
            } else {
                cpp_reader->pre_char();
                return {CPP_PLUS, "+", cpp_reader->get_cur_loc()};
            }

            //错误
            cpp_reader->pre_char();
            return {CPP_OTHER, "", 0};
        }
        case '.': 
        {
            char nc = cpp_reader->next_char();
            if (is_num(nc)) {
                cpp_reader->pre_char();
                Token t  ={ CPP_NUMBER, "", cpp_reader->get_cur_loc()};
                return lex_number(c, cpp_reader, t, 2);
            } else if (nc == '.') {
                char nnc = cpp_reader->next_char();
                if (nnc == '.') {
                    return {CPP_ELLIPSIS, "...", cpp_reader->get_cur_loc()-2};
                } else {
                    cpp_reader->pre_char();
                    return {CPP_OTHER, "", 0};
                }
            } else {
                cpp_reader->pre_char();
                return {CPP_DOT, ".", cpp_reader->get_cur_loc()};
            }
        }
        case '*':
        {
            //TODO 怎么判断是解引用还是乘号
            char nc = cpp_reader->next_char();
            if (nc == '=') {
                return {CPP_MULT_EQ, "*=", cpp_reader->get_cur_loc()-1};
            } else {
                cpp_reader->pre_char();
                return {CPP_MULT, "*", cpp_reader->get_cur_loc()};
            }
        }
        case '/':
        {
            char nc = cpp_reader->next_char();
            if (nc == '=') {
                return {CPP_DIV_EQ, "/=", cpp_reader->get_cur_loc()};
            } else if (nc == '/') {
                //注释
                Token t = {CPP_COMMENT, "//", cpp_reader->get_cur_loc()-1};
                while(true) {
                    nc = cpp_reader->next_char();
                    if (nc == '\n' || cpp_reader->eof()) {
                        return t;
                    } else {
                        t.val += nc;
                    }
                }
            } else if (nc == '*') {
                //注释
                Token t = {CPP_COMMENT, "/*", cpp_reader->get_cur_loc()-1};
                return lex_comment(cpp_reader->next_char(), cpp_reader, t, 2);
            } else {
                cpp_reader->pre_char();
                return {CPP_DIV, "/", cpp_reader->get_cur_loc()};
            }
        }
        case '%': 
        {
            char nc = cpp_reader->next_char();
            if (nc == '=') {
                return {CPP_MOD_EQ, "%=", cpp_reader->get_cur_loc()-1};
            } else {
                cpp_reader->pre_char();
                return {CPP_MOD, "%", cpp_reader->get_cur_loc()};
            }
        }
        case '&': 
        {
            char nc = cpp_reader->next_char();
            if (nc == '=') {
                return {CPP_AND_EQ, "&=", cpp_reader->get_cur_loc()-1};
            } else if (nc == '&') {
                return {CPP_AND_AND, "&&", cpp_reader->get_cur_loc()-1};
            } else {
                cpp_reader->pre_char();
                return {CPP_AND, "&", cpp_reader->get_cur_loc()};
            }
        } 
        case '|': 
        {
            char nc = cpp_reader->next_char();
            if (nc == '=') {
                return {CPP_OR_EQ, "|=", cpp_reader->get_cur_loc()-1};
            } else if (nc == '|') {
                return {CPP_OR_OR, "||", cpp_reader->get_cur_loc()-1};
            } else {
                cpp_reader->pre_char();
                return {CPP_OR, "|", cpp_reader->get_cur_loc()};
            }
        }
        case '^': 
        {
            char nc = cpp_reader->next_char();
            if (nc == '=') {
                return {CPP_XOR_EQ, "^=", cpp_reader->get_cur_loc()-1};
            } else {
                cpp_reader->pre_char();
                return {CPP_XOR, "^", cpp_reader->get_cur_loc()};
            }
        }
        case '~': 
        {  
            return {CPP_COMPL, "~", cpp_reader->get_cur_loc()};
        }
        case '?': 
        {
            return {CPP_QUERY, "?", cpp_reader->get_cur_loc()};
        }
        case '(':
        {
            return {CPP_OPEN_PAREN, "(", cpp_reader->get_cur_loc()};
        }
        case ')':
        {
            return {CPP_CLOSE_PAREN, ")", cpp_reader->get_cur_loc()};
        }
        case '[':
        {
            return {CPP_OPEN_SQUARE, "[", cpp_reader->get_cur_loc()};
        }
        case ']':
        {
            return {CPP_CLOSE_SQUARE, "]", cpp_reader->get_cur_loc()};
        }
        case '{':
        {
            return {CPP_OPEN_BRACE, "{", cpp_reader->get_cur_loc()};
        }
        case '}':
        {
            return {CPP_CLOSE_BRACE, "}", cpp_reader->get_cur_loc()};
        }        
        case ':': 
        {
            char nc = cpp_reader->next_char();
            if (nc == ':') {
                return {CPP_SCOPE, "::", cpp_reader->get_cur_loc()-1};
            } else {
                cpp_reader->pre_char();
                return {CPP_COLON, ":", cpp_reader->get_cur_loc()};
            }
        }
        case ',': 
        {
            return {CPP_COMMA, ",", cpp_reader->get_cur_loc()};
        }
        case '#': {
            return {CPP_PASTE, "#", cpp_reader->get_cur_loc()};
        }

        default:
            //ERROR
            return {CPP_OTHER, "", 0};
    }

    return {};
}

void Parser::push_token(const Token& t) {
    _ts.push_back(t);
}

const std::deque<Token>& Parser::get_tokens() const {
    return _ts;
}

static inline bool is_type(const std::string& str) {
    for (int i=0; i<sizeof(types)/sizeof(char*); ++i) {
        if (str == types[i]) {
            return true;
        }
    }

    return false;
} 

void Parser::f1() {
    for (auto t = _ts.begin(); t != _ts.end(); ) {
        //number sign
        if (t->type == CPP_PLUS || t->type == CPP_MINUS) {
            std::string sign = t->type == CPP_PLUS ? "+" : "-";
            if (t == _ts.begin()) {
                auto t_n = t+1;
                if (t_n != _ts.end() && t_n->type==CPP_NUMBER) {
                    t_n->val = sign + t_n->val;
                    t_n->loc = t->loc;
                    t = _ts.erase(t);
                    continue;
                }
            } else {
                auto t_n = t+1;
                auto t_p = t-1;
                if (t_n != _ts.end() && t_n->type==CPP_NUMBER && t_p->type != CPP_NUMBER) {
                    t_n->val = sign + t_n->val;
                    t_n->loc = t->loc;
                    t = _ts.erase(t);
                    continue;
                }
            }
        }

        if (t->type == CPP_PASTE) {
            auto t_n = t+1;
            //include header
            if (t_n != _ts.end() && t_n->type == CPP_NAME && t_n->val == "include") {
                auto t_nn = t_n+1;
                if (t_nn != _ts.end() && t_nn->type == CPP_STRING) {
                    //#include "***"
                    t_nn->type = CPP_HEADER_NAME;
                    t_nn->val = t_nn->val.substr(1,t_nn->val.length()-2);
                    t_nn->loc = t->loc;
                    t = _ts.erase(t);
                    t = _ts.erase(t);
                    continue;
                } else if (t_nn != _ts.end() && t_nn->type == CPP_LESS) {
                    //#include <***>
                    //get >
                    auto t_nnn = t_n+2;
                    std::string include_str;
                    int num_l = 0;
                    bool got = false;
                    while (t_nnn != _ts.end() && !got) {
                        if (t_nnn->type != CPP_GREATER) {
                            include_str += t_nnn->val;
                            ++num_l;
                            ++t_nnn;
                            continue;
                        } else {
                            got = true;
                        }
                    }
                    if (got) {
                        t_nnn->type = CPP_HEADER_NAME;
                        t_nnn->val = include_str;
                        t_nnn->loc = t->loc;
                        t = _ts.erase(t);//#
                        t = _ts.erase(t);//include
                        t = _ts.erase(t);//*.h
                        while(num_l > 0) {
                            t = _ts.erase(t);
                            --num_l;
                        }
                        continue;
                    }                  
                }
            } 
            //preprocess
            else if (t_n != _ts.end() && (t_n->type==CPP_NAME || t_n->type==CPP_KEYWORD)) {
                if (t_n->val == "define" || t_n->val == "if" || t_n->val == "elif" || 
                    t_n->val == "else" || t_n->val == "ifndef" || t_n->val == "ifdef" ||
                    t_n->val == "pragma" || t_n->val == "error" || 
                    t_n->val == "undef" || t_n->val == "line") {
                    t->val = t_n->val;
                    t->type = CPP_PREPROCESSOR;
                    ++t;
                    t = _ts.erase(t);//erase properssor
                    while(t->type != CPP_EOF) {
                        if (t->type == CPP_CONNECTOR && (t+1)->type == CPP_BR) {
                            //erase connector and br
                            t = _ts.erase(t);
                            t = _ts.erase(t);
                        } else if (t->type == CPP_BR) {
                            break;
                        } else {
                            (t-1)->ts.push_back(*t);
                            t = _ts.erase(t);
                        }
                    }
                    continue;
                } else if (t_n->val == "endif") {
                    t->val = t_n->val;
                    t->type = CPP_PREPROCESSOR;
                    ++t;
                    t = _ts.erase(t);//erase properssor
                    continue;
                }

            }
        }

        //type
        // if (t->type == CPP_NAME && t->val == "size_t") {
        //     t->type = CPP_TYPE;
        //     ++t;
        //     continue;
        // }
        if ((t->type == CPP_KEYWORD || t->type == CPP_NAME) && is_type(t->val)) {
            t->type = CPP_TYPE;
            ++t;
            continue;
        }

        //marco
        // if (t->type == CPP_PASTE) {
        //     auto t_n = t+1;
        //     if () 
        // }

        ++t;
    }
}

void Parser::f2() { 
    for (auto t = _ts.begin(); t != _ts.end(); ) {
        //conbine type
        if (t->type == CPP_TYPE) {
            auto t_n = t+1;
            while(t_n!=_ts.end() && t_n->type == CPP_TYPE) {
                t_n->val = t->val + " " + t_n->val;
                t = _ts.erase(t);
                t_n = t+1;
            }
            ++t;
            continue;
        }
        //erase br
        if (t->type == CPP_BR) {
            t = _ts.erase(t);
            continue;
        }

        //erase connector
        if (t->type == CPP_CONNECTOR) {
            t = _ts.erase(t);
            continue;
        }

        ++t;
    }
}

ParserGroup::ParserGroup() {

}

ParserGroup::~ParserGroup() {

}

void ParserGroup::add_parser(const std::string& file_name, Parser* parser, Reader* reader) {

    _file_name.push_back(file_name);
    _parsers.push_back(parser);
    _readers.push_back(reader);
}

Parser* ParserGroup::get_parser(const std::string& file_name) {
    for (size_t i = 0; i < _file_name.size(); ++i) {
        if (_file_name[i] == file_name) {
            return _parsers[i];
        }
    }
    return nullptr;
}

void ParserGroup::parse_marco() {
    //l1 找纯粹的 define
    for (auto it = _parsers.begin(); it != _parsers.end(); ++it) {
        Parser& parser = *(*it);
        std::deque<Token>& ts = parser._ts;
        for (auto t = ts.begin(); t != ts.end(); ) {
            if (t->type == CPP_PREPROCESSOR && t->val == "define") {
                if (t == ts.begin()) {
                    //t->type = CPP_MACRO;
                    _g_marco.push_back(*t);
                } else if ((t-1)->type != CPP_PREPROCESSOR) {
                    //t->type = CPP_MACRO;
                    _g_marco.push_back(*t);
                }
                ++t;
                continue;
            }

            ++t;
        }
    } 

    //l2 解析所有 #ifdef else #endif 和 #ifndef else #endif 条件判断语句, 进一步抽取全局宏
    for (auto it = _parsers.begin(); it != _parsers.end(); ++it) {
        Parser& parser = *(*it);
        std::deque<Token>& ts = parser._ts;
        for (auto t = ts.begin(); t != ts.end(); ) {
            if (t->type == CPP_PREPROCESSOR && (t->val == "ifndef" || t->val == "ifdef")) {
                //预处理语句开始
                std::stack<Token> is;
                while(true) {
                    is.push(*t);
                    if(is.top().val == "ifndef" || is.top().val == "ifdef") {
                        assert(!is.top().ts.empty());
                        bool target = is.top().val == "ifdef";
                        if (target == is_in_marco(is.top().ts[0].val)) {
                            //执行下一句
                            ++t;
                            continue;
                        } else {
                            //执行else分支, 找else 或者 endif
                            ++t;
                            std::stack<Token> is_r;

                    GET_ELSE_BRANCH:                        

                            while(!(t->val == "else" || t->val == "endif" || t->val == "ifdef" || t->val == "ifndef")) {
                                ++t;
                            }
                            if (is_r.empty() && t->val == "else") {
                                //找到else
                                ++t;
                                continue;
                            } else if (is_r.empty() && t->val == "endif") {
                                //找到终结的endif
                                continue;
                            } else if (!is_r.empty() && (t->val == "ifdef" || t->val== "ifndef")) {
                                //还没有遇到结束else分支, 就又遇到了嵌套的条件语句(不走的)
                                is_r.push(*(t++));
                                goto GET_ELSE_BRANCH;
                            } else if (!is_r.empty() && t->val == "endif") {
                                is_r.pop();
                                ++t;
                                goto GET_ELSE_BRANCH;
                            }
                        }
                    } else if (is.top().val == "endif"){
                        is.pop();
                        is.pop();
                        if (is.empty()) {
                            break;
                        }
                        ++t;
                    } else if (is.top().val == "define") {
                        //执行
                        _g_marco.push_back(is.top());
                        is.pop();
                        ++t;
                    } else {
                        //不是相关预处理代码
                        //std::cout << "Err:" << is.top().val << "\n";
                        is.pop();
                        ++t;
                    }
                }
                continue;   
            }

            ++t;
        }
    }

    //抽取带namespace的宏
    for (auto it = _g_marco.begin(); it != _g_marco.end(); ++it) {
        Token& t = *it;
        if (t.ts.size() > 1 && t.ts[1].val == "namespace") {
            for (auto it2 = t.ts.begin()+1;it2 != t.ts.end(); ) {
                Token tt;
                if (it2->type == CPP_NAME && is_in_marco(it2->val,tt)) {
                    it2 = t.ts.erase(it2);
                    for (auto it3 = tt.ts.begin()+1; it3 != tt.ts.end(); ++it3) {
                        it2 = t.ts.insert(it2, *it3);
                        ++it2;
                    }
                    continue;
                }
                ++it2;
            }
        }
    }

    //l3 把所有的name为marco的token type改成 marco
    //   并且展开带namespace的宏
    int idx = 0;
    for (auto it = _parsers.begin(); it != _parsers.end(); ++it) {
        std::string file_name = _file_name[idx++];
        Parser& parser = *(*it);
        std::deque<Token>& ts = parser._ts;
        for (auto t = ts.begin(); t != ts.end();) {
            Token tt;
            if (t->type == CPP_NAME && is_in_marco(t->val,tt)) {
                t->type = CPP_MACRO;
                if (tt.ts.size() > 1 && (tt.ts[1].val == "namespace" || tt.ts[1].type == CPP_CLOSE_BRACE)) {
                    //需要展开的宏
                    t = ts.erase(t);
                    for (auto it3 = tt.ts.begin()+1; it3 != tt.ts.end(); ++it3) {
                        t = ts.insert(t, *it3);
                        ++t;
                    }
                    continue;
                }
            } 
            ++t;
        }
    }
}

// void ParserGroup::extract_tempalte() {
//     //把 template尖括号中class 和 typename 后面跟的token type设置成 CPP_TYPE
//     for (auto it = _parsers.begin(); it != _parsers.end(); ++it) {
//         Parser& parser = *(*it);
//         std::deque<Token>& ts = parser._ts;
//         for (auto t = ts.begin(); t != ts.end(); ) {
//             if (t->val == "template") {
//                 //找<
//                 std::stack<Token> st;
//                 while((++t)->type != CPP_GREATER) {}
//                 st.push(*t);
                
//                 //将仅跟着class之后的Name设置成type



//             }
//         }
//     }
//     //把 模板类/结构体提取出来

// }

void ParserGroup::extract_enum() {
    for (auto it = _parsers.begin(); it != _parsers.end(); ++it) {
        Parser& parser = *(*it);
        std::deque<Token>& ts = parser._ts;
        for (auto t = ts.begin(); t != ts.end(); ++t) {
            if (t->type == CPP_KEYWORD && t->val == "enum" && (t+1)->type == CPP_NAME) {
                (t+1)->type = CPP_ENUM;
                _g_enum.insert((t+1)->val);
                ++t;
            }
        }
    }
    for (auto it = _parsers.begin(); it != _parsers.end(); ++it) {
        Parser& parser = *(*it);
        std::deque<Token>& ts = parser._ts;
        for (auto t = ts.begin(); t != ts.end(); ++t) {
            if (_g_enum.find(t->val) != _g_enum.end()) {
                t->type = CPP_TYPE;
            }
        }
    }
}

void add_base(const std::set<ClassType>& cs, ClassType c, std::set<ClassType>& bases) {
    if (!c.father.empty()) {
        auto c_f = cs.find({c.father,false,false,""});
        if (c_f != cs.end()) {
            //忽略三方模块的方法
            bases.insert(*(c_f));
            add_base(cs, *(c_f), bases);
        }
    }
}

static inline void jump_brace(std::deque<Token>::iterator& t, std::deque<Token>& ts) {
    assert(t->type == CPP_OPEN_BRACE || t->type == CPP_CLASS_BEGIN);
    std::stack<Token> ss;
    ss.push(*t);
    ++t;
    while(!ss.empty() && t != ts.end()) {
        if (t->type == CPP_OPEN_BRACE || t->type == CPP_CLASS_BEGIN) {
            ss.push(*t);
            ++t;
        } else if (t->type == CPP_CLOSE_BRACE || t->type == CPP_CLASS_END) {
            assert(ss.top().type == CPP_OPEN_BRACE || ss.top().type == CPP_CLASS_BEGIN);
            ss.pop();
            if (ss.empty()) {
                continue;
            }
            ++t;
        } else {
            ++t;
        }
    }
}

static inline void jump_angle_brace(std::deque<Token>::iterator& t, std::deque<Token>& ts) {
    assert(t->type == CPP_LESS);
    std::stack<Token> ss;
    ss.push(*t);
    ++t;
    while(!ss.empty() && t != ts.end()) {
        if (t->type == CPP_LESS) {
            ss.push(*t);
            ++t;
        } else if (t->type == CPP_GREATER) {
            assert(ss.top().type == CPP_LESS);
            ss.pop();
            if (ss.empty()) {
                continue;
            }
            ++t;
        } else {
            ++t;
        }
    }
}

static inline void jump_paren(std::deque<Token>::iterator& t, std::deque<Token>& ts) {
    std::stack<Token> s_de;
    while (t->type != CPP_OPEN_PAREN && t != ts.end()) {
        ++t;
    }
    if (t == ts.end()) {
        return;
    }
    s_de.push(*t);
    // )
    ++t;
    while (!s_de.empty()) {
        if (t->type == CPP_CLOSE_PAREN && s_de.top().type == CPP_OPEN_PAREN) {
            s_de.pop();
            ++t;
        } else if (t->type == CPP_OPEN_PAREN) {
            s_de.push(*t);
            ++t;
        } else {
            ++t;
        }
    }
};

std::map<std::string, Token> ParserGroup::parse_tempalte_para(
    std::deque<Token>::iterator t_begin, 
    std::deque<Token>::iterator t_end) {
        
    assert(t_begin->type == CPP_LESS);
    assert(t_end->type == CPP_GREATER); 
    std::map<std::string, Token> paras;
    auto t=t_begin+1;
    while (t < t_end) {
        if (t->val == "class" || t->val == "typename") {
            //向前找到第一个参数
            ++t;

            std::string t_name;
            Token t_val;
            t_val.type = CPP_TYPE;
            t_val.subject = "template";
            if (t->type == CPP_ELLIPSIS && (t+1)->type == CPP_NAME) {
                //template<class ... T> 这种形式的模板
                t_name = (t+1)->val;
                t_val.val = t_name;
                t_val.subject = "...";
                t+=2;
            } else if (t->type == CPP_NAME) {
                t_name = t->val;
                t_val.val = t_name;
                ++t;
            }
            
            std::stack<Token> sb;
            while(t->type != CPP_COMMA && t<t_end) {
                t_val.ts.push_back(*t);
                if (t->type == CPP_OPEN_BRACE) {
                    sb.push(*t);
                } else if (t->type == CPP_CLOSE_BRACE) {
                    sb.pop();
                }

                //多走一步
                ++t;
                if (t->type == CPP_COMMA && sb.empty()) {
                    continue;
                } else {
                    t_val.ts.push_back(*t);
                    ++t;
                    continue;
                }
            }

            if (t_name.empty()) {
                assert(false);
                continue;
            }

            paras[t_name] = t_val;
            ++t;
            continue;           
        } else if (t->type == CPP_TYPE) {
            Token t_t = *t;
            t_t.type = CPP_TYPE;
            ++t;
            while(t->type != CPP_NAME && t->type != CPP_COMMA && t<t_end) {
                ++t;
            }
            assert(t->type == CPP_NAME);
            paras[t->val] = t_t;

            ++t;
            continue;

        } else if (t->val == "const") {
            ++t;
            continue;
        } else {
            ++t;
            continue;
        }
    }

    return paras;
}

void ParserGroup::extract_class(
    std::deque<Token>::iterator& t,
    std::deque<Token>::iterator t_begin, 
    std::deque<Token>::iterator t_end, Scope scope,
    std::deque<Token>& ts,
    bool is_template) {
    //it begin calss
    //it end }

    //---------------------------------------------//
    //common function
    std::function<std::deque<Token>(std::deque<Token>::iterator)> recall_fn_ret = [](std::deque<Token>::iterator it_cf) {
        //找上一个 ; } comments 
        //LABEL 如果是模板函数会带tempalte<[*]>
        std::stack<Token> srets;
        auto t = it_cf-1;
        while(!(t->type == CPP_SEMICOLON || t->type == CPP_COMMENT || t->type == CPP_CLOSE_BRACE || t->type == CPP_COLON)) {
            srets.push(*(t--));
        }
        std::deque<Token> rets;
        while(!srets.empty()) {
            // if (srets.top()->val == "template") {
            //     //TODO pop all template paras
            // }
            rets.push_back(srets.top());
            srets.pop();
        }
        return rets;
    };
    //---------------------------------------------//


    std::map<std::string, Token> t_paras;
    if (is_template) {
        assert(t->val == "template");
        //在class 和 struct前找到模板类型
        t++;
        assert(t->type == CPP_LESS);
        auto t_0 = t;
        jump_angle_brace(t, ts);
        assert(t->type == CPP_GREATER);
        t_paras = parse_tempalte_para(t_0, t);
        ++t;
        
        //把class定义范围的所有template 类型设置成CPP_TYPE
        if (!t_paras.empty()) {
            auto t_c = t;
            while (t_c < t_end) {
                if (t_c->type == CPP_NAME) {
                    auto tp = t_paras.find(t_c->val);
                    if (tp != t_paras.end() && tp->second.subject == "template") {
                        t_c->type = CPP_TYPE;
                        t_c->subject = "template";
                    }
                }
                ++t_c;
            }
        }
    }

    //确定class 名称
    assert(t->val == "class" || t->val == "struct");
    const bool is_struct = t->val == "class" ? false : true;
    const TokenType cur_type = t->val == "class" ? CPP_CLASS : CPP_STURCT;
    const int default_access = t->val == "class" ? 2 : 0;
            
    ++t;
    //略过宏定义
    while (t->type != CPP_NAME) {
        t++;
    }
    std::string cur_c_name = t->val;
    auto it_n = t+1;
    if (it_n ->type == CPP_SCOPE) {
        //类中类
        assert((t+2)->type == CPP_NAME);
        t+=2;
        //增加scope
        Scope sub;
        sub.type = 1;
        sub.father = scope.father;
        sub.name = cur_c_name;
        sub.father.push_back(scope);
        for (auto itc = sub.father.begin(); itc != sub.father.end(); ++itc) {
           if (!itc->name.empty()) {
                sub.key += itc->name + "::"; 
            }
        }
    
        sub.key += sub.name;
        scope = sub;
        cur_c_name = t->val;
    }

    t->type = CPP_CLASS;
    
    //分析继承 
    std::string father;
    ++t;
    while(t->type != CPP_CLASS_BEGIN && t->type != CPP_COLON) {
        ++t;
    }
    if (t->type == CPP_COLON) {
        //存在继承关系
        t++;
        t++;
        while(t->type != CPP_CLASS_BEGIN) {
            if (t->val == "std" && (t+1)->type == CPP_SCOPE ) {
                //跳过std::shared_enable的继承
                if ((t+2)->val == "enable_shared_from_this") {
                    assert((t+3)->type == CPP_LESS);
                    t+=3;
                    jump_angle_brace(t, ts);
                    t++;
                    continue;
                } else {
                    father = (t+2)->val;
                }
                t+=3;
                continue;
            } else if (t->type == CPP_NAME) {
                father = t->val;
                ++t;
                continue;
            }
            ++t;
        }
    }
    assert(t->type == CPP_CLASS_BEGIN);

    if (_g_class.find({cur_c_name, is_struct, is_template, "", Scope()}) == _g_class.end()) {
        _g_class.insert({cur_c_name, is_struct, is_template, father, scope, t_paras});
    }
    if (_g_class_fn.find({cur_c_name, is_struct, is_template, "", Scope()}) == _g_class_fn.end()) {
        _g_class_fn[{cur_c_name, is_struct, is_template, "", scope}] = std::vector<ClassFunction>();
    }
    std::vector<ClassFunction>& class_fn = _g_class_fn.find({cur_c_name, is_struct, is_template, "", Scope()})->second;

    ++t;
    //分析成员变量和成员
    int access = default_access;
    bool next_class_template = false;
    auto t_template = t;
    std::cout << "begin extract class function: " << cur_c_name << std::endl;
    std::stack<Token> sb;
    std::function<void(std::deque<Token>::iterator)> update_t_end = 
    [&t_end](std::deque<Token>::iterator t) {
        while(t->type != CPP_CLASS_END) {
            ++t;
        }
        t_end = t;
    };

    while(t < t_end) {
        //提取类成员函数
        //默认是class 是 private, struct 是 public
        if (t->type == CPP_OPEN_BRACE) {
            sb.push(*t);
            ++t;
            continue;
        } else if (t->type == CPP_CLOSE_BRACE) {
            sb.pop();
            ++t;
            continue;
        }

        if (!sb.empty()) {
            ++t;
            continue;
        }

        if (t->val == "public") {
            access = 0;
            ++t;
            continue;
        } else if (t->val == "protected") {
            access = 1;
            ++t;
            continue;
        } else if (t->val == "private") {
            access = 2;
            ++t;
            continue;
        } else if (t->val == "template") {
            //找到了模板, 忽略其中所有的class
            //找<
            t_template = t;
            std::stack<Token> st;
            ++t;
            while(t->type != CPP_LESS && t != ts.end()) {
                ++t;
            }
            if (t == ts.end()) {
                continue;
            }
            st.push(*t);
            ++t;
            //找<>组合
            while(!st.empty() && t!= ts.end()) {
                if (t->type == CPP_LESS) {
                    st.push(*t);
                }
                else if (t->type == CPP_GREATER && st.top().type == CPP_LESS) {
                    st.pop();
                }
                ++t;
            }

            if (t == ts.end()) {
                continue;
            }

            next_class_template = (t->val == "class" || t->val == "struct");
            continue;
        } else if(t->val == "class" || t->val == "struct") {
            //可能是类中类,需要迭代
             std::cout << "may got class in class: " << (t+1)->val << std::endl;

            auto t_begin2 = t;
            while (t->type != CPP_NAME) {
                t++;
            }

        CHECK_CLASS:
            while (t->type != CPP_SCOPE && t->type != CPP_SEMICOLON && t->type != CPP_OPEN_BRACE 
                && t->type != CPP_EQ && t->type != CPP_MULT &&  t->type != CPP_AND) {
                t++;
            }
            if (t->type == CPP_EQ || t->type == CPP_MULT || t->type == CPP_AND) {
                t++;
                continue;
            }

            if (t->type == CPP_SEMICOLON) {
                continue;
            } else if (t->type == CPP_OPEN_BRACE) {
                //类定义
                t->type = CPP_CLASS_BEGIN;
                jump_brace(t, ts);
                assert(t->type == CPP_CLOSE_BRACE || t->type == CPP_CLASS_END);
                t->type = CPP_CLASS_END;

                //增加scope
                Scope sub;
                sub.type = 1;
                sub.father = scope.father;
                sub.name = cur_c_name;
                sub.father.push_back(scope);
                for (auto itc = sub.father.begin(); itc != sub.father.end(); ++itc) {
                   sub.key += itc->name + "::"; 
                }
                sub.key += sub.name;

                if (next_class_template) {
                    next_class_template = false;
                    t_template = t;
                    extract_class(t_template, t_template, t, sub, ts, true);
                } else {
                    extract_class(t_begin2, t_begin2, t, sub, ts, false);
                }
                ++t;
                continue;
            } else if (t->type == CPP_SCOPE) {
                //可能类中类暂时略过
                t+1;
                goto CHECK_CLASS; 
            }
        } else  {
            //!!这里仅仅分析class的定义的第一层作用域的内容
            /// \ 分析构造函数 

            //找括号组
            //需要避开构造函数的初始化列表
            auto t_n = t+1;
            if (t->val == cur_c_name && (t-1)->type == CPP_COMPL) {
                //析构函数
                t->type = CPP_MEMBER_FUNCTION;
                t->val = "~"+t->val;
                t->subject = cur_c_name;
                class_fn.push_back({access, cur_c_name, t->val, std::deque<Token>(), Token()});

                //删除 ~
                --t;
                t = ts.erase(t);
                update_t_end(t);

                //跳过函数的括号组
                ++t;
                jump_paren(t, ts);

                continue;
            } else if (t->val == cur_c_name && t_n <= t_end && t_n->type == CPP_OPEN_PAREN) {
                //可能是构造函数
                if ((t-1)->val == "explicit") {
                    //肯定是构造函数
                    t->type = CPP_MEMBER_FUNCTION;
                    t->subject = cur_c_name;
                    class_fn.push_back({access, cur_c_name, t->val, std::deque<Token>(), Token()});

                    ++t;
                    jump_paren(t, ts);
                    if (t->type == CPP_COLON) {
                        //初始化列表 跳过直至第一个 {
                        ++t;
                        while(t->type != CPP_OPEN_BRACE && t!=ts.end()) {
                            ++t;
                        }
                        
                    } else if (t->type == CPP_OPEN_BRACE) {
                        //构造函数体
                    } else if (t->type == CPP_SEMICOLON) {
                        //构造函数定义
                    } else {
                        //错误
                        std::cerr << "error class construction function.\n";
                    }
                                    
                } else {
                    auto t_may_c = t;
                    ++t;
                    jump_paren(t, ts);
                    if (t->type == CPP_COLON) {
                        //初始化列表 是构造函数 跳过直至第一个 {
                        t_may_c->type = CPP_MEMBER_FUNCTION;
                        t_may_c->subject = cur_c_name;
                        class_fn.push_back({access, cur_c_name, t_may_c->val, std::deque<Token>(), Token()});
                        ++t;
                        while(t->type != CPP_OPEN_BRACE && t!=ts.end()) {
                            ++t;
                        }
                        jump_brace(t, ts);
                        ++t;
                    } else if (t->type == CPP_OPEN_BRACE) {
                        //构造函数体  是构造函数
                        t_may_c->type = CPP_MEMBER_FUNCTION;
                        t_may_c->subject = cur_c_name;
                        class_fn.push_back({access, cur_c_name, t_may_c->val, std::deque<Token>(), Token()});
                        jump_brace(t, ts);
                        ++t;
                    } else if (t->type == CPP_SEMICOLON) {
                        //构造函数定义  是构造函数
                        t_may_c->type = CPP_MEMBER_FUNCTION;
                        t_may_c->subject = cur_c_name;
                        class_fn.push_back({access, cur_c_name, t_may_c->val, std::deque<Token>(), Token()});
                    } else {
                        //不是构造函数
                        continue;
                    }
                }
            } else if(t->type == CPP_NAME && t_n != ts.end() && t_n->type == CPP_OPEN_PAREN) {
                //可能是成员函数
                //std::cout << "may function: " << t->val << std::endl;
                auto t_may_c = t;
                ++t;
                jump_paren(t, ts);
                if (t->type == CPP_OPEN_BRACE || t->type == CPP_SEMICOLON || t->val == "const" || t->val == "throw" || (t->type == CPP_EQ && (t+1)->val == "0")) {
                    t_may_c->type = CPP_MEMBER_FUNCTION;
                    t_may_c->subject = cur_c_name;
                    class_fn.push_back({access, cur_c_name, t_may_c->val, recall_fn_ret(t_may_c), Token()});
                } else {
                    //不是成员函数
                    continue;
                }
            } else if(t->val == "operator") {
                //运算符重载函数
                t->type = CPP_MEMBER_FUNCTION;
                if((t+1)->type == CPP_OPEN_PAREN && (t+2)->type == CPP_CLOSE_PAREN) {
                    t->ts.push_back(*(t+1));
                    t->ts.push_back(*(t+2));
                    t++;
                    t = ts.erase(t);//(
                    t = ts.erase(t);//)
                    --t;
                    class_fn.push_back({access, cur_c_name, t->val, recall_fn_ret(t), Token()});
                } else {
                    //合并到第一个(之前的运算符
                    auto t_op = t;
                    ++t;
                    while(t->type != CPP_OPEN_PAREN) {
                        t_op->ts.push_back(*t);
                        t = ts.erase(t);
                    }
                    --t;
                    class_fn.push_back({access, cur_c_name, t->val, recall_fn_ret(t), Token()});
                }
                update_t_end(t);
                ++t;
                assert(t->type == CPP_OPEN_PAREN);
                jump_paren(t,ts);
                continue;
            } else {
                ++t;
            }
        }
    }
}

void ParserGroup::extract_class() {
    int idx = 0;
    for (auto it = _parsers.begin(); it != _parsers.end(); ++it) {
        Parser& parser = *(*it);
        std::string file_name = _file_name[idx++];
        std::cout << "extract class in: " << file_name << std::endl;
        std::deque<Token>& ts = parser._ts;
        std::deque<Scope> scopes;
        Scope root_scope;
        root_scope.type = 0;
        scopes.push_back(root_scope);
        Scope* cur_scope = &(scopes.back());
        for (auto t = ts.begin(); t != ts.end(); ) {
            //scope 
            bool next_class_template = false;
            auto t_template = t;
            if (t->val=="namespace" &&
                (t+1)!= ts.end() && (t+1)->type == CPP_NAME &&
                (t+2)!= ts.end() && (t+2)->type == CPP_OPEN_BRACE) {
                Scope sub;
                sub.type = 0;
                sub.name = (t+1)->val;
                sub.sb.push(*(t+1));
                sub.father = scopes;
                for (auto itc = scopes.begin(); itc != scopes.end(); ++itc) {
                    if (!itc->name.empty()) {
                        sub.key += itc->name + "::"; 
                    }
                }
                sub.key += sub.name;
                
                scopes.push_back(sub);
                cur_scope = &scopes.back();
                t+=3;
                continue;
            } else if (t->val=="namespace" && (t+1)->type == CPP_OPEN_BRACE) {
                //不可以在匿名域中定义class
                ++t;
                jump_brace(t, ts);
                ++t;
                continue;
            } else if (t->type == CPP_OPEN_BRACE) {
                cur_scope->sb.push(*t);
                ++t;
                continue;
            } else if (t->type == CPP_CLOSE_BRACE) {
                cur_scope->sb.pop();
                if (!cur_scope->name.empty() && cur_scope->sb.empty()) {
                    //如果不是默认作用域 则跳出当前作用域
                    scopes.pop_back();
                    cur_scope = &scopes.back();
                }
                ++t;
                continue;
            }

            //找到了模板, 忽略其中所有的class
            if (t->val == "template") {
                t_template = t;
                //找<
                std::stack<Token> st;
                ++t;
                while(t->type != CPP_LESS && t != ts.end()) {
                    ++t;
                }
                if (t == ts.end()) {
                    continue;
                }
                st.push(*t);
                ++t;
                //找<>组合
                while(!st.empty() && t!= ts.end()) {
                    if (t->type == CPP_LESS) {
                        st.push(*t);
                    }
                    else if (t->type == CPP_GREATER && st.top().type == CPP_LESS) {
                        st.pop();
                    }
                    ++t;
                }

                if (t == ts.end()) {
                    continue;
                }

                next_class_template = (t->val == "class" || t->val == "struct");
                //继续下面的分析
            } 
            
            if (t->val == "class" || t->val == "struct") {
                //确定是不是class的定义
                //TODO LABEL 不考虑 typedef struct语法
                //typedef struct {
                // ...    
                //}

                if ((t+1)->type == CPP_OPEN_BRACE) {
                    ++t;
                    continue;
                }

                auto t_begin = t;
                while (t->type != CPP_NAME) {
                    t++;
                }

            CHECK_CLASS:
                while (t->type != CPP_SCOPE && t->type != CPP_SEMICOLON && t->type != CPP_OPEN_BRACE 
                    && t->type != CPP_EQ && t->type != CPP_MULT && t->type != CPP_AND) {
                    t++;
                }
                if (t->type == CPP_EQ || t->type == CPP_MULT || t->type == CPP_AND) {
                    //C struct的赋值语句 或者 struct type*的转换
                    continue;
                }

                if (t->type == CPP_SCOPE) {
                    //类中类在类外定义
                    ++t;
                    goto CHECK_CLASS;
                }


                if (t->type == CPP_SEMICOLON) {
                    continue;
                } else if (t->type == CPP_OPEN_BRACE) {
                    //类定义
                    t->type = CPP_CLASS_BEGIN;
                    jump_brace(t, ts);
                    assert(t->type == CPP_CLOSE_BRACE || t->type == CPP_CLASS_END);
                    t->type = CPP_CLASS_END;
                    if (next_class_template) {
                        extract_class(t_template, t_template, t, *cur_scope, ts, true);
                    } else {
                        extract_class(t_begin, t_begin, t, *cur_scope, ts, false);
                    }
                    ++t;
                    continue;
                } else if (t->type == CPP_SCOPE) {
                    //可能类中类暂时略过
                    t++;
                    goto CHECK_CLASS; 
                }
                continue;
            } 

            ++t;
        }   
    }

    //分析class的继承关系
    bool steady = false;
    while(!steady) {
        steady = true;
        for (auto it_c = _g_class.begin(); it_c != _g_class.end(); ++it_c) {
        if (!it_c->father.empty() && it_c->father != "3th" && _g_class.find({it_c->father, false, false, "", Scope()}) == _g_class.end()) {
                //LABEL 继承自三方库， 虚函数不能混淆
                ClassType new_c = *it_c;
                new_c.father = "3th";
                _g_class.erase(it_c);
                _g_class.insert(new_c);
                steady = false;
                break;
            }
        }
    }

    for (auto it_c = _g_class.begin(); it_c != _g_class.end(); ++it_c) {
        _g_class_childs[*it_c] = std::set<ClassType>();
    }

    //构造类的所有child
    steady = false;
    while(!steady) {
        steady = true;
        for (auto it_c = _g_class_childs.begin(); it_c != _g_class_childs.end(); ++it_c) {
            const ClassType& c = it_c->first;
            if (!c.father.empty()) {
                auto it_base = _g_class_childs.find({c.father,false,false,"", Scope()});
                if (it_base == _g_class_childs.end()) {
                    //TODO 从三方模块继承, 这种类需要单独标记出来,不能混淆成员函数
                    continue;
                }

                //assert(it_base != _g_class_childs.end());

                auto it_item = it_base->second.find(c);
                if (it_item == it_base->second.end()) {
                    it_base->second.insert(c);
                    steady = false;
                }

                //把class的child都添加到class的base中去
                for (auto it_cc = it_c->second.begin(); it_cc != it_c->second.end(); ++it_cc) {
                    it_item = it_base->second.find(*it_cc);
                    if (it_item == it_base->second.end()) {
                        it_base->second.insert(*it_cc);
                        steady = false;
                    }
                }
            }
        }
    }

    //构造类的所有base
    for (auto it_c = _g_class.begin(); it_c != _g_class.end(); ++it_c) {
        _g_class_bases[*it_c] = std::set<ClassType>();
    }

    for (auto it_c = _g_class.begin(); it_c != _g_class.end(); ++it_c) {
        add_base(_g_class, *it_c, _g_class_bases[*it_c]);
    }


    //解析class struct 的 type
    for (auto it = _parsers.begin(); it != _parsers.end(); ++it) {
        Parser& parser = *(*it);
        std::deque<Token>& ts = parser._ts;
        for (auto t = ts.begin(); t != ts.end(); ) {
            bool tm = false;
            if (t->type == CPP_NAME && is_in_class_struct(t->val, tm)) {
                t->type = CPP_TYPE;
                if (!tm) {
                    ++t;
                    continue;
                } else {
                    //合并模板类型的类或者结构体
                    Token* c_t = &(*t);
                    if ((t+1) != ts.end() && (t+1)->type == CPP_LESS) {
                        std::stack<Token> st; 
                        st.push(*(t+1));  
                        c_t->ts.push_back(*(t+1));
                        t = ts.erase(t+1);//<
                        c_t = &(*(t-1));
                        while(!st.empty() && t != ts.end()) {
                            if (t->type == CPP_LESS) {
                                st.push(*t);
                            }
                            else if (t->type == CPP_GREATER) {
                                assert(st.top().type == CPP_LESS);
                                st.pop();
                            }
                            c_t->ts.push_back(*t);
                            t = ts.erase(t);
                            c_t = &(*(t-1));
                        }
                    } else {
                        t->type = CPP_TYPE;
                        ++t;
                        continue;
                    }
                }
            } else {
                ++t;
            }
        }
    }
}

void ParserGroup::extract_typedef() {
    //抽取typedef的类型(注意作用域)
    for (auto it = _parsers.begin(); it != _parsers.end(); ++it) {
        Parser& parser = *(*it);
        std::deque<Token>& ts = parser._ts;

        for (auto t = ts.begin(); t != ts.end(); ) {

            //---------------------------------------------------------//
            //common function
            std::function<void()> typedef_analyze = [this, &t, &ts]() {
                Token td;
                td = *t;
                td.ts.clear();
                ++t;
                while(!((t->type == CPP_NAME||t->type == CPP_TYPE) && (t+1)!=ts.end() && (t+1)->type == CPP_SEMICOLON)) {
                    td.ts.push_back(*t);
                    ++t;
                }
                t->type = CPP_TYPE;
                //td.ts.push_back(*t);
                td.val = t->val;
                if (_typedef_map.find(t->val) != _typedef_map.end()) {
                    //TODO LABEL 目前不能分辨不同域的typedef
                    //比较typedef内容
                    auto it_old = _typedef_map.find(t->val);
                    assert(it_old->second.ts.size()==td.ts.size());
                    auto it0 = it_old->second.ts.begin();
                    auto it1 = td.ts.begin();
                    for (;it1 != td.ts.end(); ++it0,++it1) {
                        assert(it1->val == it0->val);
                    }
                } else {
                    _typedef_map[t->val] = td;
                }
                ++t;
            };

            if (t->val == "typedef") {
                typedef_analyze();
                continue;
            } 

            ++t;
        }
    }

    //内部展开typedef
    bool steady = false;
    while(!steady) {
        steady = true;
        for (auto it = _typedef_map.begin(); it != _typedef_map.end(); ++it) {
            for (auto it2 = it->second.ts.begin(); it2 != it->second.ts.end(); ) {
                auto it3 = _typedef_map.find(it2->val);
                if (it3 != _typedef_map.end()) {
                    //展开
                    it2 = it->second.ts.erase(it2);
                    for (auto it4 = it3->second.ts.begin(); it4 != it3->second.ts.end(); ++it4) {
                        it2 = it->second.ts.insert(it2, *it4);
                        ++it2;
                    }
                    steady = false;
                    continue;
                }
                ++it2;
            }
        }
    }

    //展开所有的typedef
    for (auto it = _parsers.begin(); it != _parsers.end(); ++it) {
        Parser& parser = *(*it);
        std::deque<Token>& ts = parser._ts;

        for (auto t = ts.begin(); t != ts.end(); ) {
            Token tt;
            if (t->val == "typedef") {
                //跳过typedef
                ++t;
                while(t->type != CPP_SEMICOLON) {
                    ++t;
                }
                continue;
            } else if (t->type == CPP_NAME && is_in_typedef(t->val, tt)) {
                t = ts.erase(t);
                for (auto it2 = tt.ts.begin(); it2 != tt.ts.end(); ++it2) {
                    t = ts.insert(t, *it2);
                    ++t;
                }
                continue;
            } else {
                ++t;
            }
        }
    }
}

void ParserGroup::extract_container() {
    //TODO typedef已经连接了可能存在的container,需要重新分析

    //分析的容器如下
    //vector
    //deque
    //queue
    //stack
    //list
    //set
    //map
    //pair
    //auto_ptr
    //shared_ptr
    //weak_ptr
    //unique_ptr
    std::set<std::string> std_container;
    std_container.insert("vector");
    std_container.insert("deque");
    std_container.insert("queue");
    std_container.insert("stack");
    std_container.insert("list");
    std_container.insert("set");
    std_container.insert("map");
    std_container.insert("pair");
    std_container.insert("auto_ptr");
    std_container.insert("shared_ptr");
    std_container.insert("weak_ptr");
    std_container.insert("unique_ptr");
    std_container.insert("function");

    std::string ns = "std"; 

    int idx = 0;
    for (auto it = _parsers.begin(); it != _parsers.end(); ++it) {
        Parser& parser = *(*it);
        std::string file_name = _file_name[idx++];
        std::cout << "extract container: " << file_name << std::endl;
        std::deque<Token>& ts = parser._ts;
        for (auto t = ts.begin(); t != ts.end(); ) {
            auto t_n = t+1;//::
            auto t_nn = t+2;//container
            auto t_nnn = t+3;//<

            if (t->val == ns && t_n != ts.end() && t_nn != ts.end() && t_nnn != ts.end() &&
                t_n->type == CPP_SCOPE && std_container.find(t_nn->val) != std_container.end() && t_nnn->type == CPP_LESS) {
                //找到std的源头, 第一个容器
                std::stack<Token*> stt;//stack token type
                std::stack<Token> st_scope; //<>
                
                Token* root_scope = &(*t);
                root_scope->val = t_nn->val;
                root_scope->subject = ns;
                root_scope->type = CPP_TYPE;
                stt.push(root_scope);
                Token* cur_scope = stt.top();
                st_scope.push(*t_nnn); //<

                int step = 0;

                t+=4;
                step +=3;//:: container <

                while(!stt.empty() && t != ts.end()) {
                    auto t_n = t+1;//::
                    auto t_nn = t+2;//container
                    auto t_nnn = t+3;//<
                    if (t->val == ns && t_n != ts.end() && t_n->type == CPP_SCOPE &&
                        t_nn != ts.end() && std_container.find(t_nn->val) != std_container.end() &&
                        t_nnn != ts.end() && t_nnn->type == CPP_LESS) {
                        //容器的嵌套
                        const std::string cur_name = t_nn->val;
                        if (std_container.find(cur_name) != std_container.end()) {
                            Token sub;
                            sub.val = cur_name;
                            sub.type = CPP_TYPE;
                            sub.subject = ns;
                            cur_scope->ts.push_back(sub);
                            cur_scope = &(cur_scope->ts.back());//进入到下一个容器中
                            st_scope.push(*t_nnn);
                            stt.push(cur_scope);

                            step += 4; // std :: container <
                            t+=4;
                        } else {
                            //TODO 如何处理 ?
                            std::cout << "unsupported std container: " << cur_name << "\n";
                            ++t;
                            continue;
                        }                        
                    } else if (t->type == CPP_TYPE) {
                        //已知类型
                        //查找是不是模板类
                        cur_scope->ts.push_back(*t);
                        ++t;
                        ++step;
                        continue;
                    } else if (t->type == CPP_TYPE && t_n->type == CPP_LESS) {
                        //TODO 应该也是其他模块的模板, 而且是没有被解析的
                        std::cout << "invalid template name: " << t->val << std::endl; 
                        assert(false);
                    } else if (t->type == CPP_NAME) {
                        //TODO 应该是其他模块类型 直接合并
                        std::cout << "invalid 3th type: " << t->val << std::endl; 
                        //合并模板
                        std::stack<Token> tss;
                        Token tt = *t;
                        ++t;
                        ++step;
                        while(t->type != CPP_GREATER) {
                            tt.ts.push_back(*t);
                            if (t->type == CPP_LESS) {
                                tss.push(*t);
                                ++t;
                                ++step;
                            } else {
                                ++t;
                                ++step;
                            }

                            if (t->type == CPP_GREATER && !tss.empty()) {
                                tt.ts.push_back(*t);
                                tss.pop();
                                ++t;
                                ++step;
                                continue;
                            } else if (t->type == CPP_GREATER && tss.empty()) {
                                tt.ts.push_back(*t);
                                --t;
                                --step;
                                break;
                            }
                        }
                        
                        cur_scope->ts.push_back(tt);
                        continue;
                    } else if(t->type == CPP_OPEN_SQUARE || t->type == CPP_CLOSE_SQUARE ) {
                        //unique_ptr
                        cur_scope->ts.push_back(*t);
                        ++t;
                        ++step;
                        continue;
                    } else if (t->type == CPP_COMMA) {
                        ++t;
                        ++step;
                        continue;
                    } else if (t->type == CPP_MULT) {
                        //指针
                        cur_scope->ts.push_back(*t);
                        ++t;
                        ++step;
                        continue;
                    } else if (t->type == CPP_SCOPE) {
                        //指针
                        cur_scope->ts.push_back(*t);
                        ++t;
                        ++step;
                        continue;
                    } else if (t->type == CPP_OPEN_PAREN || t->type == CPP_CLOSE_PAREN) {
                        //function 会带()
                        cur_scope->ts.push_back(*t);
                        ++t;
                        ++step;
                        continue;
                    } else if (t->type == CPP_GREATER) {
                        //>
                        //jump out scope
                        assert(st_scope.top().type == CPP_LESS); 
                        st_scope.pop();
                        stt.pop();
                        cur_scope = stt.empty() ? nullptr : stt.top();
                        ++t;
                        ++step;
                        continue;
                    } else {
                        std::cout << "invalid name 2: " << t->val << std::endl; 
                        assert(false);
                        ++t;
                    }
                }   

                //erase 
                t-=step;
                while (step>0) {
                    t = ts.erase(t);
                    --step;
                }

                //std::cout << "t-1: " << (t-1)->val << std::endl;

            } else {
                ++t;
            }
        }
    }

    std_container.clear();
    std_container.insert("shared_ptr");
    ns = "boost"; 

    for (auto it = _parsers.begin(); it != _parsers.end(); ++it) {
        Parser& parser = *(*it);
        std::deque<Token>& ts = parser._ts;
        for (auto t = ts.begin(); t != ts.end(); ) {
            auto t_n = t+1;//::
            auto t_nn = t+2;//container
            auto t_nnn = t+3;//<

            if (t->val == ns && t_n != ts.end() && t_nn != ts.end() && t_nnn != ts.end() &&
                t_n->type == CPP_SCOPE && std_container.find(t_nn->val) != std_container.end() && t_nnn->type == CPP_LESS) {
                //找到std的源头, 第一个容器
                std::stack<Token*> stt;//stack token type
                std::stack<Token> st_scope; //<>
                
                Token* root_scope = &(*t);
                root_scope->val = t_nn->val;
                root_scope->subject = ns;
                root_scope->type = CPP_TYPE;
                stt.push(root_scope);
                Token* cur_scope = stt.top();
                st_scope.push(*t_nnn); //<

                int step = 0;

                t+=4;
                step +=3;//:: container <

                while(!stt.empty() && t != ts.end()) {
                    auto t_n = t+1;//::
                    auto t_nn = t+2;//container
                    auto t_nnn = t+3;//<
                    if (t->val == ns && t_n != ts.end() && t_nn != ts.end() && t_nnn != ts.end() && 
                        t_n->type == CPP_SCOPE && t_nnn->type == CPP_LESS) {
                        //容器的嵌套
                        const std::string cur_name = t_nn->val;
                        if (std_container.find(cur_name) != std_container.end()) {
                            Token sub;
                            sub.val = cur_name;
                            sub.type = CPP_TYPE;
                            sub.subject = ns;
                            cur_scope->ts.push_back(sub);
                            cur_scope = &(cur_scope->ts.back());//进入到下一个容器中
                            st_scope.push(*t_nnn);
                            stt.push(cur_scope);

                            step += 4; // std :: container <
                            t+=4;
                        } else {
                            //TODO 如何处理 ?
                            std::cout << "unsupported std container: " << cur_name << "\n";
                            ++t;
                            continue;
                        }                        
                    } else if (t->type == CPP_GREATER) {
                        //>
                        //jump out scope
                        assert(st_scope.top().type == CPP_LESS); 
                        st_scope.pop();
                        stt.pop();
                        cur_scope = stt.empty() ? nullptr : stt.top();
                        ++t;
                        ++step;
                        continue;
                    } else if (t->type == CPP_TYPE) {
                        //已知类型
                        cur_scope->ts.push_back(*t);
                        ++t;
                        ++step;
                        continue;
                    } else if (t->type == CPP_TYPE && t_n->type == CPP_LESS) {
                        //TODO 应该也是其他模块的模板, 而且是没有被解析的
                        std::cout << "invalid template name: " << t->val << std::endl; 
                        assert(false);
                    } else if (t->type == CPP_NAME) {
                        //TODO 应该是其他模块类型 直接合并

                        std::cout << "invalid template name 2: " << t->val << std::endl; 
                        //合并模板
                        std::stack<Token> tss;
                        Token tt = *t;
                        ++t;
                        ++step;
                        while(t->type != CPP_GREATER && t->type != CPP_COMMA) {
                            tt.ts.push_back(*t);
                            if (t->type == CPP_LESS) {
                                tss.push(*t);
                                ++t;
                                ++step;
                                continue;
                            } else {
                                ++t;
                                ++step;
                            }

                            if (t->type == CPP_GREATER && !tss.empty()) {
                                tss.pop();
                                ++t;
                                ++step;
                                continue;
                            } else if (t->type == CPP_GREATER && tss.empty()) {
                                --t;
                                --step;
                                break;
                            }
                        }
                        
                        cur_scope->ts.push_back(tt);
                        continue;
                    } else if(t->type == CPP_OPEN_SQUARE || t->type == CPP_CLOSE_SQUARE ) {
                        //unique_ptr
                        cur_scope->ts.push_back(*t);
                        ++t;
                        ++step;
                        continue;
                    } else if (t->type == CPP_COMMA) {
                        ++t;
                        ++step;
                        continue;
                    } else if (t->type == CPP_MULT) {
                        //指针
                        cur_scope->ts.push_back(*t);
                        ++t;
                        ++step;
                        continue;
                    } else if (t->type == CPP_SCOPE) {
                        //指针
                        cur_scope->ts.push_back(*t);
                        ++t;
                        ++step;
                        continue;
                    } else if (t->type == CPP_OPEN_PAREN || t->type == CPP_CLOSE_PAREN) {
                        //function 会带()
                        cur_scope->ts.push_back(*t);
                        ++t;
                        ++step;
                        continue;
                    } else {
                        std::cout << "invalid name 2: " << t->val << std::endl; 
                        assert(false);
                        ++t;
                    }
                }   

                //erase 
                t-=step;
                while (step>0) {
                    t = ts.erase(t);
                    --step;
                }

                //std::cout << "t-1: " << (t-1)->val << std::endl;

            } else {
                ++t;
            }
        }
    }

    //合并stl 迭代器
    for (auto it = _parsers.begin(); it != _parsers.end(); ++it) {
        Parser& parser = *(*it);
        std::deque<Token>& ts = parser._ts;
        for (auto t = ts.begin(); t != ts.end(); ) {
            auto t_n = t+1;
            auto t_nn = t+2;
            if (t->type == CPP_TYPE && t_n != ts.end() && t_nn != ts.end() &&
                t_n->type == CPP_SCOPE && (t_nn->val == "iterator" || t_nn->val == "const_iterator")) {
                //合并
                t_nn->type = CPP_TYPE;
                t_nn->ts.clear();
                t_nn->ts.push_back(*t);
                t = ts.erase(t);//del container
                t = ts.erase(t);//del ::
                ++t;
            } else {
                ++t;
            } 
        }
    } 
}

void ParserGroup::extract_extern_type() {
    //抽取std boost类型
    //TODO 这里可以用配置文件来做, 不仅仅是std boost还可以是其他的三方模块
    ScopeType std_scope;
    std_scope.scope = "std";
    std_scope.types.insert("string");
    std_scope.types.insert("atomic_int");
    std_scope.types.insert("atomic_bool");
    std_scope.types.insert("ifstream");
    std_scope.types.insert("ofstream");
    std_scope.types.insert("iostream");
    std_scope.types.insert("fstream");
    std_scope.types.insert("sstream");
    

    ScopeType boost_scope;
    boost_scope.scope = "boost";
    boost_scope.types.insert("thread");
    boost_scope.types.insert("mutex");
    boost_scope.types.insert("condition");
    boost_scope.types.insert("unique_lock");

    ScopeType boost_mutex;
    boost_mutex.scope = "mutex";
    boost_mutex.types.insert("scoped_lock");
    boost_scope.sub_scope.insert(boost_mutex);

    for (auto it = _parsers.begin(); it != _parsers.end(); ++it) {
        Parser& parser = *(*it);
        std::deque<Token>& ts = parser._ts;
        for (auto t = ts.begin(); t != ts.end(); ) {

            //---------------------------------------------------------//
            //common function
            std::function<void(const ScopeType&, const std::string&)> greedy_scope_analyze = [&t, &ts](const ScopeType& scope_type, const std::string& scope_root) {
                TokenType to_be_type = CPP_NAME;
                const ScopeType* last_scope = &scope_type;
                std::deque<Token> ts_to_be_type;

    SCOPE_:
                auto t_n = t+1;
                auto t_nn = t+2;
                auto t_nnn = t+3;
                if (t_n == ts.end() || t_nn == ts.end()) {
                    std::cerr << "std scope err\n";
                    ++t;
                    return;
                }

                if (t_n->type == CPP_SCOPE) {
                    auto it_sub_scope = last_scope->sub_scope.find({t_nn->val, std::set<std::string>(), std::set<ScopeType>()});
                    auto it_type = last_scope->types.find(t_nn->val);

                    if (it_sub_scope != last_scope->sub_scope.end() && it_type != last_scope->types.end() && t_nnn != ts.end() && t_nnn->type == CPP_SCOPE) {
                        ts_to_be_type.push_back(*t_n);//::
                        ts_to_be_type.push_back(*t_nn);//type
                        ts_to_be_type.back().subject = last_scope->scope;
                        t+=2;

                        last_scope = &(*it_sub_scope);

                    } else if (it_type != last_scope->types.end()) {
                        //end 
                        to_be_type = CPP_TYPE;
                        ts_to_be_type.push_back(*t_n);//::
                        ts_to_be_type.push_back(*t_nn);//type
                        ts_to_be_type.back().subject = last_scope->scope;
                        t+=3;
                    } else if (it_sub_scope != last_scope->sub_scope.end()) {
                        ts_to_be_type.push_back(*t_n);//::
                        ts_to_be_type.push_back(*t_nn);//type
                        ts_to_be_type.back().subject = last_scope->scope;
                        t+=2;

                        last_scope = &(*it_sub_scope);
                    } else {
                        //std function
                        std::cout << scope_root << " function: " << t_nn->val << std::endl;
                        ++t;
                        return;
                    } 
                    goto SCOPE_;
                } else if (!ts_to_be_type.empty() && to_be_type == CPP_TYPE) {
                    t -= (ts_to_be_type.size()+1);
                    assert(t->val == scope_root);
                    t->ts.clear();
                    for (auto it_types = ts_to_be_type.begin(); it_types != ts_to_be_type.end(); ++it_types) {
                        if (it_types->type != CPP_SCOPE) {
                            t->ts.push_back(*it_types);
                        }
                    }
                    t->type = CPP_TYPE;
                    t->subject = scope_type.scope;
                    ++t;
                    while(!ts_to_be_type.empty()) {
                        ts_to_be_type.pop_back();
                        t = ts.erase(t);
                    }
                    return;
                } else {
                    std::cerr << "err here.\n";
                }

                ++t;
                return;
            };
            //---------------------------------------------------------//

            //抽取所有的std的类型
            if (t->val == "std") {
                greedy_scope_analyze(std_scope, "std");
            } else if (t->val == "boost") {
                greedy_scope_analyze(boost_scope, "boost");
            }

            ++t;
        }
    }
}

void ParserGroup::combine_type_with_multi_and() {
    //extract class member & function ret
    for (auto it = _parsers.begin(); it != _parsers.end(); ++it) {
        Parser& parser = *(*it);
        std::deque<Token>& ts = parser._ts;

        for (auto t = ts.begin(); t != ts.end(); ) {    
            //conbine type with * &
            if (t->type == CPP_TYPE) {
                auto t_n = t+1;
                if (t_n!=ts.end() && (t_n->type == CPP_MULT || t_n->type == CPP_AND)) {
                    t->ts.push_back(*t_n);
                    t++;
                    t = ts.erase(t);
                    continue;
                }
            }
            ++t;
        }
    }
}
void ParserGroup::extract_class_member (
    std::string cur_c_name,
    std::deque<Token>::iterator& t, 
    std::deque<Token>::iterator t_begin, 
    std::deque<Token>::iterator t_end, 
    std::deque<Token>& ts,
    bool is_template) {

    std::map<std::string, Token> tm_paras;
    if (is_template) {
        tm_paras = get_template_class_type_paras(cur_c_name);
    }

    std::stack<Token> st_brace;
    st_brace.push(*t);

    auto it_class = _g_class.find({cur_c_name, false, false, "", Scope()});
    assert(it_class != _g_class.end());

    _g_class_variable[*it_class] = std::vector<ClassVariable>();
    std::vector<ClassVariable>& class_variable = _g_class_variable.find(*it_class)->second;

    std::string sub_class_name;
    ++t;
    while (t <= t_end) {
        auto t_n = t+1;
        if (t->type == CPP_CLASS || t->type == CPP_STURCT) {
            sub_class_name = t->val;
            ++t;
            continue;
        } else if (t->type == CPP_OPEN_BRACE) {
            st_brace.push(*t);
            ++t;
            continue;
        } else if (t->type == CPP_CLOSE_BRACE) {
            st_brace.pop();
            ++t;
            continue;
        } else if (t->type == CPP_CLASS_BEGIN) {
            //class嵌套
            auto t_begin0 = t;
            jump_brace(t, ts);
            extract_class_member(sub_class_name, t_begin0, t_begin0, t, ts, is_template);
            continue;
        } else if (st_brace.size() == 1 && t->type == CPP_TYPE && t_n->type == CPP_NAME &&
            ((t-1)->type == CPP_OPEN_BRACE || 
             (t-1)->type == CPP_CLOSE_BRACE ||
             (t-1)->type == CPP_COMMENT ||
             (t-1)->type == CPP_COLON ||
             (t-1)->type == CPP_SEMICOLON||
             (t-1)->val == "mutable" ||
             (t-1)->val == "static")) {
             
             //有可能是成员变量
             Token& t_type = *t;
             std::deque<Token*> to_be_m;
             t++;
             while(t->type == CPP_NAME || t->type == CPP_COMMA) {
                if (t->type == CPP_NAME) {
                    to_be_m.push_back(&(*t));
                }
                t++;
            }
        CHECK_MEMBER_VARIABLE_END:
            if (t->type == CPP_SEMICOLON) {
                //之前的都是成员变量
                for (auto it_to_be_m=to_be_m.begin(); it_to_be_m!=to_be_m.end(); ++it_to_be_m) {
                    (*it_to_be_m)->type = CPP_MEMBER_VARIABLE;
                    (*it_to_be_m)->ts.push_back(t_type);
                    class_variable.push_back({cur_c_name, (*it_to_be_m)->val, t_type});
                }
            } else {
                if (t->type == CPP_OPEN_SQUARE) {
                    //跳过数组(可以跳过多个)
                    ++t;
                    while(t->type != CPP_CLOSE_SQUARE) {
                        ++t;
                    }
                    ++t;
                    goto CHECK_MEMBER_VARIABLE_END;
                } else if (t->type == CPP_EQ) {
                    //带赋值构造的成员变量声明语句
                    //直接跳到 ;
                    while (t->type != CPP_SEMICOLON) {
                        ++t;
                    }
                    goto CHECK_MEMBER_VARIABLE_END;
                }

                //不是成员函数
                assert(false);
                continue;  
            }
        } else if (t->type == CPP_MEMBER_FUNCTION) {
            //成员函数
            if ((t-1)->type == CPP_TYPE) {
                auto it_fc = _g_class_fn.find({cur_c_name, false,false,"", Scope()});
                assert (it_fc != _g_class_fn.end());
                bool is_virtual = (t-2)->val == "virtual" || (t-3)->val == "virtual";
                for (auto it_fn = it_fc->second.begin(); it_fn != it_fc->second.end(); ++it_fn) {
                    if (it_fn->fn_name == t->val) {
                        it_fn->ret = *(t-1);
                        it_fn->is_virtual = is_virtual;
                        //这里不直接返回,是为了将重载的其他函数的ret也设置了
                        // ++t;
                        // continue;
                    }
                }
            } else {
                //构造函数 或者析构函数
                ++t;
            }
            ++t;
        } else {
            ++t;
        }

    }

    ++t;
}

void ParserGroup::extract_class2() {
    //1 抽取成员变量, 细化成员函数的返回值
    int idx = 0;
    for (auto it = _parsers.begin(); it != _parsers.end(); ++it) {
        Parser& parser = *(*it);
        std::deque<Token>& ts = parser._ts;
        std::string file_name = _file_name[idx++];
        std::cout << "extract class2: " << file_name << std::endl;
        std::string cur_c_name;
        for (auto t = ts.begin(); t != ts.end(); ) {
            if (t->type == CPP_CLASS || t->type == CPP_STURCT) {
                cur_c_name = t->val;
                ++t;
                continue;
            } else if (t->type == CPP_CLASS_BEGIN) {
                auto t_begin = t;
                jump_brace(t, ts);
                bool tm = false;
                assert(is_in_class_struct(cur_c_name, tm));
                extract_class_member(cur_c_name, t_begin, t_begin, t, ts, tm);
            } else {
                ++t;
            }
        }
    }

    //2 把所有成员函数的定义处 标记成 CPP_MEMBER_FUNCTION
    for (auto it = _parsers.begin(); it != _parsers.end(); ++it) {
        Parser& parser = *(*it);
        std::deque<Token>& ts = parser._ts;
        for (auto t = ts.begin(); t != ts.end(); ) {
            auto t_n = t+1;//type: class
            auto t_nn = t+2;//::
            auto t_nnn = t+3;//function
            if (t_n == ts.end() || t_nn == ts.end() || t_nnn == ts.end()) {
                ++t;
                continue;
            }

            if (t_n->type == CPP_TYPE && t_nn->type == CPP_SCOPE && (t_nnn->type == CPP_NAME || t_nnn->type == CPP_TYPE)) {
                bool tm=false;

                //LABEL 2 这里会把thread引用类成员函数作为入口函数,也标记成CPP_FUNCTION, 对接口混淆没有影响
                if (is_in_class_struct(t_n->val, tm)) {
                    auto it_fns = _g_class_fn.find({t_n->val, false, tm, ""});
                    assert(it_fns != _g_class_fn.end());
                    //先判断是不是静态函数的调用
                    auto t_c = t_nnn+1;
                    if(t_c->type == CPP_LESS) {
                        //如果是模板则跳过模板参数
                        jump_angle_brace(t_c, ts);
                        t_c+=1;
                    }

                    if (t_c->type == CPP_EQ || t_c->type == CPP_SEMICOLON) {
                        //类静态成员变量赋值
                        t+=3;
                        continue;
                    } else if(t_c->type == CPP_OPEN_PAREN) {
                        jump_paren(t_c,ts);
                        if (t_c->type != CPP_OPEN_BRACE) {
                            //这里是静态函数调用
                            t+= 3;
                            continue;
                        }
                    } else {
                        //其他场景
                        //如静态参数的调用，比如　B a(A::s_m); 这种
                        t+=3;
                        continue;
                    }

                    for (auto it_fn = it_fns->second.begin(); it_fn != it_fns->second.end(); ++it_fn) {
                        const ClassFunction& fn = *it_fn;
                        if (fn.fn_name == t_nnn->val) {
                            t_nnn->type = CPP_MEMBER_FUNCTION;
                            t_nnn->subject = t_n->val;
                            std::cout << "get class function definition: " << t_n->val << "::" << t_nnn->val << std::endl;
                        }
                    }
                } 
                t+= 3;
                continue;
            } else if (t_n->type == CPP_TYPE && t_nn->type == CPP_SCOPE && t_nnn->type == CPP_COMPL &&
                (t_nnn+1) != ts.end() && (t_nnn+1)->type == CPP_TYPE) {
                //析构函数
                bool tm=false;
                if (t_n->val == (t_nnn+1)->val && is_in_class_struct(t_n->val, tm)) {
                    t = t_nnn+1;
                    t->val = "~"+t->val;
                    t->type = CPP_MEMBER_FUNCTION;
                    --t;
                    t = ts.erase(t);
                }
            }

            ++t;
        }
    }

    //3 构造包含基类的成员函数
    //TODO 没有区分 access, 没有去重
    for (auto it = _g_class.begin(); it != _g_class.end(); ++it) {
        //在成员函数中可以调用该类 以及该类所有基类的方法
        std::vector<ClassFunction> fns = _g_class_fn.find(*it)->second;

        //把所有基类的方法提取出来
        auto it_bases = _g_class_bases.find(*it);
        assert(it_bases != _g_class_bases.end());
        
        for (auto it2 = it_bases->second.begin(); it2 != it_bases->second.end(); ++it2) {
            std::vector<ClassFunction> fns_base = _g_class_fn.find(*it2)->second;
            fns.insert(fns.end(), fns_base.begin(), fns_base.end());
        }

        _g_class_fn_with_base[*it] = fns;
    }
    
}

void ParserGroup::extract_global_var_fn() {
    //只在h文件中找
    int file_idx=0;
    for (auto it = _parsers.begin(); it != _parsers.end(); ++it) {
        Parser& parser = *(*it);
        std::string file_name = _file_name[file_idx++];
        std::cout << "extract global var fn : " << file_name << std::endl;
        bool is_h = false;
        if (file_name.size() > 2 && file_name.substr(file_name.size()-2, 2) == ".h") {
            is_h = true;
        } else if (file_name.size() > 4 && file_name.substr(file_name.size()-4, 4) == ".hpp") {
            is_h = true;
        } else {
            is_h = false;   
        }
        if (!is_h) {
            continue;
        }

        std::deque<Token>& ts = parser._ts;
        std::stack<Token> st_brace;//作用域最上层
        std::stack<Token> st_paren;//不在()内,跳过函数参数表
        //std::cout << "parse global variable from : " << it->first << std::endl;
        for (auto t = ts.begin(); t != ts.end(); ) {
            auto t_n = t+1;
            auto t_nn = t+2;
            if (t->type == CPP_OPEN_BRACE || t->type == CPP_CLASS_BEGIN) {
                st_brace.push(*t);
            } else if (t->type == CPP_CLOSE_BRACE || t->type == CPP_CLASS_END) {
            assert(st_brace.top().type == CPP_OPEN_BRACE || st_brace.top().type == CPP_CLASS_BEGIN);
                st_brace.pop();
            } if (t->type == CPP_OPEN_PAREN) {
                st_paren.push(*t);
            } else if (t->type == CPP_CLOSE_PAREN) {
                assert(st_paren.top().type == CPP_OPEN_PAREN);
                st_paren.pop();
            } else if(t->val == "template" && t_n->type == CPP_LESS) {
                //跳过模板参数
                std::stack<Token> st_angle;//不在<>内, 跳过模板
                ++t;
                st_angle.push(*t);
                ++t;
                while(!st_angle.empty()) {
                    if (t->type == CPP_GREATER) {
                        assert(st_angle.top().type == CPP_LESS);
                        st_angle.pop();
                    } else if (t->type == CPP_LESS) {
                        st_angle.push(*t);
                    }
                    ++t;
                }
                continue;

            } 
            //全局变量的提取
            else if (st_brace.empty() && st_paren.empty() && t->type == CPP_TYPE && t_n != ts.end() && t_nn != ts.end() &&
                t_n->type == CPP_NAME && t_nn->type != CPP_SCOPE &&//排除函数定义, 也是类型+name
                ((t) == ts.begin() ||
                (t-1)->type == CPP_OPEN_BRACE || //以 }结尾
                (t-1)->type == CPP_COMMENT || //以注释结尾
                (t-1)->type == CPP_MACRO || //以宏结尾
                (t-1)->type == CPP_SEMICOLON || //以;结尾,另开一头
                (t-1)->type == CPP_GREATER || //模板函数
                (t-1)->val == "inline" ||//函数修饰符
                (t-1)->val == "static" ||//函数修饰符
                (t-1)->val == "const") ) { //函数修饰符
                //有可能是全局变量  也有可能是类外的函数
                
                if (t_nn->type == CPP_OPEN_PAREN) {
                    //是类外的函数 
                    //如果是h文件中的,则作为全局函数, 如果是cpp中的则作为局部函数
                    t_n->type = CPP_FUNCTION;

                    Function fn;
                    fn.name = t_n->val;
                    fn.ret = *t;
                    _g_functions.insert(fn);

                    ++t;
                    continue;
                }

                //是全局变量
                Token& t_type = *t;
                std::deque<Token*> to_be_m;
                t++;
                while(t->type == CPP_NAME || t->type == CPP_COMMA) {
                    if (t->type == CPP_NAME) {
                        to_be_m.push_back(&(*t));
                    }
                    t++;
                }
        CHECK_GLOBAL_VARIABLE_END:
                //LABEL这里和成员变量不一样, 全局变量可以直接声明或者定义
                if (t->type == CPP_SEMICOLON || t->type == CPP_EQ )  {
                    //之前的都是全局变量, 
                    for (auto it_to_be_m=to_be_m.begin(); it_to_be_m!=to_be_m.end(); ++it_to_be_m) {
                        (*it_to_be_m)->type = CPP_GLOBAL_VARIABLE;
                        _g_variable[(*it_to_be_m)->val] = t_type;
                    }
                } else {
                    if (t->type == CPP_OPEN_SQUARE) {
                        //跳过数组(可以跳过多个)
                        ++t;
                        while(t->type != CPP_CLOSE_SQUARE) {
                            ++t;
                        }
                        ++t;
                        goto CHECK_GLOBAL_VARIABLE_END;
                    } else if (t->type == CPP_EQ) {
                        //带赋值构造的成员变量声明语句
                        //直接跳到 ;
                        while (t->type != CPP_SEMICOLON) {
                            ++t;
                        }
                        goto CHECK_GLOBAL_VARIABLE_END;
                    } else if (t->type == CPP_LESS) {
                        //模板参数如:
                        //template<typename T>
                        //int PQEleWrapper<T>::id_counter = 0;
                        
                        //跳过模板
                        jump_angle_brace(t, ts);
                        ++t;
                        goto CHECK_GLOBAL_VARIABLE_END;
                    } else if (t->type == CPP_SCOPE) {
                        //跳过
                        ++t;
                        continue;
                    }
                    assert(false);
                }
            } 
            //全局函数的提取, 寻找 fn(
            else if (st_brace.empty() && st_paren.empty() && t->type == CPP_NAME && 
                t_n != ts.end() && t_n->type == CPP_OPEN_PAREN) {
                //可能是函数
                //找前一步type
                auto t_p = t-1;
                bool get_fn_type = false;
                while(t_p != ts.begin()) {
                    if (t_p->type == CPP_TYPE) {
                        get_fn_type = true;
                        break;
                    } else if (t_p->type == CPP_MACRO) {
                        //TODO 展开宏
                        //LABEL 这里不展开宏,直接跳过
                        --t_p;
                        continue;
                    } else {
                        break;
                    }
                }
                if (get_fn_type) {
                    Function fn;
                    fn.name = t->val;
                    fn.ret = *t_p;
                    _g_functions.insert(fn);
                }
            }
            ++t;
        }
    }
}

void ParserGroup::extract_local_var_fn() {
    //局部函数是cpp文件中以static开头的函数, cpp中不写static的有可能是全局函数
    //只在cpp文件中找
    int file_idx=0;
    for (auto it = _parsers.begin(); it != _parsers.end(); ++it) {
        Parser& parser = *(*it);
        std::string file_name = _file_name[file_idx++];
        bool is_cpp = false;
        if (file_name.size() > 2 && file_name.substr(file_name.size()-4, 5) == ".cpp") {
            is_cpp = true;
        } else {
            is_cpp = false;   
        }
        if (!is_cpp) {
            continue;
        }

        _local_variable[file_name] = std::map<std::string, Token>();
        std::map<std::string, Token>& local_variable = _local_variable[file_name]; 
        _local_functions[file_name] = std::set<Function>();
        std::set<Function>& local_fn = _local_functions[file_name];

        std::deque<Token>& ts = parser._ts;
        std::stack<Token> st_brace;//作用域最上层
        std::stack<Token> st_paren;//不在()内,跳过函数参数表
        //std::cout << "parse global variable from : " << it->first << std::endl;
        bool in_annoy = false;
        for (auto t = ts.begin(); t != ts.end(); ) {
            auto t_n = t+1;
            auto t_nn = t+2;
            if (t->type == CPP_OPEN_BRACE || t->type == CPP_CLASS_BEGIN) {
                st_brace.push(*t);
            } else if (t->type == CPP_CLOSE_BRACE || t->type == CPP_CLASS_END) {
                assert(st_brace.top().type == CPP_OPEN_BRACE || st_brace.top().type == CPP_CLASS_BEGIN);
                st_brace.pop();
                if (in_annoy && st_brace.empty()) {
                    in_annoy = false;
                }
            } if (t->type == CPP_OPEN_PAREN) {
                st_paren.push(*t);
            } else if (t->type == CPP_CLOSE_PAREN) {
                assert(st_paren.top().type == CPP_OPEN_PAREN);
                st_paren.pop();
            } else if(t->val == "template" && t_n->type == CPP_LESS) {
                //跳过模板参数
                std::stack<Token> st_angle;//不在<>内, 跳过模板
                ++t;
                st_angle.push(*t);
                ++t;
                while(!st_angle.empty()) {
                    if (t->type == CPP_GREATER) {
                        assert(st_angle.top().type == CPP_LESS);
                        st_angle.pop();
                    } else if (t->type == CPP_LESS) {
                        st_angle.push(*t);
                    }
                    ++t;
                }
                continue;

            } 
            else if (st_brace.empty() && t->val == "namespace" && t_n != ts.end() && t_n->type == CPP_OPEN_BRACE) {
                //匿名函数
                st_brace.push(*t_n);
                in_annoy = true;
                ++t;
                ++t;    
                continue;
            }
            //全局变量的提取
            else if ((st_brace.empty() || in_annoy) && st_paren.empty() && t->type == CPP_TYPE && t_n != ts.end() && t_nn != ts.end() &&
                t_n->type == CPP_NAME && t_nn->type != CPP_SCOPE &&//排除函数定义, 也是类型+name
                ((t) == ts.begin() ||
                (t-1)->type == CPP_COMMENT || //以注释结尾
                (t-1)->type == CPP_MACRO || //以宏结尾
                (t-1)->type == CPP_SEMICOLON || //以;结尾,另开一头
                (t-1)->type == CPP_GREATER || //模板函数
                (t-1)->val == "inline" ||//函数修饰符
                (t-1)->val == "static" ||//函数修饰符
                (t-1)->val == "const" ||//函数修饰符
                (in_annoy && (t-1)->type==CPP_OPEN_BRACE)) ) { //namespace {后的第一个方程
                //有可能是全局变量  也有可能是类外的函数
                if (t_nn->type == CPP_OPEN_PAREN) {
                    //是类外的函数 
                    //如果是h文件中的,则作为全局函数, 如果是cpp中的则作为局部函数
                    t_n->type = CPP_FUNCTION;

                    Function fn;
                    fn.name = t_n->val;
                    fn.ret = *t;
                    local_fn.insert(fn);
                    
                    ++t;
                    continue;
                }

                //是全局变量
                Token& t_type = *t;
                std::deque<Token*> to_be_m;
                t++;
                while(t->type == CPP_NAME || t->type == CPP_COMMA) {
                    if (t->type == CPP_NAME) {
                        to_be_m.push_back(&(*t));
                    }
                    t++;
                }
        CHECK_GLOBAL_VARIABLE_END:
                //LABEL这里和成员变量不一样, 全局变量可以直接声明或者定义
                if (t->type == CPP_SEMICOLON || t->type == CPP_EQ )  {
                    //之前的都是全局变量, 
                    for (auto it_to_be_m=to_be_m.begin(); it_to_be_m!=to_be_m.end(); ++it_to_be_m) {
                        (*it_to_be_m)->type = CPP_GLOBAL_VARIABLE;
                        local_variable[(*it_to_be_m)->val] = t_type;
                    }
                } else {
                    if (t->type == CPP_OPEN_SQUARE) {
                        //跳过数组(可以跳过多个)
                        ++t;
                        while(t->type != CPP_CLOSE_SQUARE) {
                            ++t;
                        }
                        ++t;
                        goto CHECK_GLOBAL_VARIABLE_END;
                    }
                    assert(false);
                }
            } 
            //全局函数的提取, 寻找 fn(
            else if ((st_brace.empty()||in_annoy) && st_paren.empty() && t->type == CPP_NAME && 
                t_n != ts.end() && t_n->type == CPP_OPEN_PAREN) {
                //可能是函数
                //找前一步type
                auto t_p = t-1;
                bool get_fn_type = false;
                while(t_p != ts.begin()) {
                    if (t_p->type == CPP_TYPE) {
                        get_fn_type = true;
                        break;
                    } else if (t_p->type == CPP_MACRO) {
                        //TODO 展开宏
                        //LABEL 这里不展开宏,直接跳过
                        --t_p;
                        continue;
                    } else {
                        break;
                    }
                }
                if (get_fn_type) {
                    Function fn;
                    fn.name = t->val;
                    fn.ret = *t_p;
                    local_fn.insert(fn);
                }
            }
            ++t;
        }
    }
}

std::map<std::string, Token> ParserGroup::label_skip_paren(std::deque<Token>::iterator& t, const std::deque<Token>& ts) {
    std::stack<Token> sbrace;
    assert(t->type == CPP_OPEN_PAREN);
    sbrace.push(*t);
    ++t;
    std::map<std::string, Token> paras;
    while(!sbrace.empty()) {
        if (t->type == CPP_CLOSE_PAREN) {
            assert(sbrace.top().type == CPP_OPEN_PAREN);
            sbrace.pop();
            ++t;
        } else if (t->type == CPP_OPEN_PAREN) {
            sbrace.push(*t);
            ++t;
        } else if (t->type == CPP_TYPE &&
                  (t+1) != ts.end() && (t+1)->type == CPP_OPEN_BRACE &&
                  (t+2) != ts.end() && (t+2)->type == CPP_AND &&
                  (t+3) != ts.end() && (t+3)->type == CPP_NAME &&
                  (t+4) != ts.end() && (t+4)->type == CPP_CLOSE_PAREN &&
                  (t+5) != ts.end() && (t+5)->type == CPP_OPEN_SQUARE) {
            //这种参数表 type(&name)[3]
            paras[(t+3)->val] = *t;
            t += 6;
        } else if (t->type == CPP_TYPE && 
                  (t+1) != ts.end() && (t+1)->type == CPP_NAME) {
            //经典的参数表 type name, type name, 
            paras[(t+1)->val] = *t;
            t+=2;
        } else {
            ++t;
        }
    }
    if (t->val == "const") {
        ++t;
    }
    if (t->val == "throw" && (t+1)!=ts.end() && (t+2)!=ts.end() &&
       (t+1)->type == CPP_OPEN_PAREN && (t+2)->type == CPP_CLOSE_PAREN) {
        t+=3;
    }

    return paras;
}

Token ParserGroup::get_auto_type(
    std::deque<Token>::iterator t, 
    const std::deque<Token>::iterator t_start, 
    const std::string& class_name, 
    const std::string& file_name, 
    const std::map<std::string, Token>& paras,
    bool is_cpp,
    int case0) {
    assert(t->val == "auto");

    //找到赋值的终点
    auto t_end = t;
    while(t_end->type != CPP_SEMICOLON) {//TODO 如果域在构造函数列表中,则结束符号不是 ; 
        ++t_end;
    }
    //auto t_begin = t;
    t = t_end-1;

    if (0 == case0) {
        //auto a = ...;
        //t_begin += 3;
    } else {
        //auto a(...);
        //t_begin += 3;
        t--;
    }

    return get_subject_type(t, t_start, class_name, file_name, paras, is_cpp);
}

Token ParserGroup::recall_subjust_type(
    std::deque<Token>::iterator t, 
    const std::deque<Token>::iterator t_start, 
    const std::string& class_name, 
    const std::string& file_name, 
    const std::map<std::string, Token>& paras,
    bool is_cpp) {

    const std::string v_name = t->val;
    //类成员变量
    Token t_type;
    if (is_member_variable(class_name, v_name, t_type)) {
        return t_type;
    }
            
    //参数表
    auto it_p_v = paras.find(v_name);
    if (it_p_v != paras.end()) {
        return it_p_v->second;
    }

    //局部变量
    while (t <= t_start) {
        auto t_p = t-1;
        auto t_n = t+1;
        auto t_nn = t+2;
        if (t->val == v_name && t_n->type == CPP_EQ) {
            //可能是复制构造 如 type a =
            //过滤掉 type[*&]
            while((t_p->type == CPP_MULT || t_p->type == CPP_AND) && t_p<=t_start) {
                --t_p;
            }

            if (t_p->type == CPP_TYPE) {
                return *t_p;
            } else if (t_p->val == "auto") {
                //寻找赋值语句的右部
                std::cout << "get type auto.\n";
                return get_auto_type(t_p, t_start, class_name, file_name, paras, is_cpp, 1);
            } else {
                std::cout << "LABEL: " << "may be is 3th type: " << t_p->val << std::endl;
                Token tt;
                tt.type = CPP_OTHER;
                return tt;
            }
        } else if (t->val == v_name && t_n->type == CPP_OPEN_PAREN) {
            //可能是拷贝构造 如 type a(...)
            //过滤掉 type[*&]
            while((t_p->type == CPP_MULT || t_p->type == CPP_AND) && t_p<=t_start) {
                --t_p;
            }

            if (t_p->type == CPP_TYPE) {
                return *t_p;
            } else if (t_p->val == "auto") {
                //寻找赋值语句的右部
                std::cout << "get type auto.\n";
                return get_auto_type(t_p, t_start, class_name, file_name, paras, is_cpp, 2);
            } else {
                std::cout << "LABEL: " << "may be is 3th type: " << t_p->val << std::endl;
                Token tt;
                tt.type = CPP_OTHER;
                return tt;
            }
        } else if (t->val == v_name && t_n->type == CPP_SEMICOLON) {
            //没有默认参数的构造（warning） 如 type a;
            while((t_p->type == CPP_MULT || t_p->type == CPP_AND) && t_p<=t_start) {
                --t_p;
            }

            if (t_p->type == CPP_TYPE) {
                return *t_p;
            } else if (t_p->val == "auto") {
                //寻找赋值语句的右部
                std::cerr << "dont support auto it;\n";
                Token tt;
                tt.type = CPP_OTHER;
                return tt;
            } else {
                std::cout << "LABEL: " << "may be is 3th type: " << t_p->val << std::endl;
                Token tt;
                tt.type = CPP_OTHER;
                return tt;
            }
        } else if (t->val == "catch" && (t+1)->type == CPP_OPEN_PAREN) {
            //catch语句的特殊处理, 在catch的括号内部寻找可能的赋值语句
            std::stack<Token> t_c_p;
            t_c_p.push(*(t+1));
            auto t0 = t+2;
            while(!t_c_p.empty()) {
                if (t0->type == CPP_OPEN_PAREN) {
                    t_c_p.push(*t0);
                } else if (t0->type == CPP_CLOSE_PAREN) {
                    t_c_p.pop();
                } else if (t0->type == CPP_TYPE && (t0+1)->val == v_name) {
                    //变量是catch中的参数
                    return *t0;
                }
                ++t0;
            }
        }
        --t;
    }

    //全局变量
    if (is_global_variable(v_name, t_type)) {
        return t_type;
    }
            
    std::cerr << "reback to get type of: " << v_name << " failed.";
    //assert(false);
    return Token();
}

static void jump_before_square(std::deque<Token>::iterator& t, const std::deque<Token>::iterator t_start) {
    assert(t->type == CPP_CLOSE_SQUARE);
    while (t->type == CPP_OPEN_SQUARE && t>=t_start) {
        --t;
    }
}

static void jump_before_praen(std::deque<Token>::iterator& t, const std::deque<Token>::iterator t_start) {
    assert(t->type == CPP_CLOSE_PAREN);
    while (t->type == CPP_OPEN_PAREN && t>=t_start) {
        --t;
    }
}

Token ParserGroup::get_fn_ret_type(
    std::deque<Token>::iterator& t, 
    const std::deque<Token>::iterator t_start, 
    const std::string& class_name, 
    const std::string& file_name, 
    const std::map<std::string, Token>& paras, bool is_cpp) {
    
    const std::string fn_name = t->val;
    ///\ 1 查看是不是静态调用，如果是则返回false（之前已经将所有的类静态调用都设置成function了）
    if((t-1)->type == CPP_SCOPE) {
        std::cout << "static call with class: " << (t-2)->val << "\n";
        const std::string sc_name = (t-2)->val;
        Token ret;
        if (is_member_function(sc_name, fn_name, ret)) {
            return ret;
        } else {
            //3th 模块的静态调用
            Token t_o;
            t_o.type = CPP_OTHER;
            return t_o;
        }
    }

    ///\ 2 查看是否没有主语
    if ((t-1)->type != CPP_POINTER && (t-1)->type != CPP_DOT) {
        ///\3 没有主语则匹配 全局函数 / 函数成员函数 / 或者局部函数(需要是cpp文件)

        //1.1 匹配全局函数
        Token ret;
        if (is_global_function(fn_name, ret)) {
            std::cout << "is global fn\n";
            return ret;
        }

        //1.2 匹配成员函数
        //把该类以及基类的方法拿出来查看
        if (!class_name.empty() && is_member_function(class_name, fn_name, ret)) {
            return ret;
        }

        //1.3 如果是cpp则匹配局部函数
        if (is_cpp && is_global_function(fn_name, ret)) {
            return ret;
        }

        ret.type = CPP_OTHER;
        return ret;

    } else {
        --t;
        --t;
        //继续寻找主语类型

        Token tt = get_subject_type(t, t, class_name, file_name, paras, is_cpp);
        if (tt.type == CPP_OTHER) {
            return tt;
        }

        Token ret;
        if (is_member_function(tt.val, fn_name, ret)) {
            return ret;
        }

        //看是否是迭代器或者容器
        if (ret.val == "iterator" || ret.val == "const_iterator") {
            //可能不会是键值对类型
            assert(!tt.ts.empty());
            assert(!tt.ts[0].ts.empty());
            if (is_member_function(tt.ts[0].ts[0].val, fn_name, ret)) {
                return ret;
            }
        } else if (is_stl_container(ret.val)) {
            //是容器
            std::cout << "contaier " << tt.val << " 's function: " << fn_name << " called \n";
            if (is_stl_container_ret_iterator(fn_name)) {
                Token ret0;
                ret0.type = CPP_TYPE;
                ret0.val = "iterator";
                ret0.ts.push_back(ret);
                return ret0;
            } else if (is_stl_container_ret_val(fn_name)) {
                assert(!ret.ts.empty());
                return ret.ts[0];
            } else {
                Token t_o;
                t_o.type == CPP_OTHER;
                return t_o;
            }
        } else {
            //TODO 无法分析
            Token t_o;
            t_o.type == CPP_OTHER;
            return t_o;
        }
    }
}

Token ParserGroup::get_subject_type(
    std::deque<Token>::iterator& t, 
    const std::deque<Token>::iterator t_start, 
    const std::string& class_name, 
    const std::string& file_name, 
    const std::map<std::string, Token>& paras,
    bool is_cpp) {
    
    //主语类型 函数 或者 变量
    auto t_p = t-1;
    if (t->val == "first" || t->val == "second" || t->type == CPP_NAME
        &&(t_p->type == CPP_DOT || t_p->type == CPP_POINTER)) {
        auto t_r = t-2;
        Token tt = get_subject_type(t_r, t_start, class_name, file_name, paras, is_cpp);
        if (tt.type == CPP_OTHER) {
            return tt;
        }

        if ((tt.val == "pair" || tt.val == "map") && (t->val == "first" || t->val == "second")) {
            //键值对容器
            assert(tt.ts.size() ==2);
            if (t->val == "first") {
                return tt.ts[0];
            } else {
                return tt.ts[1];
            }
        } else if (tt.val == "iterator" || tt.val == "const_iterator") {
            //迭代器
            assert(!tt.ts.empty());
            assert(tt.ts[0].ts.size() ==2);
            if (t->val == "first") {
                return tt.ts[0].ts[0];
            } else {
                return tt.ts[0].ts[1];
            }
        } else {
            //t->val 是 tt的成员
            bool tm=false;
            if (is_in_class_struct(tt.val, tm)) {
            } else {
                Token t_o;
                t_o.type = CPP_OTHER;
                return t_o;
            }            
        }
    } else if (t->type == CPP_CLOSE_SQUARE) {
        //数组
        //跳过找到CPP_OPEN_SQUARE
        auto t_r = t;
        jump_before_square(t_r, t_start);
        assert(t_r->type == CPP_OPEN_SQUARE);
        --t_r;
        Token tt = get_subject_type(t_r, t_start, class_name, file_name, paras, is_cpp);
        if (tt.type == CPP_OTHER) {
            return tt;
        }

        if (tt.val == "vector") {
            //重载[]的容器
            return tt.ts[0];
        } else if (tt.val == "map") {
            //重载[]的容器
            return tt.ts[1];
        } else if (tt.type == CPP_TYPE) {
            //数组
            return tt;
        } else {
            assert(false);
            //return tt;
        }

    } else if (t->type == CPP_CLOSE_PAREN) {
        //可能用括号隔离了一个类型 或者 是另一个函数调用
        auto t_r = t;
        jump_before_praen(t_r, t_start);
        assert(t_r->type == CPP_OPEN_PAREN);
        auto t_p = t_r -1;
        if (t_p->type == CPP_NAME) {
            //函数调用
            Token tt = get_fn_ret_type(t_p, t_start, class_name, file_name, paras, is_cpp);
            return tt;
        } 
        //else if (t_p->type == CPP_MULT){}//TODO 解引用要分析吗?
        else {
            //存萃的类型，则分析括号内部的内容
            t_r = t-1;
            Token tt = get_subject_type(t_r, t_start, class_name, file_name, paras, is_cpp);
            return tt;
        }
    } else if (t->type == CPP_NAME) {
        //递归的终点
        Token tt = recall_subjust_type(t, t_start, class_name, file_name, paras, is_cpp);
        return tt;
    } else {
        assert(false);
    }

}
// //回溯寻找type是否在分析的代码模块中
bool ParserGroup::is_call_in_module(std::deque<Token>::iterator& t, 
    const std::deque<Token>::iterator t_start, 
    const std::string& class_name, 
    const std::string& file_name, 
    const std::map<std::string, Token>& paras,
    bool is_cpp, 
    Token& fn_subject_type) {

    fn_subject_type.type = CPP_OTHER;

    const std::string fn_name = t->val;
    ///\ 1 查看是不是静态调用，如果是则返回false（之前已经将所有的类静态调用都设置成function了）
    if((t-1)->type == CPP_SCOPE) {
        std::cout << "static call with class: " << (t-2)->val << " not in module.\n";
        return false;
    }
            
    ///\ 2 查看是否没有主语
    if ((t-1)->type != CPP_POINTER && (t-1)->type != CPP_DOT) {
        ///\3 没有主语则匹配 全局函数 / 函数成员函数 / 或者局部函数(需要是cpp文件)

        //1.1 匹配全局函数
        Token ret;
        if (is_global_function(fn_name, ret)) {
            std::cout << "is global fn\n";
            return true;
        }

        if (class_name.empty()) {
            std::cerr << "class name is null if it's not global fn\n";
            return false;
        }

        //1.2 匹配成员函数
        //把该类以及基类的方法拿出来查看
        if(is_member_function(class_name, fn_name, ret)) {
            std::cout << "is member fn\n";
            return true;
        } 

        //1.3 如果是cpp则匹配局部函数
        if (is_cpp && is_global_function(fn_name, ret)) {
            std::cout << "is local fn\n";
            return true;
        }

        //LABEL 其他
        //+ 构造语句也会到这里 如 type a(new a);
        //+ 第三方调用

        std::cerr << "fn dismatch, may 3th fn, or new construct\n";
        return false;
    } else {
        --t;
        --t;

        std::cout << "begin to find subject's type\n";

        Token t_type = get_subject_type(t, t, class_name, file_name, paras, is_cpp);
        std::cout  << class_name << "::" << fn_name << " called by " << t->val << " 's type: ";
        print_token(t_type, std::cout);
        std::cout << std::endl;

        if (t_type.type != CPP_TYPE) {
            std::cout << "get invalid subject type.";
            return false;
        }

        bool tm = false;
        if (is_in_class_struct(t_type.val, tm)) {
            fn_subject_type = t_type;
            return true;
        } else {
            return false;
        }
    }
}

void ParserGroup::label_call_in_fn(std::deque<Token>::iterator t, 
    const std::deque<Token>::iterator t_start,
    const std::deque<Token>::iterator t_end, 
    const std::string& class_name, 
    const std::string& file_name, 
    const std::map<std::string, Token>& paras,
    bool is_cpp) {
    //过程调用的语法为 [主语[->/.]]function(paras)
    assert(t->type == CPP_OPEN_BRACE);

    while(t<=t_end) {
        auto t_n = t+1;
        if (t->type == CPP_NAME && t_n <= t_end && t_n->type == CPP_OPEN_PAREN) {
            std::cout << "may call: " << t->val << std::endl;
            Token subject_t;
            if (is_call_in_module(t, t_start, class_name, file_name, paras, is_cpp, subject_t)) {
                t->type = CPP_CALL;
                t+=2;
            } else {
                ++t;
            }
        } else {
            ++t;
        }
    }
}

void ParserGroup::label_call()  {
    //1 先找到调用域(必须是函数域); TODO 其他域比如: 初始化列表中的调用, 全局/静态 变量的构造处调用等
    //2 在函数域中寻找过程调用
    //3 找主语
    //   3.1 如果可以直接找到主语 a->fn(), 怎从全局变量 成员变量 参数表中寻找 type
    //   3.2 如果是嵌套调用 fn1()->fn(), 则递归向前找fn1的返回值类型
    //   3.3 如果找不到主语 fn(), 则判断是不是成员函数 或者 全局函数
    //4 确定主语是混淆类中的元素, 则标记成call

    int file_idx=0;
    for (auto it = _parsers.begin(); it != _parsers.end(); ++it) {
        Parser& parser = *(*it);
        const std::string file_name = _file_name[file_idx++];
        bool is_cpp = false;
        if (file_name.size() > 2 && file_name.substr(file_name.size()-4, 5) == ".cpp") {
            is_cpp = true;
        } 

        std::cout << "label file: " << file_name << std::endl;
        std::deque<Token>& ts = parser._ts;        

        for (auto t = ts.begin(); t != ts.end(); ) {
            auto it_n = t+1;
            if (it_n == ts.end()) {
                ++t;
                continue;
            }

            if (t->type == CPP_FUNCTION && it_n->type == CPP_OPEN_PAREN) {                
                std::string class_name = "";

                //寻找()后有没有 {
                const std::string fn_name = t->val;
                ++t;
                std::cout << "label call in fn: " << fn_name << std::endl;

                std::map<std::string, Token> paras = label_skip_paren(t,ts);
                if (t->type != CPP_OPEN_BRACE) {
                    //全局函数声明
                    std::cout << "global function decalartion: " << fn_name << std::endl;
                    ++t;
                    continue;
                }
                //这个{就是回溯的终点限制
                std::cout << "label fn " << fn_name << std::endl;
                auto t_begin = t;
                jump_brace(t, ts);
                label_call_in_fn(t_begin, t_begin, t, class_name, file_name, paras, is_cpp);
                
            } else if (t->type == CPP_MEMBER_FUNCTION && it_n->type == CPP_OPEN_PAREN) {
                //寻找()后有没有 {
                const std::string fn_name = t->val;
                const std::string class_name = t->subject;
                ++t;

                std::cout << "label call in class fn: " << fn_name << std::endl;

                std::map<std::string, Token> paras = label_skip_paren(t,ts);
                if (t->type != CPP_OPEN_BRACE) {
                    //类成员函数声明
                    std::cout << "member function decalartion: " << fn_name << std::endl;
                    ++t;
                    continue;
                }

                //这个{就是回溯的终点限制
                std::cout << "label " << class_name << "::" << fn_name << std::endl;
                auto t_begin = t;
                jump_brace(t, ts);
                label_call_in_fn(t_begin, t_begin, t, class_name, file_name, paras, is_cpp);

            } else {
                ++t;
            }
        }
    }
}

static inline void print_token(const Token& t, std::ostream& out) {
    out << t.val << " ";
    for (auto it2 = t.ts.begin(); it2 != t.ts.end(); ++it2) {
        print_token(*it2, out);
    }
}

void ParserGroup::debug(const std::string& debug_out) {
    size_t i=0;
    for (auto it = _parsers.begin(); it != _parsers.end(); ++it) {
        Parser& parser = *(*it);
        const std::string& file_name = _file_name[i++];
        const std::string f = debug_out+"/"+file_name+".l1";
        std::cout << "print file token: " << file_name << std::endl;
        std::ofstream out(f, std::ios::out);
        if (!out.is_open()) {
            std::cerr << "err to open: " << f << "\n";
            continue;
        }

        const std::deque<Token>& ts = parser.get_tokens();
        for (auto it2=ts.begin(); it2!=ts.end(); ++it2) {
            const Token& t = *it2;
            out << t.type << ": ";

            print_token(t, out);
            out << std::endl;
        }

        out.close();
    }

    //print all marco
    {
        const std::string f = debug_out+"/global_marco";
        std::ofstream out(f, std::ios::out);
        if (!out.is_open()) {
            std::cerr << "err to open: " << f << "\n";
            return;
        }
        for (auto it = _g_marco.begin(); it != _g_marco.end(); ++it) { 
            out << (*it).val << ": ";
            for (auto it2 = (*it).ts.begin(); it2 != (*it).ts.end(); ++it2) {
            out << (*it2).val << " "; 
            }
            out << std::endl;
        }
        out.close();
    }

    //print all class and struct
    {
        const std::string f = debug_out+"/global_class";
        std::ofstream out(f, std::ios::out);
        if (!out.is_open()) {
            std::cerr << "err to open: " << f << "\n";
            return;
        }
        for (auto it = _g_class.begin(); it != _g_class.end(); ++it) { 
            if (it->is_struct) {
                out << "struct ";
            } else {
                out << "class ";
            }
            if (it->is_template) {
                out << "<> ";    
            }
        
            out << it->scope.key << "::" << it->name;
            if (!it->father.empty()) {
                out << " public : " << it->father;
            }
            out << "\t";

            //print child 
            out << "child: ";
            auto it_c = _g_class_childs.find(*it);
            if (it_c != _g_class_childs.end()) {
                for (auto it_cc = it_c->second.begin(); it_cc != it_c->second.end(); ++it_cc) {
                    out << it_cc->scope.key << "::" <<it_cc->name << " ";
                }
            }

            //print base
            out << "base: ";
            auto it_b = _g_class_bases.find(*it);
            if (it_b != _g_class_bases.end()) {
                for (auto it_bc = it_b->second.begin(); it_bc != it_b->second.end(); ++it_bc) {
                    out << it_bc->scope.key << "::" <<it_bc->name << " ";
                }
            }

            out << "\n";
        }

        out.close();
    }

    //print all class member function
    {
        const std::string f = debug_out+"/global_class_function";
        std::ofstream out(f, std::ios::out);
        if (!out.is_open()) {
            std::cerr << "err to open: " << f << "\n";
            return;
        }
        for (auto it0 = _g_class_fn.begin(); it0 != _g_class_fn.end(); ++it0) {
            for (auto it = it0->second.begin(); it != it0->second.end(); ++it) {
                const ClassFunction& cf = *it;
                out << cf.c_name << "::" << cf.fn_name << "\t";
                if (cf.access == 0) {
                    out << "public"; 
                } else if (cf.access == 1) {
                    out << "protected"; 
                } else {
                    out << "private"; 
                }

                out << "\t ret: ";
                // for (auto it2 = it->rets.begin(); it2 != it->rets.end(); ++it2) {
                //    out << it2->val << " "; 
                // }
                print_token(cf.ret, out);

                out << std::endl;
            }
        }
        out.close();
    }

    //print all class member variable
    {
        const std::string f = debug_out+"/global_class_variable";
        std::ofstream out(f, std::ios::out);
        if (!out.is_open()) {
            std::cerr << "err to open: " << f << "\n";
            return;
        }
        for (auto it0 = _g_class_variable.begin(); it0 != _g_class_variable.end(); ++it0) {
            for (auto it = it0->second.begin(); it != it0->second.end(); ++it) {
                const ClassVariable& cf = *it;
                out << cf.c_name << "::" << cf.m_name << "\ttype: ";
                print_token(cf.type, out);
                out << std::endl;
            }
        }
        out.close();
    }

    //print all enum
    {
        const std::string f = debug_out+"/global_enum";
        std::ofstream out(f, std::ios::out);
        if (!out.is_open()) {
            std::cerr << "err to open: " << f << "\n";
            return;
        }
        for (auto it = _g_enum.begin(); it != _g_enum.end(); ++it) {
            out << *it << "\n";
        }
        out.close();
    }

    //print all typedef 
    {
        const std::string f = debug_out+"/global_typedef";
        std::ofstream out(f, std::ios::out);
        if (!out.is_open()) {
            std::cerr << "err to open: " << f << "\n";
            return;
        }
        for (auto it = _typedef_map.begin(); it != _typedef_map.end(); ++it) {
            Token& t = it->second;
            out << "typedef " << it->first << "\t";
            for (auto it2 = t.ts.begin(); it2 != t.ts.end(); ++it2) {
                out << it2->val << " ";
                for (auto it3 = it2->ts.begin(); it3 != it2->ts.end(); ++it3) {
                    out << it3->val << " ";
                }
            }
            out << std::endl;
        }
        out.close();
    }

    //print global variable
    {
        const std::string f = debug_out+"/global_variable";
        std::ofstream out(f, std::ios::out);
        if (!out.is_open()) {
            std::cerr << "err to open: " << f << "\n";
            return;
        }
        for (auto it = _g_variable.begin(); it != _g_variable.end(); ++it) {
            out << it->first << " type: ";
            print_token(it->second, out);
            out << std::endl;
        }
        out.close();
    }

    //print global function
    {
        const std::string f = debug_out+"/global_functions";
        std::ofstream out(f, std::ios::out);
        if (!out.is_open()) {
            std::cerr << "err to open: " << f << "\n";
            return;
        }
        for (auto it_f = _g_functions.begin(); it_f != _g_functions.end(); ++it_f) {
            out << it_f->name << " ret: ";
            print_token(it_f->ret, out);
            out << std::endl;
        }
        out.close();
    }

    //print local variable
    {
        const std::string f = debug_out+"/local_variable";
        std::ofstream out(f, std::ios::out);
        if (!out.is_open()) {
            std::cerr << "err to open: " << f << "\n";
            return;
        }
        for (auto it = _local_variable.begin(); it != _local_variable.end(); ++it) {
            if (!it->second.empty()) {
                out << "file: " << it->first << " local variables: ";
                out << "\n";
            }
            for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
                out << "\t" << it2->first << " type: ";
                print_token(it2->second, out);
                out << " \n"; 
            }
        }
        out.close();
    }

    //print local function
    {
        const std::string f = debug_out+"/local_function";
        std::ofstream out(f, std::ios::out);
        if (!out.is_open()) {
            std::cerr << "err to open: " << f << "\n";
            return;
        }
        for (auto it = _local_functions.begin(); it != _local_functions.end(); ++it) {
            if (!it->second.empty()) {
                out << "file: " << it->first << " local function: ";
                out << "\n";
            }
            for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
                out << "\t" << (*it2).name << " ret: ";
                print_token((*it2).ret, out);
                out << " \n"; 
            }
        }
        out.close();
    }
}

bool ParserGroup::is_in_marco(const std::string& m) {
    for (auto it=_g_marco.begin(); it != _g_marco.end(); ++it) {
        if (!(*it).ts.empty()) {
            if((*it).ts.front().val == m) {
                return true;
            }
        }
    }

    return false;
}

bool ParserGroup::is_in_marco(const std::string& m, Token& val) {
    for (auto it=_g_marco.begin(); it != _g_marco.end(); ++it) {
        if (!(*it).ts.empty()) {
            if((*it).ts.front().val == m) {
                val = *it;
                return true;
            }
        }
    }

    return false;
}

bool ParserGroup::is_in_class_struct(const std::string& name, bool& tm) {
    for (auto it=_g_class.begin(); it != _g_class.end(); ++it) {
        if (it->name == name) {
            tm = it->is_template;
            return true;
        }
    }

    return false;
}

bool ParserGroup::is_in_typedef(const std::string& name) {
    return _typedef_map.find(name) != _typedef_map.end();
}

bool ParserGroup::is_in_typedef(const std::string& name, Token& t_type) {
    auto it = _typedef_map.find(name);
    if (it != _typedef_map.end()) {
        t_type = it->second;
        return true;
    }

    return false;
}

bool ParserGroup::is_member_function(const std::string& c_name, const std::string& fn_name) {
    auto it_fn = _g_class_fn_with_base.find({c_name, false,false, ""});
    if (it_fn != _g_class_fn_with_base.end()) {
        std::vector<ClassFunction> fns = it_fn->second;
        for (auto it = fns.begin(); it != fns.end(); ++it) {
            if (it->fn_name == fn_name){
                return true;
            }
        }
    }

    return false;
}

bool ParserGroup::is_member_function(const std::string& c_name, const std::string& fn_name, Token& ret) {
    auto it_fns = _g_class_fn_with_base.find({c_name, false,false, ""});
    if (it_fns == _g_class_fn_with_base.end()) {
        return false;
    }
    std::vector<ClassFunction> fns = it_fns->second;
    for (auto it = fns.begin(); it != fns.end(); ++it) {
        if (it->fn_name == fn_name){
            ret = it->ret;
            return true;
        }
    }

    return false;
}

bool ParserGroup::is_member_variable(const std::string& c_name, const std::string& c_v_name, Token& t_type) {
    auto it_c_vs = _g_class_variable.find({c_name,false,false,""});
    if (it_c_vs != _g_class_variable.end()) {
        for (auto it_v = it_c_vs->second.begin(); it_v != it_c_vs->second.end(); ++it_v) {
            if (it_v->m_name == c_v_name) {
                t_type = it_v->type;
                return true;
            }
        }
    }

    return false;
}

std::map<std::string, Token> ParserGroup::get_template_class_type_paras(std::string c_name) {
    auto it_c = _g_class.find({c_name,false,false,""});
    if (it_c != _g_class.end()) {
        return it_c->tm_paras;
    } else {
        return std::map<std::string, Token>();
    }
}

bool ParserGroup::is_global_variable(const std::string& v_name, Token& t_type) {
    auto it_v = _g_variable.find(v_name);
    if (it_v != _g_variable.end()) {
        t_type = it_v->second;
        return true;
    }

    return false;
}

bool ParserGroup::is_local_variable(const std::string& file_name, const std::string& v_name, Token& ret_type) {
    auto it_vs = _local_variable.find(file_name);
    if(it_vs != _local_variable.end()) {
        auto it_v = it_vs->second.find(v_name);
        if (it_v != it_vs->second.end()) {
            ret_type = it_v->second;
            return true;
        }
    }

    return false;
}

bool ParserGroup::is_local_function(const std::string& file_name, const std::string& fn_name, Token& ret_type) {
    auto it_fns = _local_functions.find(file_name);
    if(it_fns != _local_functions.end()) {
        auto it_fn = it_fns->second.find({fn_name, Token()});
        if (it_fn != it_fns->second.end()) {
            ret_type = it_fn->ret;
            return true;
        }
    }

    return false;
}

bool ParserGroup::is_global_function(const std::string& fn_name, Token& ret) {
    auto it_fn = _g_functions.find({fn_name, Token()});
    if (it_fn != _g_functions.end()) {
        ret = it_fn->ret;
        return true;
    }

    return false;
}

bool ParserGroup::is_stl_container(const std::string& name) {
    static const char* stl_containers[] = {
    "vector",
    "deque",
    "queue",
    "stack",
    "list",
    "set",
    "map",
    "pair",
    "auto_ptr",
    "shared_ptr",
    "weak_ptr",
    "unique_ptr",
    };

    for (int i=0; i<sizeof(stl_containers)/sizeof(char*); ++i) {
        if (name == stl_containers[i]) {
            return true;
        }
    }

    return false;
}

bool ParserGroup::is_stl_container_ret_iterator(const std::string& name) {
    static const char* fn[] = {
        "begin",
        "end",
        "rbegin",
        "rend",
        "cbegin",
        "cend",
        "crbegin",
        "crend",
        "find",
        "lower_bound",
        "upper_bound",
        "insert",
        "erase",
    };
    for (int i=0; i<sizeof(fn)/sizeof(char*); ++i) {
        if (name == fn[i]) {
            return true;
        }
    }

    return false;
}

bool ParserGroup::is_stl_container_ret_val(const std::string& name) {
    static const char* fn[] = {
        "front",
        "back",
        "top",
        "at",
        "data",
        "get",
        "crbegin",
        "crend",
    };
    for (int i=0; i<sizeof(fn)/sizeof(char*); ++i) {
        if (name == fn[i]) {
            return true;
        }
    }

    return false;
}