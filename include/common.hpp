#pragma once
#include <iostream>
#include <string_view>

namespace fastbuild::ui {
  enum class Level { INFO, SUCCESS, ERROR, WARN };

  inline void log(Level level, std::string_view msg) {
    switch (level) {
            case Level::INFO:    std::cout << "\033[1;34m[INFO] \033[0m"; break;
            case Level::SUCCESS: std::cout << "\033[1;32m[OK]   \033[0m"; break;
            case Level::WARN:    std::cout << "\033[1;33m[WARN] \033[0m"; break;
            case Level::ERROR:   std::cerr << "\033[1;31m[ERR]  \033[0m"; break;
        }
        std::cout << msg << std::endl;
  }
}
