#include "dialogs.h"

Dialog::Message::Message(const std::string &text): Gtk::Window(Gtk::WindowType::WINDOW_POPUP) {}

bool Dialog::Message::on_delete_event(GdkEventAny *event) {
  return true;
}
