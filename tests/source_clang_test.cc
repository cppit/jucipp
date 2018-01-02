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

#ifdef _WIN32
  g_assert_cmpstr(std::getenv("MSYSTEM_PREFIX"), !=, nullptr);
#endif
  
  Config::get().project.default_build_path="./build";
  Source::ClangView *clang_view=new Source::ClangView(boost::filesystem::canonical(std::string(JUCI_TESTS_PATH)+"/source_clang_test_files/main.cpp"),
                                                      Gsv::LanguageManager::get_default()->get_language("cpp"));
  while(!clang_view->parsed)
    flush_events();
  g_assert_cmpuint(clang_view->clang_diagnostics.size(), ==, 0);
  
  //test get_declaration and get_implementation
  clang_view->place_cursor_at_line_index(15, 7);
  auto location=clang_view->get_declaration_location();
  g_assert_cmpuint(location.line, ==, 6);
  
  clang_view->place_cursor_at_line_index(location.line, location.index);
  auto impl_locations=clang_view->get_implementation_locations();
  g_assert_cmpuint(impl_locations.size(), ==, 1);
  g_assert_cmpuint(impl_locations[0].line, ==, 11);
  
  clang_view->place_cursor_at_line_index(location.line, location.index);
  location=clang_view->get_declaration_location();
  g_assert_cmpuint(location.line, ==, 6);
  
  //test get_usages and get_methods
  auto locations=clang_view->get_usages();
  g_assert_cmpuint(locations.size(), >, 0);
  
  locations=clang_view->get_methods();
  g_assert_cmpuint(locations.size(), >, 0);
  
  //Test rename class (error if not constructor and destructor is renamed as well)
  auto saved_main=clang_view->get_buffer()->get_text();
  clang_view->place_cursor_at_line_index(0, 6);
  auto token=clang_view->get_token(clang_view->get_buffer()->get_insert()->get_iter());
  g_assert_cmpstr(token.c_str(), ==, "TestClass");
  location=clang_view->get_declaration_location();
  g_assert_cmpuint(location.line, ==, 0);
  clang_view->rename_similar_tokens("RenamedTestClass");
  while(!clang_view->parsed)
    flush_events();
  auto iter=clang_view->get_buffer()->get_insert()->get_iter();
  iter.backward_char();
  token=clang_view->get_token(iter);
  g_assert_cmpstr(token.c_str(), ==, "RenamedTestClass");
  g_assert_cmpuint(clang_view->clang_diagnostics.size(), ==, 0);
  clang_view->get_buffer()->set_text(saved_main);
  clang_view->save();
  
  //test error
  clang_view->get_buffer()->set_text(main_error);
  while(!clang_view->parsed)
    flush_events();
  g_assert_cmpuint(clang_view->clang_diagnostics.size(), >, 0);
  g_assert_cmpuint(clang_view->get_fix_its().size(), >, 0);
  
  // test remove_include_guard
  {
    clang_view->language=Gsv::LanguageManager::get_default()->get_language("chdr");
    std::string source="#ifndef F\n#define F\n#endif  // F";
    clangmm::remove_include_guard(source);
    g_assert_cmpstr(source.c_str(),==,"         \n         \n            ");
    
    source="#ifndef F\n#define F\n#endif  // F\n";
    clangmm::remove_include_guard(source);
    g_assert_cmpstr(source.c_str(),==,"         \n         \n            \n");
    
    source="/*test*/\n#ifndef F\n#define F\n#endif  // F\n";
    clangmm::remove_include_guard(source);
    g_assert_cmpstr(source.c_str(),==,"/*test*/\n         \n         \n            \n");
    
    source="//test\n#ifndef F\n#define F\n#endif  // F\n";
    clangmm::remove_include_guard(source);
    g_assert_cmpstr(source.c_str(),==,"//test\n         \n         \n            \n");
    
    source="#ifndef F /*test*/\n#define F\n#endif  // F";
    clangmm::remove_include_guard(source);
    g_assert_cmpstr(source.c_str(),==,"                  \n         \n            ");
    
    source="#ifndef F //test\n#define F\n#endif  // F";
    clangmm::remove_include_guard(source);
    g_assert_cmpstr(source.c_str(),==,"                \n         \n            ");
    
    source="#ifndef F\n//test\n#define F\n#endif  // F\n";
    clangmm::remove_include_guard(source);
    g_assert_cmpstr(source.c_str(),==,"         \n//test\n         \n            \n");
    
    source="#if !defined(F)\n#define F\n#endif\n";
    clangmm::remove_include_guard(source);
    g_assert_cmpstr(source.c_str(),==,"               \n         \n      \n");
    
    source="#ifndef F\ntest\n#define F\n#endif  // F\n";
    auto old_source=source;
    clangmm::remove_include_guard(source);
    g_assert_cmpstr(old_source.c_str(),==,source.c_str());
    
    source="test\n#ifndef F\n#define F\n#endif  // F\n";
    old_source=source;
    clangmm::remove_include_guard(source);
    g_assert_cmpstr(old_source.c_str(),==,source.c_str());
    
    source="#ifndef F\n#define F\n#endif  // F\ntest\n";
    old_source=source;
    clangmm::remove_include_guard(source);
    g_assert_cmpstr(old_source.c_str(),==,source.c_str());
  }
  
  // Test Implement method
  {
    clang_view->get_buffer()->set_text(R"(#include <string>
#include <vector>
#include <map>

namespace N {
  class T {
    void f1();
    void f1(T &t);
    void f1(const T &t);
    void f1(std::vector<T> ts);
    void f1(const std::vector<T> &ts);
    std::string f2();
    const std::string &f3();
    std::vector<T> f4();
    std::vector<T*> f5();
    std::vector<T&> f6();
    std::map<T, std::string> f7();
    static
    std::map<T,
             std::string
    > f8();
    void f9() const;
  };
}
)");
    while(!clang_view->parsed)
      flush_events();
    g_assert_cmpuint(clang_view->clang_diagnostics.size(), ==, 0);
    
    auto buffer=clang_view->get_buffer();
    
    buffer->place_cursor(buffer->get_iter_at_line_offset(6, 9));
    auto method=clang_view->get_method();
    g_assert_cmpstr(method.c_str(), ==, "void N::T::f1() {}");
    buffer->place_cursor(buffer->get_iter_at_line_offset(7, 9));
    method=clang_view->get_method();
    g_assert_cmpstr(method.c_str(), ==, "void N::T::f1(T &t) {}");
    buffer->place_cursor(buffer->get_iter_at_line_offset(8, 9));
    method=clang_view->get_method();
    g_assert_cmpstr(method.c_str(), ==, "void N::T::f1(const T &t) {}");
    buffer->place_cursor(buffer->get_iter_at_line_offset(9, 9));
    method=clang_view->get_method();
    g_assert_cmpstr(method.c_str(), ==, "void N::T::f1(std::vector<T> ts) {}");
    buffer->place_cursor(buffer->get_iter_at_line_offset(10, 9));
    method=clang_view->get_method();
    g_assert_cmpstr(method.c_str(), ==, "void N::T::f1(const std::vector<T> &ts) {}");
    buffer->place_cursor(buffer->get_iter_at_line_offset(11, 16));
    method=clang_view->get_method();
    g_assert_cmpstr(method.c_str(), ==, "std::string N::T::f2() {}");
    buffer->place_cursor(buffer->get_iter_at_line_offset(12, 23));
    method=clang_view->get_method();
    g_assert_cmpstr(method.c_str(), ==, "const std::string &N::T::f3() {}");
    buffer->place_cursor(buffer->get_iter_at_line_offset(13, 19));
    method=clang_view->get_method();
    g_assert_cmpstr(method.c_str(), ==, "std::vector<N::T> N::T::f4() {}");
    buffer->place_cursor(buffer->get_iter_at_line_offset(14, 20));
    method=clang_view->get_method();
    g_assert_cmpstr(method.c_str(), ==, "std::vector<N::T *> N::T::f5() {}");
    buffer->place_cursor(buffer->get_iter_at_line_offset(15, 20));
    method=clang_view->get_method();
    g_assert_cmpstr(method.c_str(), ==, "std::vector<N::T &> N::T::f6() {}");
    buffer->place_cursor(buffer->get_iter_at_line_offset(16, 29));
    method=clang_view->get_method();
    g_assert_cmpstr(method.c_str(), ==, "std::map<N::T, std::string> N::T::f7() {}");
    buffer->place_cursor(buffer->get_iter_at_line_offset(20, 6));
    method=clang_view->get_method();
    g_assert_cmpstr(method.c_str(), ==, "std::map<N::T, std::string> N::T::f8() {}");
    buffer->place_cursor(buffer->get_iter_at_line_offset(21, 9));
    method=clang_view->get_method();
    g_assert_cmpstr(method.c_str(), ==, "void N::T::f9() const {}");
  }
  
  clang_view->async_delete();
  clang_view->delete_thread.join();
  flush_events();
}
