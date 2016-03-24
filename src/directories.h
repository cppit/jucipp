#ifndef JUCI_DIRECTORIES_H_
#define JUCI_DIRECTORIES_H_

#include <gtkmm.h>
#include <vector>
#include <string>
#include "boost/filesystem.hpp"
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include "dispatcher.h"

class Directories : public Gtk::TreeView {
public:
  class TreeStore : public Gtk::TreeStore {
  protected:
    TreeStore() {}
    
    bool row_drop_possible_vfunc(const Gtk::TreeModel::Path &path, const Gtk::SelectionData &selection_data) const override;
    bool drag_data_received_vfunc(const TreeModel::Path &path, const Gtk::SelectionData &selection_data) override;
    bool drag_data_delete_vfunc (const Gtk::TreeModel::Path &path) override;
    
  public:
    class ColumnRecord : public Gtk::TreeModel::ColumnRecord {
    public:
      ColumnRecord() {
        add(id);
        add(name);
        add(path);
        add(color);
      }
      Gtk::TreeModelColumn<std::string> id;
      Gtk::TreeModelColumn<std::string> name;
      Gtk::TreeModelColumn<boost::filesystem::path> path;
      Gtk::TreeModelColumn<Gdk::RGBA> color;
    };
    
    static Glib::RefPtr<TreeStore> create() {return Glib::RefPtr<TreeStore>(new TreeStore());}
  };

private:
  Directories();
public:
  static Directories &get() {
    static Directories singleton;
    return singleton;
  }
  ~Directories();
  void open(const boost::filesystem::path &dir_path="");
  void update();
  void select(const boost::filesystem::path &path);
  
  std::function<void(const boost::filesystem::path &path)> on_row_activated;
  boost::filesystem::path path;
  
protected:
  bool on_button_press_event(GdkEventButton *event) override;
  
private:
  void add_path(const boost::filesystem::path &dir_path, const Gtk::TreeModel::Row &row, time_t last_write_time=0);
  Glib::RefPtr<Gtk::TreeStore> tree_store;
  TreeStore::ColumnRecord column_record;
  
  std::unordered_map<std::string, std::pair<Gtk::TreeModel::Row, std::time_t> > last_write_times;
  std::mutex update_mutex;
  std::thread update_thread;
  std::atomic<bool> stop_update_thread;
  Dispatcher dispatcher;
  
  Gtk::Menu menu;
  Gtk::MenuItem menu_item_rename;
  Gtk::MenuItem menu_item_delete;
  boost::filesystem::path menu_popup_row_path;
};

#endif  // JUCI_DIRECTORIES_H_
