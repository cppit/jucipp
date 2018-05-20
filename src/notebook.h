#pragma once
#include <iostream>
#include "gtkmm.h"
#include "source.h"
#include <type_traits>
#include <map>
#include <sigc++/sigc++.h>

class Notebook : public Gtk::Paned {
  class TabLabel : public Gtk::EventBox {
  public:
    TabLabel(const std::function<void()> &on_close);
    Gtk::Label label;
  };
  
  class CursorLocation {
  public:
    CursorLocation(Source::View *view, Glib::RefPtr<Gtk::TextBuffer::Mark> mark_) : view(view), mark(std::move(mark_)) {}
    Source::View *view;
    Glib::RefPtr<Gtk::TextBuffer::Mark> mark;
  };
  
private:
  Notebook();
public:
  static Notebook &get() {
    static Notebook singleton;
    return singleton;
  }
  
  size_t size();
  Source::View* get_view(size_t index);
  Source::View* get_current_view();
  std::vector<Source::View*> &get_views();
  
  void open(const boost::filesystem::path &file_path, size_t notebook_index=-1);
  void configure(size_t index);
  bool save(size_t index);
  bool save_current();
  bool close(size_t index);
  bool close_current();
  void next();
  void previous();
  void toggle_split();
  /// Hide/Show tabs.		
  void toggle_tabs();
  boost::filesystem::path get_current_folder();
  std::vector<std::pair<size_t, Source::View*>> get_notebook_views();

  Gtk::Label status_location;
  Gtk::Label status_file_path;
  Gtk::Label status_branch;
  Gtk::Label status_diagnostics;
  Gtk::Label status_state;
  void update_status(Source::BaseView *view);
  void clear_status();
  
  std::function<void(Source::View*)> on_change_page;
  std::function<void(Source::View*)> on_close_page;
  
  /// Cursor history
  std::vector<CursorLocation> cursor_locations;
  size_t current_cursor_location=-1;
  bool disable_next_update_cursor_locations=false;
  void delete_cursor_locations(Source::View *view);
  
private:
  size_t get_index(Source::View *view);
  Source::View *get_view(size_t notebook_index, int page);
  void focus_view(Source::View *view);
  std::pair<size_t, int> get_notebook_page(size_t index);
  
  std::vector<Gtk::Notebook> notebooks;
  std::vector<Source::View*> source_views; //Is NOT freed in destructor, this is intended for quick program exit.
  std::vector<std::unique_ptr<Gtk::Widget> > source_maps;
  std::vector<std::unique_ptr<Gtk::ScrolledWindow> > scrolled_windows;
  std::vector<std::unique_ptr<Gtk::Box> > hboxes;
  std::vector<std::unique_ptr<TabLabel> > tab_labels;
  
  bool split=false;
  size_t last_index=-1;
  
  void set_current_view(Source::View *view);
  Source::View* current_view=nullptr;
  Source::View* intermediate_view=nullptr;
  
  bool save_modified_dialog(size_t index);
};
