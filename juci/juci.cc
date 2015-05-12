#include "window.h"
#include "logging.h"

int main(int argc, char *argv[]) {
  Glib::RefPtr<Gtk::Application> app = Gtk::Application::create(
                                                                argc,
                                                                argv,
                                                                "no.sout.juci");

  init_logging();
  Window window;
  return app->run(window);
}
