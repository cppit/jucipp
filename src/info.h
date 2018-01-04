#pragma once
#include <gtkmm.h>

class Info : public Gtk::InfoBar {
  Info();
public:
  static Info &get() {
    static Info instance;
    return instance;
  }
  
  void print(const std::string &text);
  
private:
  Gtk::Label label;
  sigc::connection timeout_connection;
};
