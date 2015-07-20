#include "selectiondialog.h"

namespace sigc {
  SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
}

SelectionDialogBase::SelectionDialogBase(Gtk::TextView& text_view, bool popup): text_view(text_view), popup(popup) {}

void SelectionDialogBase::show() {
  if(rows.size()==0)
    return;
  
  if(popup)
    window=std::unique_ptr<Gtk::Window>(new Gtk::Window(Gtk::WindowType::WINDOW_POPUP));
  else
    window=std::unique_ptr<Gtk::Dialog>(new Gtk::Dialog());
  scrolled_window=std::unique_ptr<Gtk::ScrolledWindow>(new Gtk::ScrolledWindow());
  list_view_text=std::unique_ptr<Gtk::ListViewText>(new Gtk::ListViewText(1, false, Gtk::SelectionMode::SELECTION_BROWSE));
  search_entry=std::unique_ptr<Gtk::Entry>(new Gtk::Entry());
  list_view_text->set_search_entry(*search_entry);
  
  window->set_default_size(0, 0);
  window->property_decorated()=false;
  window->set_skip_taskbar_hint(true);
  scrolled_window->set_policy(Gtk::PolicyType::POLICY_AUTOMATIC, Gtk::PolicyType::POLICY_AUTOMATIC);
  list_view_text->set_enable_search(true);
  list_view_text->set_headers_visible(false);
  list_view_text->set_hscroll_policy(Gtk::ScrollablePolicy::SCROLL_NATURAL);
  list_view_text->set_activate_on_single_click(true);
  list_view_text->set_hover_selection(false);
  list_view_text->set_rules_hint(true);
  //list_view_text->set_fixed_height_mode(true); //TODO: This is buggy on OS X, remember to post an issue on GTK+ 3

  last_selected=-1;

  list_view_text->signal_cursor_changed().connect(sigc::mem_fun(*this, &SelectionDialog::cursor_changed), true);
  list_view_text->signal_realize().connect([this](){
    resize();
  });
  
  list_view_text->clear_items();
  for (auto &i : rows) {
    list_view_text->append(i.first);
  }
  
  scrolled_window->add(*list_view_text);
  if(popup)
    window->add(*scrolled_window);
  else {
    auto dialog=(Gtk::Dialog*)window.get();
    dialog->get_vbox()->pack_start(*search_entry, false, false);
    dialog->get_vbox()->pack_start(*scrolled_window, true, true);
    dialog->set_transient_for((Gtk::Window&)(*text_view.get_toplevel()));
  }
  if(rows.size()>0) {
    list_view_text->get_selection()->select(*list_view_text->get_model()->children().begin());
    list_view_text->scroll_to_row(list_view_text->get_selection()->get_selected_rows()[0]);
  }

  move();
  window->show_all();
  search_entry->show();
  shown=true;
  row_in_entry=false;
}

void SelectionDialogBase::hide() {
  shown=false;
  window->hide();
  if(tooltips)
    tooltips->hide();
  if(on_hide)
    on_hide();
}

void SelectionDialogBase::cursor_changed() {
  auto selected=list_view_text->get_selected();
  if(selected.size()>0) {
    if(selected[0]!=last_selected || last_selected==-1) {
      if(tooltips)
        tooltips->hide();
      auto row = rows.at(list_view_text->get_text(selected[0]));
      if(row.second.size()>0) {
        tooltips=std::unique_ptr<Tooltips>(new Tooltips());
        auto tooltip_text=row.second;
        auto get_tooltip_buffer=[this, tooltip_text]() {
          auto tooltip_buffer=Gtk::TextBuffer::create(text_view.get_buffer()->get_tag_table());
          //TODO: Insert newlines to tooltip_text (use 80 chars, then newline?)
          tooltip_buffer->insert_at_cursor(tooltip_text);
          return tooltip_buffer;
        };
        tooltips->emplace_back(get_tooltip_buffer, text_view, text_view.get_buffer()->create_mark(start_mark->get_iter()), text_view.get_buffer()->create_mark(text_view.get_buffer()->get_insert()->get_iter()));
        tooltips->show(true);
      }
    }
  }
  else if(tooltips)
    tooltips->hide();
  if(selected.size()>0)
    last_selected=selected[0];
  else
    last_selected=-1;
}

void SelectionDialogBase::move() {
  INFO("SelectionDialog set position");
  Gdk::Rectangle rectangle;
  text_view.get_iter_location(start_mark->get_iter(), rectangle);
  int buffer_x=rectangle.get_x();
  int buffer_y=rectangle.get_y()+rectangle.get_height();
  int window_x, window_y;
  text_view.buffer_to_window_coords(Gtk::TextWindowType::TEXT_WINDOW_TEXT, buffer_x, buffer_y, window_x, window_y);
  int root_x, root_y;
  text_view.get_window(Gtk::TextWindowType::TEXT_WINDOW_TEXT)->get_root_coords(window_x, window_y, root_x, root_y);
  window->move(root_x, root_y+1); //TODO: replace 1 with some margin
}

void SelectionDialogBase::resize() {
  INFO("SelectionDialog set size");
  
  if(list_view_text->get_realized()) {
    int row_width=0, row_height;
    Gdk::Rectangle rect;
    list_view_text->get_cell_area(list_view_text->get_model()->get_path(list_view_text->get_model()->children().begin()), *(list_view_text->get_column(0)), rect);
    row_width=rect.get_width();
    row_height=rect.get_height();

    row_width+=rect.get_x()*2; //TODO: Add correct margin x and y
    row_height+=rect.get_y()*2;

    if(row_width>text_view.get_width()/2)
      row_width=text_view.get_width()/2;
    else
      scrolled_window->set_policy(Gtk::PolicyType::POLICY_NEVER, Gtk::PolicyType::POLICY_AUTOMATIC);

    int window_height=std::min(row_height*(int)rows.size(), row_height*10);
    if(!popup)
      window_height+=search_entry->get_height();
    window->resize(row_width, window_height);
  }
}

SelectionDialog::SelectionDialog(Gtk::TextView& text_view) : SelectionDialogBase(text_view, false) {}

void SelectionDialog::show() {
  SelectionDialogBase::show();
  std::shared_ptr<std::string> search_key(new std::string());
  auto filter_model=Gtk::TreeModelFilter::create(list_view_text->get_model());
  filter_model->set_visible_func([this, search_key](const Gtk::TreeModel::const_iterator& iter){
    std::string row;
    iter->get_value(0, row);
    if(row.find(*search_key)!=std::string::npos)
      return true;
    return false;
  });
  list_view_text->set_model(filter_model);
  list_view_text->set_search_equal_func([this](const Glib::RefPtr<Gtk::TreeModel>& model, int column, const Glib::ustring& key, const Gtk::TreeModel::iterator& iter) {
    return false;
  });
  search_entry->signal_changed().connect([this, search_key, filter_model](){
    *search_key=search_entry->get_text();
    filter_model->refilter();
    list_view_text->set_search_entry(*search_entry); //TODO:Report the need of this to GTK's git (bug)
  });
  search_entry->signal_event().connect([this, search_key, filter_model](GdkEvent* event) {
    if(event->type==GDK_KEY_PRESS) {
      auto key=(GdkEventKey*)event;
      if(key->keyval==GDK_KEY_Down) {
        auto it=list_view_text->get_selection()->get_selected();
        if(it) {
          it++;
          if(it) {
            list_view_text->set_cursor(list_view_text->get_model()->get_path(it));
          }
        }
        else
          list_view_text->set_cursor(list_view_text->get_model()->get_path(list_view_text->get_model()->children().begin()));
        return true;
      }
      if(key->keyval==GDK_KEY_Up) {
        auto it=list_view_text->get_selection()->get_selected();
        if(it) {
          it--;
          if(it) {
            list_view_text->set_cursor(list_view_text->get_model()->get_path(it));
          }
        }
        return true;
      }
    }
    return false;
  });
  auto activate=[this](){
    if(on_select && list_view_text->get_selected().size()>0) {
      auto it=list_view_text->get_selection()->get_selected();
      std::string row;
      it->get_value(0, row);
      std::string selected = rows.at(row).first;
      on_select(selected);
    }
    window->hide();
  };
  search_entry->signal_activate().connect([this, activate](){
    activate();
  });
  list_view_text->signal_row_activated().connect([this, activate](const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn*) {
    activate();
   });
  window->signal_focus_out_event().connect([this](GdkEventFocus*){
    window->hide();
    return true;
  });
  list_view_text->set_cursor(list_view_text->get_model()->get_path(list_view_text->get_model()->children().begin()));
}

CompleteDialog::CompleteDialog(Gtk::TextView& text_view) : SelectionDialogBase(text_view, true) {}

