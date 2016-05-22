#include <glib.h>
#include "cmake.h"
#include "project_build.h"
#include <boost/filesystem.hpp>

int main() {
  CMake cmake(JUCI_TESTS_PATH);
  
  g_assert(cmake.project_path.filename()=="jucipp");
  
  auto functions_parameters=cmake.get_functions_parameters("project");
  g_assert(functions_parameters.at(0).second.at(0)=="juci");
  
  g_assert(cmake.get_executable(boost::filesystem::path(JUCI_TESTS_PATH)/"cmake_test.cc").filename()=="cmake_test");
  
  auto build=Project::Build::create(JUCI_TESTS_PATH);
  g_assert(dynamic_cast<Project::CMakeBuild*>(build.get()));
}
