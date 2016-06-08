#include "project.h"
#include "config.h"
#include "terminal.h"
#include "filesystem.h"
#include "directories.h"
#include <fstream>
#include "menu.h"
#include "notebook.h"
#ifdef JUCI_ENABLE_DEBUG
#include "debug_clang.h"
#endif
#include "info.h"

boost::filesystem::path Project::debug_last_stop_file_path;
std::unordered_map<std::string, std::string> Project::run_arguments;
std::unordered_map<std::string, std::string> Project::debug_run_arguments;
std::atomic<bool> Project::compiling(false);
std::atomic<bool> Project::debugging(false);
std::pair<boost::filesystem::path, std::pair<int, int> > Project::debug_stop;
std::unique_ptr<Project::Base> Project::current;
std::unordered_map<std::string, Project::Clang::DebugOptionsPopover> Project::Clang::debug_options_popovers;

Gtk::Label &Project::debug_status_label() {
  static Gtk::Label label;
  return label;
}

void Project::save_files(const boost::filesystem::path &path) {
  for(size_t c=0;c<Notebook::get().size();c++) {
    auto view=Notebook::get().get_view(c);
    if(view->get_buffer()->get_modified()) {
      if(filesystem::file_in_path(view->file_path, path))
        Notebook::get().save(c);
    }
  }
}

void Project::on_save(size_t index) {
  auto view=Notebook::get().get_view(index);
  if(!view)
    return;
  if(view->language && view->language->get_id()=="cmake") {
    boost::filesystem::path cmake_path;
    if(view->file_path.filename()=="CMakeLists.txt")
      cmake_path=view->file_path;
    else
      cmake_path=filesystem::find_file_in_path_parents("CMakeLists.txt", view->file_path.parent_path());
    
    if(!cmake_path.empty()) {
      auto build=Build::create(cmake_path);
      if(dynamic_cast<CMakeBuild*>(build.get())) {
        build->update_default(true);
        if(boost::filesystem::exists(build->get_debug_path()))
          build->update_debug(true);
        
        for(size_t c=0;c<Notebook::get().size();c++) {
          auto source_view=Notebook::get().get_view(c);
          if(auto source_clang_view=dynamic_cast<Source::ClangView*>(source_view)) {
            if(filesystem::file_in_path(source_clang_view->file_path, build->project_path))
              source_clang_view->full_reparse_needed=true;
          }
        }
      }
    }
  }
}

void Project::debug_update_status(const std::string &debug_status) {
  if(debug_status.empty())
    debug_status_label().set_text("");
  else
    debug_status_label().set_text("debug: "+debug_status);
  auto &menu=Menu::get();
  menu.actions["debug_stop"]->set_enabled(!debug_status.empty());
  menu.actions["debug_kill"]->set_enabled(!debug_status.empty());
  menu.actions["debug_step_over"]->set_enabled(!debug_status.empty());
  menu.actions["debug_step_into"]->set_enabled(!debug_status.empty());
  menu.actions["debug_step_out"]->set_enabled(!debug_status.empty());
  menu.actions["debug_backtrace"]->set_enabled(!debug_status.empty());
  menu.actions["debug_show_variables"]->set_enabled(!debug_status.empty());
  menu.actions["debug_run_command"]->set_enabled(!debug_status.empty());
  menu.actions["debug_goto_stop"]->set_enabled(!debug_status.empty());
}

void Project::debug_update_stop() {
  if(!debug_last_stop_file_path.empty()) {
    for(size_t c=0;c<Notebook::get().size();c++) {
      auto view=Notebook::get().get_view(c);
      if(view->file_path==debug_last_stop_file_path) {
        view->get_source_buffer()->remove_source_marks(view->get_buffer()->begin(), view->get_buffer()->end(), "debug_stop");
        break;
      }
    }
  }
  //Add debug stop source mark
  debug_last_stop_file_path.clear();
  for(size_t c=0;c<Notebook::get().size();c++) {
    auto view=Notebook::get().get_view(c);
    if(view->file_path==debug_stop.first) {
      if(debug_stop.second.first<view->get_buffer()->get_line_count()) {
        view->get_source_buffer()->create_source_mark("debug_stop", view->get_buffer()->get_iter_at_line(debug_stop.second.first));
        debug_last_stop_file_path=debug_stop.first;
      }
      break;
    }
  }
}

