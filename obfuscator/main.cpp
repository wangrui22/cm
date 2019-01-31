#include <boost/algorithm/string.hpp>
#include "obfuscator.h"
#include "util.h"

Obfuscator obfuscator;

static int get_source_files(std::vector<std::string>& files) {
    std::ifstream in("./file_source", std::ios::in);
    if (!in.is_open()) {
        std::cerr << "open config file source failed.\n";  
        return -1;
    }
    std::string line;
    while(std::getline(in, line)) {
        if (!line.empty() && line[0] != '#') {
            files.push_back(line);
        }
    }

    in.close();

    return 0;
}

static int get_ignore_file_name(std::vector<std::string>& files) {
    std::ifstream in("./ignore_file", std::ios::in);
    if (!in.is_open()) {
        std::cerr << "open config ignore failed.\n";  
        return -1;
    }
    std::string line;
    while(std::getline(in, line)) {
        if (!line.empty() && line[0] != '#') {
            files.push_back(line);
        }
    }

    in.close();

    return 0;
}

static int get_ignore_class_name(std::set<std::string>& names) {
    std::ifstream in("./ignore_class", std::ios::in);
    if (!in.is_open()) {
        std::cerr << "open config ignore failed.\n";  
        return -1;
    }
    std::string line;
    while(std::getline(in, line)) {
        if (!line.empty() && line[0] != '#') {
            names.insert(line);
        }
    }

    in.close();

    return 0;
}

static int get_ignore_function_name(std::set<std::string>& names) {
    std::ifstream in("./ignore_function", std::ios::in);
    if (!in.is_open()) {
        std::cerr << "open config ignore failed.\n";  
        return -1;
    }
    std::string line;
    while(std::getline(in, line)) {
        if (!line.empty() && line[0] != '#') {
            names.insert(line);
        }
    }

    in.close();

    return 0;
}

static int get_ignore_class_function_name(std::map<std::string,std::set<std::string>>& c_fn_names) {
    std::ifstream in("./ignore_class_function", std::ios::in);
    if (!in.is_open()) {
        std::cerr << "open config ignore failed.\n";  
        return -1;
    }
    std::string line;
    while(std::getline(in, line)) {
        if (!line.empty()) {
            std::vector<std::string> vals;
            boost::split(vals, line, boost::is_any_of("::"));
            if (vals.size() == 3) {
                const std::string c_name = vals[0];
                const std::string fn_name = vals[2];
                if (c_fn_names.find(c_name) == c_fn_names.end()) {
                    c_fn_names[c_name] = std::set<std::string>();
                }
                c_fn_names[c_name].insert(fn_name);
            }
        }
    }

    in.close();

    return 0;
}

int main(int argc, char* argv[]) {
    bool hash = false;
    if (argc >= 2 && std::string(argv[1]) == "hash") {
        hash = true;
    } 

    std::vector<std::string> src_dir;
    std::vector<std::string> ig_file;
    std::set<std::string> ig_class;
    std::set<std::string> ig_fn;
    std::map<std::string,std::set<std::string>> ig_c_fn_names;

    if (0 != get_source_files(src_dir)) {
        return -1;
    }

    if (0 != get_ignore_file_name(ig_file)) {
        return -1;
    }

    if (0 != get_ignore_class_name(ig_class)) {
        return -1;
    }

    if (0 != get_ignore_function_name(ig_fn)) {
        return -1;
    }

    if (0 != get_ignore_class_function_name(ig_c_fn_names)) {
        return -1;
    }

    std::set<std::string> ig_file_set;
    for (size_t i=0; i<ig_file.size(); ++i) {
        ig_file_set.insert(ig_file[i]);
    }

    for (size_t j=0; j<src_dir.size(); ++j) {

        std::cout << "parse direction: " << src_dir[j] << "\n";

        std::vector<std::string> h_file;
        std::vector<std::string> c_file;

        if (!Util::is_direction(src_dir[j])) {
            h_file.push_back(src_dir[j]);
        } else {
            std::set<std::string> post;
            post.insert(".h");
            post.insert(".hpp");
            post.insert(".inl");
            Util::get_all_file_recursion(src_dir[j], post,  h_file);

            std::set<std::string> post2;
            post2.insert(".cpp");
        
            Util::get_all_file_recursion(src_dir[j], post2,  c_file);
        }

        for (size_t i=0; i<h_file.size(); ++i) {
            std::cout << "lex file: " << h_file[i] << "\n";

            if (ig_file_set.find(Util::get_file_name(h_file[i])) != ig_file_set.end()) {
                std::cout << "find ignore file: " << h_file[i] << ". ignore it.\n";
                continue;
            }

            Reader* reader = new Reader();
            reader->read(h_file[i]);
            Lex* lex = new Lex();
            lex->set_reader(reader);
            while(true) {
                lex->push_token(lex->lex(reader));
                if (reader->eof()) {
                    break;
                }
            }

            lex->stage_token();
            lex->l1();
            lex->l2();

            std::string file_name = Util::get_file_name(h_file[i]);

            obfuscator.add_lex(file_name, h_file[i], lex, reader);
        }

        for (size_t i=0; i<c_file.size(); ++i) {
            std::cout << "lex file: " << c_file[i] << "\n";

            if (ig_file_set.find(Util::get_file_name(c_file[i])) != ig_file_set.end()) {
                std::cout << "find ignore file: " << c_file[i] << ". ignore it.\n";
                continue;
            }

            Reader* reader = new Reader();
            reader->read(c_file[i]);
            Lex* lex = new Lex();
            lex->set_reader(reader);

            while(true) {
                lex->push_token(lex->lex(reader));
                if (reader->eof()) {
                    break;
                }
            }

            lex->stage_token();

            lex->l1();
            lex->l2();

            std::string file_name = Util::get_file_name(c_file[i]);

            obfuscator.add_lex(file_name, c_file[i], lex, reader);
        }
    }

    obfuscator.set_ignore_class(ig_class);
    obfuscator.set_ignore_function(ig_fn);
    obfuscator.set_ignore_class_function(ig_c_fn_names);

    obfuscator.remove_comments();
    obfuscator.extract_enum();
    obfuscator.parse_marco();
    obfuscator.extract_extern_type();
    obfuscator.extract_class();
    obfuscator.extract_typedef();
    obfuscator.combine_type_with_multi_and_rm_const();
    obfuscator.extract_decltype();
    obfuscator.extract_container();
    obfuscator.combine_type_with_multi_and_rm_const();
    obfuscator.extract_class_member();
    obfuscator.extract_global_var_fn();
    obfuscator.extract_local_var_fn();
    obfuscator.label_call();
    obfuscator.label_fn_as_parameter();
    obfuscator.replace_call(hash);
    
    obfuscator.debug("./result");

    return 0;
}
