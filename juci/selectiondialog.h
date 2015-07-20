#ifndef JUCI_SELECTIONDIALOG_H_
#define JUCI_SELECTIONDIALOG_H_

#include "gtkmm.h"
#include "logging.h"
#include "tooltips.h"

class SelectionDialogBase {
public:
  SelectionDialogBase(Gtk::TextView& text_view, bool popup);
  virtual void show();
  virtual void hide();
  virtual void move();
  
  std::map<std::string, std::pair<std::string, std::string> > rows;
  std::function<void()> on_hide;
  bool shown=false;
  Glib::RefPtr<Gtk::TextBuffer::Mark> start_mark;
protected:
  virtual void resize();
  virtual void cursor_changed();
  
  Gtk::TextView& text_view;
  std::unique_ptr<Gtk::Window> window;
  std::unique_ptr<Gtk::ScrolledWindow> scrolled_window;
  std::unique_ptr<Gtk::ListViewText> list_view_text;
  std::unique_ptr<Gtk::Entry> search_entry;
  std::unique_ptr<Tooltips> tooltips;
  int last_selected;
private:
  bool popup;
};

class SelectionDialog : public SelectionDialogBase {
public:
  SelectionDialog(Gtk::TextView& text_view);
  void show();
  std::function<void(std::string selected)> on_select;
};

class CompletionDialog : public SelectionDialogBase {
public:
  CompletionDialog(Gtk::TextView& text_view);
  void show();
  bool on_key_release(GdkEventKey* key);
  bool on_key_press(GdkEventKey* key);
  
private:
  void select(bool hide_window=true);
  
  int show_offset;
  bool row_in_entry;
};

#endif  // JUCI_SELECTIONDIALOG_H_