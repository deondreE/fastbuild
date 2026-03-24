#include "project_generator.hpp"
#include "common.hpp"
#include "template_manager.hpp"
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string_view>
#include <format>
#include <regex>
#include <chrono>
#include <map>

namespace fastbuild {

std::string glob_cpp_files(const fs::path& root, const std::string& folder) {
    std::string files_list = "files(\n";
    fs::path search_path = root / folder;

    if (fs::exists(search_path)) {
        for (const auto& entry : fs::recursive_directory_iterator(search_path)) {
            if (entry.path().extension() == ".cpp") {
                std::string relative_path = fs::relative(entry.path(), root).string(); 
                files_list += std::format("   '{}',\n", relative_path);
            }
        }
    }
    files_list += ")";
    return files_list;
}

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
        {"DEPS_LIST", deps_section},
        {"FILES", glob_cpp_files(name_, "src")},
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
  {{FILES}},
  include_directories : inc,
  dependencies : project_deps,
  cpp_pch : 'pch/pch.hpp'
)
)";

    return TemplateEngine::render(meson_tmpl, data);
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

void ProjectGenerator::generate_clang_format(const fs::path& root) {
    std::string clang_format = R"(---
Language: Cpp
BasedOnStyle: LLVM
IndentWidth: 4
TabWidth: 4
UseTab: Never
ColumnLimit: 100
BreakBeforeBraces: Attach
AllowShortIfStatementsOnASingleLine: false
AllowShortLoopsOnASingleLine: false
AllowShortFunctionsOnASingleLine: Empty
IndentCaseLabels: true
PointerAlignment: Left
ReferenceAlignment: Left
NamespaceIndentation: None
AlwaysBreakTemplateDeclarations: Yes
SpaceAfterCStyleCast: false
SpaceBeforeParens: ControlStatements
AlignConsecutiveAssignments: false
AlignConsecutiveDeclarations: false
)";

    write_file(root / ".clang-format", clang_format);
    ui::log(ui::Level::SUCCESS, "Generated .clang-format");
}

void ProjectGenerator::generate_editor_config(const fs::path& root) {
    std::string editor_config = R"(root = true

[*]
charset = utf-8
end_of_line = lf
insert_final_newline = true
trim_trailing_whitespace = true

[*.{cpp,hpp,h,c,cc,cxx}]
indent_style = space
indent_size = 4

[*.{json,yml,yaml}]
indent_style = space
indent_size = 2

[Makefile]
indent_style = tab

[meson.build]
indent_style = space
indent_size = 2
)";

    write_file(root / ".editor_config", editor_config);
    ui::log(ui::Level::SUCCESS, "Generated .editorconfig");
}

void ProjectGenerator::generate_license(const fs::path& root, LicenseType type) {
    std::string license_content;
    std::string license_name;    
    auto now = std::chrono::system_clock::now();
    auto year = std::chrono::year_month_day{
        std::chrono::floor<std::chrono::days>(now)
    }.year();
    auto year_str = std::format("{}", static_cast<int>(year));
    
    switch (type) {
        case LicenseType::MIT:
            license_name = "MIT";
            license_content = std::format(R"(MIT License

Copyright (c) {} [Your Name]

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
)", year);
            break;
            
        case LicenseType::Apache2:
            license_name = "Apache-2.0";
            license_content = std::format(R"(Apache License
Version 2.0, January 2004
http://www.apache.org/licenses/

Copyright {} [Your Name]

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
)", year);
            break;
            
        case LicenseType::GPL3:
            license_name = "GPL-3.0";
            license_content = std::format(R"(GNU GENERAL PUBLIC LICENSE
Version 3, 29 June 2007

Copyright (C) {} [Your Name]

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
)", year);
            break;
            
        case LicenseType::Unlicense:
            license_name = "Unlicense";
            license_content = R"(This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <https://unlicense.org>
)";
            break;
    }
    
    write_file(root / "LICENSE", license_content);
    ui::log(ui::Level::SUCCESS, std::format("Generated {} license", license_name));
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

        std::string pch_content = R"(
#include <vector>
#include <iostream>

#ifdef _WIN32
    #ifdef BUILDING_DLL
        #define DLL_API __declspec(dllexport)
    #else
        #define DLL_API __declspec(dllimport)
    #endif
#else
    #define DLL_API __attribute__((visibility("default")))
#endif
        )";
        write_file(root / "pch/pch.hpp", pch_content);

        write_file(root / "src/main.cpp",
                   "#include \"pch/pch.hpp\"\n\nint main() {\n    return 0;\n}\n");

        generate_gitignore(root);
        generate_vscode(root);
        generate_clang_format(root);
        generate_editor_config(root);
        generate_license(root, LicenseType::MIT);

        ui::log(ui::Level::SUCCESS, std::format("Project '{}' created successfully.", name_));
        return true;
    } catch (const std::exception& e) {
        ui::log(ui::Level::ERROR, std::string("Generation error: ") + e.what());
        return false;
    }
}

