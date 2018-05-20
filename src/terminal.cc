#include "terminal.h"
#include "config.h"
#include "project.h"
#include "info.h"
#include "notebook.h"
#include "filesystem.h"
#include <iostream>
#include <regex>
#include <thread>

Terminal::Terminal() {
  set_editable(false);
  
  bold_tag=get_buffer()->create_tag();
  bold_tag->property_weight()=Pango::WEIGHT_ULTRAHEAVY;
  
  link_tag=get_buffer()->create_tag();
  link_tag->property_underline()=Pango::Underline::UNDERLINE_SINGLE;
  
  link_mouse_cursor=Gdk::Cursor::create(Gdk::CursorType::HAND1);
  default_mouse_cursor=Gdk::Cursor::create(Gdk::CursorType::XTERM);
}

int Terminal::process(const std::string &command, const boost::filesystem::path &path, bool use_pipes) {  
  std::unique_ptr<TinyProcessLib::Process> process;
  if(use_pipes)
    process=std::make_unique<TinyProcessLib::Process>(command, path.string(), [this](const char* bytes, size_t n) {
      async_print(std::string(bytes, n));
    }, [this](const char* bytes, size_t n) {
      async_print(std::string(bytes, n), true);
    });
  else
    process=std::make_unique<TinyProcessLib::Process>(command, path.string());
    
  if(process->get_id()<=0) {
    async_print("Error: failed to run command: " + command + "\n", true);
    return -1;
  }
  
  return process->get_exit_status();
}

int Terminal::process(std::istream &stdin_stream, std::ostream &stdout_stream, const std::string &command, const boost::filesystem::path &path, std::ostream *stderr_stream) {
  TinyProcessLib::Process process(command, path.string(), [&stdout_stream](const char* bytes, size_t n) {
    Glib::ustring umessage(std::string(bytes, n));
    Glib::ustring::iterator iter;
    while(!umessage.validate(iter)) {
      auto next_char_iter=iter;
      next_char_iter++;
      umessage.replace(iter, next_char_iter, "?");
    }
    stdout_stream.write(umessage.data(), n);
  }, [this, stderr_stream](const char* bytes, size_t n) {
    if(stderr_stream)
      stderr_stream->write(bytes, n);
    else
      async_print(std::string(bytes, n), true);
  }, true);
  
  if(process.get_id()<=0) {
    async_print("Error: failed to run command: " + command + "\n", true);
    return -1;
  }
  
  char buffer[131072];
  for(;;) {
    stdin_stream.readsome(buffer, 131072);
    auto read_n=stdin_stream.gcount();
    if(read_n==0)
      break;
    if(!process.write(buffer, read_n)) {
      break;
    }
  }
  process.close_stdin();
  
  return process.get_exit_status();
}

void Terminal::async_process(const std::string &command, const boost::filesystem::path &path, const std::function<void(int exit_status)> &callback, bool quiet) {
  std::thread async_execute_thread([this, command, path, callback, quiet]() {
    std::unique_lock<std::mutex> processes_lock(processes_mutex);
    stdin_buffer.clear();
    auto process=std::make_shared<TinyProcessLib::Process>(command, path.string(), [this, quiet](const char* bytes, size_t n) {
      if(!quiet)
        async_print(std::string(bytes, n));
    }, [this, quiet](const char* bytes, size_t n) {
      if(!quiet)
        async_print(std::string(bytes, n), true);
    }, true);
    auto pid=process->get_id();
    if (pid<=0) {
      processes_lock.unlock();
      async_print("Error: failed to run command: " + command + "\n", true);
      if(callback)
        callback(-1);
      return;
    }
    else {
      processes.emplace_back(process);
      processes_lock.unlock();
    }
      
    auto exit_status=process->get_exit_status();
    
    processes_lock.lock();
    for(auto it=processes.begin();it!=processes.end();it++) {
      if((*it)->get_id()==pid) {
        processes.erase(it);
        break;
      }
    }
    stdin_buffer.clear();
    processes_lock.unlock();
      
    if(callback)
      callback(exit_status);
  });
  async_execute_thread.detach();
}

