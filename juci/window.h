#ifndef JUCI_WINDOW_H_
#define JUCI_WINDOW_H_

#include "api.h"
#include <cstddef>
#include "directories.h"
#include "entrybox.h"

class Window : public Gtk::Window {
public:
  Window();
  Notebook notebook;
  Directories::Controller directories;
protected:
  bool on_key_press_event(GdkEventKey *event);
  bool on_delete_event (GdkEventAny *event);
private:
  Gtk::Box box;
  Gtk::VPaned vpaned;
  Gtk::Paned directory_and_notebook_panes;
  EntryBox entry_box;
  PluginApi plugin_api;
  std::mutex running;

  void hide();
  void new_file_entry();
  void open_folder_dialog();
  void open_file_dialog();
  void save_file_dialog();
  void on_directory_navigation(const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn* column);
  
  void search_and_replace_entry();
  std::string last_search;
  std::string last_replace;
  bool case_sensitive_search=true;
  bool regex_search=false;
  bool search_entry_shown=false;

};

#endif  // JUCI_WINDOW_H
