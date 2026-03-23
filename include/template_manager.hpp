#pragma once
#include <string>
#include <map>

namespace fastbuild {
  class TemplateEngine {
    public:
       static std::string render(std::string_view template_str,
                                 const std::map<std::string, std::string>& data) {
            std::string result{template_str};
            for (const auto& [key, value] : data) {
                std::string token = "{{" + key + "}}";
                size_t pos = 0;
                while ((pos = result.find(token, pos)) != std::string::npos) {
                    result.replace(pos, token.length(), value);
                    pos += value.length();
                }
            }
            return result;
      }
  };
}
