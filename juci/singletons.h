#ifndef JUCI_SINGLETONS_H_
#define JUCI_SINGLETONS_H_

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
  private:
    static std::unique_ptr<Source::Config> source_;
    static std::unique_ptr<Terminal::Config> terminal_;
    static std::unique_ptr<Directories::Config> directories_;
  };
  
  static Terminal::Controller *terminal();
  static Menu *menu();
private:
  static std::unique_ptr<Terminal::Controller> terminal_;
  static std::unique_ptr<Menu> menu_;
};

#endif // JUCI_SINGLETONS_H_
