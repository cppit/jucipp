#include <glib.h>
#include "cmake.h"
#include "project_build.h"
#include "config.h"
#include <boost/filesystem.hpp>
#include "process.hpp"

#include <iostream>
using namespace std;

int main() {
  auto tests_path=boost::filesystem::canonical(JUCI_TESTS_PATH);
  {
    auto project_path=boost::filesystem::canonical(tests_path/"..");
    
    {
      CMake cmake(project_path);
      TinyProcessLib::Process process("cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..", (project_path/"build").string(), [](const char *bytes, size_t n) {});
      g_assert(process.get_exit_status()==0);
      
      g_assert(cmake.get_executable(project_path/"build", project_path)=="");
      g_assert(cmake.get_executable(project_path/"build"/"non_existing_file.cc", project_path)=="");
    }
    {
      CMake cmake(project_path/"src");
      g_assert(cmake.get_executable(project_path/"build", project_path/"src")==project_path/"build"/"src"/"juci");
      g_assert(cmake.get_executable(project_path/"build", project_path/"src"/"cmake.cc")==project_path/"build"/"src"/"juci");
      g_assert(cmake.get_executable(project_path/"build", project_path/"src"/"juci.cc")==project_path/"build"/"src"/"juci");
      g_assert(cmake.get_executable(project_path/"build", project_path/"src"/"non_existing_file.cc")==project_path/"build"/"src"/"juci");
    }
    {
      CMake cmake(tests_path);
      
      g_assert(cmake.project_path==project_path);
      
      auto functions_parameters=cmake.get_functions_parameters("project");
      g_assert(functions_parameters.at(0).second.at(0)=="juci");
      
      g_assert(cmake.get_executable(project_path/"build", tests_path).parent_path()==project_path/"build"/"tests");
      g_assert(cmake.get_executable(project_path/"build", tests_path/"cmake_build_test.cc")==project_path/"build"/"tests"/"cmake_build_test");
      g_assert(cmake.get_executable(project_path/"build", tests_path/"non_existing_file.cc").parent_path()==project_path/"build"/"tests");
    }
    
    auto build=Project::Build::create(tests_path);
    g_assert(dynamic_cast<Project::CMakeBuild*>(build.get()));
    
    build=Project::Build::create(tests_path/"stubs");
    g_assert(dynamic_cast<Project::CMakeBuild*>(build.get()));
    g_assert(build->project_path==project_path);
    
    Config::get().project.default_build_path="./build";
    g_assert(build->get_default_path()==project_path/"build");
    
    Config::get().project.debug_build_path="<default_build_path>/debug";
    g_assert(build->get_debug_path()==project_path/"build/debug");
    
    auto project_path_filename=project_path.filename();
    Config::get().project.debug_build_path="../debug_<project_directory_name>";
    g_assert(build->get_debug_path()==project_path.parent_path()/("debug_"+project_path_filename.string()));
    
    Config::get().project.default_build_path="../build_<project_directory_name>";
    g_assert(build->get_default_path()==project_path.parent_path()/("build_"+project_path_filename.string()));
  }
  {
    auto project_path=tests_path/"source_clang_test_files";
    CMake cmake(project_path);
    
    g_assert(cmake.project_path==project_path);
    
    g_assert(cmake.get_executable(project_path/"build", project_path/"main.cpp")==boost::filesystem::path(".")/"test");
    g_assert(cmake.get_executable(project_path/"build", project_path/"non_existing_file.cpp")==boost::filesystem::path(".")/"test");
    g_assert(cmake.get_executable(project_path/"build", project_path)==boost::filesystem::path(".")/"test");
  }
}
