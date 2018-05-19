#include "project.h"
#include "config.h"
#include "terminal.h"
#include "filesystem.h"
#include "directories.h"
#include <fstream>
#include "menu.h"
#include "notebook.h"
#include "selection_dialog.h"
#ifdef JUCI_ENABLE_DEBUG
#include "debug_lldb.h"
#endif
#include "info.h"
#include "source_clang.h"
#include "source_language_protocol.h"
#include "usages_clang.h"
#include "ctags.h"
#include <future>

boost::filesystem::path Project::debug_last_stop_file_path;
std::unordered_map<std::string, std::string> Project::run_arguments;
std::unordered_map<std::string, Project::DebugRunArguments> Project::debug_run_arguments;
std::atomic<bool> Project::compiling(false);
std::atomic<bool> Project::debugging(false);
std::pair<boost::filesystem::path, std::pair<int, int> > Project::debug_stop;
std::string Project::debug_status;
std::shared_ptr<Project::Base> Project::current;
std::unique_ptr<Project::DebugOptions> Project::Base::debug_options;

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
  boost::filesystem::path build_path;
  if(view->language && view->language->get_id()=="cmake") {
    if(view->file_path.filename()=="CMakeLists.txt")
      build_path=view->file_path;
    else
      build_path=filesystem::find_file_in_path_parents("CMakeLists.txt", view->file_path.parent_path());
  }
  else if(view->language && view->language->get_id()=="meson") {
    if(view->file_path.filename()=="meson.build")
      build_path=view->file_path;
    else
      build_path=filesystem::find_file_in_path_parents("meson.build", view->file_path.parent_path());
  }
  
  if(!build_path.empty()) {
    auto build=Build::create(build_path);
    if(dynamic_cast<CMakeBuild*>(build.get()) || dynamic_cast<MesonBuild*>(build.get())) {
      build->update_default(true);
      Usages::Clang::erase_all_caches_for_project(build->project_path, build->get_default_path());
      boost::system::error_code ec;
      if(boost::filesystem::exists(build->get_debug_path()), ec)
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

void Project::debug_update_status(const std::string &new_debug_status) {
  debug_status=new_debug_status;
  if(debug_status.empty())
    debug_status_label().set_text("");
  else
    debug_status_label().set_text(debug_status);
  debug_activate_menu_items();
}

void Project::debug_activate_menu_items() {
  auto &menu=Menu::get();
  auto view=Notebook::get().get_current_view();
  menu.actions["debug_stop"]->set_enabled(!debug_status.empty());
  menu.actions["debug_kill"]->set_enabled(!debug_status.empty());
  menu.actions["debug_step_over"]->set_enabled(!debug_status.empty());
  menu.actions["debug_step_into"]->set_enabled(!debug_status.empty());
  menu.actions["debug_step_out"]->set_enabled(!debug_status.empty());
  menu.actions["debug_backtrace"]->set_enabled(!debug_status.empty());
  menu.actions["debug_show_variables"]->set_enabled(!debug_status.empty());
  menu.actions["debug_run_command"]->set_enabled(!debug_status.empty());
  menu.actions["debug_toggle_breakpoint"]->set_enabled(view && view->toggle_breakpoint);
  menu.actions["debug_goto_stop"]->set_enabled(!debug_status.empty());
}

void Project::debug_update_stop() {
  if(!debug_last_stop_file_path.empty()) {
    for(size_t c=0;c<Notebook::get().size();c++) {
      auto view=Notebook::get().get_view(c);
      if(view->file_path==debug_last_stop_file_path) {
        view->get_source_buffer()->remove_source_marks(view->get_buffer()->begin(), view->get_buffer()->end(), "debug_stop");
        view->get_source_buffer()->remove_source_marks(view->get_buffer()->begin(), view->get_buffer()->end(), "debug_breakpoint_and_stop");
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
        auto iter=view->get_buffer()->get_iter_at_line(debug_stop.second.first);
        view->get_source_buffer()->create_source_mark("debug_stop", iter);
        if(view->get_source_buffer()->get_source_marks_at_iter(iter, "debug_breakpoint").size()>0)
          view->get_source_buffer()->create_source_mark("debug_breakpoint_and_stop", iter);
        debug_last_stop_file_path=debug_stop.first;
      }
      break;
    }
  }
}

std::shared_ptr<Project::Base> Project::create() {
  std::unique_ptr<Project::Build> build;
  
  if(auto view=Notebook::get().get_current_view()) {
    build=Build::create(view->file_path);
    if(view->language) {
      auto language_id=view->language->get_id();
      if(language_id=="markdown")
        return std::shared_ptr<Project::Base>(new Project::Markdown(std::move(build)));
      if(language_id=="python")
        return std::shared_ptr<Project::Base>(new Project::Python(std::move(build)));
      if(language_id=="js")
        return std::shared_ptr<Project::Base>(new Project::JavaScript(std::move(build)));
      if(language_id=="html")
        return std::shared_ptr<Project::Base>(new Project::HTML(std::move(build)));
    }
  }
  else
    build=Build::create(Directories::get().path);
  
  if(dynamic_cast<CMakeBuild*>(build.get()) || dynamic_cast<MesonBuild*>(build.get()))
    return std::shared_ptr<Project::Base>(new Project::Clang(std::move(build)));
  else if(dynamic_cast<CargoBuild*>(build.get()))
    return std::shared_ptr<Project::Base>(new Project::Rust(std::move(build)));
  else if(dynamic_cast<NpmBuild*>(build.get()))
    return std::shared_ptr<Project::Base>(new Project::JavaScript(std::move(build)));
  else
    return std::shared_ptr<Project::Base>(new Project::Base(std::move(build)));
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

void Project::Base::recreate_build() {
  Info::get().print("Could not find a supported project");
}

void Project::Base::show_symbols() {
  auto view=Notebook::get().get_current_view();
  
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
  auto pair=Ctags::get_result(search_path);
  
  auto path=std::move(pair.first);
  auto stream=std::move(pair.second);
  stream->seekg(0, std::ios::end);
  if(stream->tellg()==0) {
    Info::get().print("No symbols found in current project");
    return;
  }
  stream->seekg(0, std::ios::beg);
  
  if(view) {
    auto dialog_iter=view->get_iter_for_dialog();
    SelectionDialog::create(view, view->get_buffer()->create_mark(dialog_iter), true, true);
  }
  else
    SelectionDialog::create(true, true);
  
  std::vector<Source::Offset> rows;
    
  std::string line;
  while(std::getline(*stream, line)) {
    auto location=Ctags::get_location(line, true);
    
    std::string row=location.file_path.string()+":"+std::to_string(location.line+1)+": "+location.source;
    rows.emplace_back(Source::Offset(location.line, location.index, location.file_path));
    SelectionDialog::get()->add_row(row);
  }
    
  if(rows.size()==0)
    return;
  SelectionDialog::get()->on_select=[rows=std::move(rows), path=std::move(path)](unsigned int index, const std::string &text, bool hide_window) {
    if(index>=rows.size())
      return;
    auto offset=rows[index];
    auto full_path=path/offset.file_path;
    if(!boost::filesystem::is_regular_file(full_path))
      return;
    Notebook::get().open(full_path);
    auto view=Notebook::get().get_current_view();
    view->place_cursor_at_line_index(offset.line, offset.index);
    view->scroll_to_cursor_delayed(view, true, false);
    view->hide_tooltips();
  };
  if(view)
    view->hide_tooltips();
  SelectionDialog::get()->show();
}

std::pair<std::string, std::string> Project::Base::debug_get_run_arguments() {
  Info::get().print("Could not find a supported project");
  return {"", ""};
}

void Project::Base::debug_start() {
  Info::get().print("Could not find a supported project");
}

#ifdef JUCI_ENABLE_DEBUG
std::pair<std::string, std::string> Project::LLDB::debug_get_run_arguments() {
  auto debug_build_path=build->get_debug_path();
  auto default_build_path=build->get_default_path();
  if(debug_build_path.empty() || default_build_path.empty())
    return {"", ""};
  
  auto project_path=build->project_path.string();
  auto run_arguments_it=debug_run_arguments.find(project_path);
  std::string arguments;
  if(run_arguments_it!=debug_run_arguments.end())
    arguments=run_arguments_it->second.arguments;
  
  if(arguments.empty()) {
    auto view=Notebook::get().get_current_view();
    auto executable=build->get_executable(view?view->file_path:Directories::get().path).string();
    
    if(!executable.empty()) {
      size_t pos=executable.find(default_build_path.string());
      if(pos!=std::string::npos)
        executable.replace(pos, default_build_path.string().size(), debug_build_path.string());
      arguments=filesystem::escape_argument(filesystem::get_short_path(executable).string());
    }
    else
      arguments=filesystem::escape_argument(filesystem::get_short_path(build->get_debug_path()).string());
  }
  
  return {project_path, arguments};
}

Project::DebugOptions *Project::LLDB::debug_get_options() {
  if(build->project_path.empty())
    return nullptr;
  
  debug_options=std::make_unique<DebugOptions>();
  
  auto &arguments=Project::debug_run_arguments[build->project_path.string()];
  
  auto remote_enabled=Gtk::manage(new Gtk::CheckButton());
  auto remote_host_port=Gtk::manage(new Gtk::Entry());
  remote_enabled->set_active(arguments.remote_enabled);
  remote_enabled->set_label("Enabled");
  remote_enabled->signal_clicked().connect([remote_enabled, remote_host_port] {
    remote_host_port->set_sensitive(remote_enabled->get_active());
  });
  
  remote_host_port->set_sensitive(arguments.remote_enabled);
  remote_host_port->set_text(arguments.remote_host_port);
  remote_host_port->set_placeholder_text("host:port");
  remote_host_port->signal_activate().connect([] {
    debug_options->hide();
  });
  
  auto self=this->shared_from_this();
  debug_options->signal_hide().connect([self, remote_enabled, remote_host_port] {
    auto &arguments=Project::debug_run_arguments[self->build->project_path.string()];
    arguments.remote_enabled=remote_enabled->get_active();
    arguments.remote_host_port=remote_host_port->get_text();
  });
  
  auto remote_vbox=Gtk::manage(new Gtk::Box(Gtk::Orientation::ORIENTATION_VERTICAL));
  remote_vbox->pack_start(*remote_enabled, true, true);
  remote_vbox->pack_end(*remote_host_port, true, true);
  
  auto remote_frame=Gtk::manage(new Gtk::Frame());
  remote_frame->set_label("Remote Debugging");
  remote_frame->add(*remote_vbox);
  
  debug_options->vbox.pack_end(*remote_frame, true, true);
  
  return debug_options.get();
}

void Project::LLDB::debug_start() {
  auto debug_build_path=build->get_debug_path();
  auto default_build_path=build->get_default_path();
  if(debug_build_path.empty() || !build->update_debug() || default_build_path.empty())
    return;
  
  auto project_path=std::make_shared<boost::filesystem::path>(build->project_path);
  
  auto run_arguments_it=debug_run_arguments.find(project_path->string());
  auto run_arguments=std::make_shared<std::string>();
  if(run_arguments_it!=debug_run_arguments.end())
    *run_arguments=run_arguments_it->second.arguments;
  
  if(run_arguments->empty()) {
    auto view=Notebook::get().get_current_view();
    *run_arguments=build->get_executable(view?view->file_path:Directories::get().path).string();
    if(run_arguments->empty()) {
      Terminal::get().print("Warning: could not find executable.\n");
      Terminal::get().print("Solution: either use Debug Set Run Arguments, or open a source file within a directory where an executable is defined.\n", true);
      return;
    }
    size_t pos=run_arguments->find(default_build_path.string());
    if(pos!=std::string::npos)
      run_arguments->replace(pos, default_build_path.string().size(), debug_build_path.string());
    *run_arguments=filesystem::escape_argument(filesystem::get_short_path(*run_arguments).string());
  }
  
  debugging=true;
  
  if(Config::get().project.clear_terminal_on_compile)
    Terminal::get().clear();
  
  Terminal::get().print("Compiling and debugging "+*run_arguments+"\n");
  Terminal::get().async_process(build->get_compile_command(), debug_build_path, [self=this->shared_from_this(), run_arguments, project_path](int exit_status){
    if(exit_status!=EXIT_SUCCESS)
      debugging=false;
    else {
      self->dispatcher.post([self, run_arguments, project_path] {
        std::vector<std::pair<boost::filesystem::path, int> > breakpoints;
        for(size_t c=0;c<Notebook::get().size();c++) {
          auto view=Notebook::get().get_view(c);
          if(filesystem::file_in_path(view->file_path, *project_path)) {
            auto iter=view->get_buffer()->begin();
            if(view->get_source_buffer()->get_source_marks_at_iter(iter, "debug_breakpoint").size()>0)
              breakpoints.emplace_back(view->file_path, iter.get_line()+1);
            while(view->get_source_buffer()->forward_iter_to_source_mark(iter, "debug_breakpoint"))
              breakpoints.emplace_back(view->file_path, iter.get_line()+1);
          }
        }
        
        std::string remote_host;
        auto debug_run_arguments_it=debug_run_arguments.find(project_path->string());
        if(debug_run_arguments_it!=debug_run_arguments.end() && debug_run_arguments_it->second.remote_enabled)
          remote_host=debug_run_arguments_it->second.remote_host_port;
        
        static auto on_exit_it=Debug::LLDB::get().on_exit.end();
        if(on_exit_it!=Debug::LLDB::get().on_exit.end())
          Debug::LLDB::get().on_exit.erase(on_exit_it);
        Debug::LLDB::get().on_exit.emplace_back([self, run_arguments](int exit_status) {
          debugging=false;
          Terminal::get().async_print(*run_arguments+" returned: "+std::to_string(exit_status)+'\n');
          self->dispatcher.post([] {
            debug_update_status("");
          });
        });
        on_exit_it=std::prev(Debug::LLDB::get().on_exit.end());
        
        static auto on_event_it=Debug::LLDB::get().on_event.end();
        if(on_event_it!=Debug::LLDB::get().on_event.end())
          Debug::LLDB::get().on_event.erase(on_event_it);
        Debug::LLDB::get().on_event.emplace_back([self](const lldb::SBEvent &event) {
          std::string status;
          boost::filesystem::path stop_path;
          unsigned stop_line=0, stop_column=0;
          
          std::unique_lock<std::mutex> lock(Debug::LLDB::get().mutex);
          auto process=lldb::SBProcess::GetProcessFromEvent(event);
          auto state=lldb::SBProcess::GetStateFromEvent(event);
          lldb::SBStream stream;
          event.GetDescription(stream);
          std::string event_desc=stream.GetData();
          event_desc.pop_back();
          auto pos=event_desc.rfind(" = ");
          if(pos!=std::string::npos && pos+3<event_desc.size())
            status=event_desc.substr(pos+3);
          if(state==lldb::StateType::eStateStopped) {
            char buffer[100];
            auto thread=process.GetSelectedThread();
            auto n=thread.GetStopDescription(buffer, 100);
            if(n>0)
              status+=" ("+std::string(buffer, n<=100?n:100)+")";
            auto line_entry=thread.GetSelectedFrame().GetLineEntry();
            if(line_entry.IsValid()) {
              lldb::SBStream stream;
              line_entry.GetFileSpec().GetDescription(stream);
              auto line=line_entry.GetLine();
              status +=" "+boost::filesystem::path(stream.GetData()).filename().string()+":"+std::to_string(line);
              auto column=line_entry.GetColumn();
              if(column==0)
                column=1;
              stop_path=filesystem::get_normal_path(stream.GetData());
              stop_line=line-1;
              stop_column=column-1;
            }
          }
          
          self->dispatcher.post([status=std::move(status), stop_path=std::move(stop_path), stop_line, stop_column] {
            debug_update_status(status);
            Project::debug_stop.first=stop_path;
            Project::debug_stop.second.first=stop_line;
            Project::debug_stop.second.second=stop_column;
            debug_update_stop();
            if(auto view=Notebook::get().get_current_view())
              view->get_buffer()->place_cursor(view->get_buffer()->get_insert()->get_iter());
          });
        });
        on_event_it=std::prev(Debug::LLDB::get().on_event.end());
        
        std::vector<std::string> startup_commands;
        if(dynamic_cast<CargoBuild*>(self->build.get())) {
          std::stringstream istream, ostream;
          if(Terminal::get().process(istream, ostream, "rustc --print sysroot")==0) {
            auto sysroot=ostream.str();
            while(!sysroot.empty() && (sysroot.back()=='\n' || sysroot.back()=='\r'))
              sysroot.pop_back();
            startup_commands.emplace_back("command script import \""+sysroot+"/lib/rustlib/etc/lldb_rust_formatters.py\"");
            startup_commands.emplace_back("type summary add --no-value --python-function lldb_rust_formatters.print_val -x \".*\" --category Rust");
            startup_commands.emplace_back("type category enable Rust");
          }
        }
        Debug::LLDB::get().start(*run_arguments, *project_path, breakpoints, startup_commands, remote_host);
      });
    }
  });
}

void Project::LLDB::debug_continue() {
  Debug::LLDB::get().continue_debug();
}

void Project::LLDB::debug_stop() {
  if(debugging)
    Debug::LLDB::get().stop();
}

void Project::LLDB::debug_kill() {
  if(debugging)
    Debug::LLDB::get().kill();
}

void Project::LLDB::debug_step_over() {
  if(debugging)
    Debug::LLDB::get().step_over();
}

void Project::LLDB::debug_step_into() {
  if(debugging)
    Debug::LLDB::get().step_into();
}

void Project::LLDB::debug_step_out() {
  if(debugging)
    Debug::LLDB::get().step_out();
}

void Project::LLDB::debug_backtrace() {
  if(debugging) {
    auto view=Notebook::get().get_current_view();
    auto backtrace=Debug::LLDB::get().get_backtrace();
    
    if(view) {
      auto iter=view->get_iter_for_dialog();
      SelectionDialog::create(view, view->get_buffer()->create_mark(iter), true, true);
    }
    else
      SelectionDialog::create(true, true);
    std::vector<Debug::LLDB::Frame> rows;
    if(backtrace.size()==0) {
      Info::get().print("No backtrace found");
      return;
    }
    
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
      rows.emplace_back(frame);
      SelectionDialog::get()->add_row(row);
      if(!cursor_set && view && frame.file_path==view->file_path) {
        SelectionDialog::get()->set_cursor_at_last_row();
        cursor_set=true;
      }
    }
    
    SelectionDialog::get()->on_select=[rows=std::move(rows)](unsigned int index, const std::string &text, bool hide_window) {
      if(index>=rows.size())
        return;
      auto frame=rows[index];
      if(!frame.file_path.empty()) {
        Notebook::get().open(frame.file_path);
        if(auto view=Notebook::get().get_current_view()) {
          Debug::LLDB::get().select_frame(frame.index);
          
          view->place_cursor_at_line_index(frame.line_nr-1, frame.line_index-1);
          view->scroll_to_cursor_delayed(view, true, true);
        }
      }
    };
    if(view)
      view->hide_tooltips();
    SelectionDialog::get()->show();
  }
}

void Project::LLDB::debug_show_variables() {
  if(debugging) {
    auto view=Notebook::get().get_current_view();
    auto variables=Debug::LLDB::get().get_variables();
    
    Gtk::TextIter iter;
    if(view) {
      iter=view->get_iter_for_dialog();
      SelectionDialog::create(view, view->get_buffer()->create_mark(iter), true, true);
    }
    else
      SelectionDialog::create(true, true);
    auto rows=std::make_shared<std::vector<Debug::LLDB::Variable>>();
    if(variables.size()==0) {
      Info::get().print("No variables found");
      return;
    }
    
    for(auto &variable: variables) {
      std::string row="#"+std::to_string(variable.thread_index_id)+":#"+std::to_string(variable.frame_index)+":"+variable.file_path.filename().string()+":"+std::to_string(variable.line_nr)+" - <b>"+Glib::Markup::escape_text(variable.name)+"</b>";
      
      rows->emplace_back(variable);
      SelectionDialog::get()->add_row(row);
    }
    
    SelectionDialog::get()->on_select=[rows](unsigned int index, const std::string &text, bool hide_window) {
      if(index>=rows->size())
        return;
      auto variable=(*rows)[index];
      Debug::LLDB::get().select_frame(variable.frame_index, variable.thread_index_id);
      if(!variable.file_path.empty()) {
        Notebook::get().open(variable.file_path);
        if(auto view=Notebook::get().get_current_view()) {
          view->place_cursor_at_line_index(variable.line_nr-1, variable.line_index-1);
          view->scroll_to_cursor_delayed(view, true, true);
        }
      }
      if(!variable.declaration_found)
        Info::get().print("Debugger did not find declaration for the variable: "+variable.name);
    };
    
    SelectionDialog::get()->on_hide=[self=this->shared_from_this()]() {
      self->debug_variable_tooltips.hide();
      self->debug_variable_tooltips.clear();
    };
    
    SelectionDialog::get()->on_changed=[self=this->shared_from_this(), rows, view, iter](unsigned int index, const std::string &text) {
      if(index>=rows->size()) {
        self->debug_variable_tooltips.hide();
        return;
      }
      self->debug_variable_tooltips.clear();
      auto create_tooltip_buffer=[rows, view, index]() {
        auto variable=(*rows)[index];
        auto tooltip_buffer=view?Gtk::TextBuffer::create(view->get_buffer()->get_tag_table()):Gtk::TextBuffer::create();
        
        Glib::ustring value=variable.value;
        if(!value.empty()) {
          Glib::ustring::iterator iter;
          while(!value.validate(iter)) {
            auto next_char_iter=iter;
            next_char_iter++;
            value.replace(iter, next_char_iter, "?");
          }
          if(view)
            tooltip_buffer->insert(tooltip_buffer->get_insert()->get_iter(), value.substr(0, value.size()-1));
          else
            tooltip_buffer->insert(tooltip_buffer->get_insert()->get_iter(), value.substr(0, value.size()-1));
        }
        
        return tooltip_buffer;
      };
      
      if(view)
        self->debug_variable_tooltips.emplace_back(create_tooltip_buffer, view, view->get_buffer()->create_mark(iter), view->get_buffer()->create_mark(iter));
      else
        self->debug_variable_tooltips.emplace_back(create_tooltip_buffer);
  
      self->debug_variable_tooltips.show(true);
    };
    
    if(view)
      view->hide_tooltips();
    SelectionDialog::get()->show();
  }
}

void Project::LLDB::debug_run_command(const std::string &command) {
  if(debugging) {
    auto command_return=Debug::LLDB::get().run_command(command);
    Terminal::get().async_print(command_return.first);
    Terminal::get().async_print(command_return.second, true);
  }
}

void Project::LLDB::debug_add_breakpoint(const boost::filesystem::path &file_path, int line_nr) {
  Debug::LLDB::get().add_breakpoint(file_path, line_nr);
}

void Project::LLDB::debug_remove_breakpoint(const boost::filesystem::path &file_path, int line_nr, int line_count) {
  Debug::LLDB::get().remove_breakpoint(file_path, line_nr, line_count);
}

bool Project::LLDB::debug_is_running() {
  return Debug::LLDB::get().is_running();
}

void Project::LLDB::debug_write(const std::string &buffer) {
  Debug::LLDB::get().write(buffer);
}

void Project::LLDB::debug_cancel() {
  Debug::LLDB::get().cancel();
}
#endif

void Project::LanguageProtocol::show_symbols() {
  if(build->project_path.empty()) {
    Info::get().print("Could not find project folder");
    return;
  }
  
  auto language_id=get_language_id();
  auto executable_name=language_id+"-language-server";
  if(filesystem::find_executable(executable_name).empty()) {
    Info::get().print("Executable "+executable_name+" not found");
    return;
  }
  
  auto project_path=std::make_shared<boost::filesystem::path>(build->project_path);
  
  auto client=::LanguageProtocol::Client::get(*project_path, language_id);
  auto capabilities=client->initialize(nullptr);
  
  if(!capabilities.workspace_symbol) {
    Info::get().print("Language server does not support workspace/symbol");
    return;
  }
  
  auto view=Notebook::get().get_current_view();
  if(view) {
    auto dialog_iter=view->get_iter_for_dialog();
    SelectionDialog::create(view, view->get_buffer()->create_mark(dialog_iter), true, true);
  }
  else
    SelectionDialog::create(true, true);
  
  SelectionDialog::get()->on_hide=[] {
    SelectionDialog::get()->on_search_entry_changed=nullptr; // To delete client object
  };
  
  auto offsets=std::make_shared<std::vector<Source::Offset>>();
  SelectionDialog::get()->on_search_entry_changed=[client, project_path, offsets](const std::string &text) {
    if(text.size()>1)
      return;
    else {
      offsets->clear();
      SelectionDialog::get()->erase_rows();
      if(text.empty())
        return;
    }
    std::vector<std::string> names;
    std::promise<void> result_processed;
    client->write_request(nullptr, "workspace/symbol", R"("query":")"+text+'"', [&result_processed, &names, offsets, project_path](const boost::property_tree::ptree &result, bool error) {
      if(!error) {
        for(auto it=result.begin();it!=result.end();++it) {
          auto name=it->second.get<std::string>("name", "");
          if(!name.empty()) {
            auto location_it=it->second.find("location");
            if(location_it!=it->second.not_found()) {
              auto file=location_it->second.get<std::string>("uri", "");
              if(file.size()>7) {
                file.erase(0, 7);
                auto range_it=location_it->second.find("range");
                if(range_it!=location_it->second.not_found()) {
                  auto start_it=range_it->second.find("start");
                  if(start_it!=range_it->second.not_found()) {
                    try {
                      offsets->emplace_back(Source::Offset(start_it->second.get<unsigned>("line"), start_it->second.get<unsigned>("character"), file));
                      names.emplace_back(name);
                    }
                    catch(...) {}
                  }
                }
              }
            }
          }
        }
      }
      result_processed.set_value();
    });
    result_processed.get_future().get();
    for(size_t c=0;c<offsets->size() && c<names.size();++c)
      SelectionDialog::get()->add_row(filesystem::get_relative_path((*offsets)[c].file_path, *project_path).string()+':'+std::to_string((*offsets)[c].line+1)+':'+std::to_string((*offsets)[c].index+1)+": "+names[c]);
  };
  
  SelectionDialog::get()->on_select=[offsets](unsigned int index, const std::string &text, bool hide_window) {
    auto &offset=(*offsets)[index];
    if(!boost::filesystem::is_regular_file(offset.file_path))
      return;
    Notebook::get().open(offset.file_path);
    auto view=Notebook::get().get_current_view();
    view->place_cursor_at_line_offset(offset.line, offset.index);
    view->scroll_to_cursor_delayed(view, true, false);
    view->hide_tooltips();
  };
  
  if(view)
    view->hide_tooltips();
  SelectionDialog::get()->show();
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
    auto executable=build->get_executable(view?view->file_path:Directories::get().path);
    
    if(!executable.empty())
      arguments=filesystem::escape_argument(filesystem::get_short_path(executable).string());
    else
      arguments=filesystem::escape_argument(filesystem::get_short_path(build->get_default_path()).string());
  }
  
  return {project_path, arguments};
}

