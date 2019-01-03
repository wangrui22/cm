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

    const std::deque<Token>& ts = parser.get_tokens();
    for (auto it=ts.begin(); it!=ts.end(); ++it) {
        const Token& t = *it;
        std::cout << t.type << ": " << t.val << std::endl;
    }


    return 0;
}