#include "project_generator.hpp"
#include "common.hpp"
#include "template_manager.hpp"
#include <exception>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <format>
#include <regex>
#include <map>

namespace fastbuild {

void ProjectGenerator::write_file(const fs::path& path, std::string_view content) {
    std::ofstream ofs(path, std::ios::out | std::ios::trunc);
    if (!ofs) throw std::runtime_error("Failed to open file for writing: " + path.string());
    ofs << content;
}

std::string ProjectGenerator::build_meson_content() {
    std::string deps_section = "project_deps = [\n";

    for (const auto& d : dependencies_) {
        deps_section += std::format("  dependency('{}'),\n", d);
    }

    for (const auto& r : remotes_) {
        deps_section += std::format("  dependency('{}', fallback: ['{}', '{}_dep']),\n",
                                   r.name, r.name, r.name);
    }
    deps_section += "]\n";

    std::map<std::string, std::string> data = {
        {"PROJECT_NAME", std::string(name_)},
        {"DEPS_LIST", deps_section}
    };

    constexpr std::string_view meson_tmpl = R"(project('{{PROJECT_NAME}}', 'cpp',
  version : '0.1',
  default_options : ['cpp_std=c++20', 'warning_level=3', 'buildtype=debug'])

cpp = meson.get_compiler('cpp')
add_project_arguments(cpp.get_supported_arguments([
  '-march=native'
]), language : 'cpp')

{{DEPS_LIST}}

# This allows #include "pch.hpp" to work
inc = include_directories('include')

executable('{{PROJECT_NAME}}',
  'src/main.cpp',
  include_directories : inc,
  dependencies : project_deps,
  cpp_pch : 'pch/pch.hpp'
)
)";

    return TemplateEngine::render(meson_tmpl, data);
}

bool ProjectGenerator::generate_basic() {
    try {
        if (name_.empty()) throw std::runtime_error("Project name cannot be empty");

        fs::path root(name_);
        fs::create_directories(root / "src");
        fs::create_directories(root / "include");
        fs::create_directories(root / "pch");
        fs::create_directories(root / "subprojects");

        write_file(root / "meson.build", build_meson_content());

        for (const auto& rd : remotes_) {
            std::string wrap_content = std::format(
                "[wrap-git]\n"
                "url = {}\n"
                "revision = {}\n"
                "depth = 1\n"
                "\n"
                "[provide]\n"
                "{} = {}_dep\n",
                rd.url, rd.revision, rd.name, rd.name
            );

            fs::path wrap_path = root / "subprojects" / (rd.name + ".wrap");
            write_file(wrap_path, wrap_content);
        }

        write_file(root / "pch/pch.hpp", "#include <iostream>\n#include <vector>\n");

        write_file(root / "src/main.cpp",
                   "#include \"pch/pch.hpp\"\n\nint main() {\n    return 0;\n}\n");

        generate_gitignore(root);
        generate_vscode(root);

        ui::log(ui::Level::SUCCESS, std::format("Project '{}' created successfully.", name_));
        return true;
    } catch (const std::exception& e) {
        ui::log(ui::Level::ERROR, std::string("Generation error: ") + e.what());
        return false;
    }
}

