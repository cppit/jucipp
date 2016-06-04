#include "window.h"
#include "config.h"
#include "menu.h"
#include "directories.h"
//#include "api.h"
#include "dialogs.h"
#include "filesystem.h"
#include "project.h"
#include "entrybox.h"
#include "info.h"

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

Window::Window() {
  set_title("juCi++");
  set_events(Gdk::POINTER_MOTION_MASK|Gdk::FOCUS_CHANGE_MASK|Gdk::SCROLL_MASK|Gdk::LEAVE_NOTIFY_MASK);
  
  set_menu_actions();
  configure();
  activate_menu_items(false);
  
  set_default_size(Config::get().window.default_size.first, Config::get().window.default_size.second);
  
  directories_scrolled_window.add(Directories::get());
  directory_and_notebook_panes.pack1(directories_scrolled_window, Gtk::SHRINK);
  notebook_vbox.pack_start(Notebook::get());
  notebook_vbox.pack_end(EntryBox::get(), Gtk::PACK_SHRINK);
  directory_and_notebook_panes.pack2(notebook_vbox, Gtk::SHRINK);
  directory_and_notebook_panes.set_position(static_cast<int>(0.2*Config::get().window.default_size.first));
  vpaned.set_position(static_cast<int>(0.75*Config::get().window.default_size.second));
  vpaned.pack1(directory_and_notebook_panes, true, false);
  
  terminal_scrolled_window.add(Terminal::get());
  terminal_vbox.pack_start(terminal_scrolled_window);
    
  info_and_status_hbox.pack_start(Notebook::get().info, Gtk::PACK_SHRINK);
  
#if GTK_VERSION_GE(3, 12)
  info_and_status_hbox.set_center_widget(Project::debug_status_label());
#else
  Project::debug_status_label().set_halign(Gtk::Align::ALIGN_CENTER);
  info_and_status_hbox.pack_start(Project::debug_status_label());
#endif
  info_and_status_hbox.pack_end(Notebook::get().status, Gtk::PACK_SHRINK);
  terminal_vbox.pack_end(info_and_status_hbox, Gtk::PACK_SHRINK);
  vpaned.pack2(terminal_vbox, true, true);
  
#if GTKMM_MAJOR_VERSION>3 || (GTKMM_MAJOR_VERSION==3 && GTKMM_MINOR_VERSION>=14)
  overlay_vbox.set_hexpand(false);
  overlay_vbox.set_halign(Gtk::Align::ALIGN_START);
  overlay_hbox.set_hexpand(false);
  overlay_hbox.set_halign(Gtk::Align::ALIGN_END);
  overlay_vbox.pack_start(Info::get(), Gtk::PACK_SHRINK, 20);
  overlay_hbox.pack_end(overlay_vbox, Gtk::PACK_SHRINK, 20);
  overlay.add(vpaned);
  overlay.add_overlay(overlay_hbox);
#if GTKMM_MAJOR_VERSION>3 || (GTKMM_MAJOR_VERSION==3 && GTKMM_MINOR_VERSION>=18)
  overlay.set_overlay_pass_through(overlay_hbox, true);
#endif
  add(overlay);
#else
  add(vpaned);
#endif
  
  show_all_children();
  Info::get().hide();

  Directories::get().on_row_activated=[this](const boost::filesystem::path &path) {
    Notebook::get().open(path);
  };

  //Scroll to end of terminal whenever info is printed
  Terminal::get().signal_size_allocate().connect([this](Gtk::Allocation& allocation){
    auto adjustment=terminal_scrolled_window.get_vadjustment();
    adjustment->set_value(adjustment->get_upper()-adjustment->get_page_size());
    Terminal::get().queue_draw();
  });

  EntryBox::get().signal_show().connect([this](){
    vpaned.set_focus_chain({&directory_and_notebook_panes});
    directory_and_notebook_panes.set_focus_chain({&notebook_vbox});
    notebook_vbox.set_focus_chain({&EntryBox::get()});
  });
  EntryBox::get().signal_hide().connect([this](){
    vpaned.unset_focus_chain();
    directory_and_notebook_panes.unset_focus_chain();
    notebook_vbox.unset_focus_chain();
  });
  EntryBox::get().signal_hide().connect([this]() {
    if(auto view=Notebook::get().get_current_view())
      view->grab_focus();
  });

  Notebook::get().on_change_page=[this](Source::View *view) {
    if(search_entry_shown && EntryBox::get().labels.size()>0) {
      view->update_search_occurrences=[this](int number){
        EntryBox::get().labels.begin()->update(0, std::to_string(number));
      };
      view->search_highlight(last_search, case_sensitive_search, regex_search);
    }

    activate_menu_items();
    
    Directories::get().select(view->file_path);
    
    if(view->full_reparse_needed) {
      if(!view->full_reparse())
        Terminal::get().async_print("Error: failed to reparse "+view->file_path.string()+". Please reopen the file manually.\n", true);
    }
    else if(view->soft_reparse_needed)
      view->soft_reparse();
    
    view->set_status(view->status);
    view->set_info(view->info);
    
#ifdef JUCI_ENABLE_DEBUG
    if(Project::debugging)
      Project::debug_update_stop();
#endif
  };
  Notebook::get().on_close_page=[](Source::View *view) {
#ifdef JUCI_ENABLE_DEBUG
    if(Project::current && Project::debugging) {
      auto iter=view->get_buffer()->begin();
      while(view->get_source_buffer()->forward_iter_to_source_mark(iter, "debug_breakpoint") ||
         view->get_source_buffer()->get_source_marks_at_iter(iter, "debug_breakpoint").size()) {
        auto end_iter=iter;
        while(!end_iter.ends_line() && end_iter.forward_char()) {}
        view->get_source_buffer()->remove_source_marks(iter, end_iter, "debug_breakpoint");
        Project::current->debug_remove_breakpoint(view->file_path, iter.get_line()+1, view->get_buffer()->get_line_count()+1);
      }
    }
#endif
    EntryBox::get().hide();
  };
  
  about.signal_response().connect([this](int d){
    about.hide();
  });
  
  about.set_version(Config::get().window.version);
  about.set_authors({"(in order of appearance)",
                     "Ted Johan Kristoffersen", 
                     "Jørgen Lien Sellæg",
                     "Geir Morten Larsen",
                     "Ole Christian Eidheim"});
  about.set_name("About juCi++");
  about.set_program_name("juCi++");
  about.set_comments("This is an open source IDE with high-end features to make your programming experience juicy");
  about.set_license_type(Gtk::License::LICENSE_MIT_X11);
  about.set_transient_for(*this);
} // Window constructor

