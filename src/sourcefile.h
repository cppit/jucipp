#ifndef JUCI_SOURCEFILE_H_
#define JUCI_SOURCEFILE_H_
#include <vector>
#include <string>
#include <boost/filesystem.hpp>
#include "gtkmm.h"

namespace juci {
  class filesystem {
  public:
    static std::string open(const std::string &path);
    static std::string open(const boost::filesystem::path &path) { return open(path.string()); }
    static bool open(const std::string &path, Glib::RefPtr<Gtk::TextBuffer> text_buffer);

    static std::vector<std::string> lines(const std::string &path);
    static std::vector<std::string> lines(const boost::filesystem::path &path) { return lines(path.string()); };

    static bool save(const std::string &path, const std::string &new_content);
    static bool save(const boost::filesystem::path &path, const std::string &new_content) { return save(path.string(), new_content); }
    static bool save(const std::string &path) { return save(path, ""); };
    static bool save(const boost::filesystem::path &path) { return save(path, ""); };
    static bool save(const std::string &path, Glib::RefPtr<Gtk::TextBuffer> text_buffer);
  };
} // namepace juci
#endif  // JUCI_SOURCEFILE_H_
