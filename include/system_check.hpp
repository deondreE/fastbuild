#include <iostream>
#include <string>
#include <array>
#include <memory>
#include <format>
#include <regex>

namespace fastbuild::cli {
  struct CheckResult {
    std::string name;
    bool found;
    std::string version;
    std::string requirement;
  };

  std::string exec(const char* cmd) {
        std::array<char, 128> buffer;
        std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
        if (!pipe) return "";
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }
        return result;
  }

  void run_doctor() {
        std::cout << "\033[1;35m[ FastBuild Doctor ]\033[0m Checking environment...\n\n";

        auto check = [](std::string name, std::string cmd, std::string reg, std::string min_ver = "") {
            std::string output = exec((cmd + " --version 2>&1").c_str());
            std::regex v_regex(reg);
            std::smatch match;
            bool found = std::regex_search(output, match, v_regex);
            
            std::string ver = found ? match[1].str() : "Not Found";
            std::string status = found ? "\033[1;32m[OK]\033[0m" : "\033[1;31m[MISSING]\033[0m";
            
            std::cout << std::format("{} {:<10} Version: {:<15}", status, name, ver);
            if (!min_ver.empty() && found) {
                if (ver < min_ver) std::cout << std::format(" \033[1;33m(Requires {}+)\033[0m", min_ver);
            }
            std::cout << "\n";
        };

        check("Meson", "meson", R"(version\s*([\d\.]+) )");
        check("Ninja", "ninja", R"(([\d\.]+))");
        check("G++", "g++", R"(\(GCC\)\s*([\d\.]+))", "13.0.0");
        check("GDB", "gdb", R"(GDB\)\s*([\d\.]+))");
        check("Git", "git", R"(version\s*([\d\.]+))");
        
        std::cout << "\nReady to build.\n";
    }
}
