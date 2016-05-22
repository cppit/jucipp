#include "terminal.h"

Terminal::Terminal() {}

bool Terminal::on_motion_notify_event(GdkEventMotion* motion_event) {return false;}
bool Terminal::on_button_press_event(GdkEventButton* button_event) {return false;}
bool Terminal::on_key_press_event(GdkEventKey *event) {return false;}

int Terminal::process(const std::string &command, const boost::filesystem::path &path, bool use_pipes) {
  return 0;
}

size_t Terminal::print(const std::string &message, bool bold) {
  return 0;
}
