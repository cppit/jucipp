#ifndef JUCI_SELECTIONDIALOG_H_
#define JUCI_SELECTIONDIALOG_H_

#include "gtkmm.h"
#include "logging.h"
#include "tooltips.h"

class SelectionDialog {
public:
  SelectionDialog(Gtk::TextView& text_view);
  void show();
  void hide();
  bool close(GdkEventFocus*);
  void move();
  bool on_key_release(GdkEventKey* key);
  bool on_key_press(GdkEventKey* key);
  
  std::map<std::string, std::pair<std::string, std::string> > rows;

  bool shown=false;
private:
  void resize();
  void select(bool hide_window=true);
  void cursor_changed();
  
  Gtk::Entry search_entry;
  Glib::RefPtr<Gtk::TextBuffer::Mark> start_mark;
  int start_offset;
  bool selected;
  
  Gtk::TextView& text_view;
  std::unique_ptr<Gtk::Window> window;
  std::unique_ptr<Gtk::ScrolledWindow> scrolled_window;
  std::unique_ptr<Gtk::ListViewText> list_view_text;
  std::unique_ptr<Tooltips> tooltips;
};

#endif  // JUCI_SELECTIONDIALOG_H_