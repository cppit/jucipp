#include "juci.h"



int main(int argc, char *argv[]) {
  Glib::RefPtr<Gtk::Application> app = Gtk::Application::create(
                                                                argc,
                                                                argv,
                                                                "no.sout.juci");
  // app->set_flags(Gio::APPLICATION_NON_UNIQUE);

  Window window;
  return app->run(window);
}
