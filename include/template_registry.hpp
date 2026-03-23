#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace fastbuild {
  namespace fs = std::filesystem;

  struct TemplateFile {
    std::string target_path;
    std::string content;
  };

  struct ProjectTemplate {
    std::string name;
    std::vector<TemplateFile> files;
    std::vector<std::string> default_deps;
  };

  class TemplateRegistry {
  public:
    explicit TemplateRegistry(const fs::path& executable_path);
    const ProjectTemplate* get_template(const std::string& name) const;
    std::vector<std::string> list_templates() const;

  private:
    std::map<std::string, ProjectTemplate> templates_;
    void load_from_disk(const fs::path& root);
  };
}
