
#ifndef JUCI_SINGLETONS_H_
#define JUCI_SINGLETONS_H_

#include "source.h"
#include "window.h"
#include "directories.h"
#include "terminal.h"
#include "notebook.h"
#include "menu.h"
#include <gtkmm.h>
#include <string>
#include "filesystem.h"
#include <iostream>

class Singleton {
public:
  class Config {
  public:
    static Source::Config *source() {return source_.get();}
    static Directories::Config *directories() {return directories_.get();}
    static Window::Config *window() { return window_.get(); }
    static Terminal::Config *terminal() {return terminal_.get();}

  private:
    static std::unique_ptr<Source::Config> source_;
    static std::unique_ptr<Window::Config> window_;
    static std::unique_ptr<Directories::Config> directories_;
    static std::unique_ptr<Terminal::Config> terminal_;
  };
  static std::string create_config_path(const std::string &subfolder);
  static std::string config_dir();
  static std::string log_dir();
  static std::string style_dir();
  static Terminal *terminal();
  static Directories *directories();
  static Gtk::Label *status();
  static Gtk::Label *info();
private:
  static std::unique_ptr<Terminal> terminal_;
  static std::unique_ptr<Gtk::Label> status_;
  static std::unique_ptr<Gtk::Label> info_;
  static std::unique_ptr<Directories> directories_;
};

#endif // JUCI_SINGLETONS_H_
