#ifndef JUCI_JUCI_H_
#define JUCI_JUCI_H_

#include <gtkmm.h>
#include "logging.h"
#include "window.h"

class Application : public Gtk::Application {
public:
  Application();
  int on_command_line(const Glib::RefPtr<Gio::ApplicationCommandLine> &cmd) override;
  void on_activate() override;
  void on_startup() override;
  std::unique_ptr<Window> window;
private:
  std::vector<boost::filesystem::path> directories;
  std::vector<boost::filesystem::path> files;
  void init_logging();
};

#endif // JUCI_JUCI_H_

