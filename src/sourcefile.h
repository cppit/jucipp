#ifndef JUCI_SOURCEFILE_H_
#define JUCI_SOURCEFILE_H_
#include <vector>
#include <string>
#include <boost/filesystem.hpp>
#include "gtkmm.h"

namespace juci {
  class filesystem {
  public:
    static std::string read(const std::string &path);
    static std::string read(const boost::filesystem::path &path) { return read(path.string()); }
    static bool read(const std::string &path, Glib::RefPtr<Gtk::TextBuffer> text_buffer);

    static std::vector<std::string> read_lines(const std::string &path);
    static std::vector<std::string> read_lines(const boost::filesystem::path &path) { return read_lines(path.string()); };

    static bool write(const std::string &path, const std::string &new_content);
    static bool write(const boost::filesystem::path &path, const std::string &new_content) { return write(path.string(), new_content); }
    static bool write(const std::string &path) { return write(path, ""); };
    static bool write(const boost::filesystem::path &path) { return write(path, ""); };
    static bool write(const std::string &path, Glib::RefPtr<Gtk::TextBuffer> text_buffer);
  };
} // namepace juci
#endif  // JUCI_SOURCEFILE_H_
