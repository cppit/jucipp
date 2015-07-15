#ifndef JUCI_TOOLTIPS_H_
#define JUCI_TOOLTIPS_H_
#include "gtkmm.h"
#include <string>
#include <list>

class Tooltip {
public:
  Tooltip(std::function<Glib::RefPtr<Gtk::TextBuffer>()> get_buffer, Gtk::TextView& text_view, Glib::RefPtr<Gtk::TextBuffer::Mark> start_mark, Glib::RefPtr<Gtk::TextBuffer::Mark> end_mark);
  ~Tooltip();
  
  void update();
  void adjust(bool disregard_drawn=false);
  
  Gdk::Rectangle activation_rectangle;
  std::unique_ptr<Gtk::Window> window;
private:
  bool tooltip_on_motion_notify_event(GdkEventMotion* event);
  
  std::function<Glib::RefPtr<Gtk::TextBuffer>()> get_buffer;
  std::unique_ptr<Gtk::TextView> tooltip_widget;
  Glib::RefPtr<Gtk::TextBuffer::Mark> start_mark;
  Glib::RefPtr<Gtk::TextBuffer::Mark> end_mark;
  Gtk::TextView& text_view;
  int tooltip_width, tooltip_height;
};

class Tooltips : public std::list<Tooltip> {
public:
  static void init() {drawn_tooltips_rectangle=Gdk::Rectangle();}
  void show(const Gdk::Rectangle& rectangle, bool disregard_drawn=false);
  void show(bool disregard_drawn=false);
  void hide();
  
  static Gdk::Rectangle drawn_tooltips_rectangle;
};

#endif  // JUCI_TOOLTIPS_H_