void Terminal::kill_last_async_process(bool force) {
  std::unique_lock<std::mutex> lock(processes_mutex);
  if(processes.empty())
    Info::get().print("No running processes");
  else
    processes.back()->kill(force);
}

void Terminal::kill_async_processes(bool force) {
  std::unique_lock<std::mutex> lock(processes_mutex);
  for(auto &process: processes)
    process->kill(force);
}

bool Terminal::on_motion_notify_event(GdkEventMotion *event) {
  Gtk::TextIter iter;
  int location_x, location_y;
  window_to_buffer_coords(Gtk::TextWindowType::TEXT_WINDOW_TEXT, event->x, event->y, location_x, location_y);
  get_iter_at_location(iter, location_x, location_y);
  if(iter.has_tag(link_tag))
    get_window(Gtk::TextWindowType::TEXT_WINDOW_TEXT)->set_cursor(link_mouse_cursor);
  else
    get_window(Gtk::TextWindowType::TEXT_WINDOW_TEXT)->set_cursor(default_mouse_cursor);
  
  // Workaround for drag-and-drop crash on MacOS
  // TODO 2018: check if this bug has been fixed
#ifdef __APPLE__
  if((event->state & GDK_BUTTON1_MASK) == 0)
    return Gtk::TextView::on_motion_notify_event(event);
  else {
    int x, y;
    window_to_buffer_coords(Gtk::TextWindowType::TEXT_WINDOW_TEXT, event->x, event->y, x, y);
    Gtk::TextIter iter;
    get_iter_at_location(iter, x, y);
    get_buffer()->select_range(get_buffer()->get_insert()->get_iter(), iter);
    return true;
  }
#else
  return Gtk::TextView::on_motion_notify_event(event);
#endif
  
  return Gtk::TextView::on_motion_notify_event(event);
}

std::tuple<size_t, size_t, std::string, std::string, std::string> Terminal::find_link(const std::string &line) {
  const static std::regex link_regex("^([A-Z]:)?([^:]+):([0-9]+):([0-9]+): .*$|" //compile warning/error/rename usages
                                     "^Assertion failed: .*file ([A-Z]:)?([^:]+), line ([0-9]+)\\.$|" //clang assert()
                                     "^[^:]*: ([A-Z]:)?([^:]+):([0-9]+): .* Assertion .* failed\\.$|" //gcc assert()
                                     "^ERROR:([A-Z]:)?([^:]+):([0-9]+):.*$"); //g_assert (glib.h)
  size_t start_position=-1, end_position=-1;
  std::string path, line_number, line_offset;
  std::smatch sm;
  if(std::regex_match(line, sm, link_regex)) {
    for(size_t sub=1;sub<link_regex.mark_count();) {
      size_t subs=sub==1?4:3;
      if(sm.length(sub+1)) {
        start_position=sm.position(sub+1)-sm.length(sub);
        end_position=sm.position(sub+subs-1)+sm.length(sub+subs-1);
        if(sm.length(sub))
          path+=sm[sub].str();
        path+=sm[sub+1].str();
        line_number=sm[sub+2].str();
        line_offset=subs==4?sm[sub+3].str():"1";
        break;
      }
      sub+=subs;
    }
  }
  return std::make_tuple(start_position, end_position, path, line_number, line_offset);
}

void Terminal::apply_link_tags(const Gtk::TextIter &start_iter, const Gtk::TextIter &end_iter) {
  auto iter=start_iter;
  Gtk::TextIter line_start;
  bool line_start_set=false;
  bool delimiter_found=false;
  bool dot_found=false;
  bool number_found=false;
  do {
    if(iter.starts_line()) {
      line_start=iter;
      line_start_set=true;
      delimiter_found=false;
      dot_found=false;
      number_found=false;
    }
    if(line_start_set && (*iter=='\\' || *iter=='/'))
      delimiter_found=true;
    else if(line_start_set && *iter=='.')
      dot_found=true;
    else if(line_start_set && (*iter>='0' && *iter<='9'))
      number_found=true;
    else if(line_start_set && delimiter_found && dot_found && number_found && iter.ends_line()) {
      auto line=get_buffer()->get_text(line_start, iter);
      //Convert to ascii for std::regex and Gtk::Iter::forward_chars
      for(size_t c=0;c<line.size();++c) {
        if(line[c]>127)
          line.replace(c, 1, "a");
      }
      auto link=find_link(line.raw());
      if(std::get<0>(link)!=static_cast<size_t>(-1)) {
        auto link_start=line_start;
        auto link_end=line_start;
        link_start.forward_chars(std::get<0>(link));
        link_end.forward_chars(std::get<1>(link));
        get_buffer()->apply_tag(link_tag, link_start, link_end);
      }
      line_start_set=false;
    }
  } while(iter.forward_char() && iter!=end_iter);
}

