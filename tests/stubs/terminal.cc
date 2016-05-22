#include "terminal.h"

Terminal::Terminal() {}

bool Terminal::on_motion_notify_event(GdkEventMotion* motion_event) {return false;}
bool Terminal::on_button_press_event(GdkEventButton* button_event) {return false;}
bool Terminal::on_key_press_event(GdkEventKey *event) {return false;}

size_t Terminal::print(const std::string &message, bool bold) {
  return 0;
}
