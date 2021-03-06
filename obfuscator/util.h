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
    
    static std::string get_file_name(const std::string& path);

    static bool is_direction(const std::string& path);

    static std::string hash(const std::string& val);
};

#endif