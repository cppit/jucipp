#include "filesystem.h"
#include <glib.h>

int main() {
  {
    auto original="test () '\"";
    auto escaped=filesystem::escape_argument(original);
    g_assert_cmpstr(escaped.c_str(), ==, "test\\ \\(\\)\\ \\'\\\"");
    auto unescaped=filesystem::unescape_argument(escaped);
    g_assert_cmpstr(unescaped.c_str(), ==, original);
  }
  {
    auto unescaped=filesystem::unescape_argument("'test \\()\"\\''");
    g_assert_cmpstr(unescaped.c_str(), ==, "test \\()\"'");
  }
  {
    auto unescaped=filesystem::unescape_argument("\"test \\'()\\\"\"");
    g_assert_cmpstr(unescaped.c_str(), ==, "test \\'()\"");
  }
  
  auto tests_path=boost::filesystem::canonical(JUCI_TESTS_PATH);
  {
    g_assert(filesystem::file_in_path(tests_path/"filesystem_test.cc", tests_path));
    g_assert(!filesystem::file_in_path(boost::filesystem::canonical(tests_path/".."/"CMakeLists.txt"), tests_path));
  }
  
  auto license_file=boost::filesystem::canonical(tests_path/".."/"LICENSE");
  {
    g_assert(filesystem::find_file_in_path_parents("LICENSE", tests_path)==license_file);
  }
  
  {
    g_assert(filesystem::get_normal_path(tests_path/".."/"LICENSE")==license_file);
    g_assert(filesystem::get_normal_path("/foo")=="/foo");
    g_assert(filesystem::get_normal_path("/foo/")=="/foo");
    g_assert(filesystem::get_normal_path("/foo/.")=="/foo");
    g_assert(filesystem::get_normal_path("/foo/./bar/..////")=="/foo");
    g_assert(filesystem::get_normal_path("/foo/.///bar/../")=="/foo");
    g_assert(filesystem::get_normal_path("../foo")=="../foo");
    g_assert(filesystem::get_normal_path("../../foo")=="../../foo");
    g_assert(filesystem::get_normal_path("../././foo")=="../foo");
    g_assert(filesystem::get_normal_path("/../foo")=="/../foo");
  }
  
  {
    boost::filesystem::path relative_path="filesystem_test.cc";
    g_assert(filesystem::get_relative_path(tests_path/relative_path, tests_path)==relative_path);
  }
}