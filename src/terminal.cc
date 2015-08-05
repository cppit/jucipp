#include "terminal.h"
#include <iostream>
#include "logging.h"
#include "singletons.h"

Terminal::InProgress::InProgress(const std::string& start_msg): stop(false) {
  waiting_print.connect([this](){
    Singleton::terminal()->print(line_nr-1, ".");
  });
  start(start_msg);
}

Terminal::InProgress::~InProgress() {
  stop=true;
  if(wait_thread.joinable())
    wait_thread.join();
}

void Terminal::InProgress::start(const std::string& msg) {
  line_nr=Singleton::terminal()->print(msg+"...\n");
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
    Singleton::terminal()->print(line_nr-1, msg);
  }
}

void Terminal::InProgress::cancel(const std::string& msg) {
  if(!stop) {
    stop=true;
    Singleton::terminal()->print(line_nr-1, msg);
  }
}

Terminal::Terminal() {
  text_view.set_editable(false);
  scrolled_window.add(text_view);
  add(scrolled_window);
  
  text_view.signal_size_allocate().connect([this](Gtk::Allocation& allocation){
    auto end=text_view.get_buffer()->create_mark(text_view.get_buffer()->end());
    text_view.scroll_to(end);
    text_view.get_buffer()->delete_mark(end);
  });
  
  async_execute_print.connect([this](){
    print(async_execute_print_string);
    async_execute_print_finished=true;
  });
}

bool Terminal::execute(const std::string &command, const std::string &path) {
  boost::filesystem::path boost_path;
  std::string cd_path_and_command;
  if(path!="") {
    boost_path=boost::filesystem::path(path);
  
    //TODO: Windows...
    cd_path_and_command="cd "+boost_path.string()+" 2>&1 && "+command;
  }
  else
    cd_path_and_command=command;

  FILE* p;
  p = popen(cd_path_and_command.c_str(), "r");
  if (p == NULL) {
    print("Error: Failed to run command" + command + "\n");
    return false;
  }
  else {
    char buffer[1024];
    while (fgets(buffer, 1024, p) != NULL) {
      print(buffer);
      while(gtk_events_pending())
        gtk_main_iteration();
    }
    return pclose(p)==0;
  }
}

void Terminal::async_execute(const std::string &command, const std::string &path, std::function<void(bool success)> callback) {
  std::thread async_execute_thread([this, command, path, callback](){
    boost::filesystem::path boost_path;
    std::string cd_path_and_command;
    if(path!="") {
      boost_path=boost::filesystem::path(path);
    
      //TODO: Windows...
      cd_path_and_command="cd "+boost_path.string()+" 2>&1 && "+command;
    }
    else
      cd_path_and_command=command;
      
    FILE* p;
    p = popen(cd_path_and_command.c_str(), "r");
    if (p == NULL) {
      async_execute_print_string="Error: Failed to run command" + command + "\n";
      async_execute_print_finished=false;
      async_execute_print();
    }
    else {
      char buffer[1024];
      while (fgets(buffer, 1024, p) != NULL) {
        async_execute_print_string=buffer;
        async_execute_print_finished=false;
        async_execute_print();
        while(!async_execute_print_finished)
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      int exit_code=pclose(p);
      if(callback)
        callback(exit_code==0);
    }
  });
  async_execute_thread.detach();
}

int Terminal::print(std::string message){
  INFO("Terminal: PrintMessage");
  text_view.get_buffer()->insert(text_view.get_buffer()->end(), "> "+message);
  return text_view.get_buffer()->end().get_line();
}

void Terminal::print(int line_nr, std::string message){
  INFO("Terminal: PrintMessage at line " << line_nr);
  auto iter=text_view.get_buffer()->get_iter_at_line(line_nr);
  while(!iter.ends_line())
    iter++;
  text_view.get_buffer()->insert(iter, message);
}

std::shared_ptr<Terminal::InProgress> Terminal::print_in_progress(std::string start_msg) {
  std::shared_ptr<Terminal::InProgress> in_progress=std::shared_ptr<Terminal::InProgress>(new Terminal::InProgress(start_msg));
  return in_progress;
}