std::unique_ptr<Project::Base> Project::create() {
  std::unique_ptr<Project::Build> build;
  
  if(auto view=Notebook::get().get_current_view()) {
    build=Build::create(view->file_path);
    if(view->language) {
      auto language_id=view->language->get_id();
      if(language_id=="markdown")
        return std::unique_ptr<Project::Base>(new Project::Markdown(std::move(build)));
      if(language_id=="python")
        return std::unique_ptr<Project::Base>(new Project::Python(std::move(build)));
      if(language_id=="js")
        return std::unique_ptr<Project::Base>(new Project::JavaScript(std::move(build)));
      if(language_id=="html")
        return std::unique_ptr<Project::Base>(new Project::HTML(std::move(build)));
    }
  }
  else
    build=Build::create(Directories::get().path);
  
  if(dynamic_cast<CMakeBuild*>(build.get()))
    return std::unique_ptr<Project::Base>(new Project::Clang(std::move(build)));
  else
    return std::unique_ptr<Project::Base>(new Project::Base(std::move(build)));
}

std::pair<std::string, std::string> Project::Base::get_run_arguments() {
  Info::get().print("Could not find a supported project");
  return {"", ""};
}

void Project::Base::compile() {
  Info::get().print("Could not find a supported project");
}

void Project::Base::compile_and_run() {
  Info::get().print("Could not find a supported project");
}

std::pair<std::string, std::string> Project::Base::debug_get_run_arguments() {
  Info::get().print("Could not find a supported project");
  return {"", ""};
}

void Project::Base::debug_start() {
  Info::get().print("Could not find a supported project");
}

Project::Clang::DebugOptionsPopover::DebugOptionsPopover() : Gtk::Popover() {
  auto platform_list=Debug::Clang::get_platform_list();
  for(auto &platform: platform_list)
    platform_list_combo_box_text.append(platform);
  if(platform_list_combo_box_text.get_model()->children().size()>0)
    platform_list_combo_box_text.set_active(0);
  url_entry.set_sensitive(false);
  platform_list_combo_box_text.signal_changed().connect([this]() {
    url_entry.set_sensitive(platform_list_combo_box_text.get_active_row_number()>0);
  });
  
  url_entry.set_placeholder_text("URL");
  
  cross_compiling_vbox.pack_start(platform_list_combo_box_text, true, true);
  cross_compiling_vbox.pack_end(url_entry, true, true);
  
  cross_compiling_frame.set_label("Cross Compiling");
  cross_compiling_frame.add(cross_compiling_vbox);
  
  vbox.pack_start(cross_compiling_frame, true, true);
  
  not_yet_implemented_label.set_text("Not yet implemented");
  vbox.pack_end(not_yet_implemented_label, true, true);
  add(vbox);
  show_all();
  set_visible(false);
}

std::pair<std::string, std::string> Project::Clang::get_run_arguments() {
  auto build_path=build->get_default_path();
  if(build_path.empty())
    return {"", ""};
  
  auto project_path=build->project_path.string();
  auto run_arguments_it=run_arguments.find(project_path);
  std::string arguments;
  if(run_arguments_it!=run_arguments.end())
    arguments=run_arguments_it->second;
  
  if(arguments.empty()) {
    auto view=Notebook::get().get_current_view();
    auto executable=build->get_executable(view?view->file_path:"").string();
    
    if(executable!="") {
      size_t pos=executable.find(project_path);
      if(pos!=std::string::npos)
        executable.replace(pos, project_path.size(), build_path.string());
      arguments=filesystem::escape_argument(executable);
    }
    else
      arguments=filesystem::escape_argument(build->get_default_path());
  }
  
  return {project_path, arguments};
}

void Project::Clang::compile() {
  auto default_build_path=build->get_default_path();
  if(default_build_path.empty() || !build->update_default())
    return;
  
  if(Config::get().project.clear_terminal_on_compile)
    Terminal::get().clear();
  
  compiling=true;
  Terminal::get().print("Compiling project "+build->project_path.string()+"\n");
  Terminal::get().async_process(Config::get().project.make_command, default_build_path, [this](int exit_status) {
    compiling=false;
  });
}

void Project::Clang::compile_and_run() {
  auto default_build_path=build->get_default_path();
  if(default_build_path.empty() || !build->update_default())
    return;
  
  auto project_path=build->project_path;
  
  auto run_arguments_it=run_arguments.find(project_path.string());
  std::string arguments;
  if(run_arguments_it!=run_arguments.end())
    arguments=run_arguments_it->second;
  
  if(arguments.empty()) {
    auto view=Notebook::get().get_current_view();
    arguments=build->get_executable(view?view->file_path:"").string();
    if(arguments.empty()) {
      Terminal::get().print("Warning: could not find executable.\n");
      Terminal::get().print("Solution: either use Project Set Run Arguments, or open a source file within a directory where add_executable is set.\n", true);
      return;
    }
    size_t pos=arguments.find(project_path.string());
    if(pos!=std::string::npos)
      arguments.replace(pos, project_path.string().size(), default_build_path.string());
    arguments=filesystem::escape_argument(arguments);
  }
  
  if(Config::get().project.clear_terminal_on_compile)
    Terminal::get().clear();
  
  compiling=true;
  Terminal::get().print("Compiling and running "+arguments+"\n");
  Terminal::get().async_process(Config::get().project.make_command, default_build_path, [this, arguments, project_path](int exit_status){
    compiling=false;
    if(exit_status==EXIT_SUCCESS) {
      Terminal::get().async_process(arguments, project_path, [this, arguments](int exit_status){
        Terminal::get().async_print(arguments+" returned: "+std::to_string(exit_status)+'\n');
      });
    }
  });
}

#ifdef JUCI_ENABLE_DEBUG
std::pair<std::string, std::string> Project::Clang::debug_get_run_arguments() {
  auto build_path=build->get_debug_path();
  if(build_path.empty())
    return {"", ""};
  
  auto project_path=build->project_path.string();
  auto run_arguments_it=debug_run_arguments.find(project_path);
  std::string arguments;
  if(run_arguments_it!=debug_run_arguments.end())
    arguments=run_arguments_it->second;
  
  if(arguments.empty()) {
    auto view=Notebook::get().get_current_view();
    auto executable=build->get_executable(view?view->file_path:"").string();
    
    if(executable!="") {
      size_t pos=executable.find(project_path);
      if(pos!=std::string::npos)
        executable.replace(pos, project_path.size(), build_path.string());
      arguments=filesystem::escape_argument(executable);
    }
    else
      arguments=filesystem::escape_argument(build->get_debug_path());
  }
  
  return {project_path, arguments};
}

Gtk::Popover *Project::Clang::debug_get_options_popover() {
  if(!build->project_path.empty())
    return &debug_options_popovers[build->project_path.string()];
  return nullptr;
}

