#include "parser.h"
#include <iostream>
#include <deque>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "invalid arguments.\n";
        return -1;
    }

    Reader reader;
    reader.read(argv[1]);

    Parser parser;

    while(true) {
        parser.push_token(parser.lex(&reader));
        if (reader.eof()) {
            break;
        }        
    }
    parser.f1();
    parser.f2();

    parser.extract_class();
    parser.extract_class_fn();

    const std::deque<Token>& ts = parser.get_tokens();
    for (auto it=ts.begin(); it!=ts.end(); ++it) {
        const Token& t = *it;
        std::cout << t.type << ": " << t.val << std::endl;
    }

    std::cout << "\n//----------------------------------------//\n";
    const std::set<std::string> classes = parser.get_classes();
    for (auto it=classes.begin(); it!=classes.end(); ++it) {
        std::cout << "class: " << *it << std::endl;
    }

    std::cout << "\n//----------------------------------------//\n";
    const std::map<std::string, std::set<std::string>> class_fns = parser.get_class_fns();
    for (auto it=class_fns.begin(); it!=class_fns.end(); ++it) {
        for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
            std::cout << it->first << "::" << *it2 << std::endl;
        }
    }
    return 0;
}
