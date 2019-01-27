#include "util.h"
#include <map>
#include <iostream>

#include <boost/filesystem.hpp>

namespace {
void get_files(const std::string& root , const std::set<std::string>& postfix,
               std::vector<std::string>& files) {
    if (root.empty()) {
        return ;
    } else {
        std::vector<std::string> dirs;

        for (boost::filesystem::directory_iterator it(root) ;
                it != boost::filesystem::directory_iterator() ; ++it) {
            if (boost::filesystem::is_directory(*it)) {
                dirs.push_back(it->path().filename().string());
            } else {
                const std::string ext = boost::filesystem::extension(*it);

                if (postfix.empty()) {
                    files.push_back(root + std::string("/") + it->path().filename().string());
                } else {
                    for (auto itext = postfix.begin(); itext != postfix.end(); ++itext) {
                        if (*itext == ext) {
                            files.push_back(root + std::string("/") + it->path().filename().string());
                            break;
                        }
                    }
                }
            }
        }

        for (unsigned int i = 0; i < dirs.size(); ++i) {
            const std::string next_dir(root + "/" + dirs[i]);
            get_files(next_dir , postfix , files);
        }

    }
}

void get_files(const std::string& root , const std::set<std::string>& postfix,
    std::map<std::string , std::vector<std::string>>& files) {
    if (root.empty()) {
        return ;
    } else {
        std::vector<std::string> dirs;

        for (boost::filesystem::directory_iterator it(root) ;
            it != boost::filesystem::directory_iterator() ; ++it) {
                if (boost::filesystem::is_directory(*it)) {
                    dirs.push_back(it->path().filename().string());
                } else {
                    const std::string ext = boost::filesystem::extension(*it);
                    if (postfix.find(ext) != postfix.end())
                    {
                        if (files.find(ext) == files.end()) {
                            std::vector<std::string> file(1 , root + std::string("/") + it->path().filename().string());
                            files[ext] = file;
                        } else {
                            files[ext].push_back(root + std::string("/") + it->path().filename().string());
                        }
                    }
                }
        }

        for (unsigned int i = 0; i < dirs.size(); ++i) {
            const std::string next_dir(root + "/" + dirs[i]);
            get_files(next_dir , postfix , files);
        }

    }
}

}

int Util::get_all_file_recursion(
    const std::string& root ,
    const std::set<std::string>& postfix ,
    std::vector<std::string>& files) {
    if (root.empty()) {
        std::cerr << "get all file from empty root.";
        return -1;
    }
    try {
        get_files(root , postfix, files);
        return 0;
    } catch(const boost::exception& ) {
        std::cerr << "get all file failed.";
        return -1;
    } catch(const std::exception& ex) {
        std::cerr << "get all file failed: " << ex.what();
        return -1;
    }
}

std::string Util::get_file_name(const std::string& path) {
    boost::filesystem::path p(path);    
    return p.filename().string();
}

bool Util::is_direction(const std::string& path) {
    return boost::filesystem::is_directory(path);
}