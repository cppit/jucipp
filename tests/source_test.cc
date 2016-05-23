#include <glib.h>
#include "source.h"
#include "filesystem.h"

int filesystem::read(const std::string &path, Glib::RefPtr<Gtk::TextBuffer> text_buffer) {
  return 0;
}

int filesystem::read_non_utf8(const std::string &path, Glib::RefPtr<Gtk::TextBuffer> text_buffer) {
  return 0;
}

bool filesystem::write(const std::string &path, Glib::RefPtr<Gtk::TextBuffer> text_buffer) {
  return false;
}

std::string hello_world=R"(#include <iostream>  
    
int main() {  
  std::cout << "hello world\n";    
})";

std::string hello_world_cleaned=R"(#include <iostream>

int main() {
  std::cout << "hello world\n";
}
)";

//Requires display server to work
//However, it is possible to use the Broadway backend if the test is run in a pure terminal environment:
//broadwayd&
//make test

int main() {
  auto app=Gtk::Application::create();
  Gsv::init();
  
  Source::View source_view("", Glib::RefPtr<Gsv::Language>());
  source_view.get_buffer()->set_text(hello_world);
  source_view.cleanup_whitespace_characters();
  g_assert(source_view.get_buffer()->get_text()==hello_world_cleaned);
}
