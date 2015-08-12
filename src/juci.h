#ifndef JUCI_JUCI_H_
#define JUCI_JUCI_H_

#include "window.h"
#include "logging.h"

class Juci : public Gtk::Application {
public:
  Juci(): Gtk::Application("no.sout.juci", Gio::APPLICATION_NON_UNIQUE | Gio::APPLICATION_HANDLES_COMMAND_LINE) {}
  
  int on_command_line(const Glib::RefPtr<Gio::ApplicationCommandLine> &cmd);
  void on_activate();

private:
  std::unique_ptr<Window> window;
  std::string directory;
  std::vector<std::string> files;
};

#endif // JUCI_JUCI_H_