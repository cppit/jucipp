#include <glib.h>
#include "debug_clang.h"
#include <thread>
#include <boost/filesystem.hpp>
#include <atomic>

int main() {
  auto build_path=boost::filesystem::canonical(JUCI_BUILD_PATH);
  auto exec_path=build_path/"tests"/"lldb_test_files"/"lldb_test_executable";
  g_assert(boost::filesystem::exists(exec_path));
  
  auto tests_path=boost::filesystem::canonical(JUCI_TESTS_PATH);
  auto source_path=tests_path/"lldb_test_files"/"main.cpp";
  g_assert(boost::filesystem::exists(source_path));
  
  std::vector<std::pair<boost::filesystem::path, int> > breakpoints;
  breakpoints.emplace_back(source_path, 2);
  
  std::atomic<bool> exited(false);
  int exit_status;
  std::atomic<int> line_nr(0);
  std::thread debug_thread([&] {
    Debug::Clang::get().start(exec_path.string(), "", breakpoints, [&](int exit_status_){
      exit_status=exit_status_;
      exited=true;
    }, [](const std::string &status) {
      
    }, [&](const boost::filesystem::path &file_path, int line_nr_, int line_index) {
      line_nr=line_nr_;
    });
  });
  debug_thread.detach();
  
  for(;;) {
    if(exited) {
      g_assert_cmpint(exit_status, ==, 0);
      break;
    }
    else if(line_nr>0) {
      for(;;) {
        if(Debug::Clang::get().is_stopped())
          break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
      g_assert_cmpint(line_nr, ==, 2);
      g_assert(Debug::Clang::get().get_backtrace().size()>0);
      auto variables=Debug::Clang::get().get_variables();
      g_assert_cmpstr(variables.at(0).name.c_str(), ==, "an_int");
      line_nr=0;
      Debug::Clang::get().step_over();
      for(;;) {
        if(line_nr>0 && Debug::Clang::get().is_stopped())
          break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
      g_assert_cmpint(line_nr, ==, 3);
      g_assert(Debug::Clang::get().get_backtrace().size()>0);
      variables=Debug::Clang::get().get_variables();
      g_assert_cmpstr(variables.at(0).name.c_str(), ==, "an_int");
      auto value=Debug::Clang::get().get_value("an_int", source_path, 2, 7);
      g_assert_cmpuint(value.size(), >, 16);
      auto value_substr=value.substr(0, 16);
      g_assert_cmpstr(value_substr.c_str(), ==, "(int) an_int = 1");
      line_nr=0;
      Debug::Clang::get().continue_debug();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  
  Debug::Clang::get().cancel();
  
  g_assert_cmpuint(Debug::Clang::get_platform_list().size(), >, 0);
}
