
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
  static std::string config_dir() { return std::string(getenv("HOME")) + "/.juci/config/"; }
  static std::string log_dir() { return std::string(getenv("HOME")) + "/.juci/log/"; }
  static std::string style_dir() { return std::string(getenv("HOME")) + "/.juci/styles/"; }
  static Terminal *terminal();
  static Gtk::Label *status();
  static Gtk::Label *info();
private:
  static std::unique_ptr<Terminal> terminal_;
  static std::unique_ptr<Gtk::Label> status_;
  static std::unique_ptr<Gtk::Label> info_;
};

#endif // JUCI_SINGLETONS_H_
