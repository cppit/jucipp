#include "window.h"
#include "config.h"
#include "menu.h"
#include "notebook.h"
#include "directories.h"
#include "dialogs.h"
#include "filesystem.h"
#include "project.h"
#include "entrybox.h"
#include "info.h"
#include "selection_dialog.h"
#include "terminal.h"

Window::Window() {
  Gsv::init();
  
  set_title("juCi++");
  set_events(Gdk::POINTER_MOTION_MASK|Gdk::FOCUS_CHANGE_MASK|Gdk::SCROLL_MASK|Gdk::LEAVE_NOTIFY_MASK);
  
  auto provider = Gtk::CssProvider::create();
  auto screen = get_screen();
  std::string border_radius_style;
  if(screen->get_rgba_visual())
    border_radius_style="border-radius: 5px; ";
#if GTK_VERSION_GE(3, 20)
  std::string notebook_style(".juci_notebook tab {border-radius: 5px 5px 0 0; padding: 0 4px; margin: 0;}");
#else
  std::string notebook_style(".juci_notebook {-GtkNotebook-tab-overlap: 0px;} .juci_notebook tab {border-radius: 5px 5px 0 0; padding: 4px 4px;}");
#endif
  provider->load_from_data(R"(
    .juci_directories *:selected {border-left-color: inherit; color: inherit; background-color: rgba(128, 128, 128 , 0.2); background-image: inherit;}
    )"+notebook_style+R"(
    .juci_info {border-radius: 5px;}
    .juci_tooltip_window {background-color: transparent;}
    .juci_tooltip_box {)"+border_radius_style+R"(padding: 3px;}
  )");
  get_style_context()->add_provider_for_screen(screen, provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  
  set_menu_actions();
  configure();
  Menu::get().toggle_menu_items();
  
  Menu::get().right_click_line_menu->attach_to_widget(*this);
  Menu::get().right_click_selected_menu->attach_to_widget(*this);
  
  EntryBox::get().signal_hide().connect([]() {
    if(auto view=Notebook::get().get_current_view())
      view->grab_focus();
  });

  Notebook::get().on_change_page=[this](Source::View *view) {
    if(search_entry_shown && EntryBox::get().labels.size()>0) {
      view->update_search_occurrences=[](int number){
        EntryBox::get().labels.begin()->update(0, std::to_string(number));
      };
      view->search_highlight(last_search, case_sensitive_search, regex_search);
    }

    Menu::get().toggle_menu_items();
    
    Directories::get().select(view->file_path);
    
    if(view->full_reparse_needed)
      view->full_reparse();
    else if(view->soft_reparse_needed)
      view->soft_reparse();
    
    Notebook::get().update_status(view);
    
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
        auto end_iter=view->get_iter_at_line_end(iter.get_line());
        view->get_source_buffer()->remove_source_marks(iter, end_iter, "debug_breakpoint");
        Project::current->debug_remove_breakpoint(view->file_path, iter.get_line()+1, view->get_buffer()->get_line_count()+1);
      }
    }
#endif
    EntryBox::get().hide();
    if(auto view=Notebook::get().get_current_view())
      Notebook::get().update_status(view);
    else {
      Notebook::get().clear_status();
      
      Menu::get().toggle_menu_items();
    }
  };
  
  signal_focus_out_event().connect([](GdkEventFocus *event) {
    if(auto view=Notebook::get().get_current_view()) {
      view->hide_tooltips();
      view->hide_dialogs();
    }
    return false;
  });
  
  signal_hide().connect([]{
    while(!Source::View::non_deleted_views.empty()) {
      while(Gtk::Main::events_pending())
        Gtk::Main::iteration(false);
    }
    // TODO 2022 (after Debian Stretch LTS has ended, see issue #354): remove:
    Project::current=nullptr;
  });
  
  Gtk::Settings::get_default()->connect_property_changed("gtk-theme-name", [] {
    Directories::get().update();
    if(auto view=Notebook::get().get_current_view())
      Notebook::get().update_status(view);
  });
  
  about.signal_response().connect([this](int d){
    about.hide();
  });
  
  about.set_logo_icon_name("juci");
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
  auto screen = get_screen();
  if(css_provider_theme)
    Gtk::StyleContext::remove_provider_for_screen(screen, css_provider_theme);
  if(Config::get().window.theme_name.empty()) {
    css_provider_theme=Gtk::CssProvider::create();
    Gtk::Settings::get_default()->property_gtk_application_prefer_dark_theme()=(Config::get().window.theme_variant=="dark");
  }
  else
    css_provider_theme=Gtk::CssProvider::get_named(Config::get().window.theme_name, Config::get().window.theme_variant);
  //TODO: add check if theme exists, or else write error to terminal
  Gtk::StyleContext::add_provider_for_screen(screen, css_provider_theme, GTK_STYLE_PROVIDER_PRIORITY_SETTINGS);
  
  auto style_scheme_manager=Source::StyleSchemeManager::get_default();
  if(css_provider_tooltips)
    Gtk::StyleContext::remove_provider_for_screen(screen, css_provider_tooltips);
  else
    css_provider_tooltips=Gtk::CssProvider::create();
  Glib::RefPtr<Gsv::Style> style;
  if(Config::get().source.style.size()>0) {
    auto scheme = style_scheme_manager->get_scheme(Config::get().source.style);
    if(scheme)
      style = scheme->get_style("def:note");
    else {
      Terminal::get().print("Error: Could not find gtksourceview style: "+Config::get().source.style+'\n', true);
    }
  }
  auto foreground_value = style && style->property_foreground_set() ? style->property_foreground().get_value() : get_style_context()->get_color().to_string();
  auto background_value = style && style->property_background_set() ? style->property_background().get_value() : get_style_context()->get_background_color().to_string();
#if GTK_VERSION_GE(3, 20)
  css_provider_tooltips->load_from_data(".juci_tooltip_box {background-color: "+background_value+";}"
                                        ".juci_tooltip_text_view text {color: "+foreground_value+";background-color: "+background_value+";}");
#else
  css_provider_tooltips->load_from_data(".juci_tooltip_box {background-color: "+background_value+";}"
                                        ".juci_tooltip_text_view *:not(:selected) {color: "+foreground_value+";background-color: "+background_value+";}");
#endif
  get_style_context()->add_provider_for_screen(screen, css_provider_tooltips, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  
  Menu::get().set_keys();
  Terminal::get().configure();
  Directories::get().update();
  if(auto view=Notebook::get().get_current_view())
    Notebook::get().update_status(view);
}

