#include "parser.h"
#include <iostream>
#include <deque>

#include "util.h"

ParserGroup parser_group;

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "invalid arguments.\n";
        return -1;
    }

    std::set<std::string> post;
    post.insert(".h");
    post.insert(".hpp");
    post.insert(".inl");

    std::vector<std::string> h_file;
    Util::get_all_file_recursion(argv[1], post,  h_file);

    std::set<std::string> post2;
    post2.insert(".cpp");
    std::vector<std::string> c_file;
    Util::get_all_file_recursion(argv[1], post2,  c_file);

    for (size_t i=0; i<h_file.size(); ++i) {
        Reader reader;
        reader.read(h_file[i]);
        Parser* parser = new Parser();
        while(true) {
            parser->push_token(parser->lex(&reader));
            if (reader.eof()) {
                break;
            }
        }

        parser->f1();
        parser->f2();

        std::string file_name = Util::get_file_name(h_file[i]);

        parser_group.add_parser(file_name, parser);
    }

    for (size_t i=0; i<c_file.size(); ++i) {
        Reader reader;
        reader.read(c_file[i]);
        Parser* parser = new Parser();
        while(true) {
            parser->push_token(parser->lex(&reader));
            if (reader.eof()) {
                break;
            }
        }

        parser->f1();
        parser->f2();

        std::string file_name = Util::get_file_name(c_file[i]);

        parser_group.add_parser(file_name, parser);
    }

    parser_group.extract_enum();
    parser_group.extract_marco();
    parser_group.extract_extern_type();
    parser_group.extract_class();
    parser_group.extract_stl_container();
    
    parser_group.debug(argv[2]);

    // Reader reader;
    // reader.read(argv[1]);

    // Parser parser;
    // while(true) {
    //     parser.push_token(parser.lex(&reader));
    //     if (reader.eof()) {
    //         break;
    //     }        
    // }
    // parser.f1();
    // parser.f2();

    // parser.extract_class();
    // parser.extract_class_fn();

    // const std::deque<Token>& ts = parser.get_tokens();
    // for (auto it=ts.begin(); it!=ts.end(); ++it) {
    //     const Token& t = *it;
    //     if (t.type == CPP_PREPROCESSOR) {
    //         std::cout << t.type << ": " << t.val << " ";
    //         for (auto it2 = t.ts.begin(); it2 != t.ts.end(); ++it2) {
    //             std::cout << it2->val << " ";
    //         }
    //         std::cout << "\n";
    //     } else {
    //         std::cout << t.type << ": " << t.val << std::endl;
    //     }
    // }

    // std::cout << "\n//----------------------------------------//\n";
    // const std::set<std::string> classes = parser.get_classes();
    // for (auto it=classes.begin(); it!=classes.end(); ++it) {
    //     std::cout << "class: " << *it << std::endl;
    // }

    // std::cout << "\n//----------------------------------------//\n";
    // const std::map<std::string, std::set<std::string>> class_fns = parser.get_class_fns();
    // for (auto it=class_fns.begin(); it!=class_fns.end(); ++it) {
    //     for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
    //         std::cout << it->first << "::" << *it2 << std::endl;
    //     }
    // }
    return 0;
}
