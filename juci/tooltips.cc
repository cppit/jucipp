#include "tooltips.h"
#include <iostream>

Tooltip::Tooltip(Gtk::TextView& text_view, const std::string& label_text, 
Glib::RefPtr<Gtk::TextBuffer::Mark> start_mark, Glib::RefPtr<Gtk::TextBuffer::Mark> end_mark, Gdk::Rectangle &tooltips_rectangle): 
text_view(text_view), Gtk::Dialog("", (Gtk::Window&)*text_view.get_toplevel()), label(label_text), 
start_mark(start_mark), end_mark(end_mark), tooltips_rectangle(tooltips_rectangle) {
  get_content_area()->add(label);
  property_decorated()=false;
  set_accept_focus(false);
}

void Tooltip::update() {
  auto iter=start_mark->get_iter();
  auto end_iter=end_mark->get_iter();
  if(iter.get_offset()<end_iter.get_offset()) {
    text_view.get_iter_location(iter, text_rectangle);
    iter++;
    while(iter!=end_iter) {
      Gdk::Rectangle rectangle;
      text_view.get_iter_location(iter, rectangle);
      text_rectangle.join(rectangle);
      iter++;
    }
  }
  int location_window_x, location_window_y;
  text_view.buffer_to_window_coords(Gtk::TextWindowType::TEXT_WINDOW_TEXT, text_rectangle.get_x(), text_rectangle.get_y(), location_window_x, location_window_y);
  text_rectangle.set_x(location_window_x);
  text_rectangle.set_y(location_window_y);
}

void Tooltip::adjust() {
  int tooltip_width, tooltip_height;
  get_size(tooltip_width, tooltip_height);
  int root_x, root_y;
  text_view.get_window(Gtk::TextWindowType::TEXT_WINDOW_TEXT)->get_root_coords(text_rectangle.get_x(), text_rectangle.get_y(), root_x, root_y);
  Gdk::Rectangle rectangle;
  rectangle.set_x(root_x);
  rectangle.set_y(root_y-tooltip_height);
  rectangle.set_width(tooltip_width);
  rectangle.set_height(tooltip_height);
  
  if(tooltips_rectangle.get_width()!=0) {
    if(rectangle.intersects(tooltips_rectangle))
      rectangle.set_y(tooltips_rectangle.get_y()-tooltip_height);
    tooltips_rectangle.join(rectangle);
  }
  else
    tooltips_rectangle=rectangle;

  if(rectangle.get_y()<0)
    rectangle.set_y(0);
  move(rectangle.get_x(), rectangle.get_y());
}

void Tooltips::add(const std::string& text, Glib::RefPtr<Gtk::TextBuffer::Mark> start_mark, Glib::RefPtr<Gtk::TextBuffer::Mark> end_mark) {
  tooltips.emplace_back(new Tooltip(text_view, text, start_mark, end_mark, tooltips_rectangle));
}

void Tooltips::show(const Gdk::Rectangle& rectangle) {
  tooltips_rectangle=Gdk::Rectangle();
  for(auto& tooltip: tooltips) {
    tooltip->update();
    if(rectangle.intersects(tooltip->text_rectangle)) {
      tooltip->show_all();
      tooltip->adjust();
    }
    else
      tooltip->hide();
  }
}

void Tooltips::show() {
  tooltips_rectangle=Gdk::Rectangle();
  for(auto& tooltip: tooltips) {
    tooltip->update();
    tooltip->show_all();
    tooltip->adjust();
  }
}

void Tooltips::hide() {
  for(auto& tooltip: tooltips) {
    tooltip->hide();
  }
}