void Window::configure() {
  Config::get().load();
  auto screen = Gdk::Screen::get_default();
  if(css_provider)
    Gtk::StyleContext::remove_provider_for_screen(screen, css_provider);
  if(Config::get().window.theme_name.empty()) {
    css_provider=Gtk::CssProvider::create();
    Gtk::Settings::get_default()->property_gtk_application_prefer_dark_theme()=(Config::get().window.theme_variant=="dark");
  }
  else
    css_provider=Gtk::CssProvider::get_named(Config::get().window.theme_name, Config::get().window.theme_variant);
  //TODO: add check if theme exists, or else write error to terminal
  Gtk::StyleContext::add_provider_for_screen(screen, css_provider, GTK_STYLE_PROVIDER_PRIORITY_SETTINGS);
  Directories::get().update();
  Menu::get().set_keys();
  Terminal::get().configure();
}

void Window::set_menu_actions() {
  auto &menu = Menu::get();
  
  menu.add_action("about", [this]() {
    about.show();
    about.present();
  });
  menu.add_action("preferences", [this]() {
    Notebook::get().open(Config::get().juci_home_path()/"config"/"config.json");
  });
  menu.add_action("quit", [this]() {
    close();
  });
  
  menu.add_action("new_file", [this]() {
    boost::filesystem::path path = Dialog::new_file(Notebook::get().get_current_folder());
    if(path!="") {
      if(boost::filesystem::exists(path)) {
        Terminal::get().print("Error: "+path.string()+" already exists.\n", true);
      }
      else {
        if(filesystem::write(path)) {
          if(Directories::get().path!="")
            Directories::get().update();
          Notebook::get().open(path);
          Terminal::get().print("New file "+path.string()+" created.\n");
        }
        else
          Terminal::get().print("Error: could not create new file "+path.string()+".\n", true);
      }
    }
  });
  menu.add_action("new_folder", [this]() {
    auto time_now=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    boost::filesystem::path path = Dialog::new_folder(Notebook::get().get_current_folder());
    if(path!="" && boost::filesystem::exists(path)) {
      boost::system::error_code ec;
      auto last_write_time=boost::filesystem::last_write_time(path, ec);
      if(!ec && last_write_time>=time_now) {
        if(Directories::get().path!="")
          Directories::get().update();
        Terminal::get().print("New folder "+path.string()+" created.\n");
      }
      else
        Terminal::get().print("Error: "+path.string()+" already exists.\n", true);
      Directories::get().select(path);
    }
  });
  menu.add_action("new_project_cpp", [this]() {
    boost::filesystem::path project_path = Dialog::new_folder(Notebook::get().get_current_folder());
    if(project_path!="") {
      auto project_name=project_path.filename().string();
      for(size_t c=0;c<project_name.size();c++) {
        if(project_name[c]==' ')
          project_name[c]='_';
      }
      auto cmakelists_path=project_path;
      cmakelists_path/="CMakeLists.txt";
      auto cpp_main_path=project_path;
      cpp_main_path/="main.cpp";
      if(boost::filesystem::exists(cmakelists_path)) {
        Terminal::get().print("Error: "+cmakelists_path.string()+" already exists.\n", true);
        return;
      }
      if(boost::filesystem::exists(cpp_main_path)) {
        Terminal::get().print("Error: "+cpp_main_path.string()+" already exists.\n", true);
        return;
      }
      std::string cmakelists="cmake_minimum_required(VERSION 2.8)\n\nproject("+project_name+")\n\nset(CMAKE_CXX_FLAGS \"${CMAKE_CXX_FLAGS} -std=c++1y -Wall -Wextra -Wno-unused-parameter\")\n\nadd_executable("+project_name+" main.cpp)\n";
      std::string cpp_main="#include <iostream>\n\nint main() {\n  std::cout << \"Hello World!\\n\";\n}\n";
      if(filesystem::write(cmakelists_path, cmakelists) && filesystem::write(cpp_main_path, cpp_main)) {
        Directories::get().open(project_path);
        Notebook::get().open(cpp_main_path);
        Directories::get().update();
        Terminal::get().print("C++ project "+project_name+" created.\n");
      }
      else
        Terminal::get().print("Error: Could not create project "+project_path.string()+"\n", true);
    }
  });
  
  menu.add_action("open_file", [this]() {
    auto path=Dialog::open_file(Notebook::get().get_current_folder());
    if(path!="")
      Notebook::get().open(path);
  });
  menu.add_action("open_folder", [this]() {
    auto path = Dialog::open_folder(Notebook::get().get_current_folder());
    if (path!="" && boost::filesystem::exists(path))
      Directories::get().open(path);
  });
  
  menu.add_action("save", [this]() {
    if(auto view=Notebook::get().get_current_view()) {
      if(Notebook::get().save_current()) {
        if(view->file_path==Config::get().juci_home_path()/"config"/"config.json") {
          configure();
          for(size_t c=0;c<Notebook::get().size();c++) {
            Notebook::get().get_view(c)->configure();
            Notebook::get().configure(c);
          }
        }
      }
    }
  });
  menu.add_action("save_as", [this]() {
    if(auto view=Notebook::get().get_current_view()) {
      auto path = Dialog::save_file_as(view->file_path);
      if(path!="") {
        std::ofstream file(path);
        if(file) {
          file << view->get_buffer()->get_text();
          file.close();
          if(Directories::get().path!="")
            Directories::get().update();
          Notebook::get().open(path);
          Terminal::get().print("File saved to: " + Notebook::get().get_current_view()->file_path.string()+"\n");
        }
        else
          Terminal::get().print("Error saving file\n");
      }
    }
  });
  
  menu.add_action("print", [this]() {
    if(auto view=Notebook::get().get_current_view()) {
      auto print_operation=Gtk::PrintOperation::create();
      auto print_compositor=Gsv::PrintCompositor::create(*view);
      
      print_operation->set_job_name(view->file_path.filename().string());
      print_compositor->set_wrap_mode(Gtk::WrapMode::WRAP_WORD_CHAR);
      
      print_operation->signal_begin_print().connect([print_operation, print_compositor](const Glib::RefPtr<Gtk::PrintContext>& print_context) {
        while(!print_compositor->paginate(print_context));
        print_operation->set_n_pages(print_compositor->get_n_pages());
      });
      print_operation->signal_draw_page().connect([print_compositor](const Glib::RefPtr<Gtk::PrintContext>& print_context, int page_nr) {
        print_compositor->draw_page(print_context, page_nr);
      });
      
      print_operation->run(Gtk::PRINT_OPERATION_ACTION_PRINT_DIALOG, *this);
    }
  });
  
  menu.add_action("edit_undo", [this]() {
    if(auto view=Notebook::get().get_current_view()) {
      auto undo_manager = view->get_source_buffer()->get_undo_manager();
      if (undo_manager->can_undo()) {
        undo_manager->undo();
        view->scroll_to(view->get_buffer()->get_insert());
      }
    }
  });
  menu.add_action("edit_redo", [this]() {
    if(auto view=Notebook::get().get_current_view()) {
      auto undo_manager = view->get_source_buffer()->get_undo_manager();
      if(undo_manager->can_redo()) {
        undo_manager->redo();
        view->scroll_to(view->get_buffer()->get_insert());
      }
    }
  });
  
  menu.add_action("edit_cut", [this]() {
    auto widget=get_focus();
    if(auto entry=dynamic_cast<Gtk::Entry*>(widget))
      entry->cut_clipboard();
    else if(auto view=Notebook::get().get_current_view())
      view->get_buffer()->cut_clipboard(Gtk::Clipboard::get());
  });
  menu.add_action("edit_copy", [this]() {
    auto widget=get_focus();
    if(auto entry=dynamic_cast<Gtk::Entry*>(widget))
      entry->copy_clipboard();
    else if(auto text_view=dynamic_cast<Gtk::TextView*>(widget))
      text_view->get_buffer()->copy_clipboard(Gtk::Clipboard::get());
  });
  menu.add_action("edit_paste", [this]() {
    auto widget=get_focus();
    if(auto entry=dynamic_cast<Gtk::Entry*>(widget))
      entry->paste_clipboard();
    else if(auto view=Notebook::get().get_current_view())
      view->paste();
  });
  
  menu.add_action("edit_find", [this]() {
    search_and_replace_entry();
  });
  
  menu.add_action("source_spellcheck", [this]() {
    if(auto view=Notebook::get().get_current_view())
      view->spellcheck();
  });
  menu.add_action("source_spellcheck_clear", [this]() {
    if(auto view=Notebook::get().get_current_view())
      view->remove_spellcheck_errors();
  });
  menu.add_action("source_spellcheck_next_error", [this]() {
    if(auto view=Notebook::get().get_current_view())
      view->goto_next_spellcheck_error();
  });
  
  menu.add_action("source_indentation_set_buffer_tab", [this]() {
    set_tab_entry();
  });
  menu.add_action("source_indentation_auto_indent_buffer", [this]() {
    auto view=Notebook::get().get_current_view();
    if(view && view->auto_indent)
      view->auto_indent();
  });
  
  menu.add_action("source_goto_line", [this]() {
    goto_line_entry();
  });
  menu.add_action("source_center_cursor", [this]() {
    if(auto view=Notebook::get().get_current_view())
      view->scroll_to_cursor_delayed(view, true, false);
  });
  
  menu.add_action("source_find_documentation", [this]() {
    if(auto view=Notebook::get().get_current_view()) {
      if(view->get_token_data) {
        auto data=view->get_token_data();        
        if(data.size()>0) {
          auto documentation_search=Config::get().source.documentation_searches.find(data[0]);
          if(documentation_search!=Config::get().source.documentation_searches.end()) {
            std::string token_query;
            for(size_t c=1;c<data.size();c++) {
              if(data[c].size()>0) {
                if(token_query.size()>0)
                  token_query+=documentation_search->second.separator;
                token_query+=data[c];
              }
            }
            if(token_query.size()>0) {
              std::unordered_map<std::string, std::string>::iterator query;
              if(data[1].size()>0)
                query=documentation_search->second.queries.find(data[1]);
              else
                query=documentation_search->second.queries.find("@empty");
              if(query==documentation_search->second.queries.end())
                query=documentation_search->second.queries.find("@any");
              
              if(query!=documentation_search->second.queries.end()) {
                std::string uri=query->second+token_query;
#ifdef __APPLE__
                Terminal::get().process("open \""+uri+"\"");
#else
                GError* error=NULL;
                gtk_show_uri(NULL, uri.c_str(), GDK_CURRENT_TIME, &error);
                g_clear_error(&error);
#endif
              }
            }
          }
        }
      }
    }
  });
  
  menu.add_action("source_goto_declaration", [this]() {
    if(auto view=Notebook::get().get_current_view()) {
      if(view->get_declaration_location) {
        auto location=view->get_declaration_location(Notebook::get().get_views());
        if(location) {
          boost::filesystem::path declaration_file;
          boost::system::error_code ec;
          declaration_file=boost::filesystem::canonical(location.file_path, ec);
          if(ec)
            return;
          Notebook::get().open(declaration_file);
          auto view=Notebook::get().get_current_view();
          auto line=static_cast<int>(location.line)-1;
          auto index=static_cast<int>(location.index)-1;
          view->place_cursor_at_line_index(line, index);
          view->scroll_to_cursor_delayed(view, true, false);
        }
      }
    }
  });
  menu.add_action("source_goto_implementation", [this]() {
    if(auto view=Notebook::get().get_current_view()) {
      if(view->get_implementation_location) {
        auto location=view->get_implementation_location(Notebook::get().get_views());
        if(location) {
          boost::filesystem::path implementation_path;
          boost::system::error_code ec;
          implementation_path=boost::filesystem::canonical(location.file_path, ec);
          if(ec)
            return;
          Notebook::get().open(implementation_path);
          auto view=Notebook::get().get_current_view();
          auto line=static_cast<int>(location.line)-1;
          auto index=static_cast<int>(location.index)-1;
          view->place_cursor_at_line_index(line, index);
          view->scroll_to_cursor_delayed(view, true, false);
          return;
        }
      }
    }
  });

  menu.add_action("source_goto_usage", [this]() {
    if(auto view=Notebook::get().get_current_view()) {
      if(view->get_usages) {
        auto usages=view->get_usages(Notebook::get().get_views());
        if(!usages.empty()) {
          auto dialog_iter=view->get_iter_for_dialog();
          view->selection_dialog=std::unique_ptr<SelectionDialog>(new SelectionDialog(*view, view->get_buffer()->create_mark(dialog_iter), true, true));
          auto rows=std::make_shared<std::unordered_map<std::string, Source::Offset> >();
          
          auto iter=view->get_buffer()->get_insert()->get_iter();
          for(auto &usage: usages) {
            std::string row;
            bool current_page=true;
            //add file name if usage is not in current page
            if(view->file_path!=usage.first.file_path) {
              row=usage.first.file_path.filename().string()+":";
              current_page=false;
            }
            row+=std::to_string(usage.first.line+1)+": "+usage.second;
            (*rows)[row]=usage.first;
            view->selection_dialog->add_row(row);
            
            //Set dialog cursor to the last row if the textview cursor is at the same line
            if(current_page) {
              if(iter.get_line()==static_cast<int>(usage.first.line) && iter.get_line_index()>=static_cast<int>(usage.first.index))
                view->selection_dialog->set_cursor_at_last_row();
            }
          }
          
          if(rows->size()==0)
            return;
          view->selection_dialog->on_select=[this, rows](const std::string &selected, bool hide_window) {
            auto offset=rows->at(selected);
            boost::filesystem::path declaration_file;
            boost::system::error_code ec;
            declaration_file=boost::filesystem::canonical(offset.file_path, ec);
            if(ec)
              return;
            Notebook::get().open(declaration_file);
            auto view=Notebook::get().get_current_view();
            view->place_cursor_at_line_index(offset.line, offset.index);
            view->scroll_to(view->get_buffer()->get_insert(), 0.0, 1.0, 0.5);
            view->delayed_tooltips_connection.disconnect();
          };
          view->selection_dialog->show();
        }
      }
    }
  });
  menu.add_action("source_goto_method", [this]() {
    if(auto view=Notebook::get().get_current_view()) {
      if(view->get_methods) {
        auto methods=Notebook::get().get_current_view()->get_methods();
        if(!methods.empty()) {
          auto iter=view->get_iter_for_dialog();
          view->selection_dialog=std::unique_ptr<SelectionDialog>(new SelectionDialog(*view, view->get_buffer()->create_mark(iter), true, true));
          auto rows=std::make_shared<std::unordered_map<std::string, Source::Offset> >();
          for(auto &method: methods) {
            (*rows)[method.second]=method.first;
            view->selection_dialog->add_row(method.second);
          }
          view->selection_dialog->on_select=[view, rows](const std::string& selected, bool hide_window) {
            auto offset=rows->at(selected);
            view->get_buffer()->place_cursor(view->get_buffer()->get_iter_at_line_index(offset.line-1, offset.index-1));
            view->scroll_to(view->get_buffer()->get_insert(), 0.0, 1.0, 0.5);
            view->delayed_tooltips_connection.disconnect();
          };
          view->selection_dialog->show();
        }
      }
    }
  });
  menu.add_action("source_rename", [this]() {
    rename_token_entry();
  });
  
  menu.add_action("source_goto_next_diagnostic", [this]() {
    if(auto view=Notebook::get().get_current_view()) {
      if(view->goto_next_diagnostic) {
        view->goto_next_diagnostic();
      }
    }
  });
  menu.add_action("source_apply_fix_its", [this]() {
    if(auto view=Notebook::get().get_current_view()) {
      if(view->get_fix_its) {
        auto buffer=view->get_buffer();
        auto fix_its=view->get_fix_its();
        std::vector<std::pair<Glib::RefPtr<Gtk::TextMark>, Glib::RefPtr<Gtk::TextMark> > > fix_it_marks;
        for(auto &fix_it: fix_its) {
          auto start_iter=buffer->get_iter_at_line_index(fix_it.offsets.first.line-1, fix_it.offsets.first.index-1);
          auto end_iter=buffer->get_iter_at_line_index(fix_it.offsets.second.line-1, fix_it.offsets.second.index-1);
          fix_it_marks.emplace_back(buffer->create_mark(start_iter), buffer->create_mark(end_iter));
        }
        size_t c=0;
        buffer->begin_user_action();
        for(auto &fix_it: fix_its) {
          if(fix_it.type==Source::FixIt::Type::INSERT) {
            buffer->insert(fix_it_marks[c].first->get_iter(), fix_it.source);
          }
          if(fix_it.type==Source::FixIt::Type::REPLACE) {
            buffer->erase(fix_it_marks[c].first->get_iter(), fix_it_marks[c].second->get_iter());
            buffer->insert(fix_it_marks[c].first->get_iter(), fix_it.source);
          }
          if(fix_it.type==Source::FixIt::Type::ERASE) {
            buffer->erase(fix_it_marks[c].first->get_iter(), fix_it_marks[c].second->get_iter());
          }
          c++;
        }
        for(auto &mark_pair: fix_it_marks) {
          buffer->delete_mark(mark_pair.first);
          buffer->delete_mark(mark_pair.second);
        }
        buffer->end_user_action();
      }
    }
  });
  
  menu.add_action("project_set_run_arguments", [this]() {
    auto project=Project::create();
    auto run_arguments=std::make_shared<std::pair<std::string, std::string> >(project->get_run_arguments());
    if(run_arguments->second.empty())
      return;
    
    EntryBox::get().clear();
    EntryBox::get().labels.emplace_back();
    auto label_it=EntryBox::get().labels.begin();
    label_it->update=[label_it](int state, const std::string& message){
      label_it->set_text("Set empty to let juCi++ deduce executable");
    };
    label_it->update(0, "");
    EntryBox::get().entries.emplace_back(run_arguments->second, [this, run_arguments](const std::string& content){
      Project::run_arguments[run_arguments->first]=content;
      EntryBox::get().hide();
    }, 50);
    auto entry_it=EntryBox::get().entries.begin();
    entry_it->set_placeholder_text("Project: Set Run Arguments");
    EntryBox::get().buttons.emplace_back("Project: set run arguments", [this, entry_it](){
      entry_it->activate();
    });
    EntryBox::get().show();
  });
  menu.add_action("compile_and_run", [this]() {
    if(Project::compiling || Project::debugging) {
      Info::get().print("Compile or debug in progress");
      return;
    }
    
    Project::current=Project::create();
    
    if(Config::get().project.save_on_compile_or_run)
      Project::save_files(Project::current->build->project_path);
    
    Project::current->compile_and_run();
  });
  menu.add_action("compile", [this]() {
    if(Project::compiling || Project::debugging) {
      Info::get().print("Compile or debug in progress");
      return;
    }
            
    Project::current=Project::create();
    
    if(Config::get().project.save_on_compile_or_run)
      Project::save_files(Project::current->build->project_path);
    
    Project::current->compile();
  });
  
  menu.add_action("run_command", [this]() {
    EntryBox::get().clear();
    EntryBox::get().labels.emplace_back();
    auto label_it=EntryBox::get().labels.begin();
    label_it->update=[label_it](int state, const std::string& message){
      label_it->set_text("Run Command directory order: opened directory, file path, current directory");
    };
    label_it->update(0, "");
    EntryBox::get().entries.emplace_back(last_run_command, [this](const std::string& content){
      if(content!="") {
        last_run_command=content;
        auto run_path=Notebook::get().get_current_folder();
        Terminal::get().async_print("Running: "+content+'\n');
  
        Terminal::get().async_process(content, run_path, [this, content](int exit_status){
          Terminal::get().async_print(content+" returned: "+std::to_string(exit_status)+'\n');
        });
      }
      EntryBox::get().hide();
    }, 30);
    auto entry_it=EntryBox::get().entries.begin();
    entry_it->set_placeholder_text("Command");
    EntryBox::get().buttons.emplace_back("Run command", [this, entry_it](){
      entry_it->activate();
    });
    EntryBox::get().show();
  });
  
  menu.add_action("kill_last_running", [this]() {
    Terminal::get().kill_last_async_process();
  });
  menu.add_action("force_kill_last_running", [this]() {
    Terminal::get().kill_last_async_process(true);
  });
  
#ifdef JUCI_ENABLE_DEBUG
  menu.add_action("debug_set_run_arguments", [this]() {
    auto project=Project::create();
    auto run_arguments=std::make_shared<std::pair<std::string, std::string> >(project->debug_get_run_arguments());
    if(run_arguments->second.empty())
      return;
    
    EntryBox::get().clear();
    EntryBox::get().labels.emplace_back();
    auto label_it=EntryBox::get().labels.begin();
    label_it->update=[label_it](int state, const std::string& message){
      label_it->set_text("Set empty to let juCi++ deduce executable");
    };
    label_it->update(0, "");
    EntryBox::get().entries.emplace_back(run_arguments->second, [this, run_arguments](const std::string& content){
      Project::debug_run_arguments[run_arguments->first]=content;
      EntryBox::get().hide();
    }, 50);
    auto entry_it=EntryBox::get().entries.begin();
    entry_it->set_placeholder_text("Debug: Set Run Arguments");
    EntryBox::get().buttons.emplace_back("Debug: set run arguments", [this, entry_it](){
      entry_it->activate();
    });
    EntryBox::get().show();
  });
  menu.add_action("debug_start_continue", [this](){
    if(Project::compiling) {
      Info::get().print("Compile in progress");
      return;
    }
    else if(Project::debugging) {
      Project::current->debug_continue();
      return;
    }
        
    Project::current=Project::create();
    
    if(Config::get().project.save_on_compile_or_run)
      Project::save_files(Project::current->build->project_path);
    
    Project::current->debug_start();
  });
  menu.add_action("debug_stop", [this]() {
    if(Project::current)
      Project::current->debug_stop();
  });
  menu.add_action("debug_kill", [this]() {
    if(Project::current)
      Project::current->debug_kill();
  });
  menu.add_action("debug_step_over", [this]() {
    if(Project::current)
      Project::current->debug_step_over();
  });
  menu.add_action("debug_step_into", [this]() {
    if(Project::current)
      Project::current->debug_step_into();
  });
  menu.add_action("debug_step_out", [this]() {
    if(Project::current)
      Project::current->debug_step_out();
  });
  menu.add_action("debug_backtrace", [this]() {
    if(Project::current)
      Project::current->debug_backtrace();
  });
  menu.add_action("debug_show_variables", [this]() {
    if(Project::current)
      Project::current->debug_show_variables();
  });
  menu.add_action("debug_run_command", [this]() {
    EntryBox::get().clear();
    EntryBox::get().entries.emplace_back(last_run_debug_command, [this](const std::string& content){
      if(content!="") {
        if(Project::current)
          Project::current->debug_run_command(content);
        last_run_debug_command=content;
      }
      EntryBox::get().hide();
    }, 30);
    auto entry_it=EntryBox::get().entries.begin();
    entry_it->set_placeholder_text("Debug Command");
    EntryBox::get().buttons.emplace_back("Run debug command", [this, entry_it](){
      entry_it->activate();
    });
    EntryBox::get().show();
  });
  menu.add_action("debug_toggle_breakpoint", [this](){
    if(auto view=Notebook::get().get_current_view()) {
      auto line_nr=view->get_buffer()->get_insert()->get_iter().get_line();
      
      if(view->get_source_buffer()->get_source_marks_at_line(line_nr, "debug_breakpoint").size()>0) {
        auto start_iter=view->get_buffer()->get_iter_at_line(line_nr);
        auto end_iter=start_iter;
        while(!end_iter.ends_line() && end_iter.forward_char()) {}
        view->get_source_buffer()->remove_source_marks(start_iter, end_iter, "debug_breakpoint");
        if(Project::current && Project::debugging)
          Project::current->debug_remove_breakpoint(view->file_path, line_nr+1, view->get_buffer()->get_line_count()+1);
      }
      else {
        view->get_source_buffer()->create_source_mark("debug_breakpoint", view->get_buffer()->get_insert()->get_iter());
        if(Project::current && Project::debugging)
          Project::current->debug_add_breakpoint(view->file_path, line_nr+1);
      }
    }
  });
  menu.add_action("debug_goto_stop", [this](){
    if(Project::debugging) {
      if(!Project::debug_stop.first.empty()) {
        Notebook::get().open(Project::debug_stop.first);
        if(auto view=Notebook::get().get_current_view()) {
          int line=Project::debug_stop.second.first-1;
          int index=Project::debug_stop.second.second-1;
          view->place_cursor_at_line_index(line, index);
          view->scroll_to_cursor_delayed(view, true, true);
        }
      }
    }
  });
  
  Project::debug_update_status("");
#endif
  
  menu.add_action("next_tab", [this]() {
    Notebook::get().next();
  });
  menu.add_action("previous_tab", [this]() {
    Notebook::get().previous();
  });
  menu.add_action("close_tab", [this]() {
    if(Notebook::get().get_current_view() && Notebook::get().close_current()) {
      if(auto view=Notebook::get().get_current_view()) {
        view->set_status(view->status);
        view->set_info(view->info);
      }
      else {
        Notebook::get().status.set_text("");
        Notebook::get().info.set_text("");
        
        activate_menu_items(false);
      }
    }
  });
  menu.add_action("window_toggle_split", [this] {
    Notebook::get().toggle_split();
  });
}

