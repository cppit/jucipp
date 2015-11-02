#include "singletons.h"

#include <iostream> //TODO: remove
using namespace std; //TODO: remove

std::unique_ptr<Config> Singleton::config;
std::unique_ptr<Menu> Singleton::menu;
std::unique_ptr<Terminal> Singleton::terminal;
std::unique_ptr<Directories> Singleton::directories;
std::unique_ptr<Gtk::Label> Singleton::info;
std::unique_ptr<Gtk::Label> Singleton::status;

void Singleton::init() {
  config=std::unique_ptr<Config>(new Config());
  menu=std::unique_ptr<Menu>(new Menu());
  terminal=std::unique_ptr<Terminal>(new Terminal());
  directories=std::unique_ptr<Directories>(new Directories());
  info=std::unique_ptr<Gtk::Label>(new Gtk::Label());
  status=std::unique_ptr<Gtk::Label>(new Gtk::Label());
}
