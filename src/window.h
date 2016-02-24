#ifndef JUCI_WINDOW_H_
#define JUCI_WINDOW_H_

#include <gtkmm.h>
#include "entrybox.h"
#include "notebook.h"
#include "project.h"
#include <atomic>

class Window : public Gtk::ApplicationWindow {
private:
  Window();
  Notebook &notebook; //convenience reference
public:
  static Window &get() {
    static Window singleton;
    return singleton;
  }

protected:
  bool on_key_press_event(GdkEventKey *event) override;
  bool on_delete_event(GdkEventAny *event) override;

private:
  Gtk::VPaned vpaned;
  Gtk::Paned directory_and_notebook_panes;
  Gtk::VBox notebook_vbox;
  Gtk::VBox terminal_vbox;
  Gtk::ScrolledWindow directories_scrolled_window;
  Gtk::ScrolledWindow terminal_scrolled_window;
  Gtk::HBox info_and_status_hbox;
  Gtk::AboutDialog about;
  EntryBox entry_box;

  void configure();
  void set_menu_actions();
  void activate_menu_items(bool activate=true);
  void search_and_replace_entry();
  void set_tab_entry();
  void goto_line_entry();
  void rename_token_entry();
  std::string last_search;
  std::string last_replace;
  std::string last_run_command;
  std::string last_run_debug_command;
  bool case_sensitive_search=true;
  bool regex_search=false;
  bool search_entry_shown=false;
};

#endif  // JUCI_WINDOW_H