void ProjectGenerator::generate_gitignore(const fs::path& root) {
  std::string gitignore_content = R"(# Build directories
build/
builddir/
subprojects/*
!subprojects/*.wrap

# Compiled binaries
*.exe
*.out
*.app
*.so
*.dylib
*.dll

# IDE files
.vscode/
.idea/
*.swp
*.swo
*~

# OS files
.DS_Store
Thumbs.db
)";

  write_file(root / ".gitignore", gitignore_content);
  ui::log(ui::Level::SUCCESS, "Generated .gitignore");
}

void ProjectGenerator::generate_vscode(const fs::path& root) {
    fs::create_directories(root / ".vscode");
     // settings.json
    std::string settings = R"({
  "files.associations": {
    "*.h": "cpp"
  },
  "C_Cpp.default.configurationProvider": "mesonbuild.mesonbuild"
})";
    write_file(root / ".vscode" / "settings.json", settings);

       std::string tasks = std::format(R"({{
  "version": "2.0.0",
  "tasks": [
    {{
      "label": "configure",
      "type": "shell",
      "command": "meson setup build",
      "problemMatcher": []
    }},
    {{
      "label": "build",
      "type": "shell",
      "command": "meson compile -C build",
      "group": {{
        "kind": "build",
        "isDefault": true
      }},
      "problemMatcher": ["$gcc"]
    }},
    {{
      "label": "run",
      "type": "shell",
      "command": "${{workspaceFolder}}/build/{}",
      "dependsOn": ["build"],
      "problemMatcher": []
    }}
  ]
}})", name_);
    write_file(root / ".vscode" / "tasks.json", tasks);

    
    // launch.json
    std::string launch = std::format(R"({{
  "version": "0.2.0",
  "configurations": [
    {{
      "name": "Debug",
      "type": "cppdbg",
      "request": "launch",
      "program": "${{workspaceFolder}}/build/{}",
      "args": [],
      "stopAtEntry": false,
      "cwd": "${{workspaceFolder}}",
      "environment": [],
      "externalConsole": false,
      "MIMode": "gdb",
      "preLaunchTask": "build"
    }}
  ]
}})", name_);
    write_file(root / ".vscode" / "launch.json", launch);
    
    ui::log(ui::Level::SUCCESS, "Generated .vscode configuration");
}

bool ProjectGenerator::add_target(const fs::path& root, std::string_view target_name, bool is_lib) {
    try {
        fs::path meson_file = root / "meson.build";
        if (!fs::exists(meson_file)) return false;

        std::string folder = is_lib ? "src/lib" : "src";
        fs::create_directories(root / folder);

        // Fix path to use the target name correctly
        std::string src_path = std::format("{}/{}.cpp", folder, target_name);
        std::string content = is_lib ? "// Library source\n" : "#include \"pch/pch.hpp\"\n\nint main() {\n    return 0;\n}\n";
        write_file(root / src_path, content);

        std::ofstream ofs(meson_file, std::ios::app);
        std::string build_block = is_lib ?
            std::format("\nlibrary('{}', '{}', include_directories: inc, dependencies: project_deps)\n", target_name, src_path) :
            std::format("\nexecutable('{}', '{}', include_directories: inc, dependencies: project_deps, cpp_pch: 'pch/pch.hpp')\n", target_name, src_path);

        ofs << build_block;

        ui::log(ui::Level::SUCCESS, std::format("Added {} target: {}", is_lib ? "library" : "executable", target_name));
        return true;
    } catch (const std::exception& e) {
        ui::log(ui::Level::ERROR, e.what());
        return false;
    }
}

bool ProjectGenerator::inject_dependency_string(const fs::path& root, std::string_view raw_line) {
    try {
        fs::path meson_file = root / "meson.build";
        if (!fs::exists(meson_file)) return false;

        std::ifstream ifs(meson_file);
        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        ifs.close();

        std::regex dep_regex(R"(project_deps\s*=\s*\[([\s\S]*?)\n\])");

        std::smatch match;
        if (!std::regex_search(content, match, dep_regex)) {
             ui::log(ui::Level::WARN, "Could not find 'project_deps' array in meson.build");
             return false;
        }

        std::string array_content = match[1].str();
        if (array_content.find(raw_line) != std::string::npos) {
            ui::log(ui::Level::WARN, "Dependency already exists in project_deps");
            return false;
        }

        // Build updated content
        std::string updated = std::regex_replace(
            content,
            dep_regex,
            std::string("project_deps = [\n") + array_content + std::string(raw_line) + "\n]"
        );

        write_file(meson_file, updated);
        return true;
    } catch (const std::exception& e) {
        ui::log(ui::Level::ERROR, e.what());
        return false;
    }
}

bool ProjectGenerator::inject_dependency(const fs::path& root, std::string_view dep_name) {
    std::string line = std::format("  dependency('{}'),\n", dep_name);
    bool success = inject_dependency_string(root, line);
    if (success) ui::log(ui::Level::SUCCESS, std::format("Added dependency '{}'", dep_name));
    return success;
}

bool ProjectGenerator::inject_remote(const fs::path& root, const RemoteDep& rd) {
    try {
        fs::create_directories(root / "subprojects");

        std::string wrap_content = std::format(
            "[wrap-git]\n"
            "url = {}\n"
            "revision = {}\n"
            "depth = 1\n"
            "\n"
            "[provide]\n"
            "{} = {}_dep\n",
            rd.url, rd.revision, rd.name, rd.name
        );
        write_file(root / "subprojects" / (rd.name + ".wrap"), wrap_content);

        std::string line = std::format("  dependency('{}', fallback: ['{}', '{}_dep']),\n",
                                           rd.name, rd.name, rd.name);

        bool success = inject_dependency_string(root, line);
        if (success) ui::log(ui::Level::SUCCESS, std::format("Added remote dependency '{}'", rd.name));
        return success;
    } catch (...) { return false; }
}

} // namespace fastbuild
