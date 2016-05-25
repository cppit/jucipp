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
  
  //test get_declaration and get_implementation
  clang_view->place_cursor_at_line_index(14-1, 8-1);
  flush_events();
  auto location=clang_view->get_declaration_location({clang_view});
  g_assert_cmpuint(location.line, ==, 5);
  
  clang_view->place_cursor_at_line_index(location.line-1, location.index-1);
  flush_events();
  location=clang_view->get_implementation_location({clang_view});
  g_assert_cmpuint(location.line, ==, 10);
  
  clang_view->place_cursor_at_line_index(location.line-1, location.index-1);
  flush_events();
  location=clang_view->get_declaration_location({clang_view});
  g_assert_cmpuint(location.line, ==, 5);
  
  //test get_usages and get_methods
  auto locations=clang_view->get_usages({clang_view});
  flush_events();
  g_assert_cmpuint(locations.size(), >, 0);
  
  locations=clang_view->get_methods();
  flush_events();
  g_assert_cmpuint(locations.size(), >, 0);
  
  //Test rename class (error if not constructor and destructor is renamed as well)
  auto saved_main=clang_view->get_buffer()->get_text();
  clang_view->place_cursor_at_line_index(1-1, 7-1);
  flush_events();
  auto token=clang_view->get_token(clang_view->get_buffer()->get_insert()->get_iter());
  g_assert_cmpstr(token.c_str(), ==, "TestClass");
  location=clang_view->get_declaration_location({clang_view});
  g_assert_cmpuint(location.line, ==, 1);
  clang_view->rename_similar_tokens({clang_view}, "RenamedTestClass");
  flush_events();
  while(!clang_view->parsed)
    flush_events();
  flush_events();
  auto iter=clang_view->get_buffer()->get_insert()->get_iter();
  iter.backward_char();
  token=clang_view->get_token(iter);
  g_assert_cmpstr(token.c_str(), ==, "RenamedTestClass");
  flush_events();
  g_assert_cmpuint(clang_view->diagnostics.size(), ==, 0);
  clang_view->get_buffer()->set_text(saved_main);
  flush_events();
  clang_view->save({clang_view});
  
  //test error
  clang_view->get_buffer()->set_text(main_error);
  flush_events();
  while(!clang_view->parsed)
    flush_events();
  g_assert_cmpuint(clang_view->diagnostics.size(), >, 0);
  
  clang_view->async_delete();
  clang_view->delete_thread.join();
  flush_events();
}
