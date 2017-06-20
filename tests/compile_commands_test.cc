#include "compile_commands.h"
#include <glib.h>

int main() {
  auto tests_path=boost::filesystem::canonical(JUCI_TESTS_PATH);
  
  {
    CompileCommands compile_commands(tests_path/"meson_test_files"/"build");
    
    g_assert(compile_commands.commands.at(0).directory=="jucipp/tests/meson_test_files/build");
    
    g_assert_cmpuint(compile_commands.commands.size(), ==, 5);
    
    g_assert_cmpstr(compile_commands.commands.at(4).parameters.at(0).c_str(), ==, "te's\"t");
    g_assert_cmpstr(compile_commands.commands.at(4).parameters.at(1).c_str(), ==, "te st");
    g_assert_cmpstr(compile_commands.commands.at(4).parameters.at(2).c_str(), ==, "test");
    g_assert_cmpstr(compile_commands.commands.at(4).parameters.at(3).c_str(), ==, "te\\st");
    g_assert_cmpstr(compile_commands.commands.at(4).parameters.at(4).c_str(), ==, "te\\\\st");
    
    auto parameter_values=compile_commands.commands.at(0).parameter_values("-o");
    g_assert_cmpuint(parameter_values.size(), ==, 1);
    g_assert_cmpstr(parameter_values.at(0).c_str(), ==, "hello_lib@sta/main.cpp.o");
    
    g_assert(boost::filesystem::canonical(compile_commands.commands.at(0).file) == tests_path/"meson_test_files"/"main.cpp");
  }
  
  {
    CompileCommands compile_commands(tests_path/"source_clang_test_files"/"build");
    
    g_assert(compile_commands.commands.at(0).directory==".");
    
    g_assert_cmpuint(compile_commands.commands.size(), ==, 1);
    
    g_assert_cmpstr(compile_commands.commands.at(0).parameters.at(2).c_str(), ==, "-Wall");
  }
}
