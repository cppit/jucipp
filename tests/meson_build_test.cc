#include "meson.h"
#include <glib.h>
#include "project.h"

int main() {
  auto tests_path=boost::filesystem::canonical(JUCI_TESTS_PATH);
  auto meson_test_files_path=boost::filesystem::canonical(tests_path/"meson_test_files");
  
  {
    Meson meson(meson_test_files_path/"a_subdir");
    g_assert(meson.project_path==meson_test_files_path);
  }
  
  {
    Meson meson(meson_test_files_path);
    g_assert(meson.project_path==meson_test_files_path);
    
    g_assert(meson.get_executable(meson_test_files_path/"build", meson_test_files_path/"main.cpp")==meson_test_files_path/"build"/"hello");
    g_assert(meson.get_executable(meson_test_files_path/"build", meson_test_files_path/"another_file.cpp")==meson_test_files_path/"build"/"another_executable");
    g_assert(meson.get_executable(meson_test_files_path/"build", meson_test_files_path/"a_subdir"/"main.cpp")==meson_test_files_path/"build"/"a_subdir"/"hello2");
    
    g_assert(meson.get_executable(meson_test_files_path/"build", meson_test_files_path/"non_existing_file.cpp")==meson_test_files_path/"build"/"hello");
    g_assert(meson.get_executable(meson_test_files_path/"build", meson_test_files_path)==meson_test_files_path/"build"/"hello");
    
    g_assert(meson.get_executable(meson_test_files_path/"build", meson_test_files_path/"a_subdir")==meson_test_files_path/"build"/"a_subdir"/"hello2");
    g_assert(meson.get_executable(meson_test_files_path/"build", meson_test_files_path/"a_subdir"/"non_existing_file.cpp")==meson_test_files_path/"build"/"a_subdir"/"hello2");
  }
  
  auto build=Project::Build::create(meson_test_files_path);
  g_assert(dynamic_cast<Project::MesonBuild*>(build.get()));
  
  build=Project::Build::create(meson_test_files_path/"a_subdir");
  g_assert(dynamic_cast<Project::MesonBuild*>(build.get()));
}
