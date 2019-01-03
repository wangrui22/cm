#ifndef MY_COMMON_H
#define MY_COMMON_H

#include <string>
#include <ostream>

enum TokenType {
    CPP_EQ = 0, // =
    CPP_NOT, // !
    CPP_GREATER,// > 
    CPP_LESS, // <
    CPP_PLUS, // +
    CPP_MINUS, // -
    CPP_MULT, // *
    CPP_DIV,// /
    CPP_MOD, // %
    CPP_AND, // &
    CPP_OR, // |
    CPP_XOR, // ^
    CPP_RSHIFT, // >>
    CPP_LSHIFT, // <<
    // CPP_MIN,
    // CPP_MAX,
    CPP_COMPL, // ~
    CPP_AND_AND, // &&
    CPP_OR_OR, // ||
    CPP_QUERY, // ?
    CPP_COLON,// :
    CPP_COMMA, // ,
    CPP_OPEN_PAREN, // (
    CPP_CLOSE_PAREN, // )
    CPP_EQ_EQ, // ==
    CPP_NOT_EQ,// !=
    CPP_GREATER_EQ,  // >=
    CPP_LESS_EQ, // <=
    CPP_PLUS_EQ, // +=
    CPP_MINUS_EQ, // -=
    CPP_MULT_EQ,// *=
    CPP_DIV_EQ, // /=
    CPP_MOD_EQ, // %=
    CPP_AND_EQ, // &=
    CPP_OR_EQ, // |=
    CPP_XOR_EQ, // ^=
    //CPP_RSHIFT_EQ, // >>= 
    //CPP_LSHIFT_EQ, // <<=
    // CPP_MIN_EQ, 
    // CPP_MAX_EQ, 
    // CPP_HASH, 
    CPP_PASTE, // #
    CPP_OPEN_SQUARE,  // [
    CPP_CLOSE_SQUARE, // ]
    CPP_OPEN_BRACE,  // {
    CPP_CLOSE_BRACE, // }
    CPP_SEMICOLON,  // ;
    CPP_ELLIPSIS, // 省略符号 ...
    CPP_PLUS_PLUS, // ++
    CPP_MINUS_MINUS, // --
    CPP_DEREF, // 解引用 *
    CPP_DOT, // .
    CPP_SCOPE, // 域 ::
    //CPP_DEREF_STAR, // ->* 暂时不解析
    //CPP_DOT_STAR, // .* 暂时不解析
    CPP_NAME,
    CPP_NUMBER, //数字
    CPP_CHAR, //单字符 'a' 
    //CPP_WCHAR, //暂时不解析
    CPP_OTHER, //解析失败的时候使用
    CPP_STRING, //字符串 
    //CPP_WSTRING,//暂时不解析
    CPP_HEADER_NAME, // <header.h> 
    CPP_COMMENT, // 注释
    CPP_MACRO_ARG, // TODO 宏 ? 不知道怎么处理
    CPP_PADDING, // TODO 不知道怎么处理
    CPP_EOF,// 文件尾
    
    //自己加的内容
    CPP_POINTER,//指针 ->
};

struct Token {
    TokenType type;
    std::string val;
    int loc;
};

inline std::ostream& operator << (std::ostream& out, const TokenType& t) {
    switch(t) {
        case CPP_EQ: 
            out << "=";
            break;
        case CPP_NOT: 
            out << "!";
            break;
        case CPP_GREATER: 
            out << ">";
            break;
        case CPP_LESS: 
            out << "<";
            break;
        case CPP_PLUS: 
            out << "+";
            break;
        case CPP_MINUS: 
            out << "-";
            break;
        case CPP_MULT: 
            out << "*";
            break;
        case CPP_DIV: 
            out << "/";
            break;
        case CPP_MOD: 
            out << "%";
            break;
        case CPP_AND: 
            out << "&";
            break;
        case CPP_OR: 
            out << "|";
            break;
        case CPP_XOR: 
            out << "^";
            break;
        case CPP_RSHIFT: 
            out << ">>";
            break;
        case CPP_LSHIFT: 
            out << "<<";
            break;
        case CPP_COMPL: 
            out << "~";
            break;
        case CPP_AND_AND: 
            out << "&&";
            break;
        case CPP_OR_OR: 
            out << "||";
            break;
        case CPP_QUERY: 
            out << "?";
            break;
        case CPP_COLON: 
            out << ":";
            break;
        case CPP_COMMA: 
            out << ",";
            break;
        case CPP_OPEN_PAREN: 
            out << "(";
            break;
        case CPP_CLOSE_PAREN: 
            out << ")";
            break;
        case CPP_EQ_EQ: 
            out << "==";
            break;
        case CPP_NOT_EQ: 
            out << "!=";
            break;
        case CPP_GREATER_EQ: 
            out << ">=";
            break;
        case CPP_LESS_EQ: 
            out << "<=";
            break;
        case CPP_PLUS_EQ: 
            out << "+=";
            break;
        case CPP_MINUS_EQ: 
            out << "-=";
            break;
        case CPP_MULT_EQ: 
            out << "*=";
            break;
        case CPP_DIV_EQ: 
            out << "/=";
            break;
        case CPP_MOD_EQ: 
            out << "%=";
            break;
        case CPP_AND_EQ: 
            out << "&=";
            break;
        case CPP_OR_EQ: 
            out << "|=";
            break;
        case CPP_XOR_EQ: 
            out << "^=";
            break;
        // case CPP_RSHIFT_EQ: 
        //     out << ">>=";
        //     break;
        // case CPP_LSHIFT_EQ: 
        //     out << "<<=";
        //     break;
        case CPP_PASTE: 
            out << "#";
            break;
        case CPP_OPEN_SQUARE: 
            out << "[";
            break;
        case CPP_CLOSE_SQUARE: 
            out << "]";
            break;
        case CPP_OPEN_BRACE: 
            out << "{";
            break;
        case CPP_CLOSE_BRACE: 
            out << "}";
            break;
        case CPP_SEMICOLON: 
            out << ";";
            break;
        case CPP_ELLIPSIS: 
            out << "...";
            break;
        case CPP_PLUS_PLUS: 
            out << "++";
            break;
        case CPP_MINUS_MINUS: 
            out << "--";
            break;
        case CPP_DEREF: 
            out << "dedef";
            break;
        case CPP_DOT: 
            out << "dot";
            break;
        case CPP_SCOPE: 
            out << "scope";
            break;
        case CPP_NAME: 
            out << "name";
            break;
        case CPP_NUMBER: 
            out << "number";
            break;
        case CPP_CHAR: 
            out << "char";
            break;
        case CPP_OTHER: 
            out << "other";
            break;
        case CPP_STRING: 
            out << "string";
            break;
        case CPP_HEADER_NAME: 
            out << "<header>";
            break;   
        case CPP_COMMENT: 
            out << "comment";
            break;   
        case CPP_MACRO_ARG: 
            out << "CPP_MACRO_ARG";
            break;   
        case CPP_PADDING: 
            out << "CPP_PADDING";
            break;   
        case CPP_EOF: 
            out << "end of file";
            break; 
        case CPP_POINTER:
            out << "->";
            break; 
        default:
            out << "INVALID";
            break;    
    }
}   

#endif