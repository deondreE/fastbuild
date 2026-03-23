#include "project_generator.hpp"
#include "common.hpp"
#include "template_manager.hpp"
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

        write_file(root / "meson.build", build_meson_content());
        write_file(root / "pch/pch.hpp", "#include <iostream>\n#include <vector>\n");

        write_file(root / "src/main.cpp",
                   "#include \"pch/pch.hpp\"\n\nint main() {\n    return 0;\n}\n");

        ui::log(ui::Level::SUCCESS, std::format("Project '{}' created successfully.", name_));
        return true;
    } catch (const std::exception& e) {
        ui::log(ui::Level::ERROR, std::string("Generation error: ") + e.what());
        return false;
    }
}

void ProjectGenerator::generate_raylib() {
    add_dependency("raylib");

    if (!generate_basic()) return;

    try {
        std::string raylib_main = R"(#include "pch.hpp"
#include <raylib.h>

int main() {
    InitWindow(800, 450, "FastBuild Raylib Game");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("Congrats! Your Raylib game is running!", 190, 200, 20, LIGHTGRAY);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
)";
        fs::path main_path = fs::path(name_) / "src" / "main.cpp";
        write_file(main_path, raylib_main);
    } catch (const std::exception& e) {
        ui::log(ui::Level::ERROR, e.what());
    }
}

bool ProjectGenerator::inject_dependency(const fs::path& root, std::string_view dep) {
    try {
        fs::path meson_file = root / "meson.build";
        if (!fs::exists(meson_file)) return false;

        std::ifstream ifs(meson_file);
        if (!ifs) return false;

        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        ifs.close();

        std::regex dep_regex(R"((project_deps\s*=\s*\[)([^\]]*)(\]))");
        std::smatch match;

        if (!std::regex_search(content, match, dep_regex)) {
             ui::log(ui::Level::WARN, "Could not find 'project_deps' array in meson.build");
             return false;
        }

        std::string updated = match[1].str() + match[2].str() +
                              std::format("  dependency('{}'),\n", dep) +
                              match[3].str();

        // This works only if write_file is static!
        write_file(meson_file, updated);
        return true;
    } catch (const std::exception& e) {
        ui::log(ui::Level::ERROR, e.what());
        return false;
    }
}

} // namespace fastbuild
