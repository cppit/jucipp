#include "singletons.h"

std::unique_ptr<Source::Config> Singleton::Config::source_=std::unique_ptr<Source::Config>(new Source::Config());
std::unique_ptr<Directories::Config> Singleton::Config::directories_=std::unique_ptr<Directories::Config>(new Directories::Config());
std::unique_ptr<Terminal::Config> Singleton::Config::terminal_=std::unique_ptr<Terminal::Config>(new Terminal::Config());
std::unique_ptr<Window::Config> Singleton::Config::window_ = std::unique_ptr<Window::Config>(new Window::Config());
std::unique_ptr<Menu::Config> Singleton::Config::menu_ = std::unique_ptr<Menu::Config>(new Menu::Config());

std::unique_ptr<Terminal> Singleton::terminal_=std::unique_ptr<Terminal>();
Terminal *Singleton::terminal() {
  if(!terminal_)
    terminal_=std::unique_ptr<Terminal>(new Terminal());
  return terminal_.get();
}

std::unique_ptr<Directories> Singleton::directories_=std::unique_ptr<Directories>();
Directories *Singleton::directories() {
  if(!directories_)
    directories_=std::unique_ptr<Directories>(new Directories());
  return directories_.get();
}

std::unique_ptr<Gtk::Label> Singleton::status_=std::unique_ptr<Gtk::Label>();
Gtk::Label *Singleton::status() {
  if(!status_)
    status_=std::unique_ptr<Gtk::Label>(new Gtk::Label());
  return status_.get();
}

std::unique_ptr<Gtk::Label> Singleton::info_=std::unique_ptr<Gtk::Label>();
Gtk::Label *Singleton::info() {
  if(!info_)
    info_=std::unique_ptr<Gtk::Label>(new Gtk::Label());
  return info_.get();
}

std::unique_ptr<Menu> Singleton::menu_=std::unique_ptr<Menu>();
Menu *Singleton::menu() {
  if(!menu_)
    menu_=std::unique_ptr<Menu>(new Menu());
  return menu_.get();
}
