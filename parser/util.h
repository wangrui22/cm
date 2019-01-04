#ifndef MY_UTIL_H
#define MY_UTIL_H

#include <string>
#include <set>
#include <vector>

class Util {
public:
    static int get_all_file_recursion(
        const std::string& root, const std::set<std::string>& postfix,
        std::vector<std::string>& files);
};

#endif