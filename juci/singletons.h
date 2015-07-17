#ifndef JUCI_SINGLETONS_H_
#define JUCI_SINGLETONS_H_

#include "keybindings.h"
#include "source.h"
#include "directories.h"
#include "terminal.h"
#include "notebook.h"
#include "menu.h"

class Singleton {
public:
  class Config {
  public:
    static Source::Config *source() {return source_.get();}
    static Terminal::Config *terminal() {return terminal_.get();}
    static Directories::Config *directories() {return directories_.get();}
    static Keybindings::Config *keybindings() {return keybindings_.get();}
  private:
    static std::unique_ptr<Source::Config> source_;
    static std::unique_ptr<Terminal::Config> terminal_;
    static std::unique_ptr<Directories::Config> directories_;
    static std::unique_ptr<Keybindings::Config> keybindings_;
  };
  
  static Terminal::Controller *terminal();
  static Keybindings *keybindings();
  static Notebook::Controller *notebook();
  static Menu::Controller *menu();
private:
  static std::unique_ptr<Terminal::Controller> terminal_;
  static std::unique_ptr<Keybindings> keybindings_;
  static std::unique_ptr<Notebook::Controller> notebook_;
  static std::unique_ptr<Menu::Controller> menu_;
};

#endif // JUCI_SINGLETONS_H_
