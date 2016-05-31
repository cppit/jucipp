#include "terminal.h"

Terminal::InProgress::InProgress(const std::string& start_msg): stop(false) {}

Terminal::InProgress::~InProgress() {}

std::shared_ptr<Terminal::InProgress> Terminal::print_in_progress(std::string start_msg) {
  return std::make_shared<Terminal::InProgress>("");
}

void Terminal::InProgress::done(const std::string& msg) {}

void Terminal::InProgress::cancel(const std::string &msg) {}

Terminal::Terminal() {}

bool Terminal::on_motion_notify_event(GdkEventMotion* motion_event) {return false;}
bool Terminal::on_button_press_event(GdkEventButton* button_event) {return false;}
bool Terminal::on_key_press_event(GdkEventKey *event) {return false;}

int Terminal::process(const std::string &command, const boost::filesystem::path &path, bool use_pipes) {
  return 0;
}

int Terminal::process(std::istream &stdin_stream, std::ostream &stdout_stream, const std::string &command, const boost::filesystem::path &path) {
  return 0;
}

size_t Terminal::print(const std::string &message, bool bold) {
  return 0;
}

void Terminal::async_print(const std::string &message, bool bold) {}