size_t Terminal::print(const std::string &message, bool bold){
#ifdef _WIN32
  //Remove color codes
  auto message_no_color=message; //copy here since operations on Glib::ustring is too slow
  size_t pos=0;
  while((pos=message_no_color.find('\e', pos))!=std::string::npos) {
    if((pos+2)>=message_no_color.size())
      break;
    if(message_no_color[pos+1]=='[') {
      size_t end_pos=pos+2;
      bool color_code_found=false;
      while(end_pos<message_no_color.size()) {
        if((message_no_color[end_pos]>='0' && message_no_color[end_pos]<='9') || message_no_color[end_pos]==';')
          end_pos++;
        else if(message_no_color[end_pos]=='m') {
          color_code_found=true;
          break;
        }
        else
          break;
      }
      if(color_code_found)
        message_no_color.erase(pos, end_pos-pos+1);
    }
  }
  Glib::ustring umessage=message_no_color;
#else
  Glib::ustring umessage=message;
#endif
  
  Glib::ustring::iterator iter;
  while(!umessage.validate(iter)) {
    auto next_char_iter=iter;
    next_char_iter++;
    umessage.replace(iter, next_char_iter, "?");
  }
  
  auto start_mark=get_buffer()->create_mark(get_buffer()->get_iter_at_line(get_buffer()->end().get_line()));
  if(bold)
    get_buffer()->insert_with_tag(get_buffer()->end(), umessage, bold_tag);
  else
    get_buffer()->insert(get_buffer()->end(), umessage);
  auto start_iter=start_mark->get_iter();
  get_buffer()->delete_mark(start_mark);
  auto end_iter=get_buffer()->get_insert()->get_iter();
  
  apply_link_tags(start_iter, end_iter);
  
  if(get_buffer()->get_line_count()>Config::get().terminal.history_size) {
    int lines=get_buffer()->get_line_count()-Config::get().terminal.history_size;
    get_buffer()->erase(get_buffer()->begin(), get_buffer()->get_iter_at_line(lines));
    deleted_lines+=static_cast<size_t>(lines);
  }
  
  return static_cast<size_t>(get_buffer()->end().get_line())+deleted_lines;
}

void Terminal::async_print(const std::string &message, bool bold) {
  dispatcher.post([message, bold] {
    Terminal::get().print(message, bold);
  });
}

void Terminal::async_print(size_t line_nr, const std::string &message) {
  dispatcher.post([this, line_nr, message] {
    if(line_nr<deleted_lines)
      return;
    
    Glib::ustring umessage=message;
    Glib::ustring::iterator iter;
    while(!umessage.validate(iter)) {
      auto next_char_iter=iter;
      next_char_iter++;
      umessage.replace(iter, next_char_iter, "?");
    }
    
    auto end_line_iter=get_buffer()->get_iter_at_line(static_cast<int>(line_nr-deleted_lines));
    while(!end_line_iter.ends_line() && end_line_iter.forward_char()) {}
    get_buffer()->insert(end_line_iter, umessage);
  });
}

void Terminal::configure() {
  link_tag->property_foreground_rgba()=get_style_context()->get_color(Gtk::StateFlags::STATE_FLAG_LINK);
  
  if(Config::get().terminal.font.size()>0) {
    override_font(Pango::FontDescription(Config::get().terminal.font));
  }
  else if(Config::get().source.font.size()>0) {
    Pango::FontDescription font_description(Config::get().source.font);
    auto font_description_size=font_description.get_size();
    if(font_description_size==0) {
      Pango::FontDescription default_font_description(Gtk::Settings::get_default()->property_gtk_font_name());
      font_description_size=default_font_description.get_size();
    }
    if(font_description_size>0)
      font_description.set_size(font_description_size*0.95);
    override_font(font_description);
  }
}