void Project::Clang::compile() {
  auto default_build_path=build->get_default_path();
  if(default_build_path.empty() || !build->update_default())
    return;
  
  compiling=true;
  
  if(Config::get().project.clear_terminal_on_compile)
    Terminal::get().clear();
  
  Terminal::get().print("Compiling project "+filesystem::get_short_path(build->project_path).string()+"\n");
  Terminal::get().async_process(build->get_compile_command(), default_build_path, [](int exit_status) {
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
    auto executable=build->get_executable(view?view->file_path:Directories::get().path);
    if(executable.empty()) {
      Terminal::get().print("Warning: could not find executable.\n");
      Terminal::get().print("Solution: either use Project Set Run Arguments, or open a source file within a directory where an executable is defined.\n", true);
      return;
    }
    arguments=filesystem::escape_argument(filesystem::get_short_path(executable).string());
  }
  
  compiling=true;
  
  if(Config::get().project.clear_terminal_on_compile)
    Terminal::get().clear();
  
  Terminal::get().print("Compiling and running "+arguments+"\n");
  Terminal::get().async_process(build->get_compile_command(), default_build_path, [arguments, project_path](int exit_status){
    compiling=false;
    if(exit_status==EXIT_SUCCESS) {
      Terminal::get().async_process(arguments, project_path, [arguments](int exit_status){
        Terminal::get().async_print(arguments+" returned: "+std::to_string(exit_status)+'\n');
      });
    }
  });
}

void Project::Clang::recreate_build() {
  if(build->project_path.empty())
    return;
  auto default_build_path=build->get_default_path();
  if(default_build_path.empty())
    return;
  
  auto debug_build_path=build->get_debug_path();
  bool has_default_build=boost::filesystem::exists(default_build_path);
  bool has_debug_build=!debug_build_path.empty() && boost::filesystem::exists(debug_build_path);
  
  if(has_default_build || has_debug_build) {
    Gtk::MessageDialog dialog(*static_cast<Gtk::Window*>(Notebook::get().get_toplevel()), "Recreate Build", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO);
    dialog.set_default_response(Gtk::RESPONSE_NO);
    std::string message="Are you sure you want to recreate ";
    if(has_default_build)
      message+=default_build_path.string();
    if(has_debug_build) {
      if(has_default_build)
        message+=" and ";
      message+=debug_build_path.string();
    }
    dialog.set_secondary_text(message+"?");
    if(dialog.run()!=Gtk::RESPONSE_YES)
      return;
    Usages::Clang::erase_all_caches_for_project(build->project_path, default_build_path);
    try {
      if(has_default_build)
        boost::filesystem::remove_all(default_build_path);
      if(has_debug_build)
        boost::filesystem::remove_all(debug_build_path);
    }
    catch(const std::exception &e) {
      Terminal::get().print(std::string("Error: could not remove build: ")+e.what()+"\n", true);
      return;
    }
  }
  
  build->update_default(true);
  if(has_debug_build)
    build->update_debug(true);
  
  for(size_t c=0;c<Notebook::get().size();c++) {
    auto source_view=Notebook::get().get_view(c);
    if(auto source_clang_view=dynamic_cast<Source::ClangView*>(source_view)) {
      if(filesystem::file_in_path(source_clang_view->file_path, build->project_path))
        source_clang_view->full_reparse_needed=true;
    }
  }
  
  if(auto view=Notebook::get().get_current_view()) {
    if(view->full_reparse_needed)
      view->full_reparse();
  }
}

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
  auto exit_status=Terminal::get().process(stdin_stream, stdout_stream, "command -v grip");
  if(exit_status==0) {
    auto command="grip -b "+filesystem::escape_argument(filesystem::get_short_path(Notebook::get().get_current_view()->file_path).string());
    Terminal::get().print("Running: "+command+" in a quiet background process\n");
    Terminal::get().async_process(command, "", nullptr, true);
  }
  else
    Terminal::get().print("Warning: install grip to preview Markdown files\n");
}

