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

public:
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

    void set_reader(Reader* reader);
    void stage_token();

    Token lex(Reader* cpp_reader);
    void push_token(const Token& t);
    const std::deque<Token>& get_tokens() const;

    //loop
    void f1();
    void f2();

private:
    std::deque<Token> _ts;
    std::deque<Token> _stage_ts;
    Reader* _reader;
};


class ParserGroup {
public:
    ParserGroup();
    ~ParserGroup();

    void add_parser(const std::string& file_name, const std::string& file_path, Parser* parser, Reader* reader);
    Parser* get_parser(const std::string& file_name);

    //按顺序调用
    void parse_marco();
    void extract_enum();
    void extract_extern_type();
    void extract_class();
    void extract_typedef();
    void extract_decltype();
    void extract_container();
    void combine_type_with_multi_and_rm_const();
    void extract_class2();
    void extract_global_var_fn();
    void extract_local_var_fn();

    void label_call();
    void label_fn_as_parameter();
    void replace_call();

    void debug(const std::string& debug_out);

private:
    bool is_in_marco(const std::string& m);
    bool is_in_marco(const std::string& m, Token& val);
    bool is_in_class_struct(const std::string& name, bool& tm);
    bool is_3th_base(const std::string& name);
    bool is_in_typedef(const std::string& name);
    bool is_in_typedef(const std::string& name, Token& t_type);
    
    bool is_member_function(const std::string& c_name, const std::string& fn_name);
    bool is_member_function(const std::string& c_name, const std::string& fn_name, Token& ret);
    bool is_local_function(const std::string& file_name, const std::string& fn_name, Token& t_type);
    bool is_global_function(const std::string& fn_name, Token& ret);

    bool is_member_variable(const std::string& c_name, const std::string& c_v_name, Token& t_type);
    bool is_global_variable(const std::string& v_name, Token& t_type);
    bool is_local_variable(const std::string& file_name, const std::string& v_name, Token& t_type);

    bool is_stl_container(const std::string& name);
    bool is_stl_container_ret_iterator(const std::string& name);
    bool is_stl_container_ret_val(const std::string& name);

    void extract_class(
        std::deque<Token>::iterator& t, 
        std::deque<Token>::iterator it_begin, 
        std::deque<Token>::iterator it_end, 
        Scope cur_scope, 
        std::deque<Token>& ts,
        bool is_template);

    std::map<std::string, Token> get_template_class_type_paras(std::string c_name);

    void extract_class_member(
        std::string c_name, 
        std::deque<Token>::iterator& t, 
        std::deque<Token>::iterator it_begin, 
        std::deque<Token>::iterator it_end, 
        std::deque<Token>& ts, 
        bool is_template);

    std::map<std::string, Token> label_skip_paren(std::deque<Token>::iterator& t, const std::deque<Token>& ts);
    Token get_auto_type(
        std::deque<Token>::iterator t, 
        const std::deque<Token>::iterator t_start, 
        const std::string& class_name, 
        const std::string& file_name, 
        const std::map<std::string, Token>& paras,
        bool is_cpp,
        int case0);
    
    Token recall_subjust_type(
        std::deque<Token>::iterator t, 
        const std::deque<Token>::iterator t_start, 
        const std::string& class_name, 
        const std::string& file_name, 
        const std::map<std::string, Token>& paras,
        bool is_cpp);

    Token get_subject_type(
        std::deque<Token>::iterator& t, 
        const std::deque<Token>::iterator t_start, 
        const std::string& class_name, 
        const std::string& file_name, 
        const std::map<std::string, Token>& paras,
        bool is_cpp);

    Token get_close_subject_type(
        std::deque<Token>::iterator& t, 
        const std::deque<Token>::iterator t_start, 
        const std::string& class_name, 
        const std::string& file_name, 
        const std::map<std::string, Token>& paras,
        bool is_cpp);
    
    Token get_fn_ret_type(
        std::deque<Token>::iterator& t, 
        const std::deque<Token>::iterator t_start, 
        const std::string& class_name, 
        const std::string& file_name, 
        const std::map<std::string, Token>& paras,
        bool is_cpp);
    
    bool is_call_in_module(std::deque<Token>::iterator t, 
        const std::deque<Token>::iterator t_start,  
        const std::string& class_name, 
        const std::string& file_name, 
        const std::map<std::string, Token>& paras,
        bool is_cpp,
        Token& fn_subject_type);

    void label_call_in_fn(std::deque<Token>::iterator t, 
        const std::deque<Token>::iterator t_start,
        const std::deque<Token>::iterator t_end, 
        const std::string& class_name, 
        const std::string& file_name, 
        const std::map<std::string, Token>& paras,
        bool is_cpp);
        
    void parse_tempalte_para(
        std::deque<Token>::iterator t_begin, 
        std::deque<Token>::iterator t_end,
        std::map<std::string, Token>& paras,
        std::vector<std::string>& paras_list);

    bool check_deref(std::deque<Token>::iterator t, bool is_fn);
private:
    std::vector<std::string> _file_name;
    std::vector<std::string> _file_path;
    std::vector<Parser*> _parsers;
    std::vector<Reader*> _readers;

    std::vector<Token> _g_marco;//全局宏定义
    std::map<std::string, ClassType> _g_class;//全局class struct
    std::map<std::string, std::map<std::string, ClassType>> _g_class_childs;//全局的子类
    std::map<std::string, std::map<std::string, ClassType>> _g_class_bases;//全局的父类
    std::map<std::string, std::vector<ClassFunction>> _g_class_fn;//全局的class的成员函数
    std::map<std::string, std::vector<ClassFunction>> _g_class_fn_with_base;//全局的class的成员函数,包含了基类的所有函数(不区分access)
    std::map<std::string, std::vector<ClassVariable>> _g_class_variable;//全局的class的成员变量
    std::set<std::string> _g_enum;//全局的枚举

    std::map<std::string, Variable> _g_variable;//全局变量<名称,type_token>
    std::map<std::string, std::map<std::string, Variable>> _local_variable;//局部变量<文件名,<名称,type_token>>

    std::map<std::string, Function> _g_functions;//全局函数
    std::map<std::string, std::map<std::string, Function>> _local_functions;//cpp的局部函数

    //typedef
    std::map<std::string, Token> _typedef_map;//key scope::name

    //std::vector<Token> _g_typedefs;//typedef 类型, 仅仅将typedef之前的token记录下来
};

#endif