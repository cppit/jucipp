#include "notebook.h"
#include "config.h"
#include "directories.h"
#include <fstream>
#include <regex>
#include "project.h"
#include "filesystem.h"

#if GTKSOURCEVIEWMM_MAJOR_VERSION > 2 & GTKSOURCEVIEWMM_MINOR_VERSION > 17
#include "gtksourceview-3.0/gtksourceview/gtksourcemap.h"
#endif

namespace sigc {
#ifndef SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
  template <typename Functor>
  struct functor_trait<Functor, false> {
    typedef decltype (::sigc::mem_fun(std::declval<Functor&>(),
                                      &Functor::operator())) _intermediate;
    typedef typename _intermediate::result_type result_type;
    typedef Functor functor_type;
  };
#else
  SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
#endif
}

Notebook::TabLabel::TabLabel(const boost::filesystem::path &path, std::function<void()> on_close) {
  set_can_focus(false);
  set_tooltip_text(path.string());
  
  auto button=Gtk::manage(new Gtk::Button());
  auto hbox=Gtk::manage(new Gtk::HBox());
  
  hbox->set_can_focus(false);
  label.set_text(path.filename().string()+' ');
  label.set_can_focus(false);
  button->set_image_from_icon_name("window-close-symbolic", Gtk::ICON_SIZE_MENU);
  button->set_can_focus(false);
  button->set_relief(Gtk::ReliefStyle::RELIEF_NONE);
  
  hbox->pack_start(label, Gtk::PACK_SHRINK);
  hbox->pack_end(*button, Gtk::PACK_SHRINK);
  add(*hbox);
  
  button->signal_clicked().connect(on_close);
  signal_button_press_event().connect([on_close](GdkEventButton *event) {
    if(event->button==GDK_BUTTON_MIDDLE) {
      on_close();
      return true;
    }
    return false;
  });
  
  show_all();
}

