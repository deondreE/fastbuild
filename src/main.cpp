#include "project_generator.hpp"
#include "file_watcher.hpp"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <regex>

using namespace fastbuild;

namespace fastbuild::cli {

    std::string get_project_name_from_meson()
    {
        namespace fs = std::filesystem;
        const fs::path meson_file = "meson.build";

        if (!fs::exists(meson_file)) {
            return "Unknown Project";
        }

        std::ifstream file(meson_file);
        if (!file.is_open()) {
            return "Unknown Project";
        }

        std::string line;
        std::regex project_name_regex(R"(project\s*\(\s*'([^']+)')");
        std::smatch match;

        while (std::getline(file, line)) {
            if (std::regex_search(line, match, project_name_regex)) {
                if (match.size() > 1) {
                    return match[1].str();
                }
            }
        }

        return "Unknown Project";
    }

    void print_banner() {
        std::cout << "\033[1;36m" << "FASTBUILD C++" << "\033[0m"
                  << " | Modern C++20 Build Orchestrator\n"
                  << "------------------------------------------\n";
    }

    void run_watcher(const std::vector<std::string>& args) {
        bool autoRun = std::find(args.begin(), args.end(), "--run") != args.end();
        bool debugMode = std::find(args.begin(), args.end(), "--debug") != args.end();

        std::string projectName = get_project_name_from_meson();
        FileWatcher watcher(".");

        watcher.watch([&]() {
            std::cout << "\033[1;33m>> Rebuilding " << projectName << "...\033[0m\n";

            if (std::system("meson compile -C build") == 0) {
                std::cout << "\033[1;32mBuild Successful!\033[0m\n";

                if (debugMode) {
                    std::string exePath = "./build/" + projectName;
                    if (std::filesystem::exists(exePath)) {
                        std::cout << "\033[1;36m>> Debugger Attached (q to quit)...\033[0m\n";
                        std::system(("gdb -q -ex run " + exePath).c_str());
                    }
                } else if (autoRun) {
                    std::cout << "\033[1;34m>> Output:\033[0m\n";
                    std::system(("./build/" + projectName).c_str());
                }
            } else {
                std::cerr << "\033[1;31mBuild Failed. Awaiting fixes...\033[0m\n";
            }
        });
    }

    void handle_scaffold() {
        std::string name;
        std::cout << "Project Name: ";
        std::getline(std::cin, name);

        std::cout << "Target Editor (vscode/vim/none): ";
        std::string editor;
        std::getline(std::cin, editor);

        ProjectGenerator gen(name);

        std::cout << "Add standard dependencies (e.g., fmt, raylib). Press Enter twice to finish:\n";
        std::string dep;
        while (std::getline(std::cin, dep) && !dep.empty()) {
            gen.add_dependency(dep);
        }

        std::cout << "Add a GitHub dependency? (y/N): ";
        std::string choice;
        std::getline(std::cin, choice);
        if (choice == "y" || choice == "Y") {
            RemoteDep rd;
            std::cout << "Dep Name: "; std::getline(std::cin, rd.name);
            std::cout << "Git URL: ";  std::getline(std::cin, rd.url);
            std::cout << "Revision: "; std::getline(std::cin, rd.revision);
            gen.add_remote(std::move(rd));
        }

        std::cout << "\n\033[1;33m>> Scaffolding " << name << "...\033[0m\n";
        if (gen.generate_basic()) {
            std::cout << "\033[1;32mProject created successfully!\033[0m\n"
                      << "Usage: cd " << name << " && meson setup build && fastbuild --watch\n";
        }
    }
}

int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv, argv + argc);
    fastbuild::cli::print_banner();

    try {
        if (argc > 1) {
            std::string command = args[1];

            if (command == "--watch") {
                fastbuild::cli::run_watcher(args);
                return 0;
            }

            if (command == "add" && argc >= 3) {
                return ProjectGenerator::inject_dependency(".", args[2]) ? 0 : 1;
            }
        }

        // Default: Scaffolding Mode
        fastbuild::cli::handle_scaffold();

    } catch (const std::exception& e) {
        std::cerr << "\033[1;31mFatal Error: " << e.what() << "\033[0m\n";
        return 1;
    }

    return 0;
}