void Window::activate_menu_items(bool activate) {
  auto &menu = Menu::get();
  auto &notebook = Notebook::get();
  menu.actions["source_indentation_auto_indent_buffer"]->set_enabled(activate ? static_cast<bool>(notebook.get_current_view()->auto_indent) : false);
  menu.actions["source_find_documentation"]->set_enabled(activate ? static_cast<bool>(notebook.get_current_view()->get_token_data) : false);
  menu.actions["source_goto_declaration"]->set_enabled(activate ? static_cast<bool>(notebook.get_current_view()->get_declaration_location) : false);
  menu.actions["source_goto_implementation"]->set_enabled(activate ? static_cast<bool>(notebook.get_current_view()->get_implementation_location) : false);
  menu.actions["source_goto_usage"]->set_enabled(activate ? static_cast<bool>(notebook.get_current_view()->get_usages) : false);
  menu.actions["source_goto_method"]->set_enabled(activate ? static_cast<bool>(notebook.get_current_view()->get_methods) : false);
  menu.actions["source_rename"]->set_enabled(activate ? static_cast<bool>(notebook.get_current_view()->rename_similar_tokens) : false);
  menu.actions["source_goto_next_diagnostic"]->set_enabled(activate ? static_cast<bool>(notebook.get_current_view()->goto_next_diagnostic) : false);
  menu.actions["source_apply_fix_its"]->set_enabled(activate ? static_cast<bool>(notebook.get_current_view()->get_fix_its) : false);
#ifdef JUCI_ENABLE_DEBUG
  if(auto view=notebook.get_current_view()) {
    if(view->language && (view->language->get_id()=="c" || view->language->get_id()=="cpp" || view->language->get_id()=="objc" || view->language->get_id()=="chdr" || view->language->get_id()=="cpphdr"))
      menu.actions["debug_toggle_breakpoint"]->set_enabled(true);
    else
      menu.actions["debug_toggle_breakpoint"]->set_enabled(false);
  }
  else
    menu.actions["debug_toggle_breakpoint"]->set_enabled(false);
#endif
}

