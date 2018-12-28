#include <iostream>
#include <regex>
#include <string>
#include <fstream>

const std::regex reg_time("[()][0-9]{4}[:.][0-9]{2}[:.][0-9]{2}-[0-9]{2}[:.][0-9]{2}[:.][0-9]{2}[:.][0-9]{1,}[)]");
const std::regex reg_number("[+-]?[0-9]+(.[0-9]+)?([eE]?[+-]?[0-9]+)?");
const std::regex reg_identifier("[_a-zA-Z][a-zA-Z0-9_]+");
const std::regex reg_comment("(//)+.*");
const std::regex reg_comment1("(/\\*).*(\\*/)");
int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "invalid arguments\n";
        return -1;
    }

    std::ifstream in(argv[1], std::ios::in);
    if (!in.is_open()) {
        std::cerr << "open file: " << argv[1] << " failed.\n";
        return -1;
    }

    int id = 0;
    std::string line;
    while(std::getline(in, line)) {
        for (std::sregex_iterator it(line.begin(), line.end(), reg_time); it != std::sregex_iterator(); ++it) {
            const std::smatch& match = *it;
            std::cout << "line id: " << id << " time : " << match.str() << "\n";
        }

        for (std::sregex_iterator it(line.begin(), line.end(), reg_number); it != std::sregex_iterator(); ++it) {
            const std::smatch& match = *it;
            std::cout << "line id: " << id << " num: " << match.str() << "\n";
        }

        for (std::sregex_iterator it (line.begin(), line.end(), reg_identifier); it != std::sregex_iterator(); ++it) {
            const std::smatch& match = *it;
            std::cout << "line id: " << id << " identifier: " << match.str() << "\n";
        }

        for (std::sregex_iterator it (line.begin(), line.end(), reg_comment); it != std::sregex_iterator(); ++it) {
            const std::smatch& match = *it;
            std::cout << "line id: " << id << " comment: " << match.str() << "\n";
        }
        for (std::sregex_iterator it (line.begin(), line.end(), reg_comment1); it != std::sregex_iterator(); ++it) {
            const std::smatch& match = *it;
            std::cout << "line id: " << id << " comment1: " << match.str() << "\n";
        }

        // if (std::regex_search(line, m_time, reg_time)) {
        //     std::cout << "time: " << id << ":" << m_time.str();
        // }

        // std::smatch m_number;
        // if (std::regex_search())
        std::cout << std::endl;
        ++id;
    }
    in.close();

    return 0;
}