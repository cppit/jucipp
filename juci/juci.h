#include "window.h"
#include "logging.h"

class Juci : public Gtk::Application {
public:
  Juci(): Gtk::Application("no.sout.juci", Gio::APPLICATION_HANDLES_COMMAND_LINE) {}
  
  int on_command_line(const Glib::RefPtr<Gio::ApplicationCommandLine> &cmd);
  void on_activate();

private:
  std::unique_ptr<Window> window;
  std::string directory;
  std::vector<std::string> files;
};
