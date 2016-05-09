#ifndef JUCI_INFO_H_
#define JUCI_INFO_H_

#include <gtkmm.h>

class Info : public Gtk::InfoBar {
  Info();
public:
  static Info &get() {
    static Info instance;
    return instance;
  }
  
  void print(const std::string &text);
  bool enabled=true;
  
private:
  Gtk::Label label;
  sigc::connection timeout_connection;
};

#endif // JUCI_INFO_H_