#include "tooltips.h"
#include <thread>

Gdk::Rectangle Tooltips::drawn_tooltips_rectangle=Gdk::Rectangle();

Tooltip::Tooltip(std::shared_ptr<Gtk::TextView> tooltip_widget, Gtk::TextView& text_view, 
Glib::RefPtr<Gtk::TextBuffer::Mark> start_mark, Glib::RefPtr<Gtk::TextBuffer::Mark> end_mark):
tooltip_widget(tooltip_widget), text_view(text_view), Gtk::Window(Gtk::WindowType::WINDOW_POPUP), 
start_mark(start_mark), end_mark(end_mark) {
  add(*tooltip_widget);
  property_decorated()=false;
  set_accept_focus(false);
  set_skip_taskbar_hint(true);
  set_default_size(0, 0);
  
  auto layout=Pango::Layout::create(tooltip_widget->get_pango_context());
  layout->set_text(tooltip_widget->get_buffer()->get_text());
  layout->get_pixel_size(tooltip_width, tooltip_height);
}

void Tooltip::update() {
  auto iter=start_mark->get_iter();
  auto end_iter=end_mark->get_iter();
  if(iter.get_offset()<end_iter.get_offset()) {
    text_view.get_iter_location(iter, activation_rectangle);
    iter++;
    while(iter!=end_iter) {
      Gdk::Rectangle rectangle;
      text_view.get_iter_location(iter, rectangle);
      activation_rectangle.join(rectangle);
      iter++;
    }
  }
  int location_window_x, location_window_y;
  text_view.buffer_to_window_coords(Gtk::TextWindowType::TEXT_WINDOW_TEXT, activation_rectangle.get_x(), activation_rectangle.get_y(), location_window_x, location_window_y);
  activation_rectangle.set_x(location_window_x);
  activation_rectangle.set_y(location_window_y);
}

void Tooltip::adjust() {
  int root_x, root_y;
  text_view.get_window(Gtk::TextWindowType::TEXT_WINDOW_TEXT)->get_root_coords(activation_rectangle.get_x(), activation_rectangle.get_y(), root_x, root_y);
  Gdk::Rectangle rectangle;
  rectangle.set_x(root_x);
  rectangle.set_y(root_y-tooltip_height);
  rectangle.set_width(tooltip_width);
  rectangle.set_height(tooltip_height);
  
  if(Tooltips::drawn_tooltips_rectangle.get_width()!=0) {
    if(rectangle.intersects(Tooltips::drawn_tooltips_rectangle))
      rectangle.set_y(Tooltips::drawn_tooltips_rectangle.get_y()-tooltip_height);
    Tooltips::drawn_tooltips_rectangle.join(rectangle);
  }
  else
    Tooltips::drawn_tooltips_rectangle=rectangle;

  move(rectangle.get_x(), rectangle.get_y());
}

void Tooltips::show(const Gdk::Rectangle& rectangle) {
  for(auto& tooltip: *this) {
    tooltip.update();
    if(rectangle.intersects(tooltip.activation_rectangle)) {
      tooltip.adjust();
      tooltip.show_all();
    }
    else
      tooltip.hide();
  }
}

void Tooltips::show() {
  for(auto& tooltip: *this) {
    tooltip.update();
    tooltip.adjust();
    tooltip.show_all();
  }
}

void Tooltips::hide() {
  for(auto& tooltip: *this) {
    tooltip.hide();
  }
}
