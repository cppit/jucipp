#include "tooltips.h"
#include "selection_dialog.h"

std::set<Tooltip*> Tooltips::shown_tooltips;
Gdk::Rectangle Tooltips::drawn_tooltips_rectangle=Gdk::Rectangle();

Tooltip::Tooltip(std::function<Glib::RefPtr<Gtk::TextBuffer>()> create_tooltip_buffer, Gtk::TextView *text_view, 
Glib::RefPtr<Gtk::TextBuffer::Mark> start_mark, Glib::RefPtr<Gtk::TextBuffer::Mark> end_mark)
    : start_mark(start_mark), end_mark(end_mark), create_tooltip_buffer(create_tooltip_buffer), text_view(text_view) {}

Tooltip::~Tooltip() {
  Tooltips::shown_tooltips.erase(this);
  if(text_view) {
    text_view->get_buffer()->delete_mark(start_mark);
    text_view->get_buffer()->delete_mark(end_mark);
  }
}

void Tooltip::update() {
  if(text_view) {
    auto iter=start_mark->get_iter();
    auto end_iter=end_mark->get_iter();
    text_view->get_iter_location(iter, activation_rectangle);
    if(iter.get_offset()<end_iter.get_offset()) {
      while(iter.forward_char() && iter!=end_iter) {
        Gdk::Rectangle rectangle;
        text_view->get_iter_location(iter, rectangle);
        activation_rectangle.join(rectangle);
      }
    }
    int location_window_x, location_window_y;
    text_view->buffer_to_window_coords(Gtk::TextWindowType::TEXT_WINDOW_TEXT, activation_rectangle.get_x(), activation_rectangle.get_y(), location_window_x, location_window_y);
    activation_rectangle.set_x(location_window_x);
    activation_rectangle.set_y(location_window_y);
  }
}

void Tooltip::show(bool disregard_drawn) {
  Tooltips::shown_tooltips.emplace(this);
  
  if(!window) {
    //init window
    window=std::make_unique<Gtk::Window>(Gtk::WindowType::WINDOW_POPUP);
    
    auto g_application=g_application_get_default();
    auto gio_application=Glib::wrap(g_application, true);
    auto application=Glib::RefPtr<Gtk::Application>::cast_static(gio_application);
    window->set_transient_for(*application->get_active_window());
    
    window->set_type_hint(Gdk::WindowTypeHint::WINDOW_TYPE_HINT_TOOLTIP);
    
    window->set_events(Gdk::POINTER_MOTION_MASK);
    window->property_decorated()=false;
    window->set_accept_focus(false);
    window->set_skip_taskbar_hint(true);
    window->set_default_size(0, 0);
    
    window->get_style_context()->add_class("juci_tooltip_window");
    auto visual = window->get_screen()->get_rgba_visual();
    if(visual)
      gtk_widget_set_visual(reinterpret_cast<GtkWidget*>(window->gobj()), visual->gobj());

    auto box=Gtk::manage(new Gtk::Box());
    box->get_style_context()->add_class("juci_tooltip_box");
    window->add(*box);

    text_buffer=create_tooltip_buffer();
    wrap_lines();
    auto tooltip_text_view=Gtk::manage(new Gtk::TextView(text_buffer));
    
    tooltip_text_view->set_editable(false);
    if(text_view) {
      auto tag=text_view->get_buffer()->get_tag_table()->lookup("def:note_background");
      box->override_background_color(tag->property_background_rgba());
      tooltip_text_view->override_background_color(tag->property_background_rgba());
    }
    else
      box->override_background_color(tooltip_text_view->get_style_context()->get_background_color());
    box->add(*tooltip_text_view);

    auto layout=Pango::Layout::create(tooltip_text_view->get_pango_context());
    layout->set_text(text_buffer->get_text());
    layout->get_pixel_size(size.first, size.second);
    size.first+=6; // 2xpadding
    size.second+=8; // 2xpadding + 2
    
    window->signal_realize().connect([this] {
      if(!text_view) {
        auto &dialog=SelectionDialog::get();
        if(dialog && dialog->is_visible()) {
          int root_x, root_y;
          dialog->get_position(root_x, root_y);
          root_x-=3; // -1xpadding
          position.first=root_x;
          position.second=root_y-size.second;
          if(position.second<0)
            position.second=0;
        }
      }
      window->move(position.first, position.second);
    });
  }
  
  int root_x=0, root_y=0;
  if(text_view) {
    //Adjust if tooltip is left of text_view
    Gdk::Rectangle visible_rect;
    text_view->get_visible_rect(visible_rect);
    int visible_x, visible_y;
    text_view->buffer_to_window_coords(Gtk::TextWindowType::TEXT_WINDOW_TEXT, visible_rect.get_x(), visible_rect.get_y(), visible_x, visible_y);
    auto activation_rectangle_x=std::max(activation_rectangle.get_x(), visible_x);
    
    text_view->get_window(Gtk::TextWindowType::TEXT_WINDOW_TEXT)->get_root_coords(activation_rectangle_x, activation_rectangle.get_y(), root_x, root_y);
    root_x-=3; // -1xpadding
    if(root_y<size.second)
      root_x+=visible_rect.get_width()*0.1;
  }
  Gdk::Rectangle rectangle;
  rectangle.set_x(root_x);
  rectangle.set_y(std::max(0, root_y-size.second));
  rectangle.set_width(size.first);
  rectangle.set_height(size.second);
  
  if(!disregard_drawn) {
    if(Tooltips::drawn_tooltips_rectangle.get_width()!=0) {
      if(rectangle.intersects(Tooltips::drawn_tooltips_rectangle)) {
        int new_y=Tooltips::drawn_tooltips_rectangle.get_y()-size.second;
        if(new_y>=0)
          rectangle.set_y(new_y);
        else
          rectangle.set_x(Tooltips::drawn_tooltips_rectangle.get_x()+Tooltips::drawn_tooltips_rectangle.get_width()+2);
      }
      Tooltips::drawn_tooltips_rectangle.join(rectangle);
    }
    else
      Tooltips::drawn_tooltips_rectangle=rectangle;
  }

  position={rectangle.get_x(), rectangle.get_y()};
  if(window->get_realized())
    window->move(position.first, position.second);
  window->show_all();
}