void CompleteDialog::show() {
  SelectionDialogBase::show();
  
  show_offset=text_view.get_buffer()->get_insert()->get_iter().get_offset();
  
  list_view_text->signal_row_activated().connect([this](const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn*) {
    if(shown) {
      select();
    }
  });
  
  auto text=text_view.get_buffer()->get_text(start_mark->get_iter(), text_view.get_buffer()->get_insert()->get_iter());
  if(text.size()>0) {
    search_entry->set_text(text);
    list_view_text->set_search_entry(*search_entry);
  }
}

void CompleteDialog::select(bool hide_window) {
  row_in_entry=true;
  auto selected=list_view_text->get_selected();
  std::pair<std::string, std::string> select;
  if(selected.size()>0) {
    select = rows.at(list_view_text->get_text(selected[0]));
    text_view.get_buffer()->erase(start_mark->get_iter(), text_view.get_buffer()->get_insert()->get_iter());
    text_view.get_buffer()->insert(start_mark->get_iter(), select.first);
  }
  if(hide_window) {
    hide();
    char find_char=select.first.back();
    if(find_char==')' || find_char=='>') {
      if(find_char==')')
        find_char='(';
      else
        find_char='<';
      size_t pos=select.first.find(find_char);
      if(pos!=std::string::npos) {
        auto start_offset=start_mark->get_iter().get_offset()+pos+1;
        auto end_offset=start_mark->get_iter().get_offset()+select.first.size()-1;
        if(start_offset!=end_offset)
          text_view.get_buffer()->select_range(text_view.get_buffer()->get_iter_at_offset(start_offset), text_view.get_buffer()->get_iter_at_offset(end_offset));
      }
    }
  }
}

bool CompleteDialog::on_key_release(GdkEventKey* key) {
  if(key->keyval==GDK_KEY_Down || key->keyval==GDK_KEY_Up)
    return false;
    
  if(show_offset>text_view.get_buffer()->get_insert()->get_iter().get_offset()) {
    hide();
  }
  else {
    auto text=text_view.get_buffer()->get_text(start_mark->get_iter(), text_view.get_buffer()->get_insert()->get_iter());
    if(text.size()>0) {
      search_entry->set_text(text);
      list_view_text->set_search_entry(*search_entry);
    }
    cursor_changed();
  }
  return false;
}

bool CompleteDialog::on_key_press(GdkEventKey* key) {
  if((key->keyval>=GDK_KEY_0 && key->keyval<=GDK_KEY_9) || 
     (key->keyval>=GDK_KEY_A && key->keyval<=GDK_KEY_Z) ||
     (key->keyval>=GDK_KEY_a && key->keyval<=GDK_KEY_z) ||
     key->keyval==GDK_KEY_underscore || key->keyval==GDK_KEY_BackSpace) {
    if(row_in_entry) {
      text_view.get_buffer()->erase(start_mark->get_iter(), text_view.get_buffer()->get_insert()->get_iter());
      row_in_entry=false;
      if(key->keyval==GDK_KEY_BackSpace) {
        return true;
      }
    }
    return false;
  }
  if(key->keyval==GDK_KEY_Shift_L || key->keyval==GDK_KEY_Shift_R || key->keyval==GDK_KEY_Alt_L || key->keyval==GDK_KEY_Alt_R || key->keyval==GDK_KEY_Control_L || key->keyval==GDK_KEY_Control_R || key->keyval==GDK_KEY_Meta_L || key->keyval==GDK_KEY_Meta_R)
    return false;
  if(key->keyval==GDK_KEY_Down) {
    auto it=list_view_text->get_selection()->get_selected();
    if(it) {
      it++;
      if(it) {
        list_view_text->set_cursor(list_view_text->get_model()->get_path(it));
      }
    }
    select(false);
    cursor_changed();
    return true;
  }
  if(key->keyval==GDK_KEY_Up) {
    auto it=list_view_text->get_selection()->get_selected();
    if(it) {
      it--;
      if(it) {
        list_view_text->set_cursor(list_view_text->get_model()->get_path(it));
      }
    }
    select(false);
    cursor_changed();
    return true;
  }
  if(key->keyval==GDK_KEY_Return || key->keyval==GDK_KEY_ISO_Left_Tab || key->keyval==GDK_KEY_Tab) {
    select();
    return true;
  }
  hide();
  return false;
}
