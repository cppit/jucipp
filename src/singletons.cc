#include "singletons.h"

std::unique_ptr<Source::Config> Singleton::Config::source_=std::unique_ptr<Source::Config>(new Source::Config());
std::unique_ptr<Directories::Config> Singleton::Config::directories_=std::unique_ptr<Directories::Config>(new Directories::Config());
std::unique_ptr<Terminal::Config> Singleton::Config::terminal_=std::unique_ptr<Terminal::Config>(new Terminal::Config());
std::unique_ptr<Window::Config> Singleton::Config::window_ = std::unique_ptr<Window::Config>(new Window::Config());
std::unique_ptr<Terminal> Singleton::terminal_=std::unique_ptr<Terminal>();
std::unique_ptr<Directories> Singleton::directories_=std::unique_ptr<Directories>();
std::unique_ptr<Window> Singleton::window_=std::unique_ptr<Window>();

Terminal *Singleton::terminal() {
  if(!terminal_)
    terminal_=std::unique_ptr<Terminal>(new Terminal());
  return terminal_.get();
}

Directories *Singleton::directories() {
  if(!directories_)
    directories_=std::unique_ptr<Directories>(new Directories());
  return directories_.get();
}

Window *Singleton::window() {
  if(!window_)
    window_=std::unique_ptr<Window>(new Window());
  return window_.get();
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

boost::filesystem::path Singleton::create_config_path(const std::string &subfolder) {
  auto home = filesystem::get_home_folder();
  if(home.empty()) {
    Singleton::terminal()->print("Could not find/write to home directory. Using defaults, no settings will be saved.");
    home = filesystem::get_tmp_folder();
    if(home.empty()) {
      std::string message("Please fix permissions of your home folder");
      std::cerr << message << std::endl;
      JFATAL(message);
      throw new std::exception;
    }
  }
  home /= subfolder;
  return home;
}

boost::filesystem::path Singleton::config_dir() { return create_config_path(".juci/config"); }
boost::filesystem::path Singleton::log_dir() { return create_config_path(".juci/log"); }
boost::filesystem::path Singleton::style_dir() { return create_config_path(".juci/styles"); }
std::string Singleton::config_json() {
  auto conf_dir = Singleton::config_dir();
  conf_dir /= "config.json"; // to ensure correct paths on windows
  return conf_dir.string();
}