bool Window::on_key_press_event(GdkEventKey *event) {
  if(event->keyval==GDK_KEY_Escape) {
    EntryBox::get().hide();
  }
#ifdef __APPLE__ //For Apple's Command-left, right, up, down keys
  else if((event->state & GDK_META_MASK)>0 && (event->state & GDK_MOD1_MASK)==0) {
    if(event->keyval==GDK_KEY_Left) {
      event->keyval=GDK_KEY_Home;
      event->state=event->state & GDK_SHIFT_MASK;
      event->hardware_keycode=115;
    }
    else if(event->keyval==GDK_KEY_Right) {
      event->keyval=GDK_KEY_End;
      event->state=event->state & GDK_SHIFT_MASK;
      event->hardware_keycode=119;
    }
    else if(event->keyval==GDK_KEY_Up) {
      event->keyval=GDK_KEY_Home;
      event->state=event->state & GDK_SHIFT_MASK;
      event->state+=GDK_CONTROL_MASK;
      event->hardware_keycode=115;
    }
    else if(event->keyval==GDK_KEY_Down) {
      event->keyval=GDK_KEY_End;
      event->state=event->state & GDK_SHIFT_MASK;
      event->state+=GDK_CONTROL_MASK;
      event->hardware_keycode=119;
    }
  }
#endif

  return Gtk::ApplicationWindow::on_key_press_event(event);
}

