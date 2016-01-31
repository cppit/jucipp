#include "terminal.h"
#include <iostream>
#include "logging.h"
#include "config.h"
#ifdef JUCI_ENABLE_DEBUG
#include "debug.h"
#endif

Terminal::InProgress::InProgress(const std::string& start_msg): stop(false) {
  waiting_print.connect([this](){
    Terminal::get().async_print(line_nr-1, ".");
  });
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
        waiting_print();
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      c++;
    }
  });
}

void Terminal::InProgress::done(const std::string& msg) {
  if(!stop) {
    stop=true;
    Terminal::get().async_print(line_nr-1, msg);
  }
}

void Terminal::InProgress::cancel(const std::string& msg) {
  if(!stop) {
    stop=true;
    Terminal::get().async_print(line_nr-1, msg);
  }
}

Terminal::Terminal() {
  bold_tag=get_buffer()->create_tag();
  bold_tag->property_weight()=PANGO_WEIGHT_BOLD;
  
  async_print_dispatcher.connect([this](){
    async_print_strings_mutex.lock();
    if(async_print_strings.size()>0) {
      for(auto &string_bold: async_print_strings)
        print(string_bold.first, string_bold.second);
      async_print_strings.clear();
    }
    async_print_strings_mutex.unlock();
  });
  async_print_on_line_dispatcher.connect([this](){
    async_print_on_line_strings_mutex.lock();
    if(async_print_on_line_strings.size()>0) {
      for(auto &line_string: async_print_on_line_strings)
        print(line_string.first, line_string.second);
      async_print_on_line_strings.clear();
    }
    async_print_on_line_strings_mutex.unlock();
  });
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
    processes_mutex.lock();
    stdin_buffer.clear();
    std::shared_ptr<Process> process(new Process(command, path.string(), [this](const char* bytes, size_t n) {
      async_print(std::string(bytes, n));
    }, [this](const char* bytes, size_t n) {
      async_print(std::string(bytes, n), true);
    }, true));
    auto pid=process->get_id();
    if (pid<=0) {
      processes_mutex.unlock();
      async_print("Error: failed to run command: " + command + "\n", true);
      if(callback)
        callback(-1);
      return;
    }
    else {
      processes.emplace_back(process);
      processes_mutex.unlock();
    }
      
    auto exit_status=process->get_exit_status();
    
    processes_mutex.lock();
    for(auto it=processes.begin();it!=processes.end();it++) {
      if((*it)->get_id()==pid) {
        processes.erase(it);
        break;
      }
    }
    stdin_buffer.clear();
    processes_mutex.unlock();
      
    if(callback)
      callback(exit_status);
  });
  async_execute_thread.detach();
}

void Terminal::kill_last_async_process(bool force) {
  processes_mutex.lock();
  if(processes.size()>0)
    processes.back()->kill(force);
  processes_mutex.unlock();
}

void Terminal::kill_async_processes(bool force) {
  processes_mutex.lock();
  for(auto &process: processes)
    process->kill(force);
  processes_mutex.unlock();
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
  
  if(bold)
    get_buffer()->insert_with_tag(get_buffer()->end(), umessage, bold_tag);
  else
    get_buffer()->insert(get_buffer()->end(), umessage);
  
  if(get_buffer()->get_line_count()>Config::get().terminal.history_size) {
    int lines=get_buffer()->get_line_count()-Config::get().terminal.history_size;
    get_buffer()->erase(get_buffer()->begin(), get_buffer()->get_iter_at_line(lines));
    deleted_lines+=static_cast<size_t>(lines);
  }
  
  return static_cast<size_t>(get_buffer()->end().get_line())+deleted_lines;
}

void Terminal::print(size_t line_nr, const std::string &message){
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
}

std::shared_ptr<Terminal::InProgress> Terminal::print_in_progress(std::string start_msg) {
  std::shared_ptr<Terminal::InProgress> in_progress=std::shared_ptr<Terminal::InProgress>(new Terminal::InProgress(start_msg));
  return in_progress;
}

void Terminal::async_print(const std::string &message, bool bold) {
  async_print_strings_mutex.lock();
  bool dispatch=true;
  if(async_print_strings.size()>0)
    dispatch=false;
  async_print_strings.emplace_back(message, bold);
  async_print_strings_mutex.unlock();
  if(dispatch)
    async_print_dispatcher();
}

void Terminal::async_print(int line_nr, const std::string &message) {
  async_print_on_line_strings_mutex.lock();
  bool dispatch=true;
  if(async_print_on_line_strings.size()>0)
    dispatch=false;
  async_print_on_line_strings.emplace_back(line_nr, message);
  async_print_on_line_strings_mutex.unlock();
  if(dispatch)
    async_print_on_line_dispatcher();
}

bool Terminal::on_key_press_event(GdkEventKey *event) {
  processes_mutex.lock();
  bool debug_is_running=false;
#ifdef JUCI_ENABLE_DEBUG
  debug_is_running=Debug::get().is_running();
#endif
  if(processes.size()>0 || debug_is_running) {
    get_buffer()->place_cursor(get_buffer()->end());
    auto unicode=gdk_keyval_to_unicode(event->keyval);
    char chr=(char)unicode;
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
        Debug::get().write(stdin_buffer);
#endif
      }
      else
        processes.back()->write(stdin_buffer);
      get_buffer()->insert_at_cursor(stdin_buffer.substr(stdin_buffer.size()-1));
      stdin_buffer.clear();
    }
  }
  processes_mutex.unlock();
  return true;
}
