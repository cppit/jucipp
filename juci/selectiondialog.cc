#include "selectiondialog.h"

SelectionDialog::SelectionDialog(Gtk::TextView& text_view): Gtk::Dialog(), text_view(text_view), 
      list_view_text(1, false, Gtk::SelectionMode::SELECTION_SINGLE) {
  property_decorated()=false;
  set_skip_taskbar_hint(true);
  scrolled_window.set_policy(Gtk::PolicyType::POLICY_NEVER, Gtk::PolicyType::POLICY_NEVER);
  list_view_text.set_enable_search(true);
  list_view_text.set_headers_visible(false);
  list_view_text.set_hscroll_policy(Gtk::ScrollablePolicy::SCROLL_NATURAL);
  list_view_text.set_activate_on_single_click(true);
}

void SelectionDialog::show(const std::map<std::string, std::string>& rows) {
  for (auto &i : rows) {
    list_view_text.append(i.first);
  }
  scrolled_window.add(list_view_text);
  get_vbox()->pack_start(scrolled_window);
  set_transient_for((Gtk::Window&)(*text_view.get_toplevel()));
  show_all();
  int popup_x = get_width();
  int popup_y = rows.size() * 20;
  adjust(popup_x, popup_y);
  
  list_view_text.signal_row_activated().connect([this](const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn*) {
    if(on_select)
      on_select(list_view_text);
    response(Gtk::RESPONSE_DELETE_EVENT);
  });
  
  signal_focus_out_event().connect(sigc::mem_fun(*this, &SelectionDialog::close), false);
  
  run();
}

bool SelectionDialog::close(GdkEventFocus*) {
  response(Gtk::RESPONSE_DELETE_EVENT);
  return true;
}

void SelectionDialog::adjust(int current_x, int current_y) {
  INFO("SelectionDialog set size");
  int view_x = text_view.get_width();
  int view_y = 150;
  bool is_never_scroll_x = true;
  bool is_never_scroll_y = true;
  if (current_x > view_x) {
    current_x = view_x;
    is_never_scroll_x = false;
  }
  if (current_y > view_y) {
    current_y = view_y;
    is_never_scroll_y = false;
  }
  scrolled_window.set_size_request(current_x, current_y);
  if (!is_never_scroll_x && !is_never_scroll_y) {
    scrolled_window.set_policy(Gtk::PolicyType::POLICY_AUTOMATIC, Gtk::PolicyType::POLICY_AUTOMATIC);
  } else if (!is_never_scroll_x && is_never_scroll_y) {
    scrolled_window.set_policy(Gtk::PolicyType::POLICY_AUTOMATIC, Gtk::PolicyType::POLICY_NEVER);
  } else if (is_never_scroll_x && !is_never_scroll_y) {
    scrolled_window.set_policy(Gtk::PolicyType::POLICY_NEVER, Gtk::PolicyType::POLICY_AUTOMATIC);
  }
  
  INFO("SelectionDialog set position");
  Gdk::Rectangle temp1, temp2;
  text_view.get_cursor_locations(text_view.get_buffer()->get_insert()->get_iter(), temp1, temp2);
  int view_edge_x = 0;
  int view_edge_y = 0;
  int x, y;
  text_view.buffer_to_window_coords(Gtk::TextWindowType::TEXT_WINDOW_WIDGET, 
                               temp1.get_x(), temp1.get_y(), x, y);
  Glib::RefPtr<Gdk::Window> gdkw = text_view.get_window(Gtk::TextWindowType::TEXT_WINDOW_WIDGET);
  gdkw->get_origin(view_edge_x, view_edge_y);

  x += view_edge_x;
  y += view_edge_y;
  if ((view_edge_x-x)*-1 > text_view.get_width()-current_x) {
    x -= current_x;
    if (x < view_edge_x) x = view_edge_x;
  }
  if ((view_edge_y-y)*-1 > text_view.get_height()-current_y) {
    y -= (current_y+14) + 15;
    if (x < view_edge_y) y = view_edge_y +15;
  }
  move(x, y+15);
}