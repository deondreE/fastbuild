#include "project_generator.hpp"
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <regex>

namespace fs = std::filesystem;

ProjectGenerator::ProjectGenerator(const std::string& name,
                                 const std::vector<std::string>& deps,
                                 const std::vector<RemoteDep>& remotes)
    : projectName(name), dependencies(deps), remoteDeps(remotes) {}

void ProjectGenerator::create_directories() {
    fs::create_directories(projectName + "/src");
    fs::create_directories(projectName + "/include");
    fs::create_directories(projectName + "/subprojects");
}

bool ProjectGenerator::add_local_dep(const std::string &projectRoot, const std::string &depName) {
    std::string mesonPath = projectRoot + "/meson.build";
    if (!fs::exists(mesonPath)) {
        std::cerr << "Error: No meson.build found in " << projectRoot << "\n";
        return false;
    }

    std::ifstream inFile(mesonPath);
    std::string content((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
    inFile.close();

    // Prevent duplicates
    if (content.find("'" + depName + "'") != std::string::npos) {
        std::cout << "Dependency '" << depName << "' already exists.\n";
        return true;
    }

    // Find the project_deps array and inject the new dependency
    // This regex looks for the content inside project_deps = [...]
    std::regex depArrayRegex(R"((project_deps\s*=\s*\[)([^\]]*)(\]))");

    // We format it nicely for the array
    std::string replacement = "$1$2  dependency('" + depName + "'),\n$3";

    std::string updatedContent = std::regex_replace(content, depArrayRegex, replacement);

    std::ofstream outFile(mesonPath);
    outFile << updatedContent;

    std::cout << "Successfully added local dependency: " << depName << "\n";
    return true;
}

bool ProjectGenerator::add_remote_dep(const std::string& projectRoot, const RemoteDep &rd) {
    std::string mesonPath = projectRoot + "/meson.build";
    if (!fs::exists(mesonPath)) return false;

    std::ifstream inFile(mesonPath);
    std::string content((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
    inFile.close();

    // Use fallback syntax: this solves the nlohmann_json and spdlog naming issues
    std::regex depArrayRegex(R"((project_deps\s*=\s*\[)([^\]]*)(\]))");
    std::string fallbackDep = "  dependency('" + rd.name + "', fallback : ['" + rd.name + "', '" + rd.name + "_dep']),\n";
    std::string replacement = "$1$2" + fallbackDep + "$3";

    std::string updatedContent = std::regex_replace(content, depArrayRegex, replacement);

    std::ofstream outFile(mesonPath);
    outFile << updatedContent;

    // Create the wrap file
    fs::create_directories(projectRoot + "/subprojects");
    std::ofstream wrap(projectRoot + "/subprojects/" + rd.name + ".wrap");
    wrap << "[wrap-git]\n"
         << "url = " << rd.url << "\n"
         << "revision = " << rd.revision << "\n"
         << "depth = 1\n";

    std::cout << "Successfully generated wrap and dependency for: " << rd.name << "\n";
    return true;
}

void ProjectGenerator::create_meson_file() {
    std::ofstream file(projectName + "/meson.build");
    file << "project('" << projectName << "', 'cpp',\n"
         << "  version : '0.1',\n"
         << "  default_options : ['cpp_std=c++20', 'warning_level=3'])\n\n";
    
    file << "cpp = meson.get_compiler('cpp')\n\n"
         << "add_project_arguments(cpp.get_supported_arguments([\n"
         << "  '-ftime-trace',\n"
         << "  '-fproc-stat-report',\n"
         << "  '-march=native'\n"
         << "]), language : 'cpp')\n\n";


    // Initialize the dependency array
    file << "project_deps = [\n";
    for (const auto& dep : dependencies) {
        file << "  dependency('" << dep << "'),\n";
    }
    for (const auto& rd : remoteDeps) {
        // Automatically add fallbacks for remote deps
        file << "  dependency('" << rd.name << "', fallback : ['" << rd.name << "', '" << rd.name << "_dep']),\n";
    }
    file << "]\n";

    file << "\nexecutable('" << projectName << "',\n"
         << "  'src/main.cpp',\n"
         << "  include_directories : include_directories('include'),\n"
         << "  dependencies : project_deps,\n"
         << "  cpp_pch : 'include/pch.hpp'\n"
         << ")\n";
}

void ProjectGenerator::fetch_remote_wraps() {
    for (const auto& rd : remoteDeps) {
        std::ofstream wrap(projectName + "/subprojects/" + rd.name + ".wrap");
        wrap << "[wrap-git]\n"
             << "url = " << rd.url << "\n"
             << "revision = " << rd.revision << "\n"
             << "depth = 1\n";
    }
}

void ProjectGenerator::create_dx_configs() {
    auto f1 = std::async(std::launch::async, [&]() {
        std::ofstream clangd(projectName + "/.clangd");
        clangd << "CompileFlags:\n  CompilationDatabase: \"build/\"\n  Add: [-std=c++20]\n";
    });

    auto f2 = std::async(std::launch::async, [&]() {
        std::ofstream cf(projectName + "/.clang-format");
        cf << "BasedOnStyle: LLVM\nIndentWidth: 4\n";
    });
}

void ProjectGenerator::create_dummy_source() {
    std::ofstream pch(projectName + "/include/pch.hpp");
    pch << "#include <iostream>\n"
        << "#include <vector>\n"
        << "#include <string>\n";

    std::ofstream main(projectName + "/src/main.cpp");
    main << "int main() {\n"
         << "    std::cout << \"Built with FastBuild!\" << std::endl;\n"
         << "    return 0;\n"
         << "}\n";
}

void ProjectGenerator::create_gitignore() {
    std::ofstream gitignore(projectName + "/.gitignore");
    gitignore << "build/\n"
              << "builddir/\n"
              << "subprojects/*\n"
              << "!subprojects/*.wrap\n"
              << ".vscode/\n"
              << ".clangd/\n"
              << "*.swp\n";
}

void ProjectGenerator::create_editor_configs() {
    fs::create_directories(projectName + "/.vscode");

    auto vscode_settings = std::async(std::launch::async, [&]() {
        std::ofstream settings(projectName + "/.vscode/settings.json");
        settings << "{\n"
                 << "  \"mesonbuild.buildFolder\": \"build\",\n"
                 << "  \"C_Cpp.intelliSenseEngine\": \"disabled\",\n"
                 << "  \"editor.formatOnSave\": true\n"
                 << "}\n";
    });

    auto vscode_launch = std::async(std::launch::async, [&]() {
        std::ofstream launch(projectName + "/.vscode/launch.json");
        launch << "{\n"
               << "  \"version\": \"0.2.0\",\n"
               << "  \"configurations\": [\n"
               << "    {\n"
               << "      \"name\": \"Debug\",\n"
               << "      \"type\": \"cppdbg\",\n"
               << "      \"request\": \"launch\",\n"
               << "      \"program\": \"${workspaceFolder}/build/" << projectName << "\",\n"
               << "      \"cwd\": \"${workspaceFolder}\",\n"
               << "      \"MIMode\": \"gdb\"\n"
               << "    }\n"
               << "  ]\n"
               << "}\n";
    });
}

bool ProjectGenerator::generate() {
    try {
        if (fs::exists(projectName)) return false;
        create_directories();

        std::vector<std::future<void>> tasks;
        tasks.push_back(std::async(std::launch::async, &ProjectGenerator::create_meson_file, this));
        tasks.push_back(std::async(std::launch::async, &ProjectGenerator::create_dummy_source, this));
        tasks.push_back(std::async(std::launch::async, &ProjectGenerator::create_dx_configs, this));
        tasks.push_back(std::async(std::launch::async, &ProjectGenerator::create_editor_configs, this));
        tasks.push_back(std::async(std::launch::async, &ProjectGenerator::create_gitignore, this));
        tasks.push_back(std::async(std::launch::async, &ProjectGenerator::fetch_remote_wraps, this));

        for(auto& t : tasks) t.get();
        return true;
    } catch (...) {
        return false;
    }
}
