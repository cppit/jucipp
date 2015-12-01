#ifndef JUCI_TOOLTIPS_H_
#define JUCI_TOOLTIPS_H_
#include "gtkmm.h"
#include <string>
#include <list>

class Tooltip {
public:
  Tooltip(std::function<Glib::RefPtr<Gtk::TextBuffer>()> create_tooltip_buffer, Gtk::TextView& text_view, Glib::RefPtr<Gtk::TextBuffer::Mark> start_mark, Glib::RefPtr<Gtk::TextBuffer::Mark> end_mark);
  ~Tooltip();
  
  void update();
  void adjust(bool disregard_drawn=false);
  
  Gdk::Rectangle activation_rectangle;
  std::unique_ptr<Gtk::Window> window;
  Glib::RefPtr<Gtk::TextBuffer::Mark> start_mark;
  Glib::RefPtr<Gtk::TextBuffer::Mark> end_mark;
private:
  void wrap_lines(Glib::RefPtr<Gtk::TextBuffer> text_buffer);
  
  std::function<Glib::RefPtr<Gtk::TextBuffer>()> create_tooltip_buffer;
  std::unique_ptr<Gtk::TextView> tooltip_widget;
  Gtk::TextView& text_view;
  int tooltip_width, tooltip_height;
};

class Tooltips {
public:
  static void init() {drawn_tooltips_rectangle=Gdk::Rectangle();}
  void show(const Gdk::Rectangle& rectangle, bool disregard_drawn=false);
  void show(bool disregard_drawn=false);
  void hide();
  void clear() {tooltip_list.clear();};
  
  template<typename... Ts>
  void emplace_back(Ts&&... params) {
    tooltip_list.emplace_back(std::forward<Ts>(params)...);
  }
   
  static Gdk::Rectangle drawn_tooltips_rectangle;
  
  private:
  std::list<Tooltip> tooltip_list;
};

#endif  // JUCI_TOOLTIPS_H_
