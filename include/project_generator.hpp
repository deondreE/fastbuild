#pragma once
#include <vector>
#include <filesystem>

namespace fastbuild {
  namespace fs = std::filesystem;

  struct RemoteDep {
    std::string name;
    std::string url;
    std::string revision;
  };

  class ProjectGenerator {
  public:
    explicit ProjectGenerator(std::string_view name) : name_(name) {}

    void add_dependency(std::string_view dep) { dependencies_.emplace_back(std::string(dep));}
    void add_remote(RemoteDep&& dep) { remotes_.push_back(std::move(dep)); }

    bool generate_basic();
    void generate_raylib();

    static bool inject_dependency(const fs::path& root, std::string_view dep);
  private:
    std::string name_;
    std::vector<std::string> dependencies_;
    std::vector<RemoteDep> remotes_;

    static void write_file(const fs::path& path, std::string_view content);
    std::string build_meson_content();
  };
}
