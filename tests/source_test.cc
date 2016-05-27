#include <glib.h>
#include "source.h"
#include "filesystem.h"

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
  
  auto tests_path=boost::filesystem::canonical(JUCI_TESTS_PATH);
  auto source_file=tests_path/"tmp"/"source_file.txt";
  
  {
    Source::View source_view(source_file, Glib::RefPtr<Gsv::Language>());
    source_view.get_buffer()->set_text(hello_world);
    g_assert(source_view.save({&source_view}));
  }
  
  Source::View source_view(source_file, Glib::RefPtr<Gsv::Language>());
  g_assert(source_view.get_buffer()->get_text()==hello_world);
  source_view.cleanup_whitespace_characters();
  g_assert(source_view.get_buffer()->get_text()==hello_world_cleaned);
  
  g_assert(boost::filesystem::remove(source_file));
  g_assert(!boost::filesystem::exists(source_file));
}