void Window::set_menu_actions() {
  auto &menu = Menu::get();
  
  menu.add_action("about", [this]() {
    about.show();
    about.present();
  });
  menu.add_action("preferences", []() {
    Notebook::get().open(Config::get().home_juci_path/"config"/"config.json");
  });
  menu.add_action("quit", [this]() {
    close();
  });
  
  menu.add_action("file_new_file", []() {
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
          if(Directories::get().path!="")
            Directories::get().on_save_file(path);
          Terminal::get().print("New file "+path.string()+" created.\n");
        }
        else
          Terminal::get().print("Error: could not create new file "+path.string()+".\n", true);
      }
    }
  });
  menu.add_action("file_new_folder", []() {
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
  menu.add_action("file_new_project_c", []() {
    boost::filesystem::path project_path = Dialog::new_folder(Notebook::get().get_current_folder());
    if(project_path!="") {
      auto project_name=project_path.filename().string();
      for(auto &chr: project_name) {
        if(chr==' ')
          chr='_';
      }
      auto cmakelists_path=project_path;
      cmakelists_path/="CMakeLists.txt";
      auto c_main_path=project_path;
      c_main_path/="main.c";
      if(boost::filesystem::exists(cmakelists_path)) {
        Terminal::get().print("Error: "+cmakelists_path.string()+" already exists.\n", true);
        return;
      }
      if(boost::filesystem::exists(c_main_path)) {
        Terminal::get().print("Error: "+c_main_path.string()+" already exists.\n", true);
        return;
      }
      std::string cmakelists="cmake_minimum_required(VERSION 2.8)\n\nproject("+project_name+")\n\nset(CMAKE_C_FLAGS \"${CMAKE_C_FLAGS} -std=c11 -Wall -Wextra\")\n\nadd_executable("+project_name+" main.c)\n";
      std::string c_main="#include <stdio.h>\n\nint main() {\n  printf(\"Hello World!\\n\");\n}\n";
      if(filesystem::write(cmakelists_path, cmakelists) && filesystem::write(c_main_path, c_main)) {
        Directories::get().open(project_path);
        Notebook::get().open(c_main_path);
        Directories::get().update();
        Terminal::get().print("C project "+project_name+" created.\n");
      }
      else
        Terminal::get().print("Error: Could not create project "+project_path.string()+"\n", true);
    }
  });
  menu.add_action("file_new_project_cpp", []() {
    boost::filesystem::path project_path = Dialog::new_folder(Notebook::get().get_current_folder());
    if(project_path!="") {
      auto project_name=project_path.filename().string();
      for(auto &chr: project_name) {
        if(chr==' ')
          chr='_';
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
      std::string cmakelists="cmake_minimum_required(VERSION 2.8)\n\nproject("+project_name+")\n\nset(CMAKE_CXX_FLAGS \"${CMAKE_CXX_FLAGS} -std=c++1y -Wall -Wextra\")\n\nadd_executable("+project_name+" main.cpp)\n";
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
  
  menu.add_action("file_open_file", []() {
    auto folder_path=Notebook::get().get_current_folder();
    if(auto view=Notebook::get().get_current_view()) {
      if(!Directories::get().path.empty() && !filesystem::file_in_path(view->file_path, Directories::get().path))
        folder_path=view->file_path.parent_path();
    }
    auto path=Dialog::open_file(folder_path);
    if(path!="")
      Notebook::get().open(path);
  });
  menu.add_action("file_open_folder", []() {
    auto path = Dialog::open_folder(Notebook::get().get_current_folder());
    if (path!="" && boost::filesystem::exists(path))
      Directories::get().open(path);
  });
  
  menu.add_action("file_reload_file", []() {
    if(auto view=Notebook::get().get_current_view()) {
      if(boost::filesystem::exists(view->file_path)) {
        std::ifstream can_read(view->file_path.string());
        if(!can_read) {
          Terminal::get().print("Error: could not read "+view->file_path.string()+"\n", true);
          return;
        }
        can_read.close();
      }
      else {
        Terminal::get().print("Error: "+view->file_path.string()+" does not exist\n", true);
        return;
      }
      
      if(view->load())
        view->full_reparse();
    }
  });
  
  menu.add_action("file_save", [this]() {
    if(auto view=Notebook::get().get_current_view()) {
      if(Notebook::get().save_current()) {
        if(view->file_path==Config::get().home_juci_path/"config"/"config.json") {
          configure();
          for(size_t c=0;c<Notebook::get().size();c++) {
            Notebook::get().get_view(c)->configure();
            Notebook::get().configure(c);
          }
        }
      }
    }
  });
  menu.add_action("file_save_as", []() {
    if(auto view=Notebook::get().get_current_view()) {
      auto path = Dialog::save_file_as(view->file_path);
      if(path!="") {
        std::ofstream file(path, std::ofstream::binary);
        if(file) {
          file << view->get_buffer()->get_text().raw();
          file.close();
          if(Directories::get().path!="")
            Directories::get().update();
          Notebook::get().open(path);
          Terminal::get().print("File saved to: " + Notebook::get().get_current_view()->file_path.string()+"\n");
        }
        else
          Terminal::get().print("Error saving file\n", true);
      }
    }
  });
  
  menu.add_action("file_print", [this]() {
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
  
  menu.add_action("edit_undo", []() {
    if(auto view=Notebook::get().get_current_view()) {
      auto undo_manager = view->get_source_buffer()->get_undo_manager();
      if (undo_manager->can_undo()) {
        view->disable_spellcheck=true;
        undo_manager->undo();
        view->disable_spellcheck=false;
        view->hide_tooltips();
        view->scroll_to(view->get_buffer()->get_insert());
      }
    }
  });
  menu.add_action("edit_redo", []() {
    if(auto view=Notebook::get().get_current_view()) {
      auto undo_manager = view->get_source_buffer()->get_undo_manager();
      if(undo_manager->can_redo()) {
        view->disable_spellcheck=true;
        undo_manager->redo();
        view->disable_spellcheck=false;
        view->hide_tooltips();
        view->scroll_to(view->get_buffer()->get_insert());
      }
    }
  });
  
  menu.add_action("edit_cut", [this]() {
    // Return if a shown tooltip has selected text
    for(auto tooltip: Tooltips::shown_tooltips) {
      auto buffer=tooltip->text_buffer;
      if(buffer && buffer->get_has_selection())
        return;
    }
    
    auto widget=get_focus();
    if(auto entry=dynamic_cast<Gtk::Entry*>(widget))
      entry->cut_clipboard();
    else if(auto view=dynamic_cast<Gtk::TextView*>(widget)) {
      if(!view->get_editable())
        return;
      auto source_view=dynamic_cast<Source::View*>(view);
      if(source_view)
        source_view->disable_spellcheck=true;
      if(!view->get_buffer()->get_has_selection()) {
        auto start=view->get_buffer()->get_iter_at_line(view->get_buffer()->get_insert()->get_iter().get_line());
        auto end=start;
        if(!end.ends_line())
          end.forward_to_line_end();
        end.forward_char();
        if(!end.starts_line()) // In case of \r\n
          end.forward_char();
        Gtk::Clipboard::get()->set_text(view->get_buffer()->get_text(start, end));
        view->get_buffer()->erase(start, end);
      }
      else
        view->get_buffer()->cut_clipboard(Gtk::Clipboard::get());
      if(source_view)
        source_view->disable_spellcheck=false;
    }
  });
  menu.add_action("edit_copy", [this]() {
    // Copy from a tooltip if it has selected text
    for(auto tooltip: Tooltips::shown_tooltips) {
      auto buffer=tooltip->text_buffer;
      if(buffer && buffer->get_has_selection()) {
        buffer->copy_clipboard(Gtk::Clipboard::get());
        return;
      }
    }
    
    auto widget=get_focus();
    if(auto entry=dynamic_cast<Gtk::Entry*>(widget))
      entry->copy_clipboard();
    else if(auto view=dynamic_cast<Gtk::TextView*>(widget)) {
      if(!view->get_buffer()->get_has_selection()) {
        auto start=view->get_buffer()->get_iter_at_line(view->get_buffer()->get_insert()->get_iter().get_line());
        auto end=start;
        if(!end.ends_line())
          end.forward_to_line_end();
        end.forward_char();
        if(!end.starts_line()) // In case of \r\n
          end.forward_char();
        Gtk::Clipboard::get()->set_text(view->get_buffer()->get_text(start, end));
      }
      else
        view->get_buffer()->copy_clipboard(Gtk::Clipboard::get());
    }
  });
  menu.add_action("edit_paste", [this]() {
    auto widget=get_focus();
    if(auto entry=dynamic_cast<Gtk::Entry*>(widget))
      entry->paste_clipboard();
    else if(auto view=dynamic_cast<Gtk::TextView*>(widget)) {
      auto source_view=dynamic_cast<Source::View*>(view);
      if(source_view) {
        source_view->disable_spellcheck=true;
        source_view->paste();
        source_view->disable_spellcheck=false;
        source_view->hide_tooltips();
      }
      else if(view->get_editable())
        view->get_buffer()->paste_clipboard(Gtk::Clipboard::get());
    }
  });
  
  menu.add_action("edit_find", [this]() {
    search_and_replace_entry();
  });
  
  menu.add_action("source_spellcheck", []() {
    if(auto view=Notebook::get().get_current_view()) {
      view->remove_spellcheck_errors();
      view->spellcheck();
    }
  });
  menu.add_action("source_spellcheck_clear", []() {
    if(auto view=Notebook::get().get_current_view())
      view->remove_spellcheck_errors();
  });
  menu.add_action("source_spellcheck_next_error", []() {
    if(auto view=Notebook::get().get_current_view())
      view->goto_next_spellcheck_error();
  });
  
  menu.add_action("source_git_next_diff", []() {
    if(auto view=Notebook::get().get_current_view())
      view->git_goto_next_diff();
  });
  menu.add_action("source_git_show_diff", []() {
    if(auto view=Notebook::get().get_current_view()) {
      auto diff_details=view->git_get_diff_details();
      if(diff_details.empty())
        return;
      std::string postfix;
      if(diff_details.size()>2) {
        size_t pos=diff_details.find("@@", 2);
        if(pos!=std::string::npos)
          postfix=diff_details.substr(0, pos+2);
      }
      Notebook::get().open(view->file_path.string()+postfix+".diff");
      if(auto new_view=Notebook::get().get_current_view()) {
        if(new_view->get_buffer()->get_text().empty()) {
          new_view->get_source_buffer()->begin_not_undoable_action();
          new_view->get_buffer()->set_text(diff_details);
          new_view->get_source_buffer()->end_not_undoable_action();
          new_view->get_buffer()->set_modified(false);
        }
      }
    }
  });
  
  menu.add_action("source_indentation_set_buffer_tab", [this]() {
    set_tab_entry();
  });
  menu.add_action("source_indentation_auto_indent_buffer", []() {
    auto view=Notebook::get().get_current_view();
    if(view && view->format_style) {
      view->disable_spellcheck=true;
      view->format_style(true);
      view->disable_spellcheck=false;
      view->hide_tooltips();
    }
  });
  
  menu.add_action("source_goto_line", [this]() {
    goto_line_entry();
  });
  menu.add_action("source_center_cursor", []() {
    if(auto view=Notebook::get().get_current_view())
      view->scroll_to_cursor_delayed(view, true, false);
  });
  menu.add_action("source_cursor_history_back", []() {
    if(Notebook::get().cursor_locations.size()==0)
      return;
    if(Notebook::get().current_cursor_location==static_cast<size_t>(-1))
      Notebook::get().current_cursor_location=Notebook::get().cursor_locations.size()-1;
    
    auto cursor_location=&Notebook::get().cursor_locations.at(Notebook::get().current_cursor_location);
    // Move to current position if current position's view is not current view
    // (in case one is looking at a new file but has not yet placed the cursor within the file)
    if(cursor_location->view!=Notebook::get().get_current_view())
      Notebook::get().open(cursor_location->view->file_path);
    else {
      if(Notebook::get().cursor_locations.size()<=1)
        return;
      if(Notebook::get().current_cursor_location==0)
        return;
      
      --Notebook::get().current_cursor_location;
      cursor_location=&Notebook::get().cursor_locations.at(Notebook::get().current_cursor_location);
      if(Notebook::get().get_current_view()!=cursor_location->view)
        Notebook::get().open(cursor_location->view->file_path);
    }
    Notebook::get().disable_next_update_cursor_locations=true;
    cursor_location->view->get_buffer()->place_cursor(cursor_location->mark->get_iter());
    cursor_location->view->scroll_to_cursor_delayed(cursor_location->view, true, false);
  });
  menu.add_action("source_cursor_history_forward", []() {
    if(Notebook::get().cursor_locations.size()<=1)
      return;
    if(Notebook::get().current_cursor_location==static_cast<size_t>(-1))
      Notebook::get().current_cursor_location=Notebook::get().cursor_locations.size()-1;
    if(Notebook::get().current_cursor_location==Notebook::get().cursor_locations.size()-1)
      return;
    
    ++Notebook::get().current_cursor_location;
    auto &cursor_location=Notebook::get().cursor_locations.at(Notebook::get().current_cursor_location);
    if(Notebook::get().get_current_view()!=cursor_location.view)
      Notebook::get().open(cursor_location.view->file_path);
    Notebook::get().disable_next_update_cursor_locations=true;
    cursor_location.view->get_buffer()->place_cursor(cursor_location.mark->get_iter());
    cursor_location.view->scroll_to_cursor_delayed(cursor_location.view, true, false);
  });
  
  menu.add_action("source_show_completion", [] {
    if(auto view=Notebook::get().get_current_view()) {
      if(view->non_interactive_completion)
        view->non_interactive_completion();
      else
        g_signal_emit_by_name(view->gobj(), "show-completion");
    }
  });
  
  menu.add_action("source_find_symbol", []() {
    auto project=Project::create();
    project->show_symbols();
  });
  
  menu.add_action("source_find_file", []() {
    auto view = Notebook::get().get_current_view();

    boost::filesystem::path search_path;
    if(view)
      search_path=view->file_path.parent_path();
    else if(!Directories::get().path.empty())
      search_path=Directories::get().path;
    else {
      boost::system::error_code ec;
      search_path=boost::filesystem::current_path(ec);
      if(ec) {
        Terminal::get().print("Error: could not find current path\n", true);
        return;
      }
    }
    auto build=Project::Build::create(search_path);
    auto project_path=build->project_path;
    boost::filesystem::path default_path, debug_path;
    if(!project_path.empty()) {
      search_path=project_path;
      default_path = build->get_default_path();
      debug_path = build->get_debug_path();
    }
  
    if(view) {
      auto dialog_iter=view->get_iter_for_dialog();
      SelectionDialog::create(view, view->get_buffer()->create_mark(dialog_iter), true, true);
    }
    else
      SelectionDialog::create(true, true);
    
    std::unordered_set<std::string> buffer_paths;
    for(auto view: Notebook::get().get_views())
      buffer_paths.emplace(view->file_path.string());
    
    std::vector<boost::filesystem::path> paths;
    // populate with all files in search_path
    for (boost::filesystem::recursive_directory_iterator iter(search_path), end; iter != end; iter++) {
      auto path = iter->path();
      // ignore folders
      if (!boost::filesystem::is_regular_file(path)) {
        if(path==default_path || path==debug_path || path.filename()==".git")
          iter.no_push();
        continue;
      }
        
      // remove project base path
      auto row_str = filesystem::get_relative_path(path, search_path).string();
      if(buffer_paths.count(path.string()))
        row_str="<b>"+row_str+"</b>";
      paths.emplace_back(path);
      SelectionDialog::get()->add_row(row_str);
    }
    
    if(paths.empty()) {
      Info::get().print("No files found in current project");
      return;
    }
  
    SelectionDialog::get()->on_select=[paths=std::move(paths)](unsigned int index, const std::string &text, bool hide_window) {
      if(index>=paths.size())
        return;
      Notebook::get().open(paths[index]);
      if (auto view=Notebook::get().get_current_view())
        view->hide_tooltips();
    };
  
    if(view)
      view->hide_tooltips();
    SelectionDialog::get()->show();
  
  });
  
  menu.add_action("source_comments_toggle", []() {
    if(auto view=Notebook::get().get_current_view()) {
      if(view->toggle_comments) {
        view->toggle_comments();
      }
    }
  });
  menu.add_action("source_comments_add_documentation", []() {
    if(auto view=Notebook::get().get_current_view()) {
      if(view->get_documentation_template) {
        auto documentation_template=view->get_documentation_template();
        auto offset=std::get<0>(documentation_template);
        if(offset) {
          if(!boost::filesystem::is_regular_file(offset.file_path))
            return;
          Notebook::get().open(offset.file_path);
          auto view=Notebook::get().get_current_view();
          auto iter=view->get_iter_at_line_pos(offset.line, offset.index);
          view->get_buffer()->insert(iter, std::get<1>(documentation_template));
          iter=view->get_iter_at_line_pos(offset.line, offset.index);
          iter.forward_chars(std::get<2>(documentation_template));
          view->get_buffer()->place_cursor(iter);
          view->scroll_to_cursor_delayed(view, true, false);
        }
      }
    }
  });
  menu.add_action("source_find_documentation", []() {
    if(auto view=Notebook::get().get_current_view()) {
      if(view->get_token_data) {
        auto data=view->get_token_data();
        std::string url;
        if(data.size()==1)
          url=data[0];
        else if(data.size()>1) {
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
              
              if(query!=documentation_search->second.queries.end())
                url=query->second+token_query;
            }
          }
        }
        if(!url.empty()) {
#ifdef __APPLE__
          Terminal::get().process("open \""+url+"\"");
#else
          GError* error=nullptr;
#if GTK_VERSION_GE(3, 22)
          gtk_show_uri_on_window(nullptr, url.c_str(), GDK_CURRENT_TIME, &error);
#else
          gtk_show_uri(nullptr, url.c_str(), GDK_CURRENT_TIME, &error);
#endif
          g_clear_error(&error);
#endif
        }
      }
    }
  });
  
  menu.add_action("source_goto_declaration", []() {
    if(auto view=Notebook::get().get_current_view()) {
      if(view->get_declaration_location) {
        auto location=view->get_declaration_location();
        if(location) {
          if(!boost::filesystem::is_regular_file(location.file_path))
            return;
          Notebook::get().open(location.file_path);
          auto view=Notebook::get().get_current_view();
          auto line=static_cast<int>(location.line);
          auto index=static_cast<int>(location.index);
          view->place_cursor_at_line_pos(line, index);
          view->scroll_to_cursor_delayed(view, true, false);
        }
      }
    }
  });
  menu.add_action("source_goto_type_declaration", []() {
    if(auto view=Notebook::get().get_current_view()) {
      if(view->get_type_declaration_location) {
        auto location=view->get_type_declaration_location();
        if(location) {
          if(!boost::filesystem::is_regular_file(location.file_path))
            return;
          Notebook::get().open(location.file_path);
          auto view=Notebook::get().get_current_view();
          auto line=static_cast<int>(location.line);
          auto index=static_cast<int>(location.index);
          view->place_cursor_at_line_pos(line, index);
          view->scroll_to_cursor_delayed(view, true, false);
        }
      }
    }
  });
  auto goto_selected_location=[](Source::View *view, const std::vector<Source::Offset> &locations) {
    if(!locations.empty()) {
      auto dialog_iter=view->get_iter_for_dialog();
      SelectionDialog::create(view, view->get_buffer()->create_mark(dialog_iter), true, true);
      std::vector<Source::Offset> rows;
      auto project_path=Project::Build::create(view->file_path)->project_path;
      if(project_path.empty()) {
        if(!Directories::get().path.empty())
          project_path=Directories::get().path;
        else
          project_path=view->file_path.parent_path();
      }
      for(auto &location: locations) {
        auto path=filesystem::get_relative_path(filesystem::get_normal_path(location.file_path), project_path);
        if(path.empty())
          path=location.file_path.filename();
        auto row=path.string()+":"+std::to_string(location.line+1);
        rows.emplace_back(location);
        SelectionDialog::get()->add_row(row);
      }
      
      if(rows.size()==0)
        return;
      else if(rows.size()==1) {
        auto location=*rows.begin();
        if(!boost::filesystem::is_regular_file(location.file_path))
          return;
        Notebook::get().open(location.file_path);
        auto view=Notebook::get().get_current_view();
        auto line=static_cast<int>(location.line);
        auto index=static_cast<int>(location.index);
        view->place_cursor_at_line_pos(line, index);
        view->scroll_to_cursor_delayed(view, true, false);
        return;
      }
      SelectionDialog::get()->on_select=[rows=std::move(rows)](unsigned int index, const std::string &text, bool hide_window) {
        if(index>=rows.size())
          return;
        auto location=rows[index];
        if(!boost::filesystem::is_regular_file(location.file_path))
          return;
        Notebook::get().open(location.file_path);
        auto view=Notebook::get().get_current_view();
        view->place_cursor_at_line_pos(location.line, location.index);
        view->scroll_to_cursor_delayed(view, true, false);
        view->hide_tooltips();
      };
      view->hide_tooltips();
      SelectionDialog::get()->show();
    }
  };
  menu.add_action("source_goto_implementation", [goto_selected_location]() {
    if(auto view=Notebook::get().get_current_view()) {
      if(view->get_implementation_locations)
        goto_selected_location(view, view->get_implementation_locations());
    }
  });
  menu.add_action("source_goto_declaration_or_implementation", [goto_selected_location]() {
    if(auto view=Notebook::get().get_current_view()) {
      if(view->get_declaration_or_implementation_locations)
        goto_selected_location(view, view->get_declaration_or_implementation_locations());
    }
  });

  menu.add_action("source_goto_usage", []() {
    if(auto view=Notebook::get().get_current_view()) {
      if(view->get_usages) {
        auto usages=view->get_usages();
        if(!usages.empty()) {
          auto dialog_iter=view->get_iter_for_dialog();
          SelectionDialog::create(view, view->get_buffer()->create_mark(dialog_iter), true, true);
          std::vector<Source::Offset> rows;
          
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
            rows.emplace_back(usage.first);
            SelectionDialog::get()->add_row(row);
            
            //Set dialog cursor to the last row if the textview cursor is at the same line
            if(current_page &&
               iter.get_line()==static_cast<int>(usage.first.line) && iter.get_line_index()>=static_cast<int>(usage.first.index)) {
              SelectionDialog::get()->set_cursor_at_last_row();
            }
          }
          
          if(rows.size()==0)
            return;
          SelectionDialog::get()->on_select=[rows=std::move(rows)](unsigned int index, const std::string &text, bool hide_window) {
            if(index>=rows.size())
              return;
            auto offset=rows[index];
            if(!boost::filesystem::is_regular_file(offset.file_path))
              return;
            Notebook::get().open(offset.file_path);
            auto view=Notebook::get().get_current_view();
            view->place_cursor_at_line_pos(offset.line, offset.index);
            view->scroll_to_cursor_delayed(view, true, false);
            view->hide_tooltips();
          };
          view->hide_tooltips();
          SelectionDialog::get()->show();
        }
      }
    }
  });
  menu.add_action("source_goto_method", []() {
    if(auto view=Notebook::get().get_current_view()) {
      if(view->get_methods) {
        auto methods=Notebook::get().get_current_view()->get_methods();
        if(!methods.empty()) {
          auto dialog_iter=view->get_iter_for_dialog();
          SelectionDialog::create(view, view->get_buffer()->create_mark(dialog_iter), true, true);
          std::vector<Source::Offset> rows;
          auto iter=view->get_buffer()->get_insert()->get_iter();
          for(auto &method: methods) {
            rows.emplace_back(method.first);
            SelectionDialog::get()->add_row(method.second);
            if(iter.get_line()>=static_cast<int>(method.first.line))
              SelectionDialog::get()->set_cursor_at_last_row();
          }
          SelectionDialog::get()->on_select=[view, rows=std::move(rows)](unsigned int index, const std::string &text, bool hide_window) {
            if(index>=rows.size())
              return;
            auto offset=rows[index];
            view->get_buffer()->place_cursor(view->get_iter_at_line_pos(offset.line, offset.index));
            view->scroll_to(view->get_buffer()->get_insert(), 0.0, 1.0, 0.5);
            view->hide_tooltips();
          };
          view->hide_tooltips();
          SelectionDialog::get()->show();
        }
      }
    }
  });
  menu.add_action("source_rename", [this]() {
    rename_token_entry();
  });
  menu.add_action("source_implement_method", []() {
    const static std::string button_text="Insert Method Implementation";
    
    if(auto view=Notebook::get().get_current_view()) {
      if(view->get_method) {
        auto iter=view->get_buffer()->get_insert()->get_iter();
        if(!EntryBox::get().buttons.empty() && EntryBox::get().buttons.back().get_label()==button_text &&
           iter.ends_line() && iter.starts_line()) {
          EntryBox::get().buttons.back().activate();
          return;
        }
        auto method=view->get_method();
        if(method.empty())
          return;
        
        EntryBox::get().clear();
        EntryBox::get().labels.emplace_back();
        EntryBox::get().labels.back().set_text(method);
        EntryBox::get().buttons.emplace_back(button_text, [method=std::move(method)](){
          if(auto view=Notebook::get().get_current_view()) {
            view->get_buffer()->insert_at_cursor(method);
            EntryBox::get().clear();
          }
        });
        EntryBox::get().show();
      }
    }
  });
  
  menu.add_action("source_goto_next_diagnostic", []() {
    if(auto view=Notebook::get().get_current_view()) {
      if(view->goto_next_diagnostic) {
        view->goto_next_diagnostic();
      }
    }
  });
  menu.add_action("source_apply_fix_its", []() {
    if(auto view=Notebook::get().get_current_view()) {
      if(view->get_fix_its) {
        auto buffer=view->get_buffer();
        auto fix_its=view->get_fix_its();
        std::vector<std::pair<Glib::RefPtr<Gtk::TextMark>, Glib::RefPtr<Gtk::TextMark> > > fix_it_marks;
        for(auto &fix_it: fix_its) {
          auto start_iter=view->get_iter_at_line_pos(fix_it.offsets.first.line, fix_it.offsets.first.index);
          auto end_iter=view->get_iter_at_line_pos(fix_it.offsets.second.line, fix_it.offsets.second.index);
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
  
  menu.add_action("project_set_run_arguments", []() {
    auto project=Project::create();
    auto run_arguments=project->get_run_arguments();
    if(run_arguments.second.empty())
      return;
    
    EntryBox::get().clear();
    EntryBox::get().labels.emplace_back();
    auto label_it=EntryBox::get().labels.begin();
    label_it->update=[label_it](int state, const std::string& message){
      label_it->set_text("Synopsis: [environment_variable=value]... executable [argument]...\nSet empty to let juCi++ deduce executable.");
    };
    label_it->update(0, "");
    EntryBox::get().entries.emplace_back(run_arguments.second, [run_arguments_first=std::move(run_arguments.first)](const std::string &content){
      Project::run_arguments[run_arguments_first]=content;
      EntryBox::get().hide();
    }, 50);
    auto entry_it=EntryBox::get().entries.begin();
    entry_it->set_placeholder_text("Project: Set Run Arguments");
    EntryBox::get().buttons.emplace_back("Project: set run arguments", [entry_it](){
      entry_it->activate();
    });
    EntryBox::get().show();
  });
  menu.add_action("project_compile_and_run", []() {
    if(Project::compiling || Project::debugging) {
      Info::get().print("Compile or debug in progress");
      return;
    }
    
    Project::current=Project::create();
    
    if(Config::get().project.save_on_compile_or_run)
      Project::save_files(Project::current->build->project_path);
    
    Project::current->compile_and_run();
  });
  menu.add_action("project_compile", []() {
    if(Project::compiling || Project::debugging) {
      Info::get().print("Compile or debug in progress");
      return;
    }
            
    Project::current=Project::create();
    
    if(Config::get().project.save_on_compile_or_run)
      Project::save_files(Project::current->build->project_path);
    
    Project::current->compile();
  });
  menu.add_action("project_recreate_build", []() {
    if(Project::compiling || Project::debugging) {
      Info::get().print("Compile or debug in progress");
      return;
    }
    
    Project::current=Project::create();

    Project::current->recreate_build();
  });
  
  menu.add_action("project_run_command", [this]() {
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
  
        Terminal::get().async_process(content, run_path, [content](int exit_status){
          Terminal::get().async_print(content+" returned: "+std::to_string(exit_status)+'\n');
        });
      }
      EntryBox::get().hide();
    }, 30);
    auto entry_it=EntryBox::get().entries.begin();
    entry_it->set_placeholder_text("Command");
    EntryBox::get().buttons.emplace_back("Run command", [entry_it](){
      entry_it->activate();
    });
    EntryBox::get().show();
  });
  
  menu.add_action("project_kill_last_running", []() {
    Terminal::get().kill_last_async_process();
  });
  menu.add_action("project_force_kill_last_running", []() {
    Terminal::get().kill_last_async_process(true);
  });
  
#ifdef JUCI_ENABLE_DEBUG
  menu.add_action("debug_set_run_arguments", []() {
    auto project=Project::create();
    auto run_arguments=project->debug_get_run_arguments();
    if(run_arguments.second.empty())
      return;
    
    EntryBox::get().clear();
    EntryBox::get().labels.emplace_back();
    auto label_it=EntryBox::get().labels.begin();
    label_it->update=[label_it](int state, const std::string& message){
      label_it->set_text("Synopsis: [environment_variable=value]... executable [argument]...\nSet empty to let juCi++ deduce executable.");
    };
    label_it->update(0, "");
    EntryBox::get().entries.emplace_back(run_arguments.second, [run_arguments_first=std::move(run_arguments.first)](const std::string& content){
      Project::debug_run_arguments[run_arguments_first].arguments=content;
      EntryBox::get().hide();
    }, 50);
    auto entry_it=EntryBox::get().entries.begin();
    entry_it->set_placeholder_text("Debug: Set Run Arguments");
    
    if(auto options=project->debug_get_options()) {
      EntryBox::get().buttons.emplace_back("", [options]() {
        options->show_all();
      });
      EntryBox::get().buttons.back().set_image_from_icon_name("preferences-system");
      EntryBox::get().buttons.back().set_always_show_image(true);
      EntryBox::get().buttons.back().set_tooltip_text("Additional Options");
      options->set_relative_to(EntryBox::get().buttons.back());
    }
    
    EntryBox::get().buttons.emplace_back("Debug: set run arguments", [entry_it](){
      entry_it->activate();
    });
    EntryBox::get().show();
  });
  menu.add_action("debug_start_continue", [](){
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
  menu.add_action("debug_stop", []() {
    if(Project::current)
      Project::current->debug_stop();
  });
  menu.add_action("debug_kill", []() {
    if(Project::current)
      Project::current->debug_kill();
  });
  menu.add_action("debug_step_over", []() {
    if(Project::current)
      Project::current->debug_step_over();
  });
  menu.add_action("debug_step_into", []() {
    if(Project::current)
      Project::current->debug_step_into();
  });
  menu.add_action("debug_step_out", []() {
    if(Project::current)
      Project::current->debug_step_out();
  });
  menu.add_action("debug_backtrace", []() {
    if(Project::current)
      Project::current->debug_backtrace();
  });
  menu.add_action("debug_show_variables", []() {
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
    EntryBox::get().buttons.emplace_back("Run debug command", [entry_it](){
      entry_it->activate();
    });
    EntryBox::get().show();
  });
  menu.add_action("debug_toggle_breakpoint", [](){
    if(auto view=Notebook::get().get_current_view()) {
      if(view->toggle_breakpoint)
        view->toggle_breakpoint(view->get_buffer()->get_insert()->get_iter().get_line());
    }
  });
  menu.add_action("debug_goto_stop", [](){
    if(Project::debugging) {
      if(!Project::debug_stop.first.empty()) {
        Notebook::get().open(Project::debug_stop.first);
        if(auto view=Notebook::get().get_current_view()) {
          int line=Project::debug_stop.second.first;
          int index=Project::debug_stop.second.second;
          view->place_cursor_at_line_index(line, index);
          view->scroll_to_cursor_delayed(view, true, true);
        }
      }
    }
  });
  
  Project::debug_update_status("");
#endif
  
  menu.add_action("window_next_tab", []() {
    Notebook::get().next();
  });
  menu.add_action("window_previous_tab", []() {
    Notebook::get().previous();
  });
  menu.add_action("window_close_tab", []() {
    if(Notebook::get().get_current_view())
      Notebook::get().close_current();
  });
  menu.add_action("window_toggle_split", [] {
    Notebook::get().toggle_split();
  });
  menu.add_action("window_toggle_full_screen", [this] {
    if(this->get_window()->get_state() & Gdk::WindowState::WINDOW_STATE_FULLSCREEN)
      unfullscreen();
    else
      fullscreen();
  });
  menu.add_action("window_toggle_tabs", [] {
    Notebook::get().toggle_tabs();
  });
  menu.add_action("window_clear_terminal", [] {
    Terminal::get().clear();
  });
  
  menu.toggle_menu_items=[] {
    auto &menu = Menu::get();
    auto view=Notebook::get().get_current_view();
    
    menu.actions["file_reload_file"]->set_enabled(view);
    menu.actions["source_spellcheck"]->set_enabled(view);
    menu.actions["source_spellcheck_clear"]->set_enabled(view);
    menu.actions["source_spellcheck_next_error"]->set_enabled(view);
    menu.actions["source_git_next_diff"]->set_enabled(view);
    menu.actions["source_git_show_diff"]->set_enabled(view);
    menu.actions["source_indentation_set_buffer_tab"]->set_enabled(view);
    menu.actions["source_goto_line"]->set_enabled(view);
    menu.actions["source_center_cursor"]->set_enabled(view);
    
    menu.actions["source_indentation_auto_indent_buffer"]->set_enabled(view && view->format_style);
    menu.actions["source_comments_toggle"]->set_enabled(view && view->toggle_comments);
    menu.actions["source_comments_add_documentation"]->set_enabled(view && view->get_documentation_template);
    menu.actions["source_find_documentation"]->set_enabled(view && view->get_token_data);
    menu.actions["source_goto_declaration"]->set_enabled(view && view->get_declaration_location);
    menu.actions["source_goto_type_declaration"]->set_enabled(view && view->get_type_declaration_location);
    menu.actions["source_goto_implementation"]->set_enabled(view && view->get_implementation_locations);
    menu.actions["source_goto_declaration_or_implementation"]->set_enabled(view && view->get_declaration_or_implementation_locations);
    menu.actions["source_goto_usage"]->set_enabled(view && view->get_usages);
    menu.actions["source_goto_method"]->set_enabled(view && view->get_methods);
    menu.actions["source_rename"]->set_enabled(view && view->rename_similar_tokens);
    menu.actions["source_implement_method"]->set_enabled(view && view->get_method);
    menu.actions["source_goto_next_diagnostic"]->set_enabled(view && view->goto_next_diagnostic);
    menu.actions["source_apply_fix_its"]->set_enabled(view && view->get_fix_its);
#ifdef JUCI_ENABLE_DEBUG
    Project::debug_activate_menu_items();
#endif
  };
}

void Window::add_widgets() {
  auto directories_scrolled_window=Gtk::manage(new Gtk::ScrolledWindow());
  directories_scrolled_window->add(Directories::get());
  
  auto notebook_vbox=Gtk::manage(new Gtk::Box(Gtk::Orientation::ORIENTATION_VERTICAL));
  notebook_vbox->pack_start(Notebook::get());
  notebook_vbox->pack_end(EntryBox::get(), Gtk::PACK_SHRINK);
  
  auto terminal_scrolled_window=Gtk::manage(new Gtk::ScrolledWindow());
  terminal_scrolled_window->add(Terminal::get());
  
  int width, height;
  get_default_size(width, height);
  
  auto notebook_and_terminal_vpaned=Gtk::manage(new Gtk::Paned(Gtk::Orientation::ORIENTATION_VERTICAL));
  notebook_and_terminal_vpaned->set_position(static_cast<int>(0.75*height));
  notebook_and_terminal_vpaned->pack1(*notebook_vbox, Gtk::SHRINK);
  notebook_and_terminal_vpaned->pack2(*terminal_scrolled_window, Gtk::SHRINK);
  
  auto hpaned=Gtk::manage(new Gtk::Paned());
  hpaned->set_position(static_cast<int>(0.2*width));
  hpaned->pack1(*directories_scrolled_window, Gtk::SHRINK);
  hpaned->pack2(*notebook_and_terminal_vpaned, Gtk::SHRINK);
  
  auto status_hbox=Gtk::manage(new Gtk::Box());
  status_hbox->set_homogeneous(true);
  status_hbox->pack_start(*Gtk::manage(new Gtk::Box()));
  auto status_right_hbox=Gtk::manage(new Gtk::Box());
  status_right_hbox->pack_end(Notebook::get().status_state, Gtk::PACK_SHRINK);
  auto status_right_overlay=Gtk::manage(new Gtk::Overlay());
  status_right_overlay->add(*status_right_hbox);
  status_right_overlay->add_overlay(Notebook::get().status_diagnostics);
  status_hbox->pack_end(*status_right_overlay);
  
  auto status_overlay=Gtk::manage(new Gtk::Overlay());
  status_overlay->add(*status_hbox);
  auto status_file_info_hbox=Gtk::manage(new Gtk::Box);
  status_file_info_hbox->pack_start(Notebook::get().status_file_path, Gtk::PACK_SHRINK);
  status_file_info_hbox->pack_start(Notebook::get().status_branch, Gtk::PACK_SHRINK);
  status_file_info_hbox->pack_start(Notebook::get().status_location, Gtk::PACK_SHRINK);
  status_overlay->add_overlay(*status_file_info_hbox);
  status_overlay->add_overlay(Project::debug_status_label());
  
  auto vbox=Gtk::manage(new Gtk::Box(Gtk::Orientation::ORIENTATION_VERTICAL));
  vbox->pack_start(*hpaned);
  vbox->pack_start(*status_overlay, Gtk::PACK_SHRINK);
  
  auto overlay_vbox=Gtk::manage(new Gtk::Box(Gtk::Orientation::ORIENTATION_VERTICAL));
  auto overlay_hbox=Gtk::manage(new Gtk::Box());
  overlay_vbox->set_hexpand(false);
  overlay_vbox->set_halign(Gtk::Align::ALIGN_START);
  overlay_vbox->pack_start(Info::get(), Gtk::PACK_SHRINK, 20);
  overlay_hbox->set_hexpand(false);
  overlay_hbox->set_halign(Gtk::Align::ALIGN_END);
  overlay_hbox->pack_end(*overlay_vbox, Gtk::PACK_SHRINK, 20);
  
  auto overlay=Gtk::manage(new Gtk::Overlay());
  overlay->add(*vbox);
  overlay->add_overlay(*overlay_hbox);
  overlay->set_overlay_pass_through(*overlay_hbox, true);
  add(*overlay);
  
  show_all_children();
  Info::get().hide();
  
  //Scroll to end of terminal whenever info is printed
  Terminal::get().signal_size_allocate().connect([terminal_scrolled_window](Gtk::Allocation& allocation){
    auto adjustment=terminal_scrolled_window->get_vadjustment();
    adjustment->set_value(adjustment->get_upper()-adjustment->get_page_size());
    Terminal::get().queue_draw();
  });
  
  EntryBox::get().signal_show().connect([hpaned, notebook_and_terminal_vpaned, notebook_vbox](){
    hpaned->set_focus_chain({notebook_and_terminal_vpaned});
    notebook_and_terminal_vpaned->set_focus_chain({notebook_vbox});
    notebook_vbox->set_focus_chain({&EntryBox::get()});
  });
  EntryBox::get().signal_hide().connect([hpaned, notebook_and_terminal_vpaned, notebook_vbox](){
    hpaned->unset_focus_chain();
    notebook_and_terminal_vpaned->unset_focus_chain();
    notebook_vbox->unset_focus_chain();
  });
}

bool Window::on_key_press_event(GdkEventKey *event) {
  if(event->keyval==GDK_KEY_Escape) {
    EntryBox::get().hide();
  }
#ifdef __APPLE__ //For Apple's Command-left, right, up, down keys
  else if((event->state & GDK_META_MASK)>0 && (event->state & GDK_MOD1_MASK)==0) {
    if(event->keyval==GDK_KEY_Left || event->keyval==GDK_KEY_KP_Left) {
      event->keyval=GDK_KEY_Home;
      event->state=event->state & GDK_SHIFT_MASK;
      event->hardware_keycode=115;
    }
    else if(event->keyval==GDK_KEY_Right || event->keyval==GDK_KEY_KP_Right) {
      event->keyval=GDK_KEY_End;
      event->state=event->state & GDK_SHIFT_MASK;
      event->hardware_keycode=119;
    }
    else if(event->keyval==GDK_KEY_Up || event->keyval==GDK_KEY_KP_Up) {
      event->keyval=GDK_KEY_Home;
      event->state=event->state & GDK_SHIFT_MASK;
      event->state+=GDK_CONTROL_MASK;
      event->hardware_keycode=115;
    }
    else if(event->keyval==GDK_KEY_Down || event->keyval==GDK_KEY_KP_Down) {
      event->keyval=GDK_KEY_End;
      event->state=event->state & GDK_SHIFT_MASK;
      event->state+=GDK_CONTROL_MASK;
      event->hardware_keycode=119;
    }
  }
#endif

  if(SelectionDialog::get() && SelectionDialog::get()->is_visible()) {
    if(SelectionDialog::get()->on_key_press(event))
      return true;
  }

  return Gtk::ApplicationWindow::on_key_press_event(event);
}

bool Window::on_delete_event(GdkEventAny *event) {
  save_session();
  
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
  EntryBox::get().entries.emplace_back(last_search, [](const std::string& content){
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
  search_entry_it->signal_key_press_event().connect([](GdkEventKey* event){
    if((event->keyval==GDK_KEY_Return || event->keyval==GDK_KEY_KP_Enter) && (event->state&GDK_SHIFT_MASK)>0) {
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

  EntryBox::get().entries.emplace_back(last_replace, [](const std::string &content){
    if(auto view=Notebook::get().get_current_view())
      view->replace_forward(content);
  });
  auto replace_entry_it=EntryBox::get().entries.begin();
  replace_entry_it++;
  replace_entry_it->set_placeholder_text("Replace");
  replace_entry_it->signal_key_press_event().connect([replace_entry_it](GdkEventKey* event){
    if((event->keyval==GDK_KEY_Return || event->keyval==GDK_KEY_KP_Enter) && (event->state&GDK_SHIFT_MASK)>0) {
      if(auto view=Notebook::get().get_current_view())
        view->replace_backward(replace_entry_it->get_text());
    }
    return false;
  });
  replace_entry_it->signal_changed().connect([this, replace_entry_it](){
    last_replace=replace_entry_it->get_text();
  });
  
  EntryBox::get().buttons.emplace_back("↑", [](){
    if(auto view=Notebook::get().get_current_view())
        view->search_backward();
  });
  EntryBox::get().buttons.back().set_tooltip_text("Find Previous\n\nShortcut: Shift+Enter in the Find entry field");
  EntryBox::get().buttons.emplace_back("⇄", [replace_entry_it](){
    if(auto view=Notebook::get().get_current_view()) {
      view->replace_forward(replace_entry_it->get_text());
    }
  });
  EntryBox::get().buttons.back().set_tooltip_text("Replace Next\n\nShortcut: Enter in the Replace entry field");
  EntryBox::get().buttons.emplace_back("↓", [](){
    if(auto view=Notebook::get().get_current_view())
      view->search_forward();
  });
  EntryBox::get().buttons.back().set_tooltip_text("Find Next\n\nShortcut: Enter in the Find entry field");
  EntryBox::get().buttons.emplace_back("Replace All", [replace_entry_it](){
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
    
    const auto activate_function=[entry_tab_char_it, entry_tab_size_it, label_it](const std::string& content){
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
    
    EntryBox::get().buttons.emplace_back("Set tab in current buffer", [entry_tab_char_it](){
      entry_tab_char_it->activate();
    });
    
    EntryBox::get().show();
  }
}

void Window::goto_line_entry() {
  EntryBox::get().clear();
  if(Notebook::get().get_current_view()) {
    EntryBox::get().entries.emplace_back("", [](const std::string& content){
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
    EntryBox::get().buttons.emplace_back("Go to line", [entry_it](){
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
        EntryBox::get().entries.emplace_back(*spelling, [view, spelling, iter=view->get_buffer()->get_insert()->get_iter()](const std::string& content){
          //TODO: gtk needs a way to check if iter is valid without dumping g_error message
          //iter->get_buffer() will print such a message, but no segfault will occur
          if(Notebook::get().get_current_view()==view && content!=*spelling && iter.get_buffer() && view->get_buffer()->get_insert()->get_iter()==iter)
            view->rename_similar_tokens(content);
          else
            Info::get().print("Operation canceled");
          EntryBox::get().hide();
        });
        auto entry_it=EntryBox::get().entries.begin();
        entry_it->set_placeholder_text("New name");
        EntryBox::get().buttons.emplace_back("Rename", [entry_it](){
          entry_it->activate();
        });
        EntryBox::get().show();
      }
    }
  }
}

void Window::save_session() {
  try {
    boost::property_tree::ptree root_pt;
    root_pt.put("folder", Directories::get().path.string());
    
    boost::property_tree::ptree files_pt;
    for(auto &notebook_view: Notebook::get().get_notebook_views()) {
      boost::property_tree::ptree file_pt;
      file_pt.put("path", notebook_view.second->file_path.string());
      file_pt.put("notebook", notebook_view.first);
      auto iter=notebook_view.second->get_buffer()->get_insert()->get_iter();
      file_pt.put("line", iter.get_line());
      file_pt.put("line_offset", iter.get_line_offset());
      files_pt.push_back(std::make_pair("", file_pt));
    }
    root_pt.add_child("files", files_pt);
    
    boost::property_tree::ptree current_file_pt;
    if(auto view=Notebook::get().get_current_view()) {
      current_file_pt.put("path", view->file_path.string());
      auto iter=view->get_buffer()->get_insert()->get_iter();
      current_file_pt.put("line", iter.get_line());
      current_file_pt.put("line_offset", iter.get_line_offset());
    }
    std::string current_path;
    if(auto view=Notebook::get().get_current_view())
      current_path=view->file_path.string();
    root_pt.put("current_file", current_path);
    
    boost::property_tree::ptree run_arguments_pt;
    for(auto &run_argument: Project::run_arguments) {
      if(run_argument.second.empty())
        continue;
      boost::system::error_code ec;
      if(boost::filesystem::exists(run_argument.first, ec) && boost::filesystem::is_directory(run_argument.first, ec)) {
        boost::property_tree::ptree run_argument_pt;
        run_argument_pt.put("path", run_argument.first);
        run_argument_pt.put("arguments", run_argument.second);
        run_arguments_pt.push_back(std::make_pair("", run_argument_pt));
      }
    }
    root_pt.add_child("run_arguments", run_arguments_pt);
    
    boost::property_tree::ptree debug_run_arguments_pt;
    for(auto &debug_run_argument: Project::debug_run_arguments) {
      if(debug_run_argument.second.arguments.empty() && !debug_run_argument.second.remote_enabled && debug_run_argument.second.remote_host_port.empty())
        continue;
      boost::system::error_code ec;
      if(boost::filesystem::exists(debug_run_argument.first, ec) && boost::filesystem::is_directory(debug_run_argument.first, ec)) {
        boost::property_tree::ptree debug_run_argument_pt;
        debug_run_argument_pt.put("path", debug_run_argument.first);
        debug_run_argument_pt.put("arguments", debug_run_argument.second.arguments);
        debug_run_argument_pt.put("remote_enabled", debug_run_argument.second.remote_enabled);
        debug_run_argument_pt.put("remote_host_port", debug_run_argument.second.remote_host_port);
        debug_run_arguments_pt.push_back(std::make_pair("", debug_run_argument_pt));
      }
    }
    root_pt.add_child("debug_run_arguments", debug_run_arguments_pt);
    
    int width, height;
    get_size(width, height);
    boost::property_tree::ptree window_pt;
    window_pt.put("width", width);
    window_pt.put("height", height);
    root_pt.add_child("window", window_pt);
    
    boost::property_tree::write_json((Config::get().home_juci_path/"last_session.json").string(), root_pt);
  }
  catch(...) {}
}

void Window::load_session(std::vector<boost::filesystem::path> &directories, std::vector<std::pair<boost::filesystem::path, size_t> > &files, std::vector<std::pair<int, int> > &file_offsets, std::string &current_file, bool read_directories_and_files) {
  try {
    boost::property_tree::ptree root_pt;
    boost::property_tree::read_json((Config::get().home_juci_path/"last_session.json").string(), root_pt);
    if(read_directories_and_files) {
      auto folder=root_pt.get<std::string>("folder");
      if(!folder.empty() && boost::filesystem::exists(folder) && boost::filesystem::is_directory(folder))
        directories.emplace_back(folder);
      
      for(auto &file_pt: root_pt.get_child("files")) {
        auto file=file_pt.second.get<std::string>("path");
        auto notebook=file_pt.second.get<size_t>("notebook");
        auto line=file_pt.second.get<int>("line");
        auto line_offset=file_pt.second.get<int>("line_offset");
        if(!file.empty() && boost::filesystem::exists(file) && !boost::filesystem::is_directory(file)) {
          files.emplace_back(file, notebook);
          file_offsets.emplace_back(line, line_offset);
        }
      }
     
      current_file=root_pt.get<std::string>("current_file");
    }
    
    for(auto &run_argument: root_pt.get_child(("run_arguments"))) {
      auto path=run_argument.second.get<std::string>("path");
      boost::system::error_code ec;
      if(boost::filesystem::exists(path, ec) && boost::filesystem::is_directory(path, ec))
        Project::run_arguments.emplace(path, run_argument.second.get<std::string>("arguments"));
    }
    
    for(auto &debug_run_argument: root_pt.get_child(("debug_run_arguments"))) {
      auto path=debug_run_argument.second.get<std::string>("path");
      boost::system::error_code ec;
      if(boost::filesystem::exists(path, ec) && boost::filesystem::is_directory(path, ec))
        Project::debug_run_arguments.emplace(path, Project::DebugRunArguments{debug_run_argument.second.get<std::string>("arguments"),
                                                                              debug_run_argument.second.get<bool>("remote_enabled"),
                                                                              debug_run_argument.second.get<std::string>("remote_host_port")});
    }
    
    auto window_pt=root_pt.get_child("window");
    set_default_size(window_pt.get<int>("width"), window_pt.get<int>("height"));
  }
  catch(...) {
    set_default_size(800, 600);
  }
}
