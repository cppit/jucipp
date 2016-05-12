#include "terminal.h"
#include "config.h"
#include "project.h"
#include "info.h"
#include "notebook.h"
#include <iostream>
//Temporary fix for current Arch Linux boost linking problem
#ifdef __GNUC_PREREQ
#if __GNUC_PREREQ(5,1)
#include <regex>
#define REGEX_NS std
#endif
#endif
#ifndef REGEX_NS
#include <boost/regex.hpp>
#define REGEX_NS boost
#endif

Terminal::InProgress::InProgress(const std::string& start_msg): stop(false) {
  start(start_msg);
}

Terminal::InProgress::~InProgress() {
  stop=true;
  if(wait_thread.joinable())
    wait_thread.join();
}

void Terminal::InProgress::start(const std::string& msg) {
  line_nr=Terminal::get().print(msg+"...\n");
  wait_thread=std::thread([this](){
    size_t c=0;
    while(!stop) {
      if(c%100==0)
        Terminal::get().async_print(line_nr-1, ".");
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      c++;
    }
  });
}

void Terminal::InProgress::done(const std::string& msg) {
  bool expected=false;
  if(stop.compare_exchange_strong(expected, true))
    Terminal::get().async_print(line_nr-1, msg);
}

void Terminal::InProgress::cancel(const std::string& msg) {
  bool expected=false;
  if(stop.compare_exchange_strong(expected, true))
    Terminal::get().async_print(line_nr-1, msg);
}

Terminal::Terminal() {
  bold_tag=get_buffer()->create_tag();
  bold_tag->property_weight()=PANGO_WEIGHT_BOLD;
  
  link_tag=get_buffer()->create_tag();
  link_tag->property_underline()=Pango::Underline::UNDERLINE_SINGLE;
  
  link_mouse_cursor=Gdk::Cursor::create(Gdk::CursorType::HAND1);
  default_mouse_cursor=Gdk::Cursor::create(Gdk::CursorType::XTERM);
}

int Terminal::process(const std::string &command, const boost::filesystem::path &path, bool use_pipes) {  
  std::unique_ptr<Process> process;
  if(use_pipes)
    process=std::unique_ptr<Process>(new Process(command, path.string(), [this](const char* bytes, size_t n) {
      async_print(std::string(bytes, n));
    }, [this](const char* bytes, size_t n) {
      async_print(std::string(bytes, n), true);
    }));
  else
    process=std::unique_ptr<Process>(new Process(command, path.string()));
    
  if(process->get_id()<=0) {
    async_print("Error: failed to run command: " + command + "\n", true);
    return -1;
  }
  
  return process->get_exit_status();
}

