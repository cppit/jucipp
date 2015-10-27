#ifndef JUCI_SINGLETONS_H_
#define JUCI_SINGLETONS_H_

#include <gtkmm.h>

#include "filesystem.h"
#include "source.h"
#include "window.h"
#include "directories.h"
#include "terminal.h"
#include "notebook.h"
#include "menu.h"
#include "window.h"
#include "config.h"

class Singleton {
public:
  class Config {
  public:
    static Source::Config *source() {return source_.get();}
    static Directories::Config *directories() {return directories_.get();}
    static Window::Config *window() { return window_.get(); }
    static Terminal::Config *terminal() {return terminal_.get();}
    static Menu::Config *menu() {return menu_.get();}
    static MainConfig *main() { return main_.get(); }

  private:
    static std::unique_ptr<Source::Config> source_;
    static std::unique_ptr<Window::Config> window_;
    static std::unique_ptr<Directories::Config> directories_;
    static std::unique_ptr<Terminal::Config> terminal_;
    static std::unique_ptr<Menu::Config> menu_;
    static std::unique_ptr<MainConfig> main_;
  };
  static Terminal *terminal();
  static Directories *directories();
  static Gtk::Label *status();
  static Gtk::Label *info();
  static Menu *menu();
  static Window *window();

private:
  static std::unique_ptr<Terminal> terminal_;
  static std::unique_ptr<Gtk::Label> status_;
  static std::unique_ptr<Gtk::Label> info_;
  static std::unique_ptr<Directories> directories_;
  static std::unique_ptr<Menu> menu_;
  static std::unique_ptr<Window> window_;
};

#endif // JUCI_SINGLETONS_H_
