#include "terminal.h"
#include <iostream>
#include "logging.h"
#include "singletons.h"
#include <unistd.h>
#include <sys/wait.h>

#include <iostream> //TODO: remove
using namespace std; //TODO: remove

//TODO: Windows...
//A working implementation of popen3, with all pipes getting closed properly. 
//TODO: Eidheim is going to publish this one on his github, along with example uses
pid_t popen3(const char *command, int &stdin, int &stdout, int &stderr) {
  pid_t pid;
  int p_stdin[2], p_stdout[2], p_stderr[2];

  if (pipe(p_stdin) != 0 || pipe(p_stdout) != 0 || pipe(p_stderr) != 0) {
    close(p_stdin[0]);
    close(p_stdout[0]);
    close(p_stderr[0]);
    close(p_stdin[1]);
    close(p_stdout[1]);
    close(p_stderr[1]);
    return -1;
  }
      
  pid = fork();

  if (pid < 0) {
    close(p_stdin[0]);
    close(p_stdout[0]);
    close(p_stderr[0]);
    close(p_stdin[1]);
    close(p_stdout[1]);
    close(p_stderr[1]);
    return pid;
  }
  else if (pid == 0) {
    close(p_stdin[1]);
    close(p_stdout[0]);
    close(p_stderr[0]);
    dup2(p_stdin[0], 0);
    dup2(p_stdout[1], 1);
    dup2(p_stderr[1], 2);

    setpgid(0, 0);
    execl("/bin/sh", "sh", "-c", command, NULL);
    perror("execl");
    exit(EXIT_FAILURE);
  }

  close(p_stdin[0]);
  close(p_stdout[1]);
  close(p_stderr[1]);
  
  stdin = p_stdin[1];
  stdout = p_stdout[0];
  stderr = p_stderr[0];

  return pid;
}

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
  
  bold_tag=text_view.get_buffer()->create_tag();
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
}

int Terminal::execute(const std::string &command, const boost::filesystem::path &path) {
  std::string cd_path_and_command;
  if(path!="") {
    //TODO: Windows...
    cd_path_and_command="cd "+path.string()+" && "+command;
  }
  else
    cd_path_and_command=command;
    
  int stdin, stdout, stderr;
  auto pid=popen3(cd_path_and_command.c_str(), stdin, stdout, stderr);
  
  if (pid<=0) {
    async_print("Error: Failed to run command: " + command + "\n");
    return -1;
  }
  else {
    std::thread stderr_thread([this, stderr](){
      char buffer[1024];
      ssize_t n;
      while ((n=read(stderr, buffer, 1024)) > 0) {
        std::string message;
        for(ssize_t c=0;c<n;c++)
          message+=buffer[c];
        async_print(message, true);
      }
    });
    stderr_thread.detach();
    std::thread stdout_thread([this, stdout](){
      char buffer[1024];
      ssize_t n;
      INFO("read");
      while ((n=read(stdout, buffer, 1024)) > 0) {
        std::string message;
        for(ssize_t c=0;c<n;c++)
          message+=buffer[c];
        async_print(message);
      }
    });
    stdout_thread.detach();
    
    int exit_code;
    waitpid(pid, &exit_code, 0);
    close(stdin);
    close(stdout);
    close(stderr);
    
    return exit_code;
  }
}

void Terminal::async_execute(const std::string &command, const boost::filesystem::path &path, std::function<void(int exit_code)> callback) {
  std::thread async_execute_thread([this, command, path, callback](){
    std::string cd_path_and_command;
    if(path!="") {
    
      //TODO: Windows...
      cd_path_and_command="cd "+path.string()+" && "+command;
    }
    else
      cd_path_and_command=command;
      
    int stdin, stdout, stderr;
    async_execute_pids_mutex.lock();
    auto pid=popen3(cd_path_and_command.c_str(), stdin, stdout, stderr);
    async_execute_pids.emplace_back(pid);
    async_execute_pids_mutex.unlock();
    
    if (pid<=0) {
      async_print("Error: Failed to run command: " + command + "\n");
      if(callback)
        callback(-1);
    }
    else {
      std::thread stderr_thread([this, stderr](){
        char buffer[1024];
        ssize_t n;
        while ((n=read(stderr, buffer, 1024)) > 0) {
          std::string message;
          for(ssize_t c=0;c<n;c++)
            message+=buffer[c];
          async_print(message, true);
        }
      });
      stderr_thread.detach();
      std::thread stdout_thread([this, stdout](){
        char buffer[1024];
        ssize_t n;
        INFO("read");
        while ((n=read(stdout, buffer, 1024)) > 0) {
          std::string message;
          for(ssize_t c=0;c<n;c++)
            message+=buffer[c];
          async_print(message);
        }
      });
      stdout_thread.detach();
      
      int exit_code;
      waitpid(pid, &exit_code, 0);
      async_execute_pids_mutex.lock();
      for(auto it=async_execute_pids.begin();it!=async_execute_pids.end();it++) {
        if(*it==pid) {
          async_execute_pids.erase(it);
          break;
        }
      }
      async_execute_pids_mutex.unlock();
      close(stdin);
      close(stdout);
      close(stderr);
      
      if(callback)
        callback(exit_code);
    }
  });
  async_execute_thread.detach();
}

void Terminal::kill_last_async_execute(bool force) {
  async_execute_pids_mutex.lock();
  if(force)
    kill(-async_execute_pids.back(), SIGTERM);
  else
    kill(-async_execute_pids.back(), SIGINT);
  async_execute_pids_mutex.unlock();
}

void Terminal::kill_async_executes(bool force) {
  async_execute_pids_mutex.lock();
  for(auto &pid: async_execute_pids) {
    if(force)
      kill(-pid, SIGTERM);
    else
      kill(-pid, SIGINT);
  }
  async_execute_pids_mutex.unlock();
}

int Terminal::print(const std::string &message, bool bold){
  INFO("Terminal: PrintMessage");
  if(bold)
    text_view.get_buffer()->insert_with_tag(text_view.get_buffer()->end(), message, bold_tag);
  else
    text_view.get_buffer()->insert(text_view.get_buffer()->end(), message);
  return text_view.get_buffer()->end().get_line();
}

void Terminal::print(int line_nr, const std::string &message, bool bold){
  INFO("Terminal: PrintMessage at line " << line_nr);
  auto iter=text_view.get_buffer()->get_iter_at_line(line_nr);
  while(!iter.ends_line())
    iter++;
  if(bold)
    text_view.get_buffer()->insert_with_tag(iter, message, bold_tag);
  else
    text_view.get_buffer()->insert(iter, message);
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
