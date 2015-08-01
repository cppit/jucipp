#ifndef JUCI_SINGLETONS_H_
#define JUCI_SINGLETONS_H_

#include "source.h"
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
    static Terminal::Config *terminal() {return terminal_.get();}
    static Directories::Config *directories() {return directories_.get();}
  private:
    static std::unique_ptr<Source::Config> source_;
    static std::unique_ptr<Terminal::Config> terminal_;
    static std::unique_ptr<Directories::Config> directories_;
  };
  static std::string config_dir() { return std::string(getenv("HOME")) + "/.juci/config/"; }
  static std::string log_dir() { return std::string(getenv("HOME")) + "/.juci/log/"; }
  static Terminal *terminal();
  static Gtk::Label *status();
private:
  static std::unique_ptr<Terminal> terminal_;
  static std::unique_ptr<Gtk::Label> status_;
};

#endif // JUCI_SINGLETONS_H_
