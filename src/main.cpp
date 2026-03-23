#include "project_generator.hpp"
#include "file_watcher.hpp"
#include <iostream>
#include <string>
#include <algorithm>
#include <vector>
#include <fstream>
#include <regex>

void print_banner() {
    std::cout << "\033[1;36m" << "FASTBUILD C++ SCAFFOLDER" << "\033[0m\n";
    std::cout << "Optimizing for speed and C++20 standard...\n\n";
}

std::string get_project_name_from_meson() {
    std::ifstream file("meson.build");
    if (!file.is_open()) return "UnknownProject";

    std::string line;
    std::regex projectRegex(R"(project\s*\(\s*'([^']+)')");
    std::smatch match;

    while (std::getline(file, line)) {
        if (std::regex_search(line, match, projectRegex)) {
            return match[1];
        }
    }
    return "UnknownProject";
}

int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv, argv + argc);
    print_banner();

    if (argc > 1 && args[1] == "--watch") {
        bool autoRun = std::find(args.begin(), args.end(), "--run") != args.end();
        bool debugMode = std::find(args.begin(), args.end(), "--debug") != args.end();

        std::string path = ".";
        std::string projectName = get_project_name_from_meson();

        FileWatcher watcher(path);
        watcher.watch([&]() {
            std::cout << "\033[1;33m" << ">> Rebuilding " << projectName << "..." << "\033[0m\n";

            std::string buildCmd = "meson compile -C build";
            int res = std::system(buildCmd.c_str());

            if (res == 0) {
                std::cout << "\033[1;32m" << "Build Successful!" << "\033[0m\n";

                if (debugMode) {
                    std::cout << "\033[1;36m" << ">> Launching GDB (type 'quit' or 'q' to exit back to watcher)..." << "\033[0m\n";
                    // Use the detected projectName
                    std::string exePath = "./build/" + projectName;

                    if (std::filesystem::exists(exePath)) {
                        // -q = quiet, -ex run = start immediately
                        std::string gdbCmd = "gdb -q -ex run " + exePath;
                        std::system(gdbCmd.c_str());
                    } else {
                        std::cerr << "Error: Could not find binary at " << exePath << "\n";
                    }
                } else if (autoRun) {
                    std::cout << "\033[1;34m" << ">> Running Output:" << "\033[0m\n";
                    std::system(("./build/" + projectName).c_str());
                }
            } else {
                std::cerr << "\033[1;31m" << "Build Failed. Fix errors to trigger re-run." << "\033[0m\n";
            }
        });
        return 0;
    }

    std::string name;
    std::cout << "Project Name: ";
    std::getline(std::cin, name);

    std::cout << "Target Editor (vscode/vim/none): ";
    std::string editor;
    std::cin >> editor;

    std::vector<std::string> deps;
    std::string dep;
    std::cout << "Enter standard deps (e.g. fmt, gtest) [empty to stop]:\n";
    while (std::getline(std::cin, dep) && !dep.empty()) {
        deps.push_back(dep);
    }

    std::vector<RemoteDep> remotes;
    std::cout << "Add a GitHub dependency? (y/n): ";
    char choice;
    std::cin >> choice;
    if (choice == 'y') {
        RemoteDep rd;
        std::cout << "Dep Name: "; std::cin >> rd.name;
        std::cout << "Git URL: "; std::cin >> rd.url;
        std::cout << "Revision (head/main): "; std::cin >> rd.revision;
        remotes.push_back(rd);
    }

    ProjectGenerator gen(name, deps, remotes);

    std::cout << "\n\033[1;33m" << ">> Scaffolding project..." << "\033[0m\n";
    if (gen.generate()) {
        std::cout << "\033[1;32m" << "Success!" << "\033[0m\n";
        std::cout << "1. cd " << name << "\n2. meson setup build\n3. meson compile -C build\n";
    } else {
        std::cerr << "Failed to generate project.\n";
    }

    return 0;
}
