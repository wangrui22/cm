#include "parser.h"
#include <iostream>
#include <fstream>
#include <deque>

#include "util.h"

ParserGroup parser_group;

static int get_source_files(std::vector<std::string>& files) {
    std::ifstream in("./file_source", std::ios::in);
    if (!in.is_open()) {
        std::cerr << "open config file source failed.\n";  
        return -1;
    }
    std::string line;
    while(std::getline(in, line)) {
        if (!line.empty()) {
            files.push_back(line);
        }
    }

    in.close();

    return 0;
}

static int get_ignore_file_name(std::vector<std::string>& files) {
    std::ifstream in("./ignore", std::ios::in);
    if (!in.is_open()) {
        std::cerr << "open config ignore failed.\n";  
        return -1;
    }
    std::string line;
    while(std::getline(in, line)) {
        if (!line.empty()) {
            files.push_back(line);
        }
    }

    in.close();

    return 0;
}

int main(int argc, char* argv[]) {
    // if (argc != 3) {
    //     std::cout << "invalid arguments.\n";
    //     return -1;
    // }

    std::vector<std::string> src_dir;
    std::vector<std::string> ig_file;

    if (0 != get_source_files(src_dir)) {
        return -1;
    }

    if (0 != get_ignore_file_name(ig_file)) {
        return -1;
    }

    std::set<std::string> ig_file_set;
    for (size_t i=0; i<ig_file.size(); ++i) {
        ig_file_set.insert(ig_file[i]);
    }

    for (size_t j=0; j<src_dir.size(); ++j) {

        std::cout << "parse direction: " << src_dir[j] << "\n";

        std::set<std::string> post;
        post.insert(".h");
        post.insert(".hpp");
        post.insert(".inl");

        std::vector<std::string> h_file;
        Util::get_all_file_recursion(src_dir[j], post,  h_file);

        std::set<std::string> post2;
        post2.insert(".cpp");
        std::vector<std::string> c_file;
        Util::get_all_file_recursion(src_dir[j], post2,  c_file);

        for (size_t i=0; i<h_file.size(); ++i) {
            std::cout << "lex file: " << h_file[i] << "\n";

            if (ig_file_set.find(Util::get_file_name(h_file[i])) != ig_file_set.end()) {
                std::cout << "find ignore file: " << h_file[i] << ". ignore it.\n";
                continue;
            }

            Reader* reader = new Reader();
            reader->read(h_file[i]);
            Parser* parser = new Parser();
            while(true) {
                parser->push_token(parser->lex(reader));
                if (reader->eof()) {
                    break;
                }
            }

            parser->f1();
            parser->f2();

            std::string file_name = Util::get_file_name(h_file[i]);

            parser_group.add_parser(file_name, h_file[i], parser, reader);
        }

        for (size_t i=0; i<c_file.size(); ++i) {
            std::cout << "lex file: " << c_file[i] << "\n";

            if (ig_file_set.find(Util::get_file_name(c_file[i])) != ig_file_set.end()) {
                std::cout << "find ignore file: " << c_file[i] << ". ignore it.\n";
                continue;
            }

            Reader* reader = new Reader();
            reader->read(c_file[i]);
            Parser* parser = new Parser();

            while(true) {
                parser->push_token(parser->lex(reader));
                if (reader->eof()) {
                    break;
                }
            }

            parser->f1();
            parser->f2();

            std::string file_name = Util::get_file_name(c_file[i]);

            parser_group.add_parser(file_name, c_file[i], parser, reader);
        }
    }

    parser_group.extract_enum();
    parser_group.parse_marco();
    parser_group.extract_extern_type();
    parser_group.extract_class();
    parser_group.extract_typedef();
    parser_group.combine_type_with_multi_and_rm_const();
    parser_group.extract_decltype();
    parser_group.extract_container();
    parser_group.combine_type_with_multi_and_rm_const();
    parser_group.extract_class2();
    parser_group.extract_global_var_fn();
    parser_group.extract_local_var_fn();
    parser_group.label_call();
    parser_group.replace_call();
    
    parser_group.debug("./result");

    return 0;
}
