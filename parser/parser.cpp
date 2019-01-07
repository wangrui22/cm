#include "parser.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>
#include <stack>

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
            for (int i=0; i<NUM_KEYWORDS; ++i) {
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
        input = 0;
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
            if (nnc == '\'' && nc != '\'') {
                return {CPP_CHAR, "\'" + std::string(1,nc) + "\'", cpp_reader->get_cur_loc()-2};
            }

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
            // Token t = cpp_reader->get_last_token();
            // //TODO 带符号数字的解析这样可以吗, 还是需要二次合并
            // if (t.type == CPP_OPEN_PAREN ||
            //     t.type == CPP_EQ ||
            //     t.type == CPP_EQ_EQ ||
            //     t.type == CPP_NOT_EQ ||
            //     t.type == CPP_GREATER ||
            //     t.type == CPP_LESS ||
            //     t.type == CPP_EQ_EQ ||
            //     t.type == CPP_NOT_EQ ||
            //     t.type == CPP_GREATER_EQ || 
            //     t.type == CPP_LESS_EQ) {
            //     Token t  ={ CPP_NUMBER, "", cpp_reader->get_cur_loc()};
            //     return lex_number(c, cpp_reader, t, 0);
            // }

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
            // Token t = cpp_reader->get_last_token();
            // if (t.type == CPP_OPEN_PAREN ||
            //     t.type == CPP_EQ ||
            //     t.type == CPP_EQ_EQ ||
            //     t.type == CPP_NOT_EQ ||
            //     t.type == CPP_GREATER ||
            //     t.type == CPP_LESS ||
            //     t.type == CPP_EQ_EQ ||
            //     t.type == CPP_NOT_EQ ||
            //     t.type == CPP_GREATER_EQ || 
            //     t.type == CPP_LESS_EQ) {
            //     Token t  ={ CPP_NUMBER, "", cpp_reader->get_cur_loc()};
            //     return lex_number(c, cpp_reader, t, 0);
            // }

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
    for (int i=0; i<NUM_TYPE; ++i) {
        if (str == types[i]) {
            return true;
        }
    }
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


// void Parser::extract_class() {
//     _class.clear();
//     for (auto t = _ts.begin(); t != _ts.end(); ++t) {
//         if (t->val == "class") {
//             //TODO 需要排除类前面跟着的修饰符
//             auto t_n = t+1;
//             if (t_n != _ts.end() && t_n->type == CPP_NAME) {
//                 t_n->type = CPP_CLASS;
//                 _class.insert(t_n->val);
//             }
//             ++t;
//         } else if (t->val == "struct") {
//             auto t_n = t+1;
//             if (t_n != _ts.end() && t_n->type == CPP_NAME) {
//                 t_n->type = CPP_STURCT;
//                 _class.insert(t_n->val);
//             }
//             ++t;
//         }
//     }
// }

// void Parser::extract_class_fn() {
//     _class_fn.clear();
//     for (auto t = _ts.begin(); t != _ts.end(); ) {
//         if (t->type == CPP_CLASS) {

//             const std::string class_name = t->val;

//             std::stack<Token> sb;
//             std::deque<Token>::iterator fn_begin = t;
//             std::deque<Token>::iterator fn_end = t;

//             //find begin scope
//             while(t->type != CPP_OPEN_BRACE) {
//                 fn_begin = t+1;
//                 ++t;
//             }
//             sb.push(*t);
//             ++t;

//             //find end scope
//             while(!sb.empty()) {
//                 if (t->type == CPP_OPEN_BRACE) {
//                     sb.push(*t);
//                     ++t;
//                     continue;
//                 }
//                 if (t->type == CPP_CLOSE_BRACE && sb.top().type == CPP_OPEN_BRACE) {
//                     sb.pop();
//                     if (sb.empty()) {
//                         fn_end = t;
//                     }
//                     ++t;
//                     continue;
//                 }
                
//                 ++t;
//             }

//             //extract class function
//             //找括号组
//             std::set<std::string> fns;
//             std::stack<std::deque<Token>::iterator> sbi;
//             auto tc = fn_begin;
//             while(tc != fn_end) {
//                 if (tc->val == "public") {
//                     break;
//                 } else {
//                     ++tc;
//                     continue;
//                 }
//             }

//             for (; tc != fn_end; ) {
//                 if (tc->type == CPP_OPEN_PAREN) {
//                     sbi.push(tc);
//                     ++tc;
//                     continue;
//                 }
//                 if (tc->type == CPP_CLOSE_PAREN && sbi.top()->type == CPP_OPEN_PAREN) {
//                     auto fend = sbi.top();
//                     sbi.pop();
//                     fns.insert((fend-1)->val);
//                     ++tc;
//                     continue;
//                 }

//                 ++tc;
//             }
//             _class_fn[class_name] = fns;
//         }

//         ++t;
//     }
// }

// const std::map<std::string, std::set<std::string>>& Parser::get_class_fns() const {
//     return _class_fn;
// }

// const std::set<std::string>& Parser::get_classes() const {
//     return _class;
// }


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
                                ++t;
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
//                 std::stack<Token> tb;
//                 while((++t)->type != CPP_GREATER) {}
//                 tb.push(*t);
                
//                 //将仅跟着class之后的Name设置成type



//             }
//         }
//     }
//     //把 模板类/结构体提取出来

// }

void ParserGroup::extract_class() {
    for (auto it = _parsers.begin(); it != _parsers.end(); ++it) {
        Parser& parser = *it->second;
        std::deque<Token>& ts = parser._ts;
        for (auto t = ts.begin(); t != ts.end(); ++t) {
            //找到了模板, 忽略其中所有的class
            if (t->val == "template") {
                //找<
                std::stack<Token> tb;
                while((++t)->type != CPP_GREATER) {}
                tb.push(*t);
                
                //将仅跟着class之后的Name设置成type
            }

            //TODO 看除了template后的第一个关键字是否是class或者struct从而判断是不是模板类
            if (t->val == "class") {
                //排除类前面跟着的修饰符： 一般是宏定义 或者 是关键字
                auto t_n = t+1;
                while (true) {
                    if (t_n != ts.end() && t_n->type == CPP_NAME) {
                        t_n->type = CPP_CLASS;
                        _g_class.insert(t_n->val);
                        break;
                    } else {
                        ++t;
                        t_n = t+1;
                    }
                }
                ++t;
            } else if (t->val == "struct") {
                auto t_n = t+1;
                while (true) {
                    if (t_n != ts.end() && t_n->type == CPP_NAME) {
                        t_n->type = CPP_STURCT;
                        _g_struct.insert(t_n->val);
                        break;
                    } else {
                        ++t;
                        t_n = t+1;
                    }
                }
                ++t;
            }
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
        for (auto it=ts.begin(); it!=ts.end(); ++it) {
            const Token& t = *it;
            if (!t.ts.empty()) {
                out << t.type << ": " << t.val << " ";
                for (auto it2 = t.ts.begin(); it2 != t.ts.end(); ++it2) {
                    out << it2->val << " ";
                }
                out << "\n";
            } else {
                out << t.type << ": " << t.val << std::endl;
            }
        }

        // out << "\n//----------------------------------------//\n";
        // const std::set<std::string> classes = parser.get_classes();
        // for (auto it=classes.begin(); it!=classes.end(); ++it) {
        //     out << "class: " << *it << std::endl;
        // }

        // out << "\n//----------------------------------------//\n";
        // const std::map<std::string, std::set<std::string>> class_fns = parser.get_class_fns();
        // for (auto it=class_fns.begin(); it!=class_fns.end(); ++it) {
        //     for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
        //         out << it->first << "::" << *it2 << std::endl;
        //     }
        // }

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
            out << "class : " << *it << "\n";
        }
        for (auto it = _g_struct.begin(); it != _g_struct.end(); ++it) { 
            out << "struct : " << *it << "\n";
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
        // if ((*it).ts == m) {
        //     return true;
        // }
    }

    return false;
}