#ifndef JUCI_DIRECTORIES_H_
#define JUCI_DIRECTORIES_H_

#include <gtkmm.h>
#include <vector>
#include <string>
#include "boost/filesystem.hpp"
#include "cmake.h"

class Directories : public Gtk::ScrolledWindow {
public:
  class Config {
  public:
    std::vector<std::string> ignored;
    std::vector<std::string> exceptions;
  };
  
  class ColumnRecord : public Gtk::TreeModel::ColumnRecord {
  public:
    ColumnRecord() {
      add(id);
      add(name);
      add(path);
    }
    Gtk::TreeModelColumn<Glib::ustring> id;
    Gtk::TreeModelColumn<Glib::ustring> name;
    Gtk::TreeModelColumn<Glib::ustring> path;
  };

  Directories();
  void open_folder(const boost::filesystem::path& dir_path="");
  void select_path(const std::string &path);
  
  std::function<void(const std::string &file)> on_row_activated;
  std::unique_ptr<CMake> cmake;
  boost::filesystem::path current_path;
  
private:
  void add_paths(const boost::filesystem::path& dir_path, const Gtk::TreeModel::Row &row, unsigned depth);
  bool ignored(std::string path);
  Gtk::TreeView tree_view;
  Glib::RefPtr<Gtk::TreeStore> tree_store;
  ColumnRecord column_record;
};

#endif  // JUCI_DIRECTORIES_H_
