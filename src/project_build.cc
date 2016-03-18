#include "project_build.h"

std::unique_ptr<Project::Build> Project::get_build(const boost::filesystem::path &path) {
  return std::unique_ptr<Project::Build>(new CMake(path));
}

Project::CMake::CMake(const boost::filesystem::path &path) : Project::Build(), cmake(path) {
  project_path=cmake.project_path;
}

boost::filesystem::path Project::CMake::get_default_build_path() {
  return cmake.get_default_build_path();
}

bool Project::CMake::update_default_build(bool force) {
  return cmake.update_default_build(force);
}

boost::filesystem::path Project::CMake::get_debug_build_path() {
  return cmake.get_debug_build_path();
}

bool Project::CMake::update_debug_build(bool force) {
  return cmake.update_debug_build(force);
}

boost::filesystem::path Project::CMake::get_executable(const boost::filesystem::path &path) {
  return cmake.get_executable(path);
}
