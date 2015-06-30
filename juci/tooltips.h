#ifndef JUCI_TOOLTIPS_H_
#define JUCI_TOOLTIPS_H_
#include "gtkmm.h"
#include <string>
#include <vector>

class Tooltip : public Gtk::Dialog {
public:
  Tooltip(Gtk::TextView& text_view, const std::string& label_text, Glib::RefPtr<Gtk::TextBuffer::Mark> start_mark, Glib::RefPtr<Gtk::TextBuffer::Mark> end_mark, Gdk::Rectangle &tooltips_rectangle);
  
  void update();
  void adjust();
  
  Gtk::Label label;
  Glib::RefPtr<Gtk::TextBuffer::Mark> start_mark;
  Glib::RefPtr<Gtk::TextBuffer::Mark> end_mark;
  Gdk::Rectangle text_rectangle;

private:
  Gtk::TextView& text_view;
  Gdk::Rectangle &tooltips_rectangle;
};

class Tooltips {
public:
  Tooltips(Gtk::TextView& text_view): text_view(text_view) {}
  
  void clear() {tooltips.clear();}
  
  void add(const std::string& text, Glib::RefPtr<Gtk::TextBuffer::Mark> start_mark, Glib::RefPtr<Gtk::TextBuffer::Mark> end_mark);
  
  void show(const Gdk::Rectangle& rectangle);
  void show();
  void hide();
  
  Gdk::Rectangle tooltips_rectangle;
private:
  Gtk::TextView& text_view;
  std::vector<std::unique_ptr<Tooltip> > tooltips;
};

#endif  // JUCI_TOOLTIPS_H_
