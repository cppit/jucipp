#ifndef JUCI_WINDOW_H_
#define JUCI_WINDOW_H_

#include "gtkmm.h"
#include "directories.h"
#include "entrybox.h"
#include "notebook.h"
#include <atomic>

class Window : public Gtk::ApplicationWindow {
public:
  Window();
  Notebook notebook;
  class Config {
  public:
    std::string theme_name;
    std::string theme_variant;
    std::string version;
    std::pair<int, int> default_size;
  };

protected:
  bool on_key_press_event(GdkEventKey *event);
  bool on_delete_event (GdkEventAny *event);

private:
  Gtk::VPaned vpaned;
  Gtk::Paned directory_and_notebook_panes;
  Gtk::VBox notebook_vbox;
  Gtk::VBox terminal_vbox;
  Gtk::ScrolledWindow terminal_scrolled_window;
  Gtk::HBox info_and_status_hbox;
  Gtk::AboutDialog about;
  EntryBox entry_box;
  std::atomic<bool> compiling;

  void configure();
  void set_menu_actions();
  void activate_menu_items(bool activate=true);
  void hide();
  void search_and_replace_entry();
  void set_tab_entry();
  void goto_line_entry();
  void rename_token_entry();
  std::string last_search;
  std::string last_replace;
  std::string last_run_command;
  bool case_sensitive_search=true;
  bool regex_search=false;
  bool search_entry_shown=false;
};

#endif  // JUCI_WINDOW_H