bool Window::on_delete_event(GdkEventAny *event) {
  Notebook::get().save_session();
  
  for(size_t c=Notebook::get().size()-1;c!=static_cast<size_t>(-1);--c) {
    if(!Notebook::get().close(c))
      return true;
  }
  Terminal::get().kill_async_processes();
#ifdef JUCI_ENABLE_DEBUG
  if(Project::current)
    Project::current->debug_cancel();
#endif

  return false;
}

void Window::search_and_replace_entry() {
  EntryBox::get().clear();
  EntryBox::get().labels.emplace_back();
  auto label_it=EntryBox::get().labels.begin();
  label_it->update=[label_it](int state, const std::string& message){
    if(state==0) {
      try {
        auto number = stoi(message);
        if(number==0)
          label_it->set_text("");
        else if(number==1)
          label_it->set_text("1 result found");
        else if(number>1)
          label_it->set_text(message+" results found");
      }
      catch(const std::exception &e) {}
    }
  };
  EntryBox::get().entries.emplace_back(last_search, [this](const std::string& content){
    if(auto view=Notebook::get().get_current_view())
      view->search_forward();
  });
  auto search_entry_it=EntryBox::get().entries.begin();
  search_entry_it->set_placeholder_text("Find");
  if(auto view=Notebook::get().get_current_view()) {
    view->update_search_occurrences=[label_it](int number){
      label_it->update(0, std::to_string(number));
    };
    view->search_highlight(search_entry_it->get_text(), case_sensitive_search, regex_search);
  }
  search_entry_it->signal_key_press_event().connect([this](GdkEventKey* event){
    if(event->keyval==GDK_KEY_Return && (event->state&GDK_SHIFT_MASK)>0) {
      if(auto view=Notebook::get().get_current_view())
        view->search_backward();
    }
    return false;
  });
  search_entry_it->signal_changed().connect([this, search_entry_it](){
    last_search=search_entry_it->get_text();
    if(auto view=Notebook::get().get_current_view())
      view->search_highlight(search_entry_it->get_text(), case_sensitive_search, regex_search);
  });

  EntryBox::get().entries.emplace_back(last_replace, [this](const std::string &content){
    if(auto view=Notebook::get().get_current_view())
      view->replace_forward(content);
  });
  auto replace_entry_it=EntryBox::get().entries.begin();
  replace_entry_it++;
  replace_entry_it->set_placeholder_text("Replace");
  replace_entry_it->signal_key_press_event().connect([this, replace_entry_it](GdkEventKey* event){
    if(event->keyval==GDK_KEY_Return && (event->state&GDK_SHIFT_MASK)>0) {
      if(auto view=Notebook::get().get_current_view())
        view->replace_backward(replace_entry_it->get_text());
    }
    return false;
  });
  replace_entry_it->signal_changed().connect([this, replace_entry_it](){
    last_replace=replace_entry_it->get_text();
  });
  
  EntryBox::get().buttons.emplace_back("↑", [this](){
    if(auto view=Notebook::get().get_current_view())
        view->search_backward();
  });
  EntryBox::get().buttons.back().set_tooltip_text("Find Previous\n\nShortcut: Shift+Enter in the Find entry field");
  EntryBox::get().buttons.emplace_back("⇄", [this, replace_entry_it](){
    if(auto view=Notebook::get().get_current_view()) {
      view->replace_forward(replace_entry_it->get_text());
    }
  });
  EntryBox::get().buttons.back().set_tooltip_text("Replace Next\n\nShortcut: Enter in the Replace entry field");
  EntryBox::get().buttons.emplace_back("↓", [this](){
    if(auto view=Notebook::get().get_current_view())
      view->search_forward();
  });
  EntryBox::get().buttons.back().set_tooltip_text("Find Next\n\nShortcut: Enter in the Find entry field");
  EntryBox::get().buttons.emplace_back("Replace All", [this, replace_entry_it](){
    if(auto view=Notebook::get().get_current_view())
      view->replace_all(replace_entry_it->get_text());
  });
  EntryBox::get().buttons.back().set_tooltip_text("Replace All");
  
  EntryBox::get().toggle_buttons.emplace_back("Aa");
  EntryBox::get().toggle_buttons.back().set_tooltip_text("Match Case");
  EntryBox::get().toggle_buttons.back().set_active(case_sensitive_search);
  EntryBox::get().toggle_buttons.back().on_activate=[this, search_entry_it](){
    case_sensitive_search=!case_sensitive_search;
    if(auto view=Notebook::get().get_current_view())
      view->search_highlight(search_entry_it->get_text(), case_sensitive_search, regex_search);
  };
  EntryBox::get().toggle_buttons.emplace_back(".*");
  EntryBox::get().toggle_buttons.back().set_tooltip_text("Use Regex");
  EntryBox::get().toggle_buttons.back().set_active(regex_search);
  EntryBox::get().toggle_buttons.back().on_activate=[this, search_entry_it](){
    regex_search=!regex_search;
    if(auto view=Notebook::get().get_current_view())
      view->search_highlight(search_entry_it->get_text(), case_sensitive_search, regex_search);
  };
  EntryBox::get().signal_hide().connect([this]() {
    for(size_t c=0;c<Notebook::get().size();c++) {
      Notebook::get().get_view(c)->update_search_occurrences=nullptr;
      Notebook::get().get_view(c)->search_highlight("", case_sensitive_search, regex_search);
    }
    search_entry_shown=false;
  });
  search_entry_shown=true;
  EntryBox::get().show();
}

