#include "parser.h"
#include <iostream>
#include <deque>

#include "util.h"

ParserGroup parser_group;

int main(int argc, char* argv[]) {
    // if (argc != 3) {
    //     std::cout << "invalid arguments.\n";
    //     return -1;
    // }

    std::vector<std::string> src_dir;
    src_dir.push_back("log");
    src_dir.push_back("util");
    src_dir.push_back("arithmetic");
    src_dir.push_back("io");
    src_dir.push_back("glresource");
    src_dir.push_back("cudaresource");
    src_dir.push_back("algorithm");
    src_dir.push_back("renderalgo");

    for (size_t j=0; j<src_dir.size(); ++j) {

        std::string src = "./test_case/" + src_dir[j];

        std::cout << "parse module: " << src << "\n";

        const bool to_be_confuse = src_dir[j] == "renderalgo";

        std::set<std::string> post;
        post.insert(".h");
        post.insert(".hpp");
        post.insert(".inl");

        std::vector<std::string> h_file;
        Util::get_all_file_recursion(src, post,  h_file);

        std::set<std::string> post2;
        post2.insert(".cpp");
        std::vector<std::string> c_file;
        Util::get_all_file_recursion(src, post2,  c_file);

        for (size_t i=0; i<h_file.size(); ++i) {
            std::cout << "lex file: " << h_file[i] << "\n";

            Reader* reader = new Reader();
            reader->read(h_file[i]);
            Parser* parser = new Parser(to_be_confuse);
            while(true) {
                parser->push_token(parser->lex(reader));
                if (reader->eof()) {
                    break;
                }
            }

            parser->f1();
            parser->f2();

            std::string file_name = Util::get_file_name(h_file[i]);

            parser_group.add_parser(file_name, parser, reader);
        }

        for (size_t i=0; i<c_file.size(); ++i) {
            std::cout << "lex file: " << c_file[i] << "\n";

            Reader* reader = new Reader();
            reader->read(c_file[i]);
            Parser* parser = new Parser(to_be_confuse);

            while(true) {
                parser->push_token(parser->lex(reader));
                if (reader->eof()) {
                    break;
                }
            }

            parser->f1();
            parser->f2();

            std::string file_name = Util::get_file_name(c_file[i]);

            parser_group.add_parser(file_name, parser, reader);
        }
    }

    parser_group.extract_enum();
    parser_group.parse_marco();
    parser_group.extract_extern_type();
    parser_group.extract_class();
    parser_group.extract_typedef();
    parser_group.extract_container();
    parser_group.combine_type_with_multi_and();
    parser_group.extract_class2();
    parser_group.extract_global_var_fn();
    parser_group.extract_local_var_fn();
    //parser_group.label_call();
    
    parser_group.debug("./result");

    return 0;
}
