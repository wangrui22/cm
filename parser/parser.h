#ifndef PARSER_H
#define PARSER_H

#include <string>
#include <deque>
#include <set>
#include <map>
#include "common.h"

class Reader {
public:
    Reader();
    ~Reader();

    int read(const std::string& file);
    char cur_char();
    char next_char();
    char pre_char();
    void next_line();
    int get_cur_line() const;
    int get_cur_loc() const;
    bool eof() const;
    std::string get_string(int loc, int len);
    void skip_white();

private:
    std::string _file_str;
    int _cur_line;
    int _cur_loc;
};

class Parser {
public: 
    Parser();
    ~Parser();

    Token lex(Reader* cpp_reader);
    void push_token(const Token& t);
    const std::deque<Token>& get_tokens() const;

    //loop
    void f1();
    void f2();

    //接口混淆
    void extract_class();
    void extract_class_fn();
    void syntax_function();

    const std::set<std::string>& get_classes() const;
    const std::map<std::string, std::set<std::string>>& get_class_fns() const;

private:
    std::deque<Token> _ts;

    std::set<std::string> _class;
    std::map<std::string, std::set<std::string>> _class_fn;
};

#endif