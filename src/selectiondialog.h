#ifndef JUCI_SELECTIONDIALOG_H_
#define JUCI_SELECTIONDIALOG_H_

#include "gtkmm.h"
#include "logging.h"
#include <unordered_map>

class ListViewText : public Gtk::TreeView {
  class ColumnRecord : public Gtk::TreeModel::ColumnRecord {
  public:
    ColumnRecord() {
      add(text);
    }
    Gtk::TreeModelColumn<std::string> text;
  };
public:
  bool use_markup;
  ListViewText(bool use_markup);
  void append(const std::string& value);
  void clear();
private:
  Glib::RefPtr<Gtk::ListStore> list_store;
  ColumnRecord column_record;
  Gtk::CellRendererText cell_renderer;
};

class SelectionDialogBase {
public:
  SelectionDialogBase(Gtk::TextView& text_view, Glib::RefPtr<Gtk::TextBuffer::Mark> start_mark, bool show_search_entry, bool use_markup);
  ~SelectionDialogBase();
  void add_row(const std::string& row);
  void show();
  void hide();
  void move();
  
  std::function<void()> on_hide;
  std::function<void(const std::string& selected, bool hide_window)> on_select;
  std::function<void(const std::string &selected)> on_changed;
  Glib::RefPtr<Gtk::TextBuffer::Mark> start_mark;
  
  bool shown=false;
protected:
  void cursor_changed();
  
  void resize();
  Gtk::TextView& text_view;
  
  std::unique_ptr<Gtk::Window> window;
  Gtk::ScrolledWindow scrolled_window;
  ListViewText list_view_text;
  Gtk::Entry search_entry;
  bool show_search_entry;
  
  std::string last_row;
};

class SelectionDialog : public SelectionDialogBase {
public:
  SelectionDialog(Gtk::TextView& text_view, Glib::RefPtr<Gtk::TextBuffer::Mark> start_mark, bool show_search_entry=true, bool use_markup=false);
  bool on_key_press(GdkEventKey* key);
};

class CompletionDialog : public SelectionDialogBase {
public:
  CompletionDialog(Gtk::TextView& text_view, Glib::RefPtr<Gtk::TextBuffer::Mark> start_mark);
  bool on_key_release(GdkEventKey* key);
  bool on_key_press(GdkEventKey* key);
  
private:
  void select(bool hide_window=true);
  
  int show_offset;
  bool row_in_entry=false;
};

#endif  // JUCI_SELECTIONDIALOG_H_