void Project::Clang::debug_start() {
  auto debug_build_path=build->get_debug_path();
  if(debug_build_path.empty() || !build->update_debug())
    return;
  auto project_path=build->project_path;
  
  auto run_arguments_it=debug_run_arguments.find(project_path.string());
  std::string run_arguments;
  if(run_arguments_it!=debug_run_arguments.end())
    run_arguments=run_arguments_it->second;
  
  if(run_arguments.empty()) {
    auto view=Notebook::get().get_current_view();
    run_arguments=build->get_executable(view?view->file_path:"").string();
    if(run_arguments.empty()) {
      Terminal::get().print("Warning: could not find executable.\n");
      Terminal::get().print("Solution: either use Debug Set Run Arguments, or open a source file within a directory where add_executable is set.\n", true);
      return;
    }
    size_t pos=run_arguments.find(project_path.string());
    if(pos!=std::string::npos)
      run_arguments.replace(pos, project_path.string().size(), debug_build_path.string());
    run_arguments=filesystem::escape_argument(run_arguments);
  }
  
  auto breakpoints=std::make_shared<std::vector<std::pair<boost::filesystem::path, int> > >();
  for(size_t c=0;c<Notebook::get().size();c++) {
    auto view=Notebook::get().get_view(c);
    if(filesystem::file_in_path(view->file_path, project_path)) {
      auto iter=view->get_buffer()->begin();
      if(view->get_source_buffer()->get_source_marks_at_iter(iter, "debug_breakpoint").size()>0)
        breakpoints->emplace_back(view->file_path, iter.get_line()+1);
      while(view->get_source_buffer()->forward_iter_to_source_mark(iter, "debug_breakpoint"))
        breakpoints->emplace_back(view->file_path, iter.get_line()+1);
    }
  }
  
  if(Config::get().project.clear_terminal_on_compile)
    Terminal::get().clear();
  
  debugging=true;
  Terminal::get().print("Compiling and debugging "+run_arguments+"\n");
  Terminal::get().async_process(Config::get().project.make_command, debug_build_path, [this, breakpoints, run_arguments, project_path](int exit_status){
    if(exit_status!=EXIT_SUCCESS)
      debugging=false;
    else {
      std::unique_lock<std::mutex> lock(debug_start_mutex);
      Debug::Clang::get().start(run_arguments, project_path, *breakpoints, [this, run_arguments](int exit_status){
        debugging=false;
        Terminal::get().async_print(run_arguments+" returned: "+std::to_string(exit_status)+'\n');
      }, [this](const std::string &status) {
        dispatcher.post([this, status] {
          debug_update_status(status);
        });
      }, [this](const boost::filesystem::path &file_path, int line_nr, int line_index) {
        dispatcher.post([this, file_path, line_nr, line_index] {
          Project::debug_stop.first=file_path;
          Project::debug_stop.second.first=line_nr-1;
          Project::debug_stop.second.second=line_index-1;
          
          debug_update_stop();
          if(auto view=Notebook::get().get_current_view())
            view->get_buffer()->place_cursor(view->get_buffer()->get_insert()->get_iter());
        });
      });
    }
  });
}

void Project::Clang::debug_continue() {
  Debug::Clang::get().continue_debug();
}

void Project::Clang::debug_stop() {
  if(debugging)
    Debug::Clang::get().stop();
}

void Project::Clang::debug_kill() {
  if(debugging)
    Debug::Clang::get().kill();
}

void Project::Clang::debug_step_over() {
  if(debugging)
    Debug::Clang::get().step_over();
}

void Project::Clang::debug_step_into() {
  if(debugging)
    Debug::Clang::get().step_into();
}

void Project::Clang::debug_step_out() {
  if(debugging)
    Debug::Clang::get().step_out();
}

void Project::Clang::debug_backtrace() {
  auto view=Notebook::get().get_current_view();
  if(view && debugging) {
    auto backtrace=Debug::Clang::get().get_backtrace();
    
    auto iter=view->get_iter_for_dialog();
    view->selection_dialog=std::unique_ptr<SelectionDialog>(new SelectionDialog(*view, view->get_buffer()->create_mark(iter), true, true));
    auto rows=std::make_shared<std::unordered_map<std::string, Debug::Clang::Frame> >();
    if(backtrace.size()==0)
      return;
    
    bool cursor_set=false;
    for(auto &frame: backtrace) {
      std::string row="<i>"+frame.module_filename+"</i>";
      
      //Shorten frame.function_name if it is too long
      if(frame.function_name.size()>120) {
        frame.function_name=frame.function_name.substr(0, 58)+"...."+frame.function_name.substr(frame.function_name.size()-58);
      }
      if(frame.file_path.empty())
        row+=" - "+Glib::Markup::escape_text(frame.function_name);
      else {
        auto file_path=frame.file_path.filename().string();
        row+=":<b>"+Glib::Markup::escape_text(file_path)+":"+std::to_string(frame.line_nr)+"</b> - "+Glib::Markup::escape_text(frame.function_name);
      }
      (*rows)[row]=frame;
      view->selection_dialog->add_row(row);
      if(!cursor_set && frame.file_path==view->file_path) {
        view->selection_dialog->set_cursor_at_last_row();
        cursor_set=true;
      }
    }
    
    view->selection_dialog->on_select=[this, rows](const std::string& selected, bool hide_window) {
      auto frame=rows->at(selected);
      if(!frame.file_path.empty()) {
        Notebook::get().open(frame.file_path);
        if(auto view=Notebook::get().get_current_view()) {
          Debug::Clang::get().select_frame(frame.index);
          
          view->place_cursor_at_line_index(frame.line_nr-1, frame.line_index-1);
          view->scroll_to_cursor_delayed(view, true, true);
        }
      }
    };
    view->hide_tooltips();
    view->selection_dialog->show();
  }
}

void Project::Clang::debug_show_variables() {
  auto view=Notebook::get().get_current_view();
  if(debugging && view) {
    auto variables=Debug::Clang::get().get_variables();
    
    auto iter=view->get_iter_for_dialog();
    view->selection_dialog=std::unique_ptr<SelectionDialog>(new SelectionDialog(*view, view->get_buffer()->create_mark(iter), true, true));
    auto rows=std::make_shared<std::unordered_map<std::string, Debug::Clang::Variable> >();
    if(variables.size()==0)
      return;
    
    for(auto &variable: variables) {
      std::string row="#"+std::to_string(variable.thread_index_id)+":#"+std::to_string(variable.frame_index)+":"+variable.file_path.filename().string()+":"+std::to_string(variable.line_nr)+" - <b>"+Glib::Markup::escape_text(variable.name)+"</b>";
      
      (*rows)[row]=variable;
      view->selection_dialog->add_row(row);
    }
    
    view->selection_dialog->on_select=[this, rows](const std::string& selected, bool hide_window) {
      auto variable=rows->at(selected);
      if(!variable.file_path.empty()) {
        Notebook::get().open(variable.file_path);
        if(auto view=Notebook::get().get_current_view()) {
          Debug::Clang::get().select_frame(variable.frame_index, variable.thread_index_id);
          
          view->place_cursor_at_line_index(variable.line_nr-1, variable.line_index-1);
          view->scroll_to_cursor_delayed(view, true, true);
        }
      }
    };
    
    view->selection_dialog->on_hide=[this]() {
      debug_variable_tooltips.hide();
      debug_variable_tooltips.clear();
    };
    
    view->selection_dialog->on_changed=[this, rows, iter](const std::string &selected) {
      if(selected.empty()) {
        debug_variable_tooltips.hide();
        return;
      }
      if(auto view=Notebook::get().get_current_view()) {
        debug_variable_tooltips.clear();
        auto create_tooltip_buffer=[this, rows, view, selected]() {
          auto variable=rows->at(selected);
          auto tooltip_buffer=Gtk::TextBuffer::create(view->get_buffer()->get_tag_table());
          
          Glib::ustring value=variable.value;
          if(!value.empty()) {
            Glib::ustring::iterator iter;
            while(!value.validate(iter)) {
              auto next_char_iter=iter;
              next_char_iter++;
              value.replace(iter, next_char_iter, "?");
            } 
            tooltip_buffer->insert_with_tag(tooltip_buffer->get_insert()->get_iter(), value.substr(0, value.size()-1), "def:note");
          }
          
          return tooltip_buffer;
        };
        
        debug_variable_tooltips.emplace_back(create_tooltip_buffer, *view, view->get_buffer()->create_mark(iter), view->get_buffer()->create_mark(iter));
    
        debug_variable_tooltips.show(true);
      }
    };
    
    view->hide_tooltips();
    view->selection_dialog->show();
  }
}

