#ifndef MY_LEX_H
#define MY_LEX_H

#include "common.h"

class Reader {
public:
    std::string _file_str;
    std::string _file_path;
    int _cur_line;
    int _cur_loc;

public:
    Reader();
    ~Reader();

    int read(const std::string& file);
    const std::string& get_file_path();
    char cur_char();
    char next_char();
    char pre_char();
    void next_line();
    int get_cur_line() const;
    int get_cur_loc() const;
    bool eof() const;
    std::string get_string(int loc, int len);
    void skip_white();
};

class Lex {
public:
    std::deque<Token> _ts;
    std::deque<Token> _stage_ts;
    Reader* _reader;

public:
    Lex();
    ~Lex();

    void set_reader(Reader* reader);
    void stage_token();

    Token lex(Reader* cpp_reader);
    void push_token(const Token& t);

    //loop
    void l1();
    void l2();
};

#endif