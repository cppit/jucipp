#include "juci.h"
int main(int argc, char *argv[]) {
  Glib::RefPtr<Gtk::Application> app = Gtk::Application::create(
								argc,
								argv,
								"no.sout.juci"
								);
  Window window;
  return app->run(window);;

}

////////////////
//// Window ////
////////////////
std::shared_ptr<Source::Controller> Window::source() {
  return std::shared_ptr<Source::Controller>(&source_);
}