bool ProjectGenerator::add_target(const fs::path& root, std::string_view target_name, bool is_lib) {
    try {
        fs::path meson_file = root / "meson.build";
        if (!fs::exists(meson_file)) return false;

        std::string folder = is_lib ? "src/lib" : "src";
        fs::create_directories(root / folder);

        std::string src_path = std::format("{}/{}.cpp", folder, target_name);
        std::string content = is_lib ? "// Library source\n" : "#include \"pch/pch.hpp\"\n\nint main() {\n    return 0;\n}\n";
        write_file(root / src_path, content);

        std::ifstream ifs(meson_file);
        std::string existing_content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        ifs.close();

        if (is_lib) {
            std::string dep_var = std::format("{}_dep", target_name);
            std::string lib_build_block = std::format(
                "{0}_lib = library('{1}', '{2}', include_directories: inc, dependencies: project_deps)\n"
                "{0}_dep = declare_dependency(link_with: {0}_lib, include_directories: inc)\n\n",
                target_name, target_name, src_path);

            size_t exe_pos = existing_content.find("executable(");
            if (exe_pos != std::string::npos) {
                existing_content.insert(exe_pos, lib_build_block);

                std::regex dep_pattern(R"(dependencies\s*:\s*(?:\[([^\]]*)\]|([a-zA-Z0-9_]+)))");
                std::smatch match;
                std::string exe_part = existing_content.substr(exe_pos + lib_build_block.length());
                
                if (std::regex_search(exe_part, match, dep_pattern)) {
                    std::string new_dep_list;
                    
                    if (match[1].matched) { // It was already an array [item1, item2]
                        std::string current_items = match[1].str();
                        if (current_items.find(dep_var) == std::string::npos) {
                            std::string sep = (current_items.empty() || current_items.back() == ' ') ? "" : ", ";
                            new_dep_list = std::format("[{}{}{}]", current_items, sep, dep_var);
                        }
                    } else if (match[2].matched) { // It was a single variable like project_deps
                        std::string var_name = match[2].str();
                        if (var_name != dep_var) {
                            new_dep_list = std::format("[{}, {}]", var_name, dep_var);
                        }
                    }

                    if (!new_dep_list.empty()) {
                        std::string full_replacement = "dependencies : " + new_dep_list;
                        exe_part.replace(match.position(), match.length(), full_replacement);
                        
                        // Reconstruct the full content
                        existing_content = existing_content.substr(0, exe_pos + lib_build_block.length()) + exe_part;
                    }
                }
            } else {
                existing_content += "\n" + lib_build_block;
            }
        } else {
            // Adding a standard executable
            std::string exe_block = std::format(
                "\nexecutable('{}',\n  '{}',\n  include_directories : inc,\n  dependencies : [project_deps],\n  cpp_pch : 'pch/pch.hpp'\n)\n",
                target_name, src_path);
            existing_content += exe_block;
        }

        std::ofstream ofs(meson_file, std::ios::trunc);
        ofs << existing_content;

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
        fs::path subprojects_dir = root / "subprojects";
        fs::path patch_dir = subprojects_dir / "packagefiles" / rd.name;
        fs::create_directories(patch_dir);

        std::string wrap_content = std::format(
            "[wrap-git]\n"
            "url = {}\n"
            "revision = {}\n"
            "depth = 1\n"
            "patch_directory = {}\n" 
            "\n"
            "[provide]\n"
            "{} = {}_dep\n",
            rd.url, rd.revision, rd.name, rd.name, rd.name
        );
        write_file(subprojects_dir / (rd.name + ".wrap"), wrap_content);

        std::string synth_meson;
        if (rd.name == "imgui") {
            synth_meson = R"(project('imgui', 'cpp', version : '1.89')
cpp = meson.get_compiler('cpp')
imgui_inc = include_directories('.', 'backends')
imgui_deps = [dependency('vulkan')]
imgui_src = files(
    'imgui.cpp',
    'imgui_draw.cpp',
    'imgui_widgets.cpp',
    'imgui_tables.cpp',
    'imgui_demo.cpp',
    'backends/imgui_impl_glfw.cpp',
    'backends/imgui_impl_vulkan.cpp'
)

if host_machine.system() == 'windows'
    imgui_src += files('backends/imgui_impl_dx12.cpp', 'backends/imgui_impl_win32.cpp')
    imgui_deps += [cpp.find_library('d3d12'), cpp.find_library('dxgi'), cpp.find_library('d3dcompiler')]
endif

imgui_lib = static_library('imgui',
    imgui_src,
    include_directories : imgui_inc,
    dependencies : imgui_deps
)

imgui_dep = declare_dependency(
    link_with : imgui_lib,
    include_directories : imgui_inc,
    dependencies : imgui_deps
)
)";
        }
        else if (rd.name == "stb") {
         synth_meson = R"(project('stb', 'cpp', version : '1.0')
stb_inc = include_directories('.')
stb_dep = declare_dependency(include_directories : stb_inc)
)";
        }
        else {
            synth_meson = std::format(
                "project('{0}', 'cpp', version : '0.1')\n"
                "{0}_inc = include_directories('.')\n"
                "{0}_dep = declare_dependency(include_directories : {0}_inc)\n",
                rd.name
            );
        }

         write_file(patch_dir / "meson.build", synth_meson);

        std::string line = std::format("  dependency('{}', fallback: ['{}', '{}_dep']),\n",
                                           rd.name, rd.name, rd.name);

        return inject_dependency_string(root, line);
    } catch (const std::exception& e) {
        ui::log(ui::Level::ERROR, e.what());
        return false;
    }
}

} // namespace fastbuild