int Terminal::process(std::istream &stdin_stream, std::ostream &stdout_stream, const std::string &command, const boost::filesystem::path &path) {
  Process process(command, path.string(), [this, &stdout_stream](const char* bytes, size_t n) {
    Glib::ustring umessage(std::string(bytes, n));
    Glib::ustring::iterator iter;
    while(!umessage.validate(iter)) {
      auto next_char_iter=iter;
      next_char_iter++;
      umessage.replace(iter, next_char_iter, "?");
    }
    stdout_stream.write(umessage.data(), n);
  }, [this](const char* bytes, size_t n) {
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

void Terminal::async_process(const std::string &command, const boost::filesystem::path &path, std::function<void(int exit_status)> callback) {
  std::thread async_execute_thread([this, command, path, callback](){
    std::unique_lock<std::mutex> processes_lock(processes_mutex);
    stdin_buffer.clear();
    std::shared_ptr<Process> process(new Process(command, path.string(), [this](const char* bytes, size_t n) {
      async_print(std::string(bytes, n));
    }, [this](const char* bytes, size_t n) {
      async_print(std::string(bytes, n), true);
    }, true));
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

bool Terminal::on_motion_notify_event(GdkEventMotion *motion_event) {
  Gtk::TextIter iter;
  int location_x, location_y;
  window_to_buffer_coords(Gtk::TextWindowType::TEXT_WINDOW_TEXT, motion_event->x, motion_event->y, location_x, location_y);
  get_iter_at_location(iter, location_x, location_y);
  if(iter.has_tag(link_tag))
    get_window(Gtk::TextWindowType::TEXT_WINDOW_TEXT)->set_cursor(link_mouse_cursor);
  else
    get_window(Gtk::TextWindowType::TEXT_WINDOW_TEXT)->set_cursor(default_mouse_cursor);
  return Gtk::TextView::on_motion_notify_event(motion_event);
}

void Terminal::apply_link_tags(Gtk::TextIter start_iter, Gtk::TextIter end_iter) {
  auto iter=start_iter;
  size_t colons=0;
  Gtk::TextIter start_path_iter;
  bool possible_path=false;
  //Search for path with line and index
  //Simple implementation. Not sure if it is work the effort to make it work 100% on all platforms.
  do {
    if(iter.starts_line()) {
      start_path_iter=iter;
      possible_path=true;
      colons=0;
    }
    if(possible_path) {
      if(*iter==' ' || *iter=='\t' || iter.ends_line())
        possible_path=false;
      if(*iter==':') {
        colons++;
#ifdef _WIN32
        if(colons==4 && possible_path)
#else
        if(colons==3 && possible_path)
#endif
          get_buffer()->apply_tag(link_tag, start_path_iter, iter);
      }
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
  
  auto start_mark=get_buffer()->create_mark(get_buffer()->get_iter_at_line(get_buffer()->get_insert()->get_iter().get_line()));
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

std::shared_ptr<Terminal::InProgress> Terminal::print_in_progress(std::string start_msg) {
  auto in_progress=std::shared_ptr<Terminal::InProgress>(new Terminal::InProgress(start_msg), [this](Terminal::InProgress *in_progress) {
    {
      std::unique_lock<std::mutex> lock(in_progresses_mutex);
      in_progresses.erase(in_progress);
    }
    delete in_progress;
  });
  {
    std::unique_lock<std::mutex> lock(in_progresses_mutex);
    in_progresses.emplace(in_progress.get());
  }
  return in_progress;
}

void Terminal::async_print(const std::string &message, bool bold) {
  dispatcher.post([this, message, bold] {
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
#if GTKMM_MAJOR_VERSION>3 || (GTKMM_MAJOR_VERSION>=3 && GTKMM_MINOR_VERSION>=12)
  link_tag->property_foreground_rgba()=get_style_context()->get_color(Gtk::StateFlags::STATE_FLAG_LINK);
#endif
  
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
  {
    std::unique_lock<std::mutex> lock(in_progresses_mutex);
    for(auto &in_progress: in_progresses)
      in_progress->stop=true;
  }
  while(g_main_context_pending(NULL))
    g_main_context_iteration(NULL, false);
  get_buffer()->set_text("");
}

bool Terminal::on_button_press_event(GdkEventButton* button_event) {
  if(button_event->type==GDK_BUTTON_PRESS && button_event->button==GDK_BUTTON_PRIMARY) {
    Gtk::TextIter iter;
    int location_x, location_y;
    window_to_buffer_coords(Gtk::TextWindowType::TEXT_WINDOW_TEXT, button_event->x, button_event->y, location_x, location_y);
    get_iter_at_location(iter, location_x, location_y);
    auto start_iter=iter;
    auto end_iter=iter;
    if(iter.has_tag(link_tag) &&
       start_iter.backward_to_tag_toggle(link_tag) && end_iter.forward_to_tag_toggle(link_tag)) {
      std::string path_str=get_buffer()->get_text(start_iter, end_iter);
#ifdef _WIN32
      const static REGEX_NS::regex path_regex("^([A-Z]:[^:]+):([0-9]+):([0-9]+)$");
#else
      const static REGEX_NS::regex path_regex("^([^:]+):([0-9]+):([0-9]+)$");
#endif
      REGEX_NS::smatch sm;
      if(REGEX_NS::regex_match(path_str, sm, path_regex)) {
        boost::system::error_code ec;
        auto path=boost::filesystem::canonical(sm[1].str(), ec);
        if(!ec && boost::filesystem::is_regular_file(path)) {
          Notebook::get().open(path);
          if(Notebook::get().get_current_page()!=-1) {
            auto view=Notebook::get().get_current_view();
            try {
              int line = std::stoi(sm[2].str())-1;
              int index = std::stoi(sm[3].str())-1;
              line=std::min(line, view->get_buffer()->get_line_count()-1);
              if(line>=0) {
                auto iter=view->get_buffer()->get_iter_at_line(line);
                while(!iter.ends_line())
                  iter.forward_char();
                auto end_line_index=iter.get_line_index();
                index=std::min(index, end_line_index);
                
                view->get_buffer()->place_cursor(view->get_buffer()->get_iter_at_line_index(line, index));
                view->scroll_to_cursor_delayed(view, true, true);
                return true;
              }
            }
            catch(const std::exception &) {}
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
  debug_is_running=Project::current_language?Project::current_language->debug_is_running():false;
#endif
  if(processes.size()>0 || debug_is_running) {
    get_buffer()->place_cursor(get_buffer()->end());
    auto unicode=gdk_keyval_to_unicode(event->keyval);
    char chr=static_cast<char>(unicode);
    if(unicode>=32 && unicode<=126) {
      stdin_buffer+=chr;
      get_buffer()->insert_at_cursor(stdin_buffer.substr(stdin_buffer.size()-1));
    }
    else if(event->keyval==GDK_KEY_BackSpace) {
      if(stdin_buffer.size()>0 && get_buffer()->get_char_count()>0) {
        auto iter=get_buffer()->end();
        iter--;
        stdin_buffer.pop_back();
        get_buffer()->erase(iter, get_buffer()->end());
      }
    }
    else if(event->keyval==GDK_KEY_Return) {
      stdin_buffer+='\n';
      if(debug_is_running) {
#ifdef JUCI_ENABLE_DEBUG
        Project::current_language->debug_write(stdin_buffer);
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
