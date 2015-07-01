#ifndef JUCI_TOOLTIPS_H_
#define JUCI_TOOLTIPS_H_
#include "gtkmm.h"
#include <string>
#include <vector>

class Tooltip : public Gtk::Dialog {
public:
  Tooltip(Gtk::TextView& text_view, const std::string& label_text, Glib::RefPtr<Gtk::TextBuffer::Mark> start_mark, Glib::RefPtr<Gtk::TextBuffer::Mark> end_mark);
  
  void update();
  void adjust();
  
  Gtk::Label label;
  Glib::RefPtr<Gtk::TextBuffer::Mark> start_mark;
  Glib::RefPtr<Gtk::TextBuffer::Mark> end_mark;
  Gdk::Rectangle text_rectangle;

private:
  Gtk::TextView& text_view;
};

class Tooltips {
public:
  Tooltips(Gtk::TextView& text_view): text_view(text_view) {}
  
  void clear() {tooltips.clear();}
  
  void add(const std::string& text, Glib::RefPtr<Gtk::TextBuffer::Mark> start_mark, Glib::RefPtr<Gtk::TextBuffer::Mark> end_mark);
  
  void show(const Gdk::Rectangle& rectangle, bool clear_tooltips_rectangle=true);
  void show(bool clear_tooltips_rectangle=true);
  void hide();
  
  static Gdk::Rectangle tooltips_rectangle;
private:
  Gtk::TextView& text_view;
  std::vector<std::unique_ptr<Tooltip> > tooltips;
};

#endif  // JUCI_TOOLTIPS_H_