Notebook::Notebook() : Gtk::HPaned(), notebooks(2) {
  Gsv::init();
  
  for(auto &notebook: notebooks) {
    notebook.set_scrollable();
    notebook.set_group_name("source_notebooks");
    notebook.signal_switch_page().connect([this](Gtk::Widget *widget, guint) {
      auto hbox=dynamic_cast<Gtk::HBox*>(widget);
      for(size_t c=0;c<hboxes.size();++c) {
        if(hboxes[c].get()==hbox) {
          focus_view(source_views[c]);
          set_current_view(source_views[c]);
          break;
        }
      }
      last_index=-1;
    });
    notebook.signal_page_added().connect([this](Gtk::Widget* widget, guint) {
      auto hbox=dynamic_cast<Gtk::HBox*>(widget);
      for(size_t c=0;c<hboxes.size();++c) {
        if(hboxes[c].get()==hbox) {
          focus_view(source_views[c]);
          set_current_view(source_views[c]);
          break;
        }
      }
    });
    
    auto provider = Gtk::CssProvider::create();
      //GtkNotebook-tab-overlap got removed in gtk 3.20, but margin works in 3.20
#if GTK_VERSION_GE(3, 20)
        provider->load_from_data("tab {border-radius: 5px 5px 0 0; padding: 0 4px; margin: 0;}");
#else
        provider->load_from_data(".notebook {-GtkNotebook-tab-overlap: 0px;} tab {border-radius: 5px 5px 0 0; padding: 4px 4px;}");
#endif
      notebook.get_style_context()->add_provider(provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  }
  pack1(notebooks[0], true, true);
}

size_t Notebook::size() {
  return source_views.size();
}

Source::View* Notebook::get_view(size_t index) {
  if(index>=size())
    return nullptr;
  return source_views[index];
}

Source::View* Notebook::get_current_view() {
  if(intermediate_view) {
    for(auto view: source_views) {
      if(view==intermediate_view)
        return view;
    }
  }
  for(auto view: source_views) {
    if(view==current_view)
      return view;
  }
  //In case there exist a tab that has not yet received focus again in a different notebook
  for(int notebook_index=0;notebook_index<2;++notebook_index) {
    auto page=notebooks[notebook_index].get_current_page();
    if(page>=0)
      return get_view(notebook_index, page);
  }
  return nullptr;
}

std::vector<Source::View*> &Notebook::get_views() {
  return source_views;
}

void Notebook::open(const boost::filesystem::path &file_path, size_t notebook_index) {
  if(notebook_index==1 && !split)
    toggle_split();
  
  for(size_t c=0;c<size();c++) {
    if(file_path==source_views[c]->file_path) {
      auto notebook_page=get_notebook_page(c);
      notebooks[notebook_page.first].set_current_page(notebook_page.second);
      focus_view(source_views[c]);
      return;
    }
  }
  
  if(boost::filesystem::exists(file_path)) {
    std::ifstream can_read(file_path.string());
    if(!can_read) {
      Terminal::get().print("Error: could not open "+file_path.string()+"\n", true);
      return;
    }
    can_read.close();
  }
  
  auto last_view=get_current_view();
  
  auto language=Source::guess_language(file_path);
  if(language && (language->get_id()=="chdr" || language->get_id()=="cpphdr" || language->get_id()=="c" || language->get_id()=="cpp" || language->get_id()=="objc"))
    source_views.emplace_back(new Source::ClangView(file_path, language));
  else
    source_views.emplace_back(new Source::GenericView(file_path, language));
  
  source_views.back()->scroll_to_cursor_delayed=[this](Source::View* view, bool center, bool show_tooltips) {
    while(Gtk::Main::events_pending())
      Gtk::Main::iteration(false);
    if(get_current_view()==view) {
      if(center)
        view->scroll_to(view->get_buffer()->get_insert(), 0.0, 1.0, 0.5);
      else
        view->scroll_to(view->get_buffer()->get_insert());
      if(!show_tooltips)
        view->hide_tooltips();
    }
  };
  source_views.back()->on_update_status=[this](Source::View* view, const std::string &status_text) {
    if(get_current_view()==view)
      status.set_text(status_text+" ");
  };
  source_views.back()->on_update_info=[this](Source::View* view, const std::string &info_text) {
    if(get_current_view()==view)
      info.set_text(" "+info_text);
  };
  
  scrolled_windows.emplace_back(new Gtk::ScrolledWindow());
  hboxes.emplace_back(new Gtk::HBox());
  scrolled_windows.back()->add(*source_views.back());
  hboxes.back()->pack_start(*scrolled_windows.back());

#if GTKSOURCEVIEWMM_MAJOR_VERSION > 2 & GTKSOURCEVIEWMM_MINOR_VERSION > 17
  source_maps.emplace_back(Glib::wrap(gtk_source_map_new()));
  gtk_source_map_set_view(GTK_SOURCE_MAP(source_maps.back()->gobj()), source_views.back()->gobj());
#endif
  configure(source_views.size()-1);
  
  //Set up tab label
  auto source_view=source_views.back();
  tab_labels.emplace_back(new TabLabel(file_path, [this, source_view]() {
    auto index=get_index(source_view);
    if(index!=static_cast<size_t>(-1))
      close(index);
  }));
  
  //Add star on tab label when the page is not saved:
  source_view->get_buffer()->signal_modified_changed().connect([this, source_view]() {
    std::string title=source_view->file_path.filename().string();
    if(source_view->get_buffer()->get_modified())
      title+='*';
    else
      title+=' ';
    
    for(size_t c=0;c<size();c++) {
      if(source_views[c]==source_view) {
        auto &tab_label=tab_labels.at(c);
        tab_label->label.set_text(title);
        tab_label->set_tooltip_text(source_view->file_path.string());
        return;
      }
    }
  });
  
  source_view->signal_focus_in_event().connect([this, source_view](GdkEventFocus *) {
    set_current_view(source_view);
    return false;
  });
  
  if(notebook_index==static_cast<size_t>(-1)) {
    if(!split)
      notebook_index=0;
    else if(notebooks[0].get_n_pages()==0)
      notebook_index=0;
    else if(notebooks[1].get_n_pages()==0)
      notebook_index=1;
    else if(last_view)
      notebook_index=get_notebook_page(get_index(last_view)).first;
  }
  auto &notebook=notebooks[notebook_index];
  
  notebook.append_page(*hboxes.back(), *tab_labels.back());
  
  notebook.set_tab_reorderable(*hboxes.back(), true);
  notebook.set_tab_detachable(*hboxes.back(), true);
  show_all_children();
  
  notebook.set_current_page(notebook.get_n_pages()-1);
  last_index=-1;
  if(last_view) {
    auto index=get_index(last_view);
    auto notebook_page=get_notebook_page(index);
    if(notebook_page.first==notebook_index)
      last_index=index;
  }
  
  set_focus_child(*source_views.back());
  source_view->get_buffer()->set_modified(false);
  focus_view(source_view);
}

void Notebook::configure(size_t index) {
#if GTKSOURCEVIEWMM_MAJOR_VERSION > 2 & GTKSOURCEVIEWMM_MINOR_VERSION > 17
  auto source_font_description=Pango::FontDescription(Config::get().source.font);
  auto source_map_font_desc=Pango::FontDescription(static_cast<std::string>(source_font_description.get_family())+" "+Config::get().source.map_font_size); 
  source_maps.at(index)->override_font(source_map_font_desc);
  if(Config::get().source.show_map) {
    if(hboxes.at(index)->get_children().size()==1)
      hboxes.at(index)->pack_end(*source_maps.at(index), Gtk::PACK_SHRINK);
  }
  else if(hboxes.at(index)->get_children().size()==2)
    hboxes.at(index)->remove(*source_maps.at(index));
#endif
}

bool Notebook::save(size_t index) {
  if(!source_views[index]->save(source_views))
    return false;
  Project::on_save(index);
  return true;
}

bool Notebook::save_current() {
  if(auto view=get_current_view())
    return save(get_index(view));
  return false;
}

void Notebook::save_session() {
  try {
    boost::property_tree::ptree pt_root, pt_files;
    pt_root.put("folder", Directories::get().path.string());
    for(size_t notebook_index=0;notebook_index<notebooks.size();++notebook_index) {
      for(int page=0;page<notebooks[notebook_index].get_n_pages();++page) {
        auto view=get_view(notebook_index, page);
        boost::property_tree::ptree pt_child;
        pt_child.put("notebook", notebook_index);
        pt_child.put("file", view->file_path.string());
        pt_files.push_back(std::make_pair("", pt_child));
      }
    }
    pt_root.add_child("files", pt_files);
    if(auto view=Notebook::get().get_current_view())
      pt_root.put("current_file", view->file_path.string());
    boost::property_tree::write_json((Config::get().juci_home_path()/"last_session.json").string(), pt_root);
  }
  catch(const std::exception &) {}
}

bool Notebook::close(size_t index) {
  if(auto view=get_view(index)) {
    if(view->get_buffer()->get_modified()){
      if(!save_modified_dialog(index))
        return false;
    }
    if(view==get_current_view()) {
      bool focused=false;
      if(last_index!=static_cast<size_t>(-1)) {
        auto notebook_page=get_notebook_page(last_index);
        if(notebook_page.first==get_notebook_page(get_index(view)).first) {
          focus_view(source_views[last_index]);
          notebooks[notebook_page.first].set_current_page(notebook_page.second);
          last_index=-1;
          focused=true;
        }
      }
      if(!focused) {
        auto notebook_page=get_notebook_page(get_index(view));
        if(notebook_page.second>0)
          focus_view(get_view(notebook_page.first, notebook_page.second-1));
        else {
          size_t notebook_index=notebook_page.first==0?1:0;
          if(notebooks[notebook_index].get_n_pages()>0)
            focus_view(get_view(notebook_index, notebooks[notebook_index].get_current_page()));
          else
            set_current_view(nullptr);
        }
      }
    }
    else if(index==last_index)
      last_index=-1;
    else if(index<last_index && last_index!=static_cast<size_t>(-1))
      last_index--;
    
    auto notebook_page=get_notebook_page(index);
    notebooks[notebook_page.first].remove_page(notebook_page.second);
#if GTKSOURCEVIEWMM_MAJOR_VERSION > 2 & GTKSOURCEVIEWMM_MINOR_VERSION > 17
    source_maps.erase(source_maps.begin()+index);
#endif

    if(on_close_page)
      on_close_page(view);
    
    if(auto clang_view=dynamic_cast<Source::ClangView*>(view))
      clang_view->async_delete();
    else
      delete view;
    source_views.erase(source_views.begin()+index);
    scrolled_windows.erase(scrolled_windows.begin()+index);
    hboxes.erase(hboxes.begin()+index);
    tab_labels.erase(tab_labels.begin()+index);
  }
  return true;
}

bool Notebook::close_current() {
  return close(get_index(get_current_view()));
}

void Notebook::next() {
  if(auto view=get_current_view()) {
    auto notebook_page=get_notebook_page(get_index(view));
    int page=notebook_page.second+1;
    if(page>=notebooks[notebook_page.first].get_n_pages())
      notebooks[notebook_page.first].set_current_page(0);
    else
      notebooks[notebook_page.first].set_current_page(page);
  }
}

void Notebook::previous() {
  if(auto view=get_current_view()) {
    auto notebook_page=get_notebook_page(get_index(view));
    int page=notebook_page.second-1;
    if(page<0)
      notebooks[notebook_page.first].set_current_page(notebooks[notebook_page.first].get_n_pages()-1);
    else
      notebooks[notebook_page.first].set_current_page(page);
  }
}

void Notebook::toggle_split() {
  if(!split) {
    pack2(notebooks[1], true, true);
    set_position(get_width()/2);
    show_all();
    //Make sure the position is correct
    //TODO: report bug to gtk if it is not fixed in gtk3.22
    Glib::signal_timeout().connect([this] {
      set_position(get_width()/2);
      return false;
    }, 200);
  }
  else {
    for(size_t c=size()-1;c!=static_cast<size_t>(-1);--c) {
      auto notebook_index=get_notebook_page(c).first;
      if(notebook_index==1 && !close(c))
        return;
    }
    remove(notebooks[1]);
  }
  split=!split;
}

boost::filesystem::path Notebook::get_current_folder() {
  if(!Directories::get().path.empty())
    return Directories::get().path;
  else if(auto view=get_current_view())
    return view->file_path.parent_path();
  else
    return boost::filesystem::path();
}

size_t Notebook::get_index(Source::View *view) {
  for(size_t c=0;c<size();++c) {
    if(source_views[c]==view)
      return c;
  }
  return -1;
}

Source::View *Notebook::get_view(size_t notebook_index, int page) {
  if(notebook_index==static_cast<size_t>(-1) || notebook_index>=notebooks.size() ||
     page<0 || page>=notebooks[notebook_index].get_n_pages())
    return nullptr;
  auto hbox=dynamic_cast<Gtk::HBox*>(notebooks[notebook_index].get_nth_page(page));
  auto scrolled_window=dynamic_cast<Gtk::ScrolledWindow*>(hbox->get_children()[0]);
  return dynamic_cast<Source::View*>(scrolled_window->get_children()[0]);
}

void Notebook::focus_view(Source::View *view) {
  intermediate_view=view;
  view->grab_focus();
}

std::pair<size_t, int> Notebook::get_notebook_page(size_t index) {
  if(index>=hboxes.size())
    return {-1, -1};
  for(size_t c=0;c<notebooks.size();++c) {
    auto page_num=notebooks[c].page_num(*hboxes[index]);
    if(page_num>=0)
      return {c, page_num};
  }
  return {-1, -1};
}

void Notebook::set_current_view(Source::View *view) {
  intermediate_view=nullptr;
  if(current_view!=view) {
    if(auto view=get_current_view()) {
      view->hide_tooltips();
      view->hide_dialogs();
    }
    current_view=view;
    if(view && on_change_page)
      on_change_page(view);
  }
}

bool Notebook::save_modified_dialog(size_t index) {
  Gtk::MessageDialog dialog(*static_cast<Gtk::Window*>(get_toplevel()), "Save file!", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO);
  dialog.set_default_response(Gtk::RESPONSE_YES);
  dialog.set_secondary_text("Do you want to save: " + get_view(index)->file_path.string()+" ?");
  int result = dialog.run();
  if(result==Gtk::RESPONSE_YES) {
    save(index);
    return true;
  }
  else if(result==Gtk::RESPONSE_NO) {
    return true;
  }
  else {
    return false;
  }
}
