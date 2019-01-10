#ifndef PARSER_H
#define PARSER_H

#include <string>
#include <deque>
#include <set>
#include <map>
#include <vector>
#include "common.h"

class Reader {
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

private:
    std::string _file_str;
    std::string _file_path;
    int _cur_line;
    int _cur_loc;
};

class Parser {
public: 
    friend class ParserGroup;

    Parser(bool to_be_confuse=false);
    ~Parser();

    Token lex(Reader* cpp_reader);
    void push_token(const Token& t);
    const std::deque<Token>& get_tokens() const;

    //loop
    void f1();
    void f2();

private:
    std::deque<Token> _ts;
    bool _to_be_confuse;
};


class ParserGroup {
public:
    ParserGroup();
    ~ParserGroup();

    void add_parser(const std::string& file_name, Parser* parser, Reader* reader);
    Parser* get_parser(const std::string& file_name);

    //按顺序调用
    void extract_marco();
    void extract_enum();
    void extract_extern_type();
    void extract_class();
    void extract_typedef();
    void extract_stl_container();
    void combine_type2();
    void extract_class2();

    void debug(const std::string& debug_out);

private:
    bool is_in_marco(const std::string& m);
    bool is_in_class_struct(const std::string& name, bool& tm);
    bool is_in_typedef(const std::string& name);
private:
    std::map<std::string, Parser*> _parsers;
    std::map<std::string, Reader*> _readers;

    std::vector<Token> _g_marco;//全局宏定义
    std::set<ClassType> _g_class;//全局class struct
    std::map<ClassType, std::vector<ClassFunction>> _g_class_fn;//全局的class的成员函数
    std::map<ClassType, std::vector<ClassVariable>> _g_class_variable;//全局的class的成员变量
    std::set<std::string> _g_enum;//全局的枚举

    std::vector<Token> _g_typedefs;//typedef 类型, 仅仅将typedef之前的token记录下来  
};

#endif