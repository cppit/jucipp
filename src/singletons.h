#ifndef JUCI_SINGLETONS_H_
#define JUCI_SINGLETONS_H_

#include <gtkmm.h>
#include "directories.h"
#include "terminal.h"
#include "menu.h"
#include "config.h"

class Singleton {
public:  
  static std::unique_ptr<Config> config;
  static std::unique_ptr<Menu> menu;
  static std::unique_ptr<Terminal> terminal;
  static std::unique_ptr<Directories> directories;
  static std::unique_ptr<Gtk::Label> info;
  static std::unique_ptr<Gtk::Label> status;
  
  static void init();
};

#endif // JUCI_SINGLETONS_H_
