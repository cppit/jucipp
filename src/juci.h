#ifndef JUCI_JUCI_H_
#define JUCI_JUCI_H_

#include <gtkmm.h>
#include <boost/filesystem.hpp>

class Application : public Gtk::Application {
public:
  Application();
  int on_command_line(const Glib::RefPtr<Gio::ApplicationCommandLine> &cmd) override;
  void on_activate() override;
  void on_startup() override;
private:
  std::vector<boost::filesystem::path> directories;
  std::vector<boost::filesystem::path> files;
  std::vector<std::string> errors;
};

#endif // JUCI_JUCI_H_

