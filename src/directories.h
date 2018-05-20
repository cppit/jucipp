#pragma once
#include <gtkmm.h>
#include <vector>
#include <string>
#include "boost/filesystem.hpp"
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include "git.h"
#include "dispatcher.h"

class Directories : public Gtk::ListViewText {
  class DirectoryData {
  public:
    Gtk::TreeModel::Row row;
    Glib::RefPtr<Gio::FileMonitor> monitor;
    std::shared_ptr<Git::Repository> repository;
    std::shared_ptr<sigc::connection> connection;
  };
  
  enum class PathType {KNOWN, UNKNOWN};
  
  class TreeStore : public Gtk::TreeStore {
  protected:
    TreeStore()=default;
    
    bool row_drop_possible_vfunc(const Gtk::TreeModel::Path &path, const Gtk::SelectionData &selection_data) const override;
    bool drag_data_received_vfunc(const TreeModel::Path &path, const Gtk::SelectionData &selection_data) override;
    bool drag_data_delete_vfunc (const Gtk::TreeModel::Path &path) override;
    
  public:
    class ColumnRecord : public Gtk::TreeModel::ColumnRecord {
    public:
      ColumnRecord() {
        add(id);
        add(name);
        add(markup);
        add(path);
        add(type);
      }
      Gtk::TreeModelColumn<std::string> id;
      Gtk::TreeModelColumn<std::string> name;
      Gtk::TreeModelColumn<Glib::ustring> markup;
      Gtk::TreeModelColumn<boost::filesystem::path> path;
      Gtk::TreeModelColumn<PathType> type;
    };
    
    static Glib::RefPtr<TreeStore> create() {return Glib::RefPtr<TreeStore>(new TreeStore());}
  };

  Directories();
public:
  static Directories &get() {
    static Directories singleton;
    return singleton;
  }
  ~Directories() override;
  
  void open(const boost::filesystem::path &dir_path="");
  void update();
  void on_save_file(const boost::filesystem::path &file_path);
  void select(const boost::filesystem::path &path);
  
  boost::filesystem::path path;
  
protected:
  bool on_button_press_event(GdkEventButton *event) override;
  
private:
  void add_or_update_path(const boost::filesystem::path &dir_path, const Gtk::TreeModel::Row &row, bool include_parent_paths);
  void remove_path(const boost::filesystem::path &dir_path);
  void colorize_path(boost::filesystem::path dir_path_, bool include_parent_paths);
  
  Glib::RefPtr<Gtk::TreeStore> tree_store;
  TreeStore::ColumnRecord column_record;
  
  std::unordered_map<std::string, DirectoryData> directories;
  
  Dispatcher dispatcher;
  
  Gtk::Menu menu;
  Gtk::MenuItem menu_item_new_file;
  Gtk::MenuItem menu_item_new_folder;
  Gtk::SeparatorMenuItem menu_item_separator;
  Gtk::MenuItem menu_item_rename;
  Gtk::MenuItem menu_item_delete;
  Gtk::Menu menu_root;
  Gtk::MenuItem menu_root_item_new_file;
  Gtk::MenuItem menu_root_item_new_folder;
  boost::filesystem::path menu_popup_row_path;
};
