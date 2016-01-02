#include "window.h"
#include "logging.h"
#include "config.h"
#include "menu.h"
#include "directories.h"
//#include "api.h"
#include "dialogs.h"
#include "filesystem.h"
#ifdef JUCI_ENABLE_DEBUG
#include "debug.h"
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

Window::Window() : compiling(false), debugging(false) {
  JDEBUG("start");
  set_title("juCi++");
  set_events(Gdk::POINTER_MOTION_MASK|Gdk::FOCUS_CHANGE_MASK|Gdk::SCROLL_MASK|Gdk::LEAVE_NOTIFY_MASK);
  set_menu_actions();
  configure();
  set_default_size(Config::get().window.default_size.first, Config::get().window.default_size.second);
  
  //PluginApi(&this->notebook, &this->menu);
  
  add(vpaned);
  
  directories_scrolled_window.add(Directories::get());
  directory_and_notebook_panes.pack1(directories_scrolled_window, Gtk::SHRINK);
  notebook_vbox.pack_start(notebook);
  notebook_vbox.pack_end(entry_box, Gtk::PACK_SHRINK);
  directory_and_notebook_panes.pack2(notebook_vbox, Gtk::SHRINK);
  directory_and_notebook_panes.set_position(static_cast<int>(0.2*Config::get().window.default_size.first));
  vpaned.set_position(static_cast<int>(0.75*Config::get().window.default_size.second));
  vpaned.pack1(directory_and_notebook_panes, true, false);
  
  terminal_scrolled_window.add(Terminal::get());
  terminal_vbox.pack_start(terminal_scrolled_window);
    
  info_and_status_hbox.pack_start(notebook.info, Gtk::PACK_SHRINK);
#if GTK_VERSION_GE(3, 12)
  info_and_status_hbox.set_center_widget(debug_status_label);
#else
  debug_status_label.set_halign(Gtk::Align::ALIGN_CENTER);
  info_and_status_hbox.pack_start(debug_status_label);
#endif
  info_and_status_hbox.pack_end(notebook.status, Gtk::PACK_SHRINK);
  terminal_vbox.pack_end(info_and_status_hbox, Gtk::PACK_SHRINK);
  vpaned.pack2(terminal_vbox, true, true);
  
  show_all_children();

  Directories::get().on_row_activated=[this](const boost::filesystem::path &path) {
    notebook.open(path);
  };

  //Scroll to end of terminal whenever info is printed
  Terminal::get().signal_size_allocate().connect([this](Gtk::Allocation& allocation){
    auto adjustment=terminal_scrolled_window.get_vadjustment();
    adjustment->set_value(adjustment->get_upper()-adjustment->get_page_size());
    Terminal::get().queue_draw();
  });

  entry_box.signal_show().connect([this](){
    vpaned.set_focus_chain({&directory_and_notebook_panes});
    directory_and_notebook_panes.set_focus_chain({&notebook_vbox});
    notebook_vbox.set_focus_chain({&entry_box});
  });
  entry_box.signal_hide().connect([this](){
    vpaned.unset_focus_chain();
    directory_and_notebook_panes.unset_focus_chain();
    notebook_vbox.unset_focus_chain();
  });
  entry_box.signal_hide().connect([this]() {
    if(notebook.get_current_page()!=-1) {
      notebook.get_current_view()->grab_focus();
    }
  });

  notebook.signal_switch_page().connect([this](Gtk::Widget* page, guint page_num) {
    if(notebook.get_current_page()!=-1) {
      auto view=notebook.get_current_view();
      if(search_entry_shown && entry_box.labels.size()>0) {
        view->update_search_occurrences=[this](int number){
          entry_box.labels.begin()->update(0, std::to_string(number));
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
    }
  });
  notebook.signal_page_removed().connect([this](Gtk::Widget* page, guint page_num) {
    entry_box.hide();
  });
  
  about.signal_response().connect([this](int d){
    about.hide();
  });
  
  debug_update_stop.connect([this](){
    debug_stop_mutex.lock();
    for(int c=0;c<notebook.size();c++) {
      auto view=notebook.get_view(c);
      if(view->file_path==debug_last_stop_file_path) {
        view->get_source_buffer()->remove_source_marks(view->get_buffer()->begin(), view->get_buffer()->end(), "debug_stop");
        break;
      }
    }
    //Add debug stop source mark
    for(int c=0;c<notebook.size();c++) {
      auto view=notebook.get_view(c);
      if(view->file_path==debug_stop.first) {
        if(debug_stop.second.first-1<view->get_buffer()->get_line_count()) {
          view->get_source_buffer()->create_source_mark("debug_stop", view->get_buffer()->get_iter_at_line(debug_stop.second.first-1));
          debug_last_stop_file_path=debug_stop.first;
        }
        view->get_buffer()->place_cursor(view->get_buffer()->get_insert()->get_iter());
        break;
      }
    }
    debug_stop_mutex.unlock();
  });
  
  auto &menu=Menu::get();
  menu.actions["debug_stop"]->set_enabled(false);
  menu.actions["debug_kill"]->set_enabled(false);
  menu.actions["debug_step_over"]->set_enabled(false);
  menu.actions["debug_step_into"]->set_enabled(false);
  menu.actions["debug_step_out"]->set_enabled(false);
  menu.actions["debug_backtrace"]->set_enabled(false);
  menu.actions["debug_run_command"]->set_enabled(false);
  menu.actions["debug_goto_stop"]->set_enabled(false);
  debug_update_status.connect([this](){
    debug_status_mutex.lock();
    if(debug_status.empty()) {
      debug_status_label.set_text("");
      auto &menu=Menu::get();
      menu.actions["debug_stop"]->set_enabled(false);
      menu.actions["debug_kill"]->set_enabled(false);
      menu.actions["debug_step_over"]->set_enabled(false);
      menu.actions["debug_step_into"]->set_enabled(false);
      menu.actions["debug_step_out"]->set_enabled(false);
      menu.actions["debug_backtrace"]->set_enabled(false);
      menu.actions["debug_run_command"]->set_enabled(false);
      menu.actions["debug_goto_stop"]->set_enabled(false);
    }
    else {
      debug_status_label.set_text("debug: "+debug_status);
      auto &menu=Menu::get();
      menu.actions["debug_stop"]->set_enabled();
      menu.actions["debug_kill"]->set_enabled();
      menu.actions["debug_step_over"]->set_enabled();
      menu.actions["debug_step_into"]->set_enabled();
      menu.actions["debug_step_out"]->set_enabled();
      menu.actions["debug_backtrace"]->set_enabled();
      menu.actions["debug_run_command"]->set_enabled();
      menu.actions["debug_goto_stop"]->set_enabled();
    }
    debug_status_mutex.unlock();
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
  JDEBUG("end");
} // Window constructor

std::unique_ptr<CMake> Window::get_cmake() {
  boost::filesystem::path path;
  if(notebook.get_current_page()!=-1)
    path=notebook.get_current_view()->file_path.parent_path();
  else
    path=Directories::get().current_path;
  if(path.empty())
    return nullptr;
  return std::unique_ptr<CMake>(new CMake(path));
}

void Window::configure() {
  Config::get().load();
  auto style_context = Gtk::StyleContext::create();
  auto screen = Gdk::Screen::get_default();
  auto css_provider = Gtk::CssProvider::get_named(Config::get().window.theme_name, Config::get().window.theme_variant);
  //TODO: add check if theme exists, or else write error to terminal
  style_context->add_provider_for_screen(screen, css_provider, GTK_STYLE_PROVIDER_PRIORITY_SETTINGS);
  Directories::get().update();
  Menu::get().set_keys();
}

void Window::set_menu_actions() {
  auto &menu = Menu::get();
  
  menu.add_action("about", [this]() {
    about.show();
    about.present();
  });
  menu.add_action("preferences", [this]() {
    notebook.open(Config::get().juci_home_path()/"config"/"config.json");
  });
  menu.add_action("quit", [this]() {
    close();
  });
  
  menu.add_action("new_file", [this]() {
    boost::filesystem::path path = Dialog::new_file();
    if(path!="") {
      if(boost::filesystem::exists(path)) {
        Terminal::get().print("Error: "+path.string()+" already exists.\n", true);
      }
      else {
        if(filesystem::write(path)) {
          if(Directories::get().current_path!="")
            Directories::get().update();
          notebook.open(path);
          Terminal::get().print("New file "+path.string()+" created.\n");
        }
        else
          Terminal::get().print("Error: could not create new file "+path.string()+".\n", true);
      }
    }
  });
  menu.add_action("new_folder", [this]() {
    auto time_now=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    boost::filesystem::path path = Dialog::new_folder();
    if(path!="" && boost::filesystem::exists(path)) {
      boost::system::error_code ec;
      auto last_write_time=boost::filesystem::last_write_time(path, ec);
      if(!ec && last_write_time>=time_now) {
        if(Directories::get().current_path!="")
          Directories::get().update();
        Terminal::get().print("New folder "+path.string()+" created.\n");
      }
      else
        Terminal::get().print("Error: "+path.string()+" already exists.\n", true);
      Directories::get().select(path);
    }
  });
  menu.add_action("new_project_cpp", [this]() {
    boost::filesystem::path project_path = Dialog::new_folder();
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
      std::string cmakelists="cmake_minimum_required(VERSION 2.8)\n\nproject("+project_name+")\n\nset(CMAKE_CXX_FLAGS \"${CMAKE_CXX_FLAGS} -std=c++1y -Wall\")\n\nadd_executable("+project_name+" main.cpp)\n";
      std::string cpp_main="#include <iostream>\n\nusing namespace std;\n\nint main() {\n  cout << \"Hello World!\" << endl;\n\n  return 0;\n}\n";
      if(filesystem::write(cmakelists_path, cmakelists) && filesystem::write(cpp_main_path, cpp_main)) {
        Directories::get().open(project_path);
        notebook.open(cpp_main_path);
        Terminal::get().print("C++ project "+project_name+" created.\n");
      }
      else
        Terminal::get().print("Error: Could not create project "+project_path.string()+"\n", true);
    }
  });
  
  menu.add_action("open_file", [this]() {
    auto path=Dialog::open_file();
    if(path!="")
      notebook.open(path);
  });
  menu.add_action("open_folder", [this]() {
    auto path = Dialog::open_folder();
    if (path!="" && boost::filesystem::exists(path))
      Directories::get().open(path);
  });
  
  menu.add_action("save", [this]() {
    if(notebook.get_current_page()!=-1) {
      if(notebook.save_current()) {
        if(notebook.get_current_page()!=-1) {
          if(notebook.get_current_view()->file_path==Config::get().juci_home_path()/"config"/"config.json") {
            configure();
            for(int c=0;c<notebook.size();c++) {
              notebook.get_view(c)->configure();
              notebook.configure(c);
            }
          }
        }
      }
    }
  });
  menu.add_action("save_as", [this]() {
    if(notebook.get_current_page()!=-1) {
      auto path = Dialog::save_file_as(notebook.get_current_view()->file_path);
      if(path!="") {
        std::ofstream file(path);
        if(file) {
          file << notebook.get_current_view()->get_buffer()->get_text();
          file.close();
          if(Directories::get().current_path!="")
            Directories::get().update();
          notebook.open(path);
          Terminal::get().print("File saved to: " + notebook.get_current_view()->file_path.string()+"\n");
        }
        else
          Terminal::get().print("Error saving file\n");
      }
    }
  });
  
  menu.add_action("edit_undo", [this]() {
    if(notebook.get_current_page()!=-1) {
      auto undo_manager = notebook.get_current_view()->get_source_buffer()->get_undo_manager();
      if (undo_manager->can_undo()) {
        undo_manager->undo();
        notebook.get_current_view()->scroll_to(notebook.get_current_view()->get_buffer()->get_insert());
      }
    }
  });
  menu.add_action("edit_redo", [this]() {
    if(notebook.get_current_page()!=-1) {
      auto undo_manager = notebook.get_current_view()->get_source_buffer()->get_undo_manager();
      if(undo_manager->can_redo()) {
        undo_manager->redo();
        notebook.get_current_view()->scroll_to(notebook.get_current_view()->get_buffer()->get_insert());
      }
    }
  });
  
  menu.add_action("edit_cut", [this]() {
    auto widget=get_focus();
    if(auto entry=dynamic_cast<Gtk::Entry*>(widget))
      entry->cut_clipboard();
    else if(notebook.get_current_page()!=-1)
      notebook.get_current_view()->get_buffer()->cut_clipboard(Gtk::Clipboard::get());
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
    else if(notebook.get_current_page()!=-1)
      notebook.get_current_view()->paste();
  });
  
  menu.add_action("edit_find", [this]() {
    search_and_replace_entry();
  });
  
  menu.add_action("source_spellcheck", [this]() {
    if(notebook.get_current_page()!=-1)
        notebook.get_current_view()->spellcheck();
  });
  menu.add_action("source_spellcheck_clear", [this]() {
    if(notebook.get_current_page()!=-1)
        notebook.get_current_view()->remove_spellcheck_errors();
  });
  menu.add_action("source_spellcheck_next_error", [this]() {
    if(notebook.get_current_page()!=-1)
        notebook.get_current_view()->goto_next_spellcheck_error();
  });
  
  menu.add_action("source_indentation_set_buffer_tab", [this]() {
    set_tab_entry();
  });
  menu.add_action("source_indentation_auto_indent_buffer", [this]() {
    if(notebook.get_current_page()!=-1 && notebook.get_current_view()->auto_indent)
      notebook.get_current_view()->auto_indent();
  });
  
  menu.add_action("source_goto_line", [this]() {
    goto_line_entry();
  });
  menu.add_action("source_center_cursor", [this]() {
    if(notebook.get_current_page()!=-1) {
      auto view=notebook.get_current_view();
      
      while(g_main_context_pending(NULL))
        g_main_context_iteration(NULL, false);
      if(notebook.get_current_page()!=-1 && notebook.get_current_view()==view)
        view->scroll_to(view->get_buffer()->get_insert(), 0.0, 1.0, 0.5);
    }
  });
  
  menu.add_action("source_find_documentation", [this]() {
    if(notebook.get_current_page()!=-1) {
      if(notebook.get_current_view()->get_token_data) {
        auto data=notebook.get_current_view()->get_token_data();        
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
    if(notebook.get_current_page()!=-1) {
      if(notebook.get_current_view()->get_declaration_location) {
        auto location=notebook.get_current_view()->get_declaration_location();
        if(!location.file_path.empty()) {
          boost::filesystem::path declaration_file;
          boost::system::error_code ec;
          declaration_file=boost::filesystem::canonical(location.file_path, ec);
          if(ec)
            declaration_file=location.file_path;
          notebook.open(declaration_file);
          auto line=static_cast<int>(location.line)-1;
          auto index=static_cast<int>(location.index)-1;
          auto view=notebook.get_current_view();
          line=std::min(line, view->get_buffer()->get_line_count()-1);
          if(line>=0) {
            auto iter=view->get_buffer()->get_iter_at_line(line);
            while(!iter.ends_line())
              iter.forward_char();
            auto end_line_index=iter.get_line_index();
            index=std::min(index, end_line_index);
            
            view->get_buffer()->place_cursor(view->get_buffer()->get_iter_at_line_index(line, index));
            while(g_main_context_pending(NULL))
              g_main_context_iteration(NULL, false);
            if(notebook.get_current_page()!=-1 && notebook.get_current_view()==view) {
              view->scroll_to(view->get_buffer()->get_insert(), 0.0, 1.0, 0.5);
              view->delayed_tooltips_connection.disconnect();
            }
          }
        }
      }
    }
  });
  menu.add_action("source_goto_usage", [this]() {
    if(notebook.get_current_page()!=-1) {
      auto current_view=notebook.get_current_view();
      if(current_view->get_token && current_view->get_usages) {
        auto token=current_view->get_token();
        if(token) {
          auto iter=current_view->get_buffer()->get_insert()->get_iter();
          Gdk::Rectangle visible_rect;
          current_view->get_visible_rect(visible_rect);
          Gdk::Rectangle iter_rect;
          current_view->get_iter_location(iter, iter_rect);
          iter_rect.set_width(1);
          if(!visible_rect.intersects(iter_rect)) {
            current_view->get_iter_at_location(iter, 0, visible_rect.get_y()+visible_rect.get_height()/3);
          }
          current_view->selection_dialog=std::unique_ptr<SelectionDialog>(new SelectionDialog(*current_view, current_view->get_buffer()->create_mark(iter), true, true));
          auto rows=std::make_shared<std::unordered_map<std::string, Source::Offset> >();
          
          //First add usages in current file
          auto usages=current_view->get_usages(token);
          for(auto &usage: usages) {
            auto iter=current_view->get_buffer()->get_iter_at_line_index(usage.first.line, usage.first.index);
            auto row=std::to_string(iter.get_line()+1)+':'+std::to_string(iter.get_line_offset()+1)+' '+usage.second;
            (*rows)[row]=usage.first;
            current_view->selection_dialog->add_row(row);
          }
          //Then the remaining opened files
          for(int page=0;page<notebook.size();page++) {
            auto view=notebook.get_view(page);
            if(view!=current_view) {
              if(view->get_usages) {
                auto usages=view->get_usages(token);
                for(auto &usage: usages) {
                  auto iter=view->get_buffer()->get_iter_at_line_index(usage.first.line, usage.first.index);
                  auto row=usage.first.file_path.filename().string()+":"+std::to_string(iter.get_line()+1)+':'+std::to_string(iter.get_line_offset()+1)+' '+usage.second;
                  (*rows)[row]=usage.first;
                  current_view->selection_dialog->add_row(row);
                }
              }
            }
          }
          
          if(rows->size()==0)
            return;
          current_view->selection_dialog->on_select=[this, rows](const std::string& selected, bool hide_window) {
            auto offset=rows->at(selected);
            boost::filesystem::path declaration_file;
            boost::system::error_code ec;
            declaration_file=boost::filesystem::canonical(offset.file_path, ec);
            if(ec)
              declaration_file=offset.file_path;
            notebook.open(declaration_file);
            auto view=notebook.get_current_view();
            view->get_buffer()->place_cursor(view->get_buffer()->get_iter_at_line_index(offset.line, offset.index));
            view->scroll_to(view->get_buffer()->get_insert(), 0.0, 1.0, 0.5);
            view->delayed_tooltips_connection.disconnect();
          };
          current_view->selection_dialog->show();
        }
      }
    }
  });
  menu.add_action("source_goto_method", [this]() {
    if(notebook.get_current_page()!=-1) {
      if(notebook.get_current_view()->goto_method) {
        notebook.get_current_view()->goto_method();
      }
    }
  });
  menu.add_action("source_rename", [this]() {
    rename_token_entry();
  });
  
  menu.add_action("source_goto_next_diagnostic", [this]() {
    if(notebook.get_current_page()!=-1) {
      if(notebook.get_current_view()->goto_next_diagnostic) {
        notebook.get_current_view()->goto_next_diagnostic();
      }
    }
  });
  menu.add_action("source_apply_fix_its", [this]() {
    if(notebook.get_current_page()!=-1) {
      if(notebook.get_current_view()->apply_fix_its) {
        notebook.get_current_view()->apply_fix_its();
      }
    }
  });
  
  menu.add_action("project_set_run_arguments", [this]() {
    auto cmake=get_cmake();
    if(!cmake)
      return;
    auto project_path=std::make_shared<boost::filesystem::path>(cmake->project_path);
    if(project_path->empty())
      return;
    auto run_arguments_it=project_run_arguments.find(project_path->string());
    std::string run_arguments;
    if(run_arguments_it!=project_run_arguments.end())
      run_arguments=run_arguments_it->second;

    if(run_arguments.empty()) {
      boost::filesystem::path cmake_path;
      if(notebook.get_current_page()!=-1)
        cmake_path=notebook.get_current_view()->file_path.parent_path();
      else
        cmake_path=Directories::get().current_path;
      if(cmake_path.empty())
        return;
      CMake cmake(cmake_path);
      if(cmake.project_path.empty())
        return;
      auto executable=cmake.get_executable(notebook.get_current_page()!=-1?notebook.get_current_view()->file_path:"").string();
      
      if(executable!="") {
        auto project_path=cmake.project_path;
        auto default_build_path=CMake::get_default_build_path(project_path);
        if(!default_build_path.empty()) {
          size_t pos=executable.find(project_path.string());
          if(pos!=std::string::npos)
            executable.replace(pos, project_path.string().size(), default_build_path.string());
        }
        run_arguments=filesystem::escape_argument(executable);
      }
      else
        run_arguments=filesystem::escape_argument(CMake::get_default_build_path(cmake.project_path));
    }
    
    entry_box.clear();
    entry_box.labels.emplace_back();
    auto label_it=entry_box.labels.begin();
    label_it->update=[label_it](int state, const std::string& message){
      label_it->set_text("Set empty to let juCi++ deduce executable");
    };
    label_it->update(0, "");
    entry_box.entries.emplace_back(run_arguments, [this, project_path](const std::string& content){
      project_run_arguments[project_path->string()]=content;
      entry_box.hide();
    }, 50);
    auto entry_it=entry_box.entries.begin();
    entry_it->set_placeholder_text("Project: Set Run Arguments");
    entry_box.buttons.emplace_back("Project: set run arguments", [this, entry_it](){
      entry_it->activate();
    });
    entry_box.show();
  });
  menu.add_action("compile_and_run", [this]() {
    if(compiling)
      return;
    
    auto cmake=get_cmake();
    if(!cmake)
      return;
    auto project_path=cmake->project_path;
    if(project_path.empty())
      return;
    
    auto default_build_path=CMake::get_default_build_path(project_path);
    if(default_build_path.empty())
      return;
    
    auto run_arguments_it=project_run_arguments.find(project_path.string());
    std::string run_arguments;
    if(run_arguments_it!=project_run_arguments.end())
      run_arguments=run_arguments_it->second;
    
    std::string command;
    if(!run_arguments.empty()) {
      command=run_arguments;
    }
    else {
      command=cmake->get_executable(notebook.get_current_page()!=-1?notebook.get_current_view()->file_path:"").string();
      if(command.empty()) {
        Terminal::get().print("Could not find add_executable in the following paths:\n");
        for(auto &path: cmake->paths)
          Terminal::get().print("  "+path.string()+"\n");
        Terminal::get().print("Solution: either use Project Set Run Arguments, or open a source file within a directory where add_executable is set.\n", true);
        return;
      }
      size_t pos=command.find(project_path.string());
      if(pos!=std::string::npos)
        command.replace(pos, project_path.string().size(), default_build_path.string());
      command=filesystem::escape_argument(command);
    }
    
    compiling=true;
    Terminal::get().print("Compiling and running "+command+"\n");
    Terminal::get().async_process(Config::get().terminal.make_command, default_build_path, [this, command, default_build_path](int exit_status){
      compiling=false;
      if(exit_status==EXIT_SUCCESS) {
        Terminal::get().async_process(command, default_build_path, [this, command](int exit_status){
          Terminal::get().async_print(command+" returned: "+std::to_string(exit_status)+'\n');
        });
      }
    });
  });
  menu.add_action("compile", [this]() {
    if(compiling)
      return;
    boost::filesystem::path cmake_path;
    if(notebook.get_current_page()!=-1)
      cmake_path=notebook.get_current_view()->file_path.parent_path();
    else
      cmake_path=Directories::get().current_path;
    if(cmake_path.empty())
      return;
    CMake cmake(cmake_path);
    if(cmake.project_path!="") {
      auto default_build_path=CMake::get_default_build_path(cmake.project_path);
      if(default_build_path.empty())
        return;
      compiling=true;
      Terminal::get().print("Compiling project "+cmake.project_path.string()+"\n");
      Terminal::get().async_process(Config::get().terminal.make_command, default_build_path, [this](int exit_status){
        compiling=false;
      });
    }
  });
  
  menu.add_action("run_command", [this]() {
    entry_box.clear();
    entry_box.labels.emplace_back();
    auto label_it=entry_box.labels.begin();
    label_it->update=[label_it](int state, const std::string& message){
      label_it->set_text("Run Command directory order: file project path, opened directory, current directory");
    };
    label_it->update(0, "");
    entry_box.entries.emplace_back(last_run_command, [this](const std::string& content){
      if(content!="") {
        last_run_command=content;
        auto run_path=notebook.get_current_folder();
        Terminal::get().async_print("Running: "+content+'\n');
  
        Terminal::get().async_process(content, run_path, [this, content](int exit_status){
          Terminal::get().async_print(content+" returned: "+std::to_string(exit_status)+'\n');
        });
      }
      entry_box.hide();
    }, 30);
    auto entry_it=entry_box.entries.begin();
    entry_it->set_placeholder_text("Command");
    entry_box.buttons.emplace_back("Run command", [this, entry_it](){
      entry_it->activate();
    });
    entry_box.show();
  });
  
  menu.add_action("kill_last_running", [this]() {
    Terminal::get().kill_last_async_process();
  });
  menu.add_action("force_kill_last_running", [this]() {
    Terminal::get().kill_last_async_process(true);
  });
  
#ifdef JUCI_ENABLE_DEBUG
  menu.add_action("debug_set_run_arguments", [this]() {
    auto cmake=get_cmake();
    if(!cmake)
      return;
    auto project_path=std::make_shared<boost::filesystem::path>(cmake->project_path);
    if(project_path->empty())
      return;
    auto run_arguments_it=debug_run_arguments.find(project_path->string());
    std::string run_arguments;
    if(run_arguments_it!=debug_run_arguments.end())
      run_arguments=run_arguments_it->second;
  
    if(run_arguments.empty()) {
      boost::filesystem::path cmake_path;
      if(notebook.get_current_page()!=-1)
        cmake_path=notebook.get_current_view()->file_path.parent_path();
      else
        cmake_path=Directories::get().current_path;
      if(cmake_path.empty())
        return;
      CMake cmake(cmake_path);
      if(cmake.project_path.empty())
        return;
      auto executable=cmake.get_executable(notebook.get_current_page()!=-1?notebook.get_current_view()->file_path:"").string();
      
      if(executable!="") {
        auto project_path=cmake.project_path;
        auto debug_build_path=CMake::get_debug_build_path(project_path);
        if(!debug_build_path.empty()) {
          size_t pos=executable.find(project_path.string());
          if(pos!=std::string::npos)
            executable.replace(pos, project_path.string().size(), debug_build_path.string());
        }
        run_arguments=filesystem::escape_argument(executable);
      }
      else
        run_arguments=filesystem::escape_argument(CMake::get_debug_build_path(cmake.project_path));
    }
    
    entry_box.clear();
    entry_box.labels.emplace_back();
    auto label_it=entry_box.labels.begin();
    label_it->update=[label_it](int state, const std::string& message){
      label_it->set_text("Set empty to let juCi++ deduce executable");
    };
    label_it->update(0, "");
    entry_box.entries.emplace_back(run_arguments, [this, project_path](const std::string& content){
      debug_run_arguments[project_path->string()]=content;
      entry_box.hide();
    }, 50);
    auto entry_it=entry_box.entries.begin();
    entry_it->set_placeholder_text("Project: Set Run Arguments");
    entry_box.buttons.emplace_back("Project: set run arguments", [this, entry_it](){
      entry_it->activate();
    });
    entry_box.show();
  });
  menu.add_action("debug_start_continue", [this](){
    if(debugging) {
      Debug::get().continue_debug();
      return;
    }
    
    auto cmake=get_cmake();
    if(!cmake)
      return;
    auto project_path=cmake->project_path;
    if(project_path.empty())
      return;
        
    auto debug_build_path=CMake::get_debug_build_path(project_path);
    if(debug_build_path.empty())
      return;
    if(!CMake::create_debug_build(project_path))
      return;
    
    auto run_arguments_it=debug_run_arguments.find(project_path.string());
    std::string run_arguments;
    if(run_arguments_it!=debug_run_arguments.end())
      run_arguments=run_arguments_it->second;
    
    std::string command;
    if(!run_arguments.empty()) {
      command=run_arguments;
    }
    else {
      command=cmake->get_executable(notebook.get_current_page()!=-1?notebook.get_current_view()->file_path:"").string();
      if(command.empty()) {
        Terminal::get().print("Could not find add_executable in the following paths:\n");
        for(auto &path: cmake->paths)
          Terminal::get().print("  "+path.string()+"\n");
        Terminal::get().print("Solution: either use Debug Set Run Arguments, or open a source file within a directory where add_executable is set.\n", true);
        return;
      }
      size_t pos=command.find(project_path.string());
      if(pos!=std::string::npos)
        command.replace(pos, project_path.string().size(), debug_build_path.string());
      command=filesystem::escape_argument(command);
    }
    
    auto breakpoints=std::make_shared<std::vector<std::pair<boost::filesystem::path, int> > >();
    for(int c=0;c<notebook.size();c++) {
      auto view=notebook.get_view(c);
      if(project_path==view->project_path) {
        auto iter=view->get_buffer()->begin();
        if(view->get_source_buffer()->get_source_marks_at_iter(iter, "debug_breakpoint").size()>0)
          breakpoints->emplace_back(view->file_path, iter.get_line()+1);
        while(view->get_source_buffer()->forward_iter_to_source_mark(iter, "debug_breakpoint"))
          breakpoints->emplace_back(view->file_path, iter.get_line()+1);
      }
    }
    
    debugging=true;
    Terminal::get().print("Compiling and debugging "+command+"\n");
    Terminal::get().async_process(Config::get().terminal.make_command, debug_build_path, [this, breakpoints, command, debug_build_path](int exit_status){
      if(exit_status!=EXIT_SUCCESS)
        debugging=false;
      else {
        debug_start_mutex.lock();
        Debug::get().start(command, debug_build_path, breakpoints, [this, command](int exit_status){
          debugging=false;
          Terminal::get().async_print(command+" returned: "+std::to_string(exit_status)+'\n');
        }, [this](const std::string &status) {
          debug_status_mutex.lock();
          debug_status=status;
          debug_status_mutex.unlock();
          debug_update_status();
        }, [this](const boost::filesystem::path &file_path, int line_nr, int line_index) {
          debug_stop_mutex.lock();
          debug_stop.first=file_path;
          debug_stop.second.first=line_nr;
          debug_stop.second.second=line_index;
          debug_stop_mutex.unlock();
          debug_update_stop();
          //Remove debug stop source mark
        });
        debug_start_mutex.unlock();
      }
    });
  });
  menu.add_action("debug_stop", [this]() {
    if(debugging) {
      Debug::get().stop();
    }
  });
  menu.add_action("debug_kill", [this]() {
    if(debugging) {
      Debug::get().kill();
    }
  });
  menu.add_action("debug_step_over", [this]() {
    if(debugging)
      Debug::get().step_over();
  });
  menu.add_action("debug_step_into", [this]() {
    if(debugging)
      Debug::get().step_into();
  });
  menu.add_action("debug_step_out", [this]() {
    if(debugging)
      Debug::get().step_out();
  });
  menu.add_action("debug_backtrace", [this]() {
    if(debugging && notebook.get_current_page()!=-1) {
      auto backtrace=Debug::get().get_backtrace();
      
      auto view=notebook.get_current_view();
      auto buffer=view->get_buffer();
      auto iter=buffer->get_insert()->get_iter();
      Gdk::Rectangle visible_rect;
      view->get_visible_rect(visible_rect);
      Gdk::Rectangle iter_rect;
      view->get_iter_location(iter, iter_rect);
      iter_rect.set_width(1);
      if(!visible_rect.intersects(iter_rect)) {
        view->get_iter_at_location(iter, 0, visible_rect.get_y()+visible_rect.get_height()/3);
      }
      view->selection_dialog=std::unique_ptr<SelectionDialog>(new SelectionDialog(*view, buffer->create_mark(iter), true, true));
      auto rows=std::make_shared<std::unordered_map<std::string, Debug::Frame> >();
      if(backtrace.size()==0)
        return;
      
      std::string project_path;
      auto cmake=get_cmake();
      if(cmake)
        project_path=cmake->project_path.string();
      
      for(auto &frame: backtrace) {
        std::string row;
        if(frame.file_path.empty())
          row=frame.function_name;
        else {
          auto file_path=frame.file_path;
          if(!project_path.empty()) {
            auto pos=file_path.find(project_path);
            if(pos==0)
              file_path.erase(0, project_path.size()+1);
          }
          
          row="<b>"+Glib::Markup::escape_text(file_path)+":"+std::to_string(frame.line_nr)+"</b> "+Glib::Markup::escape_text(frame.function_name);
        }
        (*rows)[row]=frame;
        view->selection_dialog->add_row(row);
      }
      
      view->selection_dialog->on_select=[this, rows](const std::string& selected, bool hide_window) {
        auto frame=rows->at(selected);
        if(!frame.file_path.empty()) {
          notebook.open(frame.file_path);
          if(notebook.get_current_page()!=-1) {
            auto view=notebook.get_current_view();
            
            view->get_buffer()->place_cursor(view->get_buffer()->get_iter_at_line_index(frame.line_nr-1, frame.line_index-1));
            
            while(g_main_context_pending(NULL))
              g_main_context_iteration(NULL, false);
            if(notebook.get_current_page()!=-1 && notebook.get_current_view()==view)
              view->scroll_to(view->get_buffer()->get_insert(), 0.0, 1.0, 0.5);
          }
        }
      };
      view->selection_dialog->show();
    }
  });
  menu.add_action("debug_run_command", [this]() {
    entry_box.clear();
    entry_box.entries.emplace_back(last_run_debug_command, [this](const std::string& content){
      if(content!="") {
        if(debugging) {
          auto command_return=Debug::get().run_command(content);
          Terminal::get().async_print(command_return.first);
          Terminal::get().async_print(command_return.second, true);
        }
        last_run_debug_command=content;
      }
      entry_box.hide();
    }, 30);
    auto entry_it=entry_box.entries.begin();
    entry_it->set_placeholder_text("Debug Command");
    entry_box.buttons.emplace_back("Run debug command", [this, entry_it](){
      entry_it->activate();
    });
    entry_box.show();
  });
  menu.add_action("debug_toggle_breakpoint", [this](){
    bool debug_is_stopped_or_running=Debug::get().is_stopped() || Debug::get().is_running();
    if(Debug::get().is_invalid() || debug_is_stopped_or_running) {
      if(notebook.get_current_page()!=-1) {
        auto view=notebook.get_current_view();
        auto line_nr=view->get_buffer()->get_insert()->get_iter().get_line();
        
        if(view->get_source_buffer()->get_source_marks_at_line(line_nr, "debug_breakpoint").size()>0) {
          auto start_iter=view->get_buffer()->get_iter_at_line(line_nr);
          auto end_iter=start_iter;
          while(!end_iter.ends_line() && end_iter.forward_char()) {}
          view->get_source_buffer()->remove_source_marks(start_iter, end_iter, "debug_breakpoint");
          if(debug_is_stopped_or_running)
            Debug::get().remove_breakpoint(view->file_path, line_nr+1, view->get_buffer()->get_line_count()+1);
        }
        else {
          view->get_source_buffer()->create_source_mark("debug_breakpoint", view->get_buffer()->get_insert()->get_iter());
          if(debug_is_stopped_or_running)
            Debug::get().add_breakpoint(view->file_path, line_nr+1);
        }
      }
    }
  });
  menu.add_action("debug_goto_stop", [this](){
    if(debugging) {
      debug_stop_mutex.lock();
      auto debug_stop_copy=debug_stop;
      debug_stop_mutex.unlock();
      if(!debug_stop_copy.first.empty()) {
        notebook.open(debug_stop_copy.first);
        if(notebook.get_current_page()!=-1) {
          auto view=notebook.get_current_view();
          
          int line_nr=debug_stop_copy.second.first-1;
          int line_index=debug_stop_copy.second.second-1;
          if(line_nr<view->get_buffer()->get_line_count()) {
            auto iter=view->get_buffer()->get_iter_at_line(line_nr);
            auto end_line_iter=iter;
            while(!iter.ends_line() && iter.forward_char()) {}
            auto line=view->get_buffer()->get_text(iter, end_line_iter);
            if(static_cast<size_t>(line_index)>=line.bytes())
              line_index=0;
            view->get_buffer()->place_cursor(view->get_buffer()->get_iter_at_line_index(line_nr, line_index));
            
            while(g_main_context_pending(NULL))
              g_main_context_iteration(NULL, false);
            if(notebook.get_current_page()!=-1 && notebook.get_current_view()==view)
              view->scroll_to(view->get_buffer()->get_insert(), 0.0, 1.0, 0.5);
          }
          debug_update_stop();
        }
      }
    }
  });
#endif
  
  menu.add_action("next_tab", [this]() {
    if(notebook.get_current_page()!=-1) {
      notebook.open(notebook.get_view((notebook.get_current_page()+1)%notebook.size())->file_path);
    }
  });
  menu.add_action("previous_tab", [this]() {
    if(notebook.get_current_page()!=-1) {
      int previous_page=notebook.get_current_page()-1;
      if(previous_page<0)
        previous_page=notebook.size()-1;
      notebook.open(notebook.get_view(previous_page)->file_path);
    }
  });
  menu.add_action("close_tab", [this]() {
    notebook.close_current_page();
    if(notebook.get_current_page()!=-1) {
      notebook.get_current_view()->set_status(notebook.get_current_view()->status);
      notebook.get_current_view()->set_info(notebook.get_current_view()->info);
    }
    else {
      notebook.status.set_text("");
      notebook.info.set_text("");
      
      activate_menu_items(false);
    }
  });
  activate_menu_items(false);
}

void Window::activate_menu_items(bool activate) {
  auto &menu = Menu::get();
  menu.actions["source_indentation_auto_indent_buffer"]->set_enabled(activate ? static_cast<bool>(notebook.get_current_view()->auto_indent) : false);
  menu.actions["source_find_documentation"]->set_enabled(activate ? static_cast<bool>(notebook.get_current_view()->get_token_data) : false);
  menu.actions["source_goto_declaration"]->set_enabled(activate ? static_cast<bool>(notebook.get_current_view()->get_declaration_location) : false);
  menu.actions["source_goto_usage"]->set_enabled(activate ? static_cast<bool>(notebook.get_current_view()->get_usages) : false);
  menu.actions["source_goto_method"]->set_enabled(activate ? static_cast<bool>(notebook.get_current_view()->goto_method) : false);
  menu.actions["source_rename"]->set_enabled(activate ? static_cast<bool>(notebook.get_current_view()->rename_similar_tokens) : false);
  menu.actions["source_goto_next_diagnostic"]->set_enabled(activate ? static_cast<bool>(notebook.get_current_view()->goto_next_diagnostic) : false);
  menu.actions["source_apply_fix_its"]->set_enabled(activate ? static_cast<bool>(notebook.get_current_view()->apply_fix_its) : false);
}

bool Window::on_key_press_event(GdkEventKey *event) {
  if(event->keyval==GDK_KEY_Escape) {
    entry_box.hide();
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
  auto size=notebook.size();
  for(int c=0;c<size;c++) {
    if(!notebook.close_current_page())
      return true;
  }
  Terminal::get().kill_async_processes();
#ifdef JUCI_ENABLE_DEBUG
  debug_start_mutex.lock();
  Debug::get().delete_debug();
  debug_start_mutex.unlock();
#endif
  return false;
}

void Window::search_and_replace_entry() {
  entry_box.clear();
  entry_box.labels.emplace_back();
  auto label_it=entry_box.labels.begin();
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
  entry_box.entries.emplace_back(last_search, [this](const std::string& content){
    if(notebook.get_current_page()!=-1)
      notebook.get_current_view()->search_forward();
  });
  auto search_entry_it=entry_box.entries.begin();
  search_entry_it->set_placeholder_text("Find");
  if(notebook.get_current_page()!=-1) {
    notebook.get_current_view()->update_search_occurrences=[label_it](int number){
      label_it->update(0, std::to_string(number));
    };
    notebook.get_current_view()->search_highlight(search_entry_it->get_text(), case_sensitive_search, regex_search);
  }
  search_entry_it->signal_key_press_event().connect([this](GdkEventKey* event){
    if(event->keyval==GDK_KEY_Return && (event->state&GDK_SHIFT_MASK)>0) {
      if(notebook.get_current_page()!=-1)
        notebook.get_current_view()->search_backward();
    }
    return false;
  });
  search_entry_it->signal_changed().connect([this, search_entry_it](){
    last_search=search_entry_it->get_text();
    if(notebook.get_current_page()!=-1)
      notebook.get_current_view()->search_highlight(search_entry_it->get_text(), case_sensitive_search, regex_search);
  });

  entry_box.entries.emplace_back(last_replace, [this](const std::string &content){
    if(notebook.get_current_page()!=-1)
      notebook.get_current_view()->replace_forward(content);
  });
  auto replace_entry_it=entry_box.entries.begin();
  replace_entry_it++;
  replace_entry_it->set_placeholder_text("Replace");
  replace_entry_it->signal_key_press_event().connect([this, replace_entry_it](GdkEventKey* event){
    if(event->keyval==GDK_KEY_Return && (event->state&GDK_SHIFT_MASK)>0) {
      if(notebook.get_current_page()!=-1)
        notebook.get_current_view()->replace_backward(replace_entry_it->get_text());
    }
    return false;
  });
  replace_entry_it->signal_changed().connect([this, replace_entry_it](){
    last_replace=replace_entry_it->get_text();
  });

  entry_box.buttons.emplace_back("Replace all", [this, replace_entry_it](){
    if(notebook.get_current_page()!=-1)
      notebook.get_current_view()->replace_all(replace_entry_it->get_text());
  });
  entry_box.toggle_buttons.emplace_back("Match case");
  entry_box.toggle_buttons.back().set_active(case_sensitive_search);
  entry_box.toggle_buttons.back().on_activate=[this, search_entry_it](){
    case_sensitive_search=!case_sensitive_search;
    if(notebook.get_current_page()!=-1)
      notebook.get_current_view()->search_highlight(search_entry_it->get_text(), case_sensitive_search, regex_search);
  };
  entry_box.toggle_buttons.emplace_back("Use regex");
  entry_box.toggle_buttons.back().set_active(regex_search);
  entry_box.toggle_buttons.back().on_activate=[this, search_entry_it](){
    regex_search=!regex_search;
    if(notebook.get_current_page()!=-1)
      notebook.get_current_view()->search_highlight(search_entry_it->get_text(), case_sensitive_search, regex_search);
  };
  entry_box.signal_hide().connect([this]() {
    for(int c=0;c<notebook.size();c++) {
      notebook.get_view(c)->update_search_occurrences=nullptr;
      notebook.get_view(c)->search_highlight("", case_sensitive_search, regex_search);
    }
    search_entry_shown=false;
  });
  search_entry_shown=true;
  entry_box.show();
}

void Window::set_tab_entry() {
  entry_box.clear();
  if(notebook.get_current_page()!=-1) {
    auto tab_char_and_size=notebook.get_current_view()->get_tab_char_and_size();
    
    entry_box.labels.emplace_back();
    auto label_it=entry_box.labels.begin();
    
    entry_box.entries.emplace_back(std::to_string(tab_char_and_size.second));
    auto entry_tab_size_it=entry_box.entries.begin();
    entry_tab_size_it->set_placeholder_text("Tab size");
    
    char tab_char=tab_char_and_size.first;
    std::string tab_char_string;
    if(tab_char==' ')
      tab_char_string="space";
    else if(tab_char=='\t')
      tab_char_string="tab";
      
    entry_box.entries.emplace_back(tab_char_string);
    auto entry_tab_char_it=entry_box.entries.rbegin();
    entry_tab_char_it->set_placeholder_text("Tab char");
    
    const auto activate_function=[this, entry_tab_char_it, entry_tab_size_it, label_it](const std::string& content){
      if(notebook.get_current_page()!=-1) {
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
          notebook.get_current_view()->set_tab_char_and_size(tab_char, tab_size);
          entry_box.hide();
        }
        else {
          label_it->set_text("Tab size must be >0 and tab char set to either 'space' or 'tab'");
        }
      }
    };
    
    entry_tab_char_it->on_activate=activate_function;
    entry_tab_size_it->on_activate=activate_function;
    
    entry_box.buttons.emplace_back("Set tab in current buffer", [this, entry_tab_char_it](){
      entry_tab_char_it->activate();
    });
    
    entry_box.show();
  }
}

void Window::goto_line_entry() {
  entry_box.clear();
  if(notebook.get_current_page()!=-1) {
    entry_box.entries.emplace_back("", [this](const std::string& content){
      if(notebook.get_current_page()!=-1) {
        auto view=notebook.get_current_view();
        try {
          auto line = stoi(content);
          if(line>0 && line<=view->get_buffer()->get_line_count()) {
            line--;
            
            view->get_buffer()->place_cursor(view->get_buffer()->get_iter_at_line(line));
            while(g_main_context_pending(NULL))
              g_main_context_iteration(NULL, false);
            if(notebook.get_current_page()!=-1 && notebook.get_current_view()==view)
              view->scroll_to(view->get_buffer()->get_insert(), 0.0, 1.0, 0.5);
          }
        }
        catch(const std::exception &e) {}  
        entry_box.hide();
      }
    });
    auto entry_it=entry_box.entries.begin();
    entry_it->set_placeholder_text("Line number");
    entry_box.buttons.emplace_back("Go to line", [this, entry_it](){
      entry_it->activate();
    });
    entry_box.show();
  }
}

void Window::rename_token_entry() {
  entry_box.clear();
  if(notebook.get_current_page()!=-1) {
    if(notebook.get_current_view()->get_token) {
      auto token=std::make_shared<Source::Token>(notebook.get_current_view()->get_token());
      if(*token) {
        entry_box.labels.emplace_back();
        auto label_it=entry_box.labels.begin();
        label_it->update=[label_it](int state, const std::string& message){
          label_it->set_text("Warning: only opened and parsed tabs will have its content renamed, and modified files will be saved");
        };
        label_it->update(0, "");
        entry_box.entries.emplace_back(token->spelling, [this, token](const std::string& content){
          if(notebook.get_current_page()!=-1 && content!=token->spelling) {
            std::vector<int> modified_pages;
            for(int c=0;c<notebook.size();c++) {
              auto view=notebook.get_view(c);
              if(view->rename_similar_tokens) {
                auto number=view->rename_similar_tokens(*token, content);
                if(number>0) {
                  Terminal::get().print("Replaced "+std::to_string(number)+" occurrences in file "+view->file_path.string()+"\n");
                  notebook.save(c);
                  modified_pages.emplace_back(c);
                }
              }
            }
            for(auto &page: modified_pages)
              notebook.get_view(page)->soft_reparse_needed=false;
            entry_box.hide();
          }
        });
        auto entry_it=entry_box.entries.begin();
        entry_it->set_placeholder_text("New name");
        entry_box.buttons.emplace_back("Rename", [this, entry_it](){
          entry_it->activate();
        });
        entry_box.show();
      }
    }
  }
}
