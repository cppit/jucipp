#pragma once
#include "gtkmm.h"
#include <string>
#include <list>
#include <functional>
#include <set>

class Tooltip {
public:
  Tooltip(std::function<Glib::RefPtr<Gtk::TextBuffer>()> create_tooltip_buffer_, Gtk::TextView *text_view, Glib::RefPtr<Gtk::TextBuffer::Mark> start_mark_, Glib::RefPtr<Gtk::TextBuffer::Mark> end_mark_);
  Tooltip(std::function<Glib::RefPtr<Gtk::TextBuffer>()> create_tooltip_buffer_) : Tooltip(std::move(create_tooltip_buffer_), nullptr, Glib::RefPtr<Gtk::TextBuffer::Mark>(), Glib::RefPtr<Gtk::TextBuffer::Mark>()) {}
  ~Tooltip();
  
  void update();
  void show(bool disregard_drawn=false, const std::function<void()> &on_motion=nullptr);
  void hide(const std::pair<int, int> &last_mouse_pos = {-1, -1}, const std::pair<int, int> &mouse_pos = {-1, -1});
  
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
  Gdk::Rectangle rectangle;
  
  bool shown=false;
};

class Tooltips {
public:
  static std::set<Tooltip*> shown_tooltips;
  static Gdk::Rectangle drawn_tooltips_rectangle;
  static void init() {drawn_tooltips_rectangle=Gdk::Rectangle();}
  
  void show(const Gdk::Rectangle& rectangle, bool disregard_drawn=false);
  void show(bool disregard_drawn=false);
  void hide(const std::pair<int, int> &last_mouse_pos = {-1, -1}, const std::pair<int, int> &mouse_pos = {-1, -1});
  void clear() {tooltip_list.clear();};
  
  template<typename... Ts>
  void emplace_back(Ts&&... params) {
    tooltip_list.emplace_back(std::forward<Ts>(params)...);
  }
  
  std::function<void()> on_motion;
  
private:
  std::list<Tooltip> tooltip_list;
};