void Tooltip::hide() {
  Tooltips::shown_tooltips.erase(this);
  if(window)
    window->hide();
}

void Tooltip::wrap_lines() {
  if(!text_buffer)
    return;
  
  auto iter=text_buffer->begin();
  
  while(iter) {
    auto last_space=text_buffer->end();
    bool end=false;
    for(unsigned c=0;c<=80;c++) {
      if(!iter) {
        end=true;
        break;
      }
      if(*iter==' ')
        last_space=iter;
      if(*iter=='\n') {
        end=true;
        iter.forward_char();
        break;
      }
      iter.forward_char();
    }
    if(!end) {
      while(!last_space && iter) { //If no space (word longer than 80)
        iter.forward_char();
        if(iter && *iter==' ')
          last_space=iter;
      }
      if(iter && last_space) {
        auto mark=text_buffer->create_mark(last_space);
        auto last_space_p=last_space;
        last_space.forward_char();
        text_buffer->erase(last_space_p, last_space);
        text_buffer->insert(mark->get_iter(), "\n");
        
        iter=mark->get_iter();
        iter.forward_char();

        text_buffer->delete_mark(mark);
      }
    }
  }
}

void Tooltips::show(const Gdk::Rectangle& rectangle, bool disregard_drawn) {
  for(auto &tooltip : tooltip_list) {
    tooltip.update();
    if(rectangle.intersects(tooltip.activation_rectangle)) {
      tooltip.show(disregard_drawn);
      shown=true;
    }
    else
      tooltip.hide();
  }
}

void Tooltips::show(bool disregard_drawn) {
  for(auto &tooltip : tooltip_list) {
    tooltip.update();
    tooltip.show(disregard_drawn);
    shown=true;
  }
}

void Tooltips::hide() {
  for(auto &tooltip : tooltip_list)
    tooltip.hide();
  shown=false;
}
