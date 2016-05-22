#include <glib.h>
#include "cmake.h"
#include "project_build.h"
#include "config.h"
#include <boost/filesystem.hpp>

int main() {
  auto tests_path=boost::filesystem::canonical(JUCI_TESTS_PATH);
  auto project_path=boost::filesystem::canonical(tests_path/"..");
  
  CMake cmake(tests_path);
  
  g_assert(cmake.project_path==project_path);
  
  auto functions_parameters=cmake.get_functions_parameters("project");
  g_assert(functions_parameters.at(0).second.at(0)=="juci");
  
  g_assert(cmake.get_executable(tests_path/"cmake_test.cc").filename()=="cmake_test");
  
  auto build=Project::Build::create(tests_path);
  g_assert(dynamic_cast<Project::CMakeBuild*>(build.get()));
  
  build=Project::Build::create(tests_path/"stubs");
  g_assert(dynamic_cast<Project::CMakeBuild*>(build.get()));
  g_assert(build->project_path==project_path);
  
  Config::get().project.default_build_path="./build";
  g_assert(build->get_default_path()==project_path/"./build");
  
  Config::get().project.debug_build_path="<default_build_path>/debug";
  g_assert(build->get_debug_path()==project_path/"./build/debug");
  
  auto project_path_filename=project_path.filename();
  Config::get().project.debug_build_path="../debug_<project_directory_name>";
  g_assert(build->get_debug_path()==project_path/("../debug_"+project_path_filename.string()));
  
  Config::get().project.default_build_path="../build_<project_directory_name>";
  g_assert(build->get_default_path()==project_path/("../build_"+project_path_filename.string()));
}
