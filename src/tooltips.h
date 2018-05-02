#pragma once
#include "gtkmm.h"
#include <string>
#include <list>
#include <functional>
#include <set>

class Tooltip {
public:
  Tooltip(std::function<Glib::RefPtr<Gtk::TextBuffer>()> create_tooltip_buffer, Gtk::TextView *text_view, Glib::RefPtr<Gtk::TextBuffer::Mark> start_mark, Glib::RefPtr<Gtk::TextBuffer::Mark> end_mark);
  Tooltip(std::function<Glib::RefPtr<Gtk::TextBuffer>()> create_tooltip_buffer) : Tooltip(create_tooltip_buffer, nullptr, Glib::RefPtr<Gtk::TextBuffer::Mark>(), Glib::RefPtr<Gtk::TextBuffer::Mark>()) {}
  ~Tooltip();
  
  void update();
  void show(bool disregard_drawn=false);
  void hide();
  
  Gdk::Rectangle activation_rectangle;
  Glib::RefPtr<Gtk::TextBuffer::Mark> start_mark;
  Glib::RefPtr<Gtk::TextBuffer::Mark> end_mark;
  
  Glib::RefPtr<Gtk::TextBuffer> text_buffer;
private:
  std::unique_ptr<Gtk::Window> window;
  void wrap_lines();
  
  std::function<Glib::RefPtr<Gtk::TextBuffer>()> create_tooltip_buffer;
  Gtk::TextView *text_view;
  std::pair<int, int> size;
  std::pair<int, int> position;
};

class Tooltips {
public:
  static std::set<Tooltip*> shown_tooltips;
  static void init() {drawn_tooltips_rectangle=Gdk::Rectangle();}
  void show(const Gdk::Rectangle& rectangle, bool disregard_drawn=false);
  void show(bool disregard_drawn=false);
  bool shown=false;
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
