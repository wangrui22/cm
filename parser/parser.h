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

    Parser();
    ~Parser();

    Token lex(Reader* cpp_reader);
    void push_token(const Token& t);
    const std::deque<Token>& get_tokens() const;

    //loop
    void f1();
    void f2();

    //接口混淆
    // void extract_class();
    // void extract_class_fn();

    // const std::set<std::string>& get_classes() const;
    // const std::map<std::string, std::set<std::string>>& get_class_fns() const;

private:
    std::deque<Token> _ts;

    // std::set<std::string> _class;
    // std::map<std::string, std::set<std::string>> _class_fn;
};


class ParserGroup {
public:
    ParserGroup();
    ~ParserGroup();

    void add_parser(const std::string& file_name, Parser* parser);
    Parser* get_parser(const std::string& file_name);

    void extract_marco();
    void extract_enum();
    void extract_extern_type();
    //void extract_tempalte();
    void extract_class();
    void extract_stl_container();

    void debug(const std::string& debug_out);

private:
    bool is_in_marco(const std::string& m);
    bool is_in_class_struct(const std::string& name, bool& tm);
private:
    std::vector<Token> _g_marco;//全局宏定义
    std::set<ClassType> _g_class;//全局class struct
    std::set<ClassFunction> _g_class_fn;//全局的class的成员函数
    std::set<std::string> _g_enum;//全局的枚举
    std::map<std::string, Parser*> _parsers;

    std::vector<Token> _g_typedefs;//typedef 类型, 仅仅将typedef之前的token记录下来
    
};

#endif