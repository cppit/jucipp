#ifndef JUCI_JUCI_H_
#define JUCI_JUCI_H_

#include "window.h"
#include "logging.h"

class app : public Gtk::Application {
 public:
  app();
  int on_command_line(const Glib::RefPtr<Gio::ApplicationCommandLine> &cmd);
  void on_activate();

 private:
  std::unique_ptr<Window> window;
  std::vector<boost::filesystem::path> directories;
  std::vector<boost::filesystem::path> files;
  void init_logging();
};

#endif // JUCI_JUCI_H_

