#pragma once

#include <string>
#include <vector>
#include <filesystem>

enum class ProjectTemplate {
  BASIC,
  RAYLIB,
  LINUX_APP
};

struct RemoteDep {
    std::string name;
    std::string url;
    std::string revision;
};

class ProjectGenerator {
public:
ProjectGenerator(const std::string& name,
                const std::vector<std::string>& local_deps,
                const std::vector<RemoteDep>& remote_deps);
    bool generate();

    static bool add_local_dep(const std::string& projectRoot, const std::string& depName);
    static bool add_remote_dep(const std::string& projectRoot, const RemoteDep& rd);

private:
   std::string projectName;
   std::vector<std::string> dependencies;
   std::vector<RemoteDep> remoteDeps;

   void create_directories();
   void create_meson_file();
   void create_dummy_source();
   void create_dx_configs();
   void create_editor_configs();
   void create_gitignore();
   void fetch_remote_wraps();
   void setup_raylib_deps();
};
