#include "parser.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>

Reader::Reader() {

}

Reader::~Reader() {

}

int Reader::read(const std::string& file) {
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
    return _cur_loc > _file_str.length()-1;
}

std::string Reader::get_string(int loc, int len) {
    assert(loc+len < _file_str.length());
    return _file_str.substr(loc, len);
}

void Reader::push_token(const Token& t) {
    _ts.push_back(t);
}

Token Reader::get_last_token() {
    if (_ts.empty()) {
        return {CPP_OTHER, "",0};
    } else {
        return _ts.back();
    }
}

void Reader::skip_white() {
    char c = _file_str[_cur_loc];
    if (c==' ' || c=='\t' || c=='\f' || c=='\v' || c=='\0') {
        ++_cur_loc;
        skip_white();
    }
}

const std::deque<Token>& Reader::get_tokens() const {
    return _ts;
}

static inline bool is_num(char c) {
    return c >= '0' && c <='9';
}

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
        if (per_status == 2 || per_status == 4 || per_status == 7) {
            if (c == 'f') {
                //f的尾注
                token.val += c;
                return token;
            }
        } else {
            token.type = CPP_OTHER;
            return token;
        }
    }
    token.val += c;

    int next_status = DFA[input][per_status];
    return lex_number(cpp_reader->next_char(), cpp_reader, token, next_status);
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
        if (per_status == 2) {
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
    //  /    | 1 |-1 |-1 | 4 |-1 |
    //  *    |-1 | 2 | 3 | 3 |-1 |
    // other |-1 |-1 | 2 | 2 |-1 |
    const static char DFA[3][5] = {
        1,-1,-1,4,-1,
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
            return lex(cpp_reader);
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
            if (nc == '>') {
                return {CPP_RSHIFT, ">>", cpp_reader->get_cur_loc()-1};
            } else if (nc == '=')  {
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
        case 's': case 't':           case 'v': case 'w': case 'x':
        case 'y': case 'z':
        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
        case 'G': case 'H': case 'I': case 'J': case 'K':
        case 'M': case 'N': case 'O': case 'P': case 'Q':
        case 'S': case 'T':           case 'V': case 'W': case 'X':
        case 'Y': case 'Z':
        {
            Token t  ={ CPP_NAME, "", cpp_reader->get_cur_loc()};
            return lex_identifier(c, cpp_reader, t, 0);
            break;
        }

        //number
        case '0': case '1': case '2': case '3': case '4':
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
            Token t = cpp_reader->get_last_token();
            //数字
            if (t.type == CPP_OPEN_PAREN ||
                t.type == CPP_EQ ||
                t.type == CPP_EQ_EQ ||
                t.type == CPP_NOT_EQ ||
                t.type == CPP_GREATER ||
                t.type == CPP_LESS ||
                t.type == CPP_EQ_EQ ||
                t.type == CPP_NOT_EQ ||
                t.type == CPP_GREATER_EQ || 
                t.type == CPP_LESS_EQ) {
                Token t  ={ CPP_NUMBER, "", cpp_reader->get_cur_loc()};
                return lex_number(c, cpp_reader, t, 0);
            }

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
            Token t = cpp_reader->get_last_token();
            if (t.type == CPP_OPEN_PAREN ||
                t.type == CPP_EQ ||
                t.type == CPP_EQ_EQ ||
                t.type == CPP_NOT_EQ ||
                t.type == CPP_GREATER ||
                t.type == CPP_LESS ||
                t.type == CPP_EQ_EQ ||
                t.type == CPP_NOT_EQ ||
                t.type == CPP_GREATER_EQ || 
                t.type == CPP_LESS_EQ) {
                Token t  ={ CPP_NUMBER, "", cpp_reader->get_cur_loc()};
                return lex_number(c, cpp_reader, t, 0);
            }

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