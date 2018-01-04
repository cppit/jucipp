#pragma once
#include <boost/filesystem.hpp>
#include <vector>

class Meson {
public:
  Meson(const boost::filesystem::path &path);
  
  boost::filesystem::path project_path;
  
  bool update_default_build(const boost::filesystem::path &default_build_path, bool force=false);
  bool update_debug_build(const boost::filesystem::path &debug_build_path, bool force=false);
  
  boost::filesystem::path get_executable(const boost::filesystem::path &build_path, const boost::filesystem::path &file_path);
};
