#include "window.h"
#include "logging.h"

void init_logging() {
  add_common_attributes();
  add_file_log(keywords::file_name = "juci.log",
               keywords::auto_flush = true);
  INFO("Logging initalized");
}

int main(int argc, char *argv[]) {
  Glib::RefPtr<Gtk::Application> app = Gtk::Application::create(
                                                                argc,
                                                                argv,
                                                                "no.sout.juci");
  init_logging();
  Window window;
  return app->run(window);
}