void Terminal::clear() {
  get_buffer()->set_text("");
}

bool Terminal::on_button_press_event(GdkEventButton* button_event) {
  //open clicked link in terminal
  if(button_event->type==GDK_BUTTON_PRESS && button_event->button==GDK_BUTTON_PRIMARY) {
    Gtk::TextIter iter;
    int location_x, location_y;
    window_to_buffer_coords(Gtk::TextWindowType::TEXT_WINDOW_TEXT, button_event->x, button_event->y, location_x, location_y);
    get_iter_at_location(iter, location_x, location_y);
    if(iter.has_tag(link_tag)) {
      auto start_iter=get_buffer()->get_iter_at_line(iter.get_line());
      auto end_iter=start_iter;
      while(!end_iter.ends_line() && end_iter.forward_char()) {}
      auto link=find_link(get_buffer()->get_text(start_iter, end_iter).raw());
      if(std::get<0>(link)!=static_cast<size_t>(-1)) {
        boost::filesystem::path path=std::get<2>(link);
        std::string line=std::get<3>(link);
        std::string index=std::get<4>(link);
        
        if(!path.empty() && *path.begin()=="~") { // boost::filesystem does not recognize ~
          boost::filesystem::path corrected_path;
          corrected_path=filesystem::get_home_path();
          if(!corrected_path.empty()) {
            auto it=path.begin();
            ++it;
            for(;it!=path.end();++it)
              corrected_path/=*it;
            path=corrected_path;
          }
        }
        
        if(path.is_relative()) {
          if(Project::current) {
            auto absolute_path=Project::current->build->get_default_path()/path;
            if(boost::filesystem::exists(absolute_path))
              path=absolute_path;
            else
              path=Project::current->build->get_debug_path()/path;
          }
          else
            return Gtk::TextView::on_button_press_event(button_event);
        }
        if(boost::filesystem::is_regular_file(path)) {
          Notebook::get().open(path);
          if(auto view=Notebook::get().get_current_view()) {
            try {
              int line_int = std::stoi(line)-1;
              int index_int = std::stoi(index)-1;
              view->place_cursor_at_line_index(line_int, index_int);
              view->scroll_to_cursor_delayed(view, true, true);
              return true;
            }
            catch(...) {}
          }
        }
      }
    }
  }
  return Gtk::TextView::on_button_press_event(button_event);
}

bool Terminal::on_key_press_event(GdkEventKey *event) {
  std::unique_lock<std::mutex> lock(processes_mutex);
  bool debug_is_running=false;
#ifdef JUCI_ENABLE_DEBUG
  debug_is_running=Project::current?Project::current->debug_is_running():false;
#endif
  if(processes.size()>0 || debug_is_running) {
    auto unicode=gdk_keyval_to_unicode(event->keyval);
    if(unicode>=32 && unicode!=126 && unicode!=0) {
      get_buffer()->place_cursor(get_buffer()->end());
      stdin_buffer+=unicode;
      get_buffer()->insert_at_cursor(stdin_buffer.substr(stdin_buffer.size()-1));
    }
    else if(event->keyval==GDK_KEY_BackSpace) {
      get_buffer()->place_cursor(get_buffer()->end());
      if(stdin_buffer.size()>0 && get_buffer()->get_char_count()>0) {
        auto iter=get_buffer()->end();
        iter--;
        stdin_buffer.erase(stdin_buffer.size()-1);
        get_buffer()->erase(iter, get_buffer()->end());
      }
    }
    else if(event->keyval==GDK_KEY_Return || event->keyval==GDK_KEY_KP_Enter) {
      get_buffer()->place_cursor(get_buffer()->end());
      stdin_buffer+='\n';
      if(debug_is_running) {
#ifdef JUCI_ENABLE_DEBUG
        Project::current->debug_write(stdin_buffer);
#endif
      }
      else
        processes.back()->write(stdin_buffer);
      get_buffer()->insert_at_cursor(stdin_buffer.substr(stdin_buffer.size()-1));
      stdin_buffer.clear();
    }
  }
  return true;
}
