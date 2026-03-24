#include "project_generator.hpp"
#include "system_check.hpp"
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

    void print_help() {
         std::cout << R"(
USAGE:
    fastbuild [COMMAND] [OPTIONS]

COMMANDS:
    (no args)              Interactive project scaffolding
    add <dep>              Add a Meson dependency to project_deps
    add-remote             Add a remote git dependency (interactive)
    add-exe <name>         Add a new executable target
    add-lib <name>         Add a new library target
    --watch [--run|--debug] Watch for file changes and rebuild
    doctor                 Check system dependencies
    help                   Show this help message

OPTIONS:
    --run                  Auto-run executable after successful build (watch mode)
    --debug                Attach debugger after successful build (watch mode)

EXAMPLES:
    # Create new project
    fastbuild

    # Add dependencies
    fastbuild add fmt
    fastbuild add nlohmann_json

    # Add remote dependency
    fastbuild add-remote

    # Watch and auto-run
    fastbuild --watch --run

    # Watch with debugger
    fastbuild --watch --debug

    # Add targets
    fastbuild add-exe my_tool
    fastbuild add-lib my_library

For more information, see: man fastbuild
)";
    }
    

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

    void handle_add_remote() {
        RemoteDep rd;
    std::cout << "Dependency Name: "; std::getline(std::cin, rd.name);
    std::cout << "GitHub Git URL:  "; std::getline(std::cin, rd.url);
    std::cout << "Revision/Tag:    "; std::getline(std::cin, rd.revision);

    if (rd.name.empty() || rd.url.empty()) {
        std::cerr << "Error: Name and URL are required.\n";
        return;
    }

    if (ProjectGenerator::inject_remote(".", rd)) {
        std::cout << "\033[1;32mRemote dependency added and .wrap file created!\033[0m\n";
    }
    }
}

int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv, argv + argc);
    fastbuild::cli::print_banner();

    try {
        if (argc > 1) {
            std::string cmd = args[1];

            if (cmd == "doctor") {
                fastbuild::cli::run_doctor();
                return 0; // Exit after command
            }
            else if (cmd == "add-exe" && argc >= 3) {
                ProjectGenerator::add_target(".", args[2], false);
                return 0; // Exit after command
            }
            else if (cmd == "add-lib" && argc >= 3) {
                ProjectGenerator::add_target(".", args[2], true);
                return 0; // Exit after command
            }
            else if (cmd == "--watch") {
                fastbuild::cli::run_watcher(args);
                return 0; // Exit after command
            }
            else if (cmd == "add" && argc >= 3) {
                ProjectGenerator::inject_dependency(".", args[2]);
                return 0; // Exit after command
            }
            else if (cmd == "add-remote") {
                fastbuild::cli::handle_add_remote();
                return 0;
            }
            if (cmd == "help" || cmd == "--help" || cmd == "-h") {
                fastbuild::cli::print_help();
                return 0;
            }

            // If we got here, a command was provided but not recognized
            std::cerr << "\033[1;31mUnknown command: " << cmd << "\033[0m\n";
            return 1;
        }

        // If no arguments were provided (argc == 1), run scaffold
        fastbuild::cli::handle_scaffold();

    } catch (const std::exception& e) {
        std::cerr << "\033[1;31mFatal Error: " << e.what() << "\033[0m\n";
        return 1;
    }

    return 0;
}