void Window::set_tab_entry() {
  EntryBox::get().clear();
  if(auto view=Notebook::get().get_current_view()) {
    auto tab_char_and_size=view->get_tab_char_and_size();
    
    EntryBox::get().labels.emplace_back();
    auto label_it=EntryBox::get().labels.begin();
    
    EntryBox::get().entries.emplace_back(std::to_string(tab_char_and_size.second));
    auto entry_tab_size_it=EntryBox::get().entries.begin();
    entry_tab_size_it->set_placeholder_text("Tab size");
    
    char tab_char=tab_char_and_size.first;
    std::string tab_char_string;
    if(tab_char==' ')
      tab_char_string="space";
    else if(tab_char=='\t')
      tab_char_string="tab";
      
    EntryBox::get().entries.emplace_back(tab_char_string);
    auto entry_tab_char_it=EntryBox::get().entries.rbegin();
    entry_tab_char_it->set_placeholder_text("Tab char");
    
    const auto activate_function=[this, entry_tab_char_it, entry_tab_size_it, label_it](const std::string& content){
      if(auto view=Notebook::get().get_current_view()) {
        char tab_char=0;
        unsigned tab_size=0;
        try {
          tab_size = static_cast<unsigned>(std::stoul(entry_tab_size_it->get_text()));
          std::string tab_char_string=entry_tab_char_it->get_text();
          std::transform(tab_char_string.begin(), tab_char_string.end(), tab_char_string.begin(), ::tolower);
          if(tab_char_string=="space")
            tab_char=' ';
          else if(tab_char_string=="tab")
            tab_char='\t';
        }
        catch(const std::exception &e) {}

        if(tab_char!=0 && tab_size>0) {
          view->set_tab_char_and_size(tab_char, tab_size);
          EntryBox::get().hide();
        }
        else {
          label_it->set_text("Tab size must be >0 and tab char set to either 'space' or 'tab'");
        }
      }
    };
    
    entry_tab_char_it->on_activate=activate_function;
    entry_tab_size_it->on_activate=activate_function;
    
    EntryBox::get().buttons.emplace_back("Set tab in current buffer", [this, entry_tab_char_it](){
      entry_tab_char_it->activate();
    });
    
    EntryBox::get().show();
  }
}

