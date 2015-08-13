#ifndef JUCI_THEME_H_
#define JUCI_THEME_H_

#include <string>

namespace Theme {
  class Config {
  public:
    std::string current_theme() { return theme + "/" + main; }
    std::string theme, main;
  };
}

#endif  // JUCI_THEME_H_
