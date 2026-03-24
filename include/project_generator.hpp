#pragma once
#include <string_view>
#include <vector>
#include <filesystem>

namespace fastbuild {
  namespace fs = std::filesystem;

  struct RemoteDep {
    std::string name;
    std::string url;
    std::string revision;
  };

  enum class LicenseType {
    MIT,
    Apache2,
    GPL3,
    Unlicense
  };
  
  class ProjectGenerator {
  public:
    explicit ProjectGenerator(std::string_view name) : name_(name) {}

    void add_dependency(std::string_view dep) { dependencies_.emplace_back(std::string(dep));}
    void add_remote(RemoteDep&& dep) { remotes_.push_back(std::move(dep)); }
    static bool add_target(const fs::path& root, std::string_view target_name, bool is_lib);

    bool generate_basic();
    void generate_raylib();
    void generate_gitignore(const fs::path& root);
    void generate_vscode(const fs::path& root);
    void generate_clang_format(const fs::path& root);
    void generate_editor_config(const fs::path& root);
    void generate_license(const fs::path& root, LicenseType type);

    static bool inject_dependency(const fs::path& root, std::string_view dep);
    static bool inject_remote(const fs::path& root, const RemoteDep& rd);
    static bool inject_dependency_string(const fs::path& root, std::string_view raw_dep_line);
  private:
    std::string name_;
    std::vector<std::string> dependencies_;
    std::vector<RemoteDep> remotes_;

    static void write_file(const fs::path& path, std::string_view content);
    void write_wrap_file(const fs::path& path, const RemoteDep& rd);
    std::string sythesize_meson(std::string_view name);
   static std::string inject_dep_into_executable(const std::string& content, const std::string& dep_var);
    std::string build_meson_content();
  };
}
