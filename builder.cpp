#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <algorithm>
#include <cstdlib>

namespace fs = std::filesystem;

struct Target {
    std::vector<std::string> dependencies;
    std::vector<std::string> commands;
    bool is_phony = false;
};

class ScriptParser {
public:
    std::map<std::string, Target> targets;
    std::string default_target;

    void parse(const std::string& filename) {
        std::cout << "[PARSER] Starting parse of " << filename << "\n";
        std::ifstream file(filename);
        std::string line;
        std::string current_target;

        while (std::getline(file, line)) {
            //ScriptParser::trim(line);
            std::cout << "line = " << line << "\n";
            if (line.empty() || line[0] == '#') {
                std::cout << "[PARSER] Skipping empty/comment line\n";
                continue;
            }

            if (line.find(':') != std::string::npos) {
                size_t colon_pos = line.find(':');
                current_target = line.substr(0, colon_pos);
                ScriptParser::trim(current_target);
                std::cout << "[PARSER] Found target: " << current_target << "\n";
                
                std::string deps = line.substr(colon_pos + 1);
                std::vector<std::string> dependencies;
                ScriptParser::split(deps, dependencies);
                
                if (default_target.empty()) {
                    default_target = current_target;
                    std::cout << "[PARSER] Setting default target to: " << default_target << "\n";
                }
                
                targets[current_target].dependencies = dependencies;
                std::cout << "[PARSER] Dependencies for " << current_target << ": ";
                for (const auto& d : dependencies) std::cout << d << " ";
                std::cout << "\n";

                // Assume phony by default, will check commands later
                targets[current_target].is_phony = true;
            } else if (!current_target.empty() && (line[0] == '\t' || line[0] == ' ')) {
                ScriptParser::trim(line);
                targets[current_target].commands.push_back(line);
                std::cout << "[PARSER] Added command to " << current_target << ": " << line << "\n";

                // Check if command creates the target file
                if (line.find("-o " + current_target) != std::string::npos) {
                    std::cout << "[PARSER] Marking " << current_target << " as NOT phony (creates file)\n";
                    targets[current_target].is_phony = false;
                }
            }
        }
        std::cout << "[PARSER] Parse complete. Found " << targets.size() << " targets\n";
    }

private:
    static void trim(std::string& s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
            return !std::isspace(ch);
        }));
        s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
            return !std::isspace(ch);
        }).base(), s.end());
    }

    static void split(const std::string& s, std::vector<std::string>& out) {
        size_t pos = 0;
        while (pos < s.length()) {
            size_t end = s.find(' ', pos);
            if (end == std::string::npos) end = s.length();
            if (end != pos) {
                out.push_back(s.substr(pos, end - pos));
            }
            pos = end + 1;
        }
    }
};

class BuildSystem {
public:
    ScriptParser parser;
    std::vector<std::string> built_targets;

    BuildSystem(const std::string& script_file) {
        std::cout << "[BUILDER] Initializing build system with " << script_file << "\n";
        parser.parse(script_file);
    }

    bool needs_build(const std::string& target) {
        std::cout << "\n[BUILDER] Checking if needs build: " << target << "\n";
        
        if (!fs::exists(target)) {
            std::cout << "  - Target does not exist\n";
            return true;
        }
        if (parser.targets[target].is_phony) {
            std::cout << "  - Phony target, always rebuild\n";
            return true;
        }

        auto target_time = fs::last_write_time(target);
        std::cout << "  - Target exists, modified: " 
                  << target_time.time_since_epoch().count() << "\n";

        for (const auto& dep : parser.targets[target].dependencies) {
            std::cout << "  Checking dependency: " << dep << "\n";
            
            if (parser.targets.count(dep)) {
                std::cout << "  - Dependency is another target\n";
                if (needs_build(dep)) {
                    std::cout << "  - Dependency needs rebuild\n";
                    return true;
                }
            } else if (fs::exists(dep)) {
                auto dep_time = fs::last_write_time(dep);
                std::cout << "  - File dependency exists, modified: " 
                          << dep_time.time_since_epoch().count() << "\n";
                
                if (dep_time > target_time) {
                    std::cout << "  - Dependency is newer than target\n";
                    return true;
                }
            } else {
                std::cerr << "[ERROR] Dependency '" << dep << "' not found!\n";
                exit(1);
            }
        }
        std::cout << "  - No need to rebuild\n";
        return false;
    }

    void build(const std::string& target_name) {
        std::cout << "\n[BUILDER] Starting build of: " << target_name << "\n";
        
        if (std::find(built_targets.begin(), built_targets.end(), target_name) != built_targets.end()) {
            std::cout << "  - Already built, skipping\n";
            return;
        }
        
        Target& target = parser.targets[target_name];
        std::cout << "  - Found " << target.commands.size() << " commands\n";
        
        std::cout << "  Processing dependencies:\n";
        for (const auto& dep : target.dependencies) {
            std::cout << "  - Dependency: " << dep << "\n";
            if (parser.targets.count(dep)) {
                std::cout << "    Building dependency target\n";
                build(dep);
            } else {
                std::cout << "    File dependency, no build needed\n";
            }
        }

        if (needs_build(target_name)) {
            std::cout << "  - Needs build, executing commands\n";
            for (const auto& cmd : target.commands) {
                std::cout << "    Executing: " << cmd << "\n";
                int result = std::system(cmd.c_str());
                if (result != 0) {
                    std::cerr << "[ERROR] Command failed: " << cmd << "\n";
                    exit(1);
                }
            }
        } else {
            std::cout << "  - Up to date, skipping\n";
        }
        
        built_targets.push_back(target_name);
        std::cout << "  - Build complete for " << target_name << "\n";
    }
};

int main(int argc, char* argv[]) {
    std::cout << "=== Starting build system ===\n";
    std::string script_file = "build.script";
    std::string target_name;

    std::cout << "Checking for script file...\n";
    if (!fs::exists(script_file)) {
        std::cerr << "[ERROR] build.script not found!\n";
        return 1;
    }

    std::cout << "Initializing builder...\n";
    BuildSystem builder(script_file);
    
    target_name = (argc > 1) ? argv[1] : builder.parser.default_target;
    std::cout << "Selected target: " << target_name << "\n";

    if (!builder.parser.targets.count(target_name)) {
        std::cerr << "[ERROR] Target '" << target_name << "' not defined!\n";
        return 1;
    }

    std::cout << "Starting build process...\n";
    builder.build(target_name);
    std::cout << "=== Build finished ===\n";
    return 0;
}
