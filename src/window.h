#ifndef JUCI_WINDOW_H_
#define JUCI_WINDOW_H_

#include "gtkmm.h"
#include "directories.h"
#include "entrybox.h"
#include "notebook.h"
#include "menu.h"
#include <boost/property_tree/json_parser.hpp>
#include <atomic>

class Window : public Gtk::Window {
public:
  Window();
  Directories directories;
  Notebook notebook;
  class Config {
  public:
    std::string theme_name;
    std::string theme_variant;
    boost::property_tree::ptree keybindings;
  };

protected:
  bool on_key_press_event(GdkEventKey *event);
  bool on_delete_event (GdkEventAny *event);

private:
  Gtk::Box box;
  Gtk::VPaned vpaned;
  Gtk::Paned directory_and_notebook_panes;
  Gtk::VBox notebook_vbox;
  Gtk::VBox terminal_vbox;
  Gtk::ScrolledWindow terminal_scrolled_window;
  Gtk::HBox status_hbox;
  EntryBox entry_box;
  Menu menu;
  std::atomic<bool> compiling;

  void create_menu();
  void hide();
  void new_file_dialog();
  void new_folder_dialog();
  void new_cpp_project_dialog();
  void open_folder_dialog();
  void open_file_dialog();
  void save_file_dialog();
  void search_and_replace_entry();
  void goto_line_entry();
  void rename_token_entry();
  void generate_keybindings();
  std::string last_search;
  std::string last_replace;
  std::string last_run_command;
  bool case_sensitive_search=true;
  bool regex_search=false;
  bool search_entry_shown=false;
};

#endif  // JUCI_WINDOW_H
