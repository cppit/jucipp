#include "window.h"

int main(int argc, char *argv[]) {
  Glib::RefPtr<Gtk::Application> app = Gtk::Application::create(
                                                                argc,
                                                                argv,
                                                                "no.sout.juci");

  Window window;

  //api::LoadPlugin("juci_api_test");
  return app->run(window);
}
