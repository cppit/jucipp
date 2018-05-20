#pragma once
#include <string>

namespace Documentation {
  class CppReference {
  public:
    static std::string get_url(const std::string &symbol) noexcept;
  };
}