void Project::Clang::debug_run_command(const std::string &command) {
  if(debugging) {
    auto command_return=Debug::Clang::get().run_command(command);
    Terminal::get().async_print(command_return.first);
    Terminal::get().async_print(command_return.second, true);
  }
}

void Project::Clang::debug_add_breakpoint(const boost::filesystem::path &file_path, int line_nr) {
  Debug::Clang::get().add_breakpoint(file_path, line_nr);
}

void Project::Clang::debug_remove_breakpoint(const boost::filesystem::path &file_path, int line_nr, int line_count) {
  Debug::Clang::get().remove_breakpoint(file_path, line_nr, line_count);
}

bool Project::Clang::debug_is_running() {
  return Debug::Clang::get().is_running();
}

void Project::Clang::debug_write(const std::string &buffer) {
  Debug::Clang::get().write(buffer);
}

void Project::Clang::debug_cancel() {
  std::unique_lock<std::mutex> lock(debug_start_mutex);
  Debug::Clang::get().cancel();
}
#endif

Project::Markdown::~Markdown() {
  if(!last_temp_path.empty()) {
    boost::filesystem::remove(last_temp_path);
    last_temp_path=boost::filesystem::path();
  }
}

void Project::Markdown::compile_and_run() {
  if(!last_temp_path.empty()) {
    boost::filesystem::remove(last_temp_path);
    last_temp_path=boost::filesystem::path();
  }
  
  std::stringstream stdin_stream, stdout_stream;
  auto exit_status=Terminal::get().process(stdin_stream, stdout_stream, "markdown "+filesystem::escape_argument(Notebook::get().get_current_view()->file_path.string()));
  if(exit_status==0) {
    boost::system::error_code ec;
    auto temp_path=boost::filesystem::temp_directory_path(ec);
    if(!ec) {
      temp_path/=boost::filesystem::unique_path();
      temp_path+=".html";
      if(!boost::filesystem::exists(temp_path)) {
        last_temp_path=temp_path;
        std::ofstream file_stream(temp_path.string(), std::fstream::binary);
        file_stream << stdout_stream.rdbuf();
        file_stream.close();
        
        auto uri=temp_path.string();
#ifdef __APPLE__
        Terminal::get().process("open "+filesystem::escape_argument(uri));
#else
#ifdef __linux
        uri="file://"+uri;
#endif
        GError* error=NULL;
        gtk_show_uri(NULL, uri.c_str(), GDK_CURRENT_TIME, &error);
        g_clear_error(&error);
#endif
      }
    }
  }
}

void Project::Python::compile_and_run() {
  auto command="python "+filesystem::escape_argument(Notebook::get().get_current_view()->file_path.string());
  Terminal::get().print("Running "+command+"\n");
  Terminal::get().async_process(command, Notebook::get().get_current_view()->file_path.parent_path(), [command](int exit_status) {
    Terminal::get().async_print(command+" returned: "+std::to_string(exit_status)+'\n');
  });
}

void Project::JavaScript::compile_and_run() {
  auto command="node --harmony "+filesystem::escape_argument(Notebook::get().get_current_view()->file_path.string());
  Terminal::get().print("Running "+command+"\n");
  Terminal::get().async_process(command, Notebook::get().get_current_view()->file_path.parent_path(), [command](int exit_status) {
    Terminal::get().async_print(command+" returned: "+std::to_string(exit_status)+'\n');
  });
}

void Project::HTML::compile_and_run() {
  auto uri=Notebook::get().get_current_view()->file_path.string();
#ifdef __APPLE__
  Terminal::get().process("open "+filesystem::escape_argument(uri));
#else
#ifdef __linux
  uri="file://"+uri;
#endif
  GError* error=NULL;
  gtk_show_uri(NULL, uri.c_str(), GDK_CURRENT_TIME, &error);
  g_clear_error(&error);
#endif
}
