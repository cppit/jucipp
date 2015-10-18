#ifndef JUCI_SOURCEFILE_H_
#define JUCI_SOURCEFILE_H_
#include <vector>
#include <string>
#include <boost/filesystem.hpp>
#include <gtkmm.h>

enum filesystem_options {
  DIRS = 1,
  FILES = 2,
  ALL = DIRS | FILES,
  SORTED = 4
};

class filesystem {
public:
  static std::string read(const std::string &path);
  static std::string read(const boost::filesystem::path &path) { return read(path.string()); }
  static int read(const std::string &path, Glib::RefPtr<Gtk::TextBuffer> text_buffer);
  static int read(const boost::filesystem::path &path, Glib::RefPtr<Gtk::TextBuffer> text_buffer) { return read(path.string(), text_buffer); }

  static int read_non_utf8(const std::string &path, Glib::RefPtr<Gtk::TextBuffer> text_buffer);
  static int read_non_utf8(const boost::filesystem::path &path, Glib::RefPtr<Gtk::TextBuffer> text_buffer) { return read_non_utf8(path.string(), text_buffer); }

  static std::vector<std::string> read_lines(const std::string &path);
  static std::vector<std::string> read_lines(const boost::filesystem::path &path) { return read_lines(path.string()); };

  static bool write(const std::string &path, const std::string &new_content);
  static bool write(const boost::filesystem::path &path, const std::string &new_content) { return write(path.string(), new_content); }
  static bool write(const std::string &path) { return write(path, ""); };
  static bool write(const boost::filesystem::path &path) { return write(path, ""); };
  static bool write(const std::string &path, Glib::RefPtr<Gtk::TextBuffer> text_buffer);
  static bool write(const boost::filesystem::path &path, Glib::RefPtr<Gtk::TextBuffer> text_buffer) { return write(path.string(), text_buffer); }
  static std::vector<boost::filesystem::path> get_directory_content(const boost::filesystem::path &path, int filter);
  static std::vector<boost::filesystem::path> dirs(const boost::filesystem::path &path);
  static std::vector<boost::filesystem::path> files(const boost::filesystem::path &path);
  static std::vector<boost::filesystem::path> contents(const boost::filesystem::path &path);
  static std::vector<boost::filesystem::path> seperate_contents(const boost::filesystem::path &path);

  static std::string get_home_folder();
  static std::string get_tmp_folder();
};
#endif  // JUCI_SOURCEFILE_H_
