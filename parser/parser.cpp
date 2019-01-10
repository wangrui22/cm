#include "parser.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>
#include <stack>
#include <functional>

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

Parser::Parser() {

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
            //TODO 运算符右移 和 模板有冲突
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
        if (t->type == CPP_NAME && t->val == "size_t") {
            t->type = CPP_TYPE;
            ++t;
            continue;
        }
        if (t->type == CPP_KEYWORD && is_type(t->val)) {
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

void ParserGroup::add_parser(const std::string& file_name, Parser* parser) {
    _parsers[file_name] = parser;
}

Parser* ParserGroup::get_parser(const std::string& file_name) {
    auto it = _parsers.find(file_name);
    if (it != _parsers.end()) {
        return it->second;
    } else {
        return nullptr;
    }
}

void ParserGroup::extract_marco() {
    //l1 找纯粹的 define
    for (auto it = _parsers.begin(); it != _parsers.end(); ++it) {
        Parser& parser = *it->second;
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
        Parser& parser = *it->second;
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

    //l3 把所有的name为marco的token type改成 marco
    for (auto it = _parsers.begin(); it != _parsers.end(); ++it) {
        Parser& parser = *it->second;
        std::deque<Token>& ts = parser._ts;
        for (auto t = ts.begin(); t != ts.end(); ++t) {
            if (t->type == CPP_NAME && is_in_marco(t->val)) {
                t->type = CPP_MACRO;
            }
        }
    }
}

// void ParserGroup::extract_tempalte() {
//     //把 template尖括号中class 和 typename 后面跟的token type设置成 CPP_TYPE
//     for (auto it = _parsers.begin(); it != _parsers.end(); ++it) {
//         Parser& parser = *it->second;
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
        Parser& parser = *it->second;
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
        Parser& parser = *it->second;
        std::deque<Token>& ts = parser._ts;
        for (auto t = ts.begin(); t != ts.end(); ++t) {
            if (_g_enum.find(t->val) != _g_enum.end()) {
                t->type = CPP_TYPE;
            }
        }
    }
}

void ParserGroup::extract_class() {
    for (auto it = _parsers.begin(); it != _parsers.end(); ++it) {
        Parser& parser = *it->second;
        std::deque<Token>& ts = parser._ts;
        bool next_class_template = false;
        for (auto t = ts.begin(); t != ts.end(); ) {
            //找到了模板, 忽略其中所有的class
            if (t->val == "template") {
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
            } else if (t->val == "class" || t->val == "struct") {
                //TODO 看除了template后的第一个关键字是否是class或者struct从而判断是不是模板类
                //排除类前面跟着的修饰符： 一般是宏定义 或者 是关键字
                auto t_n = t+1;

                //略过类中类
                auto t_nn = t+2;
                if (t_nn == ts.end()) {
                    continue;
                } else if (t_nn->type == CPP_SCOPE) {
                    ++t;
                    continue;
                }

                std::string cur_c_name;
                const bool is_struct = t->val == "class" ? false : true;
                const TokenType cur_type = t->val == "class" ? CPP_CLASS : CPP_STURCT;
                const TokenType cur_begin_type = t->val == "class" ? CPP_CLASS_BEGIN : CPP_STRUCT_BEGIN;
                const TokenType cur_end_type = t->val == "class" ? CPP_CLASS_END : CPP_STRUCT_END;
                const int default_access = t->val == "class" ? 2 : 0;

                while (true) {
                    if (t_n != ts.end() && t_n->type == CPP_NAME) {
                        t_n->type = cur_type;
                        cur_c_name = t_n->val;
                        if (_g_class.find({t_n->val, is_struct, next_class_template, ""}) == _g_class.end()) {
                            _g_class.insert({t_n->val, is_struct, next_class_template, ""});
                        }
                        next_class_template = false;
                        break;
                    } else {
                        ++t;
                        t_n = t+1;
                    }
                }
                //对class进行分析
                //继承关系 { 或者 ; 之前有没有 : 
                //成员函数 TODO这一步是不是在类型判断后再做
CLASS_SCOPE:
                while(!(t->type == CPP_OPEN_BRACE || t->type == CPP_SEMICOLON || t->type == CPP_COLON) && t!=ts.end()) {
                    ++t;
                }
                if (t == ts.end()) {
                    continue;
                }

                if (t->type == CPP_COLON && (t+1)!=ts.end() && (t+1)->val == "public") {
                    //找到继承的关系
                    if ((t+2)!=ts.end() && (t+3)!=ts.end() && (t+3)!=ts.end() &&
                        (t+2)->val == "std" && (t+3)->type == CPP_SCOPE && (t+4)->val == "enable_shared_from_this") {
                        //TODO 这里不处理直接继承enable_shared_from_this的情况
                        //std::cout << "catch enable_shared_from_this\n";
                        ++t;
                        goto CLASS_SCOPE;
                    } else if ((t+2)!=ts.end() && ((t+2)->type == CPP_NAME || (t+2)->type == CPP_CLASS || (t+2)->type == CPP_STURCT)) {
                        std::set<ClassType>::iterator t_c = _g_class.find({cur_c_name, is_struct, false, ""});
                        if (t_c != _g_class.end()) {
                            _g_class.erase(t_c);
                        }
                        _g_class.insert({t_n->val, is_struct, next_class_template, (t+2)->val});
                        //std::cout << "catch father class\n";
                        ++t;
                        goto CLASS_SCOPE;
                    }  
                                    
                } else if(t->type == CPP_SEMICOLON) {
                    std::cout << "class declaration\n";
                    continue;
                } else if(t->type == CPP_OPEN_BRACE) {
                    //提取类成员函数
                    std::cout << "begin extract class function: " << cur_c_name << std::endl;
                    //默认是class 是 private, struct 是 public
                    int access = default_access;
                    //找class定义的作用域
                    std::stack<Token> st;
                    st.push(*t);
                    t->type = cur_begin_type;
                    ++t;

                    while(!st.empty() && t != ts.end()) {

                        //---------------------------------------------//
                        //common function
                        std::function<void()> jump_paren = [&t, &ts]() {
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

                        std::function<std::deque<Token>(std::deque<Token>::iterator)> recall_fn_ret = [&ts](std::deque<Token>::iterator it_cf) {
                            //找上一个 ; } comments 
                            //TODO 如果是模板函数会带tempalte<[*]>
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


                        // if (it->first =="mi_ray_caster.h" && t->val == "set_mask_label_level") {
                        //     std::cout << "got it\n";
                        // }
                        if (t->type == CPP_CLOSE_BRACE && (st.top().type == CPP_OPEN_BRACE ||st.top().type == cur_begin_type )) {
                            st.pop();
                            ++t;
                            continue;
                        } else if (t->type == CPP_OPEN_BRACE) {
                            st.push(*t);
                            ++t;
                            continue;
                        } else if (t->val == "public") {
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
                        } else if (st.size() == 1) {//!!这里仅仅分析class的定义的第一层作用域的内容
                            /// \ 分析构造函数 

                            //找括号组
                            //需要避开构造函数的初始化列表
                            auto t_n = t+1;
                            if (t->val == cur_c_name && (t-1)->type == CPP_COMPL) {
                                //析构函数
                                t->type = CPP_FUNCTION;
                                _g_class_fn.insert({access, cur_c_name, "~"+t->val, std::deque<Token>()});

                                //跳过函数的括号组
                                ++t;
                                jump_paren();

                                continue;
                            } else if (t->val == cur_c_name && t_n != ts.end() && t_n->type == CPP_OPEN_PAREN) {
                                //可能是构造函数
                                if ((t-1)->val == "explicit") {
                                    //肯定是构造函数
                                    t->type = CPP_FUNCTION;
                                    _g_class_fn.insert({access, cur_c_name, t->val, std::deque<Token>()});

                                    ++t;
                                    jump_paren();
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
                                    jump_paren();
                                    if (t->type == CPP_COLON) {
                                        //初始化列表 是构造函数 跳过直至第一个 {
                                        // if (cur_c_name == "FPS") {
                                        //     std::cout << "got it\n";
                                        // }

                                        t_may_c->type = CPP_FUNCTION;
                                        _g_class_fn.insert({access, cur_c_name, t_may_c->val, std::deque<Token>()});
                                        
                                        ++t;
                                        while(t->type != CPP_OPEN_BRACE && t!=ts.end()) {
                                            ++t;
                                        }
                                    } else if (t->type == CPP_OPEN_BRACE) {
                                        //构造函数体  是构造函数
                                        t_may_c->type = CPP_FUNCTION;
                                        _g_class_fn.insert({access, cur_c_name, t_may_c->val, std::deque<Token>()});
                                    } else if (t->type == CPP_SEMICOLON) {
                                        //构造函数定义  是构造函数
                                        t_may_c->type = CPP_FUNCTION;
                                        _g_class_fn.insert({access, cur_c_name, t_may_c->val, std::deque<Token>()});
                                    } else {
                                        //不是构造函数
                                        continue;
                                    }
                                }
                            } else if(t->type == CPP_NAME && t_n != ts.end() && t_n->type == CPP_OPEN_PAREN) {
                                //可能是成员函数
                                std::cout << "may function: " << t->val << std::endl;
                                auto t_may_c = t;
                                ++t;
                                jump_paren();
                                if (t->type == CPP_OPEN_BRACE || t->type == CPP_SEMICOLON || t->val == "const" || t->val == "throw") {
                                    t_may_c->type = CPP_FUNCTION;
                                    _g_class_fn.insert({access, cur_c_name, t_may_c->val, recall_fn_ret(t_may_c)});
                                } else {
                                    //不是成员函数
                                    continue;
                                }
                            } else {
                                ++t;
                            }
                        } else {
                            ++t;
                        }
                    }

                    assert((t-1)->type == CPP_CLOSE_BRACE);
                    (t-1)->type = cur_end_type;

                    continue;
                }

            } else {
                ++t;
            }
        }   
    }

    //解析class struct 的 type
    for (auto it = _parsers.begin(); it != _parsers.end(); ++it) {
        Parser& parser = *it->second;
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


    //抽取stl的类型

    //抽取类成员函数的ret

    //分析类的成员变量
    


    //分析类成员变量
    // for (auto it = _parsers.begin(); it != _parsers.end(); ++it) {
    //     Parser& parser = *it->second;
    //     std::deque<Token>& ts = parser._ts;
    //     std::string cur_c_name;
    //     std::string cur_s_name;
    //     for (auto t = ts.begin(); t != ts.end(); ) {
    //         if (t->type == CPP_CLASS) {
    //             cur_c_name = t->val;
    //             cur_s_name = "";
    //         } else if (t->type == CPP_STURCT) {
    //             cur_s_name = t->val;
    //             cur_c_name = "";
    //         } else if (t->type == CPP_CLASS_BEGIN) {
    //             //分析class
                
                

                
    //         } else if (t->type == CPP_STRUCT_BEGIN) {
    //             //分析struct


    //         } else {
    //             ++t;
    //         }
    //     }
    // }

    //分析typedef
    

    //分析template的类型

    //引入stl 类型?

    //引入容器

}

void ParserGroup::extract_typedef() {
    //抽取typedef的类型(注意作用域)
    for (auto it = _parsers.begin(); it != _parsers.end(); ++it) {
        Parser& parser = *it->second;
        std::deque<Token>& ts = parser._ts;
        std::string cur_c_name;

        for (auto t = ts.begin(); t != ts.end(); ) {

            //---------------------------------------------------------//
            //common function
            std::function<void()> typedef_analyze = [this, &t, &ts, &cur_c_name]() {
                Token td;
                td = *t;
                td.ts.clear();
                ++t;
                while(!(t->type == CPP_NAME && (t+1)!=ts.end() && (t+1)->type == CPP_SEMICOLON)) {
                    td.ts.push_back(*t);
                    ++t;
                }
                t->type = CPP_TYPE;
                td.ts.push_back(*t);
                td.val = t->val;
                td.subject = cur_c_name;
                _g_typedefs.push_back(td);
                ++t;
            };

            if (t->type == CPP_CLASS) {
                cur_c_name = t->val;
            } else if (t->type == CPP_STURCT) {
                cur_c_name = t->val;
            } else if (t->type == CPP_CLASS_BEGIN) {
                ++t;
                while(t->type != CPP_CLASS_END) {
                    if (t->val == "typedef") {
                        typedef_analyze();
                        continue;
                    }
                    ++t;
                }
                cur_c_name = "";
                
            } else if (t->type == CPP_STRUCT_BEGIN) {
                ++t;
                while(t->type != CPP_STRUCT_END) {
                    if (t->val == "typedef") {
                        typedef_analyze();
                        continue;
                    }
                    ++t;
                }
                cur_c_name = "";

            } else if (t->val == "typedef") {
                typedef_analyze();
                continue;
            } 

            ++t;
        }
    }

    //把所有typedef 都设置成type
    for (auto it = _parsers.begin(); it != _parsers.end(); ++it) {
        Parser& parser = *it->second;
        std::deque<Token>& ts = parser._ts;

        for (auto t = ts.begin(); t != ts.end(); ++t) {
            if (t->type == CPP_NAME && is_in_typedef(t->val)) {
                t->type = CPP_TYPE;
            }
        }
    }
}

void ParserGroup::extract_stl_container() {
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


    for (auto it = _parsers.begin(); it != _parsers.end(); ++it) {
        Parser& parser = *it->second;
        std::deque<Token>& ts = parser._ts;
        for (auto t = ts.begin(); t != ts.end(); ) {
            auto t_n = t+1;//::
            auto t_nn = t+2;//container
            auto t_nnn = t+3;//<

            if (t->val == "std" && t_n != ts.end() && t_nn != ts.end() && t_nnn != ts.end() &&
                t_n->type == CPP_SCOPE && std_container.find(t_nn->val) != std_container.end() && t_nnn->type == CPP_LESS) {
                //找到std的源头, 第一个容器
                std::stack<Token*> stt;//stack token type
                std::stack<Token> st_scope; //<>
                
                Token* root_scope = &(*t);
                root_scope->val = t_nn->val;
                root_scope->subject = "std";
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
                    if (t->val == "std" && t_n != ts.end() && t_nn != ts.end() && t_nnn != ts.end() && 
                        t_n->type == CPP_SCOPE && t_nnn->type == CPP_LESS) {
                        //容器的嵌套
                        const std::string cur_name = t_nn->val;
                        if (std_container.find(cur_name) != std_container.end()) {
                            Token sub;
                            sub.val = cur_name;
                            sub.type = CPP_TYPE;
                            sub.subject = "std";
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
                    } else if (t->type == CPP_NAME && t_n->type == CPP_LESS) {
                        //TODO 应该也是其他模块的模板, 而且是没有被解析的
                        std::cout << "invalid template name 2: " << t->val << std::endl; 
                        assert(false);
                    } else if (t->type == CPP_NAME) {
                        //TODO 应该也是其他模块的模板, 而且是没有被解析的
                        cur_scope->ts.push_back(*t);
                        ++t;
                        ++step;
                        continue;
                        std::cout << "invalid name: " << t->val << std::endl; 
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
}

void ParserGroup::extract_extern_type() {
    //抽取std boost类型
    //TODO 这里可以用配置文件来做, 不仅仅是std boost还可以是其他的三方模块
    ScopeType std_scope;
    std_scope.scope = "std";
    std_scope.types.insert("string");
    std_scope.types.insert("atomic_int");
    std_scope.types.insert("atomic_bool");

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
        Parser& parser = *it->second;
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
                        t+=2;

                        last_scope = &(*it_sub_scope);

                    } else if (it_type != last_scope->types.end()) {
                        //end 
                        to_be_type = CPP_TYPE;
                        ts_to_be_type.push_back(*t_n);//::
                        ts_to_be_type.push_back(*t_nn);//type
                        t+=3;
                    } else if (it_sub_scope != last_scope->sub_scope.end()) {
                        ts_to_be_type.push_back(*t_n);//::
                        ts_to_be_type.push_back(*t_nn);//type
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
                    t->ts = ts_to_be_type;
                    t->type = CPP_TYPE;
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

static inline void print_token(const Token& t, std::ofstream& out) {
    if (t.ts.empty()) {
        out << t.val << " ";
        return;
    } else {
        for (auto it2 = t.ts.begin(); it2 != t.ts.end(); ++it2) {
            out << t.val << " ";
            print_token(*it2, out);
        }
    }
}

void ParserGroup::debug(const std::string& debug_out) {
    for (auto it = _parsers.begin(); it != _parsers.end(); ++it) {
        Parser& parser = *it->second;
        const std::string f = debug_out+"/"+it->first+".l1";
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
            
            out << it->name;
            if (!it->father.empty()) {
                out << " public : " << it->father;
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
        for (auto it = _g_class_fn.begin(); it != _g_class_fn.end(); ++it) {
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
            for (auto it2 = it->rets.begin(); it2 != it->rets.end(); ++it2) {
               out << it2->val << " "; 
            }

            out << std::endl;
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
        for (auto it = _g_typedefs.begin(); it != _g_typedefs.end(); ++it) {
            Token& t = *it;
            if (!t.subject.empty()) {
                out << t.subject << "::";    
            }
            out << t.val << "\t";
            for (auto it2 = t.ts.begin(); it2 != t.ts.end(); ++it2) {
                out << it2->val << " ";
            }
            out << std::endl;
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
    for (auto it=_g_typedefs.begin(); it != _g_typedefs.end(); ++it) {
        if (it->val == name) {
            return true;
        }
    }

    return false;
}