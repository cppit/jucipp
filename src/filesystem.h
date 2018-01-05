#pragma once
#include <vector>
#include <string>
#include <boost/filesystem.hpp>

class filesystem {
public:
  static std::string read(const std::string &path);
  static std::string read(const boost::filesystem::path &path) { return read(path.string()); }

  static std::vector<std::string> read_lines(const std::string &path);
  static std::vector<std::string> read_lines(const boost::filesystem::path &path) { return read_lines(path.string()); };

  static bool write(const std::string &path, const std::string &new_content);
  static bool write(const boost::filesystem::path &path, const std::string &new_content) { return write(path.string(), new_content); }
  static bool write(const std::string &path) { return write(path, ""); };
  static bool write(const boost::filesystem::path &path) { return write(path, ""); };

  static std::string escape_argument(const std::string &argument);
  static std::string unescape_argument(const std::string &argument);
  
  static boost::filesystem::path get_home_path() noexcept;
  static boost::filesystem::path get_short_path(const boost::filesystem::path &path) noexcept;
  
  static bool file_in_path(const boost::filesystem::path &file_path, const boost::filesystem::path &path);
  static boost::filesystem::path find_file_in_path_parents(const std::string &file_name, const boost::filesystem::path &path);
  
  /// Return path with dot, dot-dot and directory separator elements removed
  static boost::filesystem::path get_normal_path(const boost::filesystem::path &path) noexcept;
  
  /// Returns empty path on failure
  static boost::filesystem::path get_relative_path(const boost::filesystem::path &path, const boost::filesystem::path &base) noexcept;
  
  /// Return executable with latest version in filename on systems that is lacking executable_name symbolic link
  static boost::filesystem::path get_executable(const boost::filesystem::path &executable_name) noexcept;

  static const std::vector<boost::filesystem::path> &get_executable_search_paths();
  
  static boost::filesystem::path find_executable(const std::string &executable_name);
};
