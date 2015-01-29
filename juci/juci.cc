#include "view.h"

int main (int argc, char *argv[]) {
    Glib::RefPtr <Gtk::Application> app = Gtk::Application::create(argc, argv, "org.bachelor.juci");

    View view;

    return app->run(view);
}