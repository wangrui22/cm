#ifndef MY_COMMON_H
#define MY_COMMON_H

#include <string>
#include <ostream>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <stack>
#include <deque>
#include <set>
#include <map>
#include <cassert>
#include <functional>
#include <algorithm>

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
    CPP_MACRO, // TODO 宏 ? 不知道怎么处理
    CPP_PADDING, // TODO 不知道怎么处理
    CPP_EOF,// 文件尾
    
    //自己加的内容
    CPP_TYPE, //类型
    CPP_KEYWORD,//关键字
    CPP_POINTER,//指针 ->
    CPP_OPEN_ANGLE, //<
    CPP_CLOSE_ANGLE, //>
    CPP_CLASS, // 类名

    /*
    if
    elif
    else
    endif
    ifdef
    ifndef
    define
    undef
    include//这个合并在Header中
    error
    pragma
    line
    */
    CPP_BR, //换行
    CPP_CONNECTOR, //连接符号 '\\'  /
    CPP_PREPROCESSOR, //预处理 和#连接的内容
    CPP_ENUM,//枚举
    CPP_CLASS_BEGIN,// class 开始的 {
    CPP_CLASS_END,// class 结束的 {
    //CPP_STRUCT_BEGIN,//struct 开始的 {
    //CPP_STRUCT_END,//struct 结束的 }
    CPP_MEMBER_FUNCTION, //成员函数
    CPP_MEMBER_VARIABLE, //成员变量
    CPP_GLOBAL_VARIABLE, //全局变量
    CPP_FUNCTION,//定义在类外的过程
    CPP_CALL,//函数调用
};

struct Token {
    TokenType type;
    std::string val;
    int loc;
    //多遍的时候会合并到这里
    //1 CPP_PREPROCESS 预处理语句 存在#的token中, ts后面是预处理语句的token
    //2 CPP_TYPE 类型, 会展开所有的类型 如模板类型, 容器等
    //3 CPP_MEMBER_VARIABLE 成员变量
    std::deque<Token> ts;

    //token 的主体
    //type 类型: 如果是空则是全局的
    //          如果有值则作用域为subject(这里是class名)
    //              主要有两种情况:1 class内部定义的typedef 2 模板函数的模板类型
    //function 类型: 如果是空则是全局的
    //               如果有值则作用域为subject(这里是class名)
    std::string subject;

    bool deref;

    Token():loc(-1),deref(false) {}

    Token(TokenType type0, const std::string& val0, int loc0):
    type(type0),val(val0),loc(loc0),deref(false) {}
};

//作用域, 为空是全局作用域
struct Scope {
    std::string name;
    std::string key;
    int type;//0 namespace 1 class/stuct 3 function
    std::stack<Token> sb;// store brace to check scope
    std::deque<Scope> father;
};

struct ClassType {
    std::string name;
    bool is_struct;
    bool is_template;
    std::string father;//不考虑多重继承
    Scope scope;
    std::map<std::string, Token> tm_paras;//模板参数
    std::vector<std::string> tm_paras_list;
};

struct ClassFunction {
    int access;//0 public 1 protected 2 private
    std::string c_name;
    std::string fn_name;//function name
    std::deque<Token> rets;//origin return tokens
    Token ret;//return token
    bool is_virtual;
};

struct Variable {
    std::string name;
    Token type;
    Scope scope;
};

struct ClassVariable {
    std::string c_name;
    std::string m_name;//member name
    Token type;
};

struct Function {
    std::string name;
    Token ret;
    Scope scope;
};

//由namespace 或者 struct/class中定义， 用作容器分析
struct ScopeType {
    std::string scope;
    std::set<std::string> types;
    std::map<std::string, ScopeType> sub_scope;
};

const static std::string ANONYMOUS_SCOPE = "0anonymous_scope0";
const static std::string THIRD_CLASS = "3th";

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
            out << "header";
            break;   
        case CPP_COMMENT: 
            out << "comment";
            break;   
        case CPP_MACRO: 
            out << "marco";
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
        case CPP_KEYWORD: 
            out << "keyword";
            break;
        case CPP_TYPE: 
            out << "type";
            break;
        case CPP_CLASS:
            out << "class";
            break;
        case CPP_BR: 
            out << "br";
            break;
        case CPP_PREPROCESSOR:
            out << "preprocessor";
            break;
        case CPP_CONNECTOR: 
            out << "\\";
            break;
        case CPP_MEMBER_FUNCTION:
            out << "member function";
            break;
        case CPP_FUNCTION:
            out << "function0";
            break;
        case CPP_CLASS_BEGIN:
            out << "class begin";
            break;
        case CPP_CLASS_END:
            out << "class end";
            break;
        case CPP_GLOBAL_VARIABLE:
            out << "global variable: ";
            break;
        case CPP_MEMBER_VARIABLE:
            out << "member variable: ";
            break;
        case CPP_CALL:
            out << "call: ";
            break;
        default:
            out << "INVALID";
            break;    
    }
    return out;
}   

#endif