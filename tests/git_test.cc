#include <glib.h>
#include <gtkmm.h>
#include "git.h"
#include <boost/filesystem.hpp>

int main() {
  auto app=Gtk::Application::create();
  
  auto tests_path=boost::filesystem::canonical(JUCI_TESTS_PATH);
  auto jucipp_path=tests_path.parent_path();
  auto git_path=jucipp_path/".git";
  
  try {
    auto repository=Git::get_repository(tests_path);
    
    g_assert(repository->get_path()==git_path);
    g_assert(repository->get_work_path()==jucipp_path);
    
    auto status=repository->get_status();
    
    auto diff=repository->get_diff((boost::filesystem::path("tests")/"git_test.cc"));
    auto lines=diff.get_lines("#include added\n#include <glib.h>\n#include modified\n#include \"git.h\"\n");
    g_assert_cmpuint(lines.added.size(), ==, 1);
    g_assert_cmpuint(lines.modified.size(), ==, 1);
    g_assert_cmpuint(lines.removed.size(), ==, 1);
  }
  catch(const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
  
  try {
    g_assert(Git::Repository::get_root_path(tests_path)==git_path);
  }
  catch(const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
  
  {
    std::string old_text("line 1\nline2\n\nline4\n\n");
    std::string new_text("line2\n\nline41\nline5\n\nline 5\nline 6\n");
    auto hunks=Git::Repository::Diff::get_hunks(old_text, new_text);
    assert(hunks.size()==3);
    assert(hunks[0].old_lines.first==1);
    assert(hunks[0].old_lines.second==1);
    assert(hunks[0].new_lines.first==0);
    assert(hunks[0].new_lines.second==0);
    assert(hunks[1].old_lines.first==4);
    assert(hunks[1].old_lines.second==1);
    assert(hunks[1].new_lines.first==3);
    assert(hunks[1].new_lines.second==2);
    assert(hunks[2].old_lines.first==5);
    assert(hunks[2].old_lines.second==0);
    assert(hunks[2].new_lines.first==6);
    assert(hunks[2].new_lines.second==2);
  }
}
