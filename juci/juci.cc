#include "window.h"
#include "logging.h"

int main(int argc, char *argv[]) {
  Glib::RefPtr<Gtk::Application> app = Gtk::Application::create(
                                                                argc,
                                                                argv,
                                                                "no.sout.juci");

  add_file_log("juci.log");
  INFO("Logging initalized");
  Window window;
  return app->run(window);
}