void Window::goto_line_entry() {
  EntryBox::get().clear();
  if(Notebook::get().get_current_view()) {
    EntryBox::get().entries.emplace_back("", [this](const std::string& content){
      if(auto view=Notebook::get().get_current_view()) {
        try {
          view->place_cursor_at_line_index(stoi(content)-1, 0);
          view->scroll_to_cursor_delayed(view, true, false);
        }
        catch(const std::exception &e) {}  
        EntryBox::get().hide();
      }
    });
    auto entry_it=EntryBox::get().entries.begin();
    entry_it->set_placeholder_text("Line number");
    EntryBox::get().buttons.emplace_back("Go to line", [this, entry_it](){
      entry_it->activate();
    });
    EntryBox::get().show();
  }
}

void Window::rename_token_entry() {
  EntryBox::get().clear();
  if(auto view=Notebook::get().get_current_view()) {
    if(view->get_token_spelling && view->rename_similar_tokens) {
      auto spelling=std::make_shared<std::string>(view->get_token_spelling());
      if(!spelling->empty()) {
        EntryBox::get().labels.emplace_back();
        auto label_it=EntryBox::get().labels.begin();
        label_it->update=[label_it](int state, const std::string& message){
          label_it->set_text("Warning: only opened files will be refactored, and altered files will be saved");
        };
        label_it->update(0, "");
        auto iter=std::make_shared<Gtk::TextIter>(view->get_buffer()->get_insert()->get_iter());
        EntryBox::get().entries.emplace_back(*spelling, [this, view, spelling, iter](const std::string& content){
          //TODO: gtk needs a way to check if iter is valid without dumping g_error message
          //iter->get_buffer() will print such a message, but no segfault will occur
          if(Notebook::get().get_current_view()==view && content!=*spelling && iter->get_buffer() && view->get_buffer()->get_insert()->get_iter()==*iter) {
            auto renamed_pairs=view->rename_similar_tokens(Notebook::get().get_views(), content);
            for(auto &renamed: renamed_pairs)
              Terminal::get().print("Replaced "+std::to_string(renamed.second)+" occurrence"+(renamed.second>1?"s":"")+" in file "+renamed.first.string()+"\n");
          }
          else
            Info::get().print("Operation canceled");
          EntryBox::get().hide();
        });
        auto entry_it=EntryBox::get().entries.begin();
        entry_it->set_placeholder_text("New name");
        EntryBox::get().buttons.emplace_back("Rename", [this, entry_it](){
          entry_it->activate();
        });
        EntryBox::get().show();
      }
    }
  }
}
