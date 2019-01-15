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
    void extract_container();
    void combine_type2();
    void extract_class2();
    void extract_global_var_fn();
    void extract_local_var_fn();
    void extract_typedef2();//重新构造typedef 的类型

    void label_call();

    void debug(const std::string& debug_out);

private:
    bool is_in_marco(const std::string& m);
    bool is_in_class_struct(const std::string& name, bool& tm);
    bool is_in_typedef(const std::string& name);
    bool is_in_typedef(const std::string& name, Token& t_type);
    
    std::set<ClassType> is_in_class_function(const std::string& name);//待测试 好像没有用
    
    bool is_member_function(const std::string& c_name, const std::string& fn_name);
    bool is_member_function(const std::string& c_name, const std::string& fn_name, Token& ret);
    bool is_local_function(const std::string& file_name, const std::string& fn_name, Token& t_type);
    bool is_global_function(const std::string& fn_name, Token& ret);

    bool is_member_variable(const std::string& c_name, const std::string& c_v_name, Token& t_type);
    bool is_global_variable(const std::string& v_name, Token& t_type);
    bool is_local_variable(const std::string& file_name, const std::string& v_name, Token& t_type);

    bool is_stl_container(const std::string& name);

private:
    std::vector<std::string> _file_name;
    std::vector<Parser*> _parsers;
    std::vector<Reader*> _readers;

    std::vector<Token> _g_marco;//全局宏定义
    std::set<ClassType> _g_class;//全局class struct
    std::map<ClassType, std::set<ClassType>> _g_class_childs;//全局的子类
    std::map<ClassType, std::set<ClassType>> _g_class_bases;//全局的父类
    std::map<ClassType, std::vector<ClassFunction>> _g_class_fn;//全局的class的成员函数
    std::map<ClassType, std::vector<ClassFunction>> _g_class_fn_with_base;//全局的class的成员函数,包含了基类的所有函数(不区分access)
    std::map<ClassType, std::vector<ClassVariable>> _g_class_variable;//全局的class的成员变量
    std::set<std::string> _g_enum;//全局的枚举

    std::map<std::string, Token> _g_variable;//全局变量<名称,type_token>
    std::map<std::string, std::map<std::string, Token>> _local_variable;//局部变量<文件名,<名称,type_token>>

    std::set<Function> _g_functions;//全局函数
    std::map<std::string, std::set<Function>> _local_functions;//cpp的局部函数

    std::vector<Token> _g_typedefs;//typedef 类型, 仅仅将typedef之前的token记录下来
};

#endif