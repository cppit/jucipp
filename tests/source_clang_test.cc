#include <glib.h>
#include "source_clang.h"
#include "config.h"
#include "filesystem.h"

std::string main_error=R"(int main() {
  int number=2;
  number=3
}
)";

//Requires display server to work
//However, it is possible to use the Broadway backend if the test is run in a pure terminal environment:
//broadwayd&
//make test

void flush_events() {
  while(Gtk::Main::events_pending())
    Gtk::Main::iteration(false);
}

int main() {
  auto app=Gtk::Application::create();
  Gsv::init();
  
  Config::get().project.default_build_path="./build";
  Source::ClangView *clang_view=new Source::ClangView(boost::filesystem::canonical(std::string(JUCI_TESTS_PATH)+"/clang_project/main.cpp"),
                                                      Gsv::LanguageManager::get_default()->get_language("cpp"));
  while(!clang_view->parsed)
    flush_events();
  g_assert_cmpuint(clang_view->diagnostics.size(), ==, 0);
  
  clang_view->place_cursor_at_line_index(10-1, 8-1);
  flush_events();
  auto location=clang_view->get_declaration_location({clang_view});
  g_assert_cmpuint(location.line, ==, 3);
  clang_view->place_cursor_at_line_index(location.line-1, location.index-1);
  flush_events();
  location=clang_view->get_implementation_location({clang_view});
  g_assert_cmpuint(location.line, ==, 6);
  clang_view->place_cursor_at_line_index(location.line-1, location.index-1);
  flush_events();
  location=clang_view->get_declaration_location({clang_view});
  g_assert_cmpuint(location.line, ==, 3);
  
  clang_view->get_buffer()->set_text(main_error);
  flush_events();
  while(!clang_view->parsed)
    flush_events();
  g_assert_cmpuint(clang_view->diagnostics.size(), >, 0);
  
  clang_view->async_delete();
  clang_view->delete_thread.join();
  flush_events();
}
