#include "dialogs.h"

Dialog::Message::Message(const std::string &text): Gtk::MessageDialog(text, false, Gtk::MessageType::MESSAGE_INFO, Gtk::ButtonsType::BUTTONS_NONE, true) {}

bool Dialog::Message::on_delete_event(GdkEventAny *event) {
  return true;
}
