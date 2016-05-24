#include <glib.h>
#include "source_clang.h"
#include "config.h"
#include "filesystem.h"

std::string hello_world_error=R"(#include <iostream>

int main() {
  std::cout << "hello world\n"
}
)";

//Requires display server to work
//However, it is possible to use the Broadway backend if the test is run in a pure terminal environment:
//broadwayd&
//make test

int main() {
  auto app=Gtk::Application::create();
  Gsv::init();
  
  Config::get().project.default_build_path="./build";
  Source::ClangView *clang_view=new Source::ClangView(boost::filesystem::canonical(std::string(JUCI_TESTS_PATH)+"/clang_project/main.cpp"),
                                                      Gsv::LanguageManager::get_default()->get_language("cpp"));
  while(!clang_view->parsed) {
    while(Gtk::Main::events_pending())
      Gtk::Main::iteration(false);
  }
  g_assert(clang_view->diagnostics.size()==0);
  clang_view->get_buffer()->set_text(hello_world_error);
  while(Gtk::Main::events_pending())
    Gtk::Main::iteration(false);
  while(!clang_view->parsed) {
    while(Gtk::Main::events_pending())
      Gtk::Main::iteration(false);
  }
  g_assert(clang_view->diagnostics.size()>0);
  
  clang_view->async_delete();
  clang_view->delete_thread.join();
  while(Gtk::Main::events_pending())
    Gtk::Main::iteration(false);
}