void Project::Python::compile_and_run() {
  auto command=Config::get().project.python_command+' '+filesystem::escape_argument(filesystem::get_short_path(Notebook::get().get_current_view()->file_path).string());
  Terminal::get().print("Running "+command+"\n");
  Terminal::get().async_process(command, Notebook::get().get_current_view()->file_path.parent_path(), [command](int exit_status) {
    Terminal::get().async_print(command+" returned: "+std::to_string(exit_status)+'\n');
  });
}

void Project::JavaScript::compile_and_run() {
  std::string command;
  boost::filesystem::path path;
  if(!build->project_path.empty()) {
    command="npm start";
    path=build->project_path;
  }
  else {
    auto view=Notebook::get().get_current_view();
    if(!view) {
      Info::get().print("No executable found");
      return;
    }
    command="node --harmony "+filesystem::escape_argument(filesystem::get_short_path(view->file_path).string());
    path=view->file_path.parent_path();
  }
  Terminal::get().print("Running "+command+"\n");
  Terminal::get().async_process(command, path, [command](int exit_status) {
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
  GError* error=nullptr;
#if GTK_VERSION_GE(3, 22)
  gtk_show_uri_on_window(nullptr, uri.c_str(), GDK_CURRENT_TIME, &error);
#else
  gtk_show_uri(nullptr, uri.c_str(), GDK_CURRENT_TIME, &error);
#endif
  g_clear_error(&error);
#endif
}

std::pair<std::string, std::string> Project::Rust::get_run_arguments() {
  auto project_path=build->project_path.string();
  auto run_arguments_it=run_arguments.find(project_path);
  std::string arguments;
  if(run_arguments_it!=run_arguments.end())
    arguments=run_arguments_it->second;
  
  if(arguments.empty())
    arguments=filesystem::get_short_path(build->get_executable(project_path)).string();
  
  return {project_path, arguments};
}

void Project::Rust::compile() {
  compiling=true;
  
  if(Config::get().project.clear_terminal_on_compile)
    Terminal::get().clear();
  
  Terminal::get().print("Compiling project "+filesystem::get_short_path(build->project_path).string()+"\n");
  
  auto command=build->get_compile_command();
  Terminal::get().async_process(command, build->project_path, [](int exit_status) {
    compiling=false;
  });
}

void Project::Rust::compile_and_run() {
  compiling=true;
  
  if(Config::get().project.clear_terminal_on_compile)
    Terminal::get().clear();
  
  auto arguments=get_run_arguments().second;
  Terminal::get().print("Compiling and running "+arguments+"\n");
  
  auto self=this->shared_from_this();
  Terminal::get().async_process(build->get_compile_command(), build->project_path, [self, arguments=std::move(arguments)](int exit_status) {
    compiling=false;
    if(exit_status==EXIT_SUCCESS) {
      Terminal::get().async_process(arguments, self->build->project_path, [arguments](int exit_status) {
        Terminal::get().async_print(arguments+" returned: "+std::to_string(exit_status)+'\n');
      });
    }
  });
}
