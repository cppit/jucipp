#include "terminal.h"
#include <iostream>
#include "logging.h"
#include "singletons.h"
#include <unistd.h>
#include <sys/wait.h>
#include <unordered_map>

#include <iostream> //TODO: remove
using namespace std; //TODO: remove

#define READ 0
#define WRITE 1

int execute_status=-1;
std::mutex async_and_sync_execute_mutex;
std::unordered_map<pid_t, std::pair<int, int> > async_execute_descriptors;
std::unordered_map<pid_t, int> async_execute_status;

//TODO: Windows...
//Coppied partially from http://www.linuxprogrammingblog.com/code-examples/sigaction
void signal_execl_exit(int sig, siginfo_t *siginfo, void *context) {
  async_and_sync_execute_mutex.lock();
  int status;
  while (waitpid(siginfo->si_pid, &status, WNOHANG) > 0) {}
  
  if(async_execute_descriptors.find(siginfo->si_pid)!=async_execute_descriptors.end()) {
    async_execute_status[siginfo->si_pid]=status;
    close(async_execute_descriptors.at(siginfo->si_pid).first);
    close(async_execute_descriptors.at(siginfo->si_pid).second);
    async_execute_descriptors.erase(siginfo->si_pid);
  }
  else
    execute_status=status;
  async_and_sync_execute_mutex.unlock();
}

//TODO: Windows...
//Copied from http://stackoverflow.com/questions/12778672/killing-process-that-has-been-created-with-popen2
pid_t popen2(const char *command, int *input_descriptor, int *output_descriptor) {
  pid_t pid;
  int p_stdin[2], p_stdout[2];

  if (pipe(p_stdin) != 0 || pipe(p_stdout) != 0)
    return -1;

  pid = fork();

  if (pid < 0)
    return pid;
  else if (pid == 0) {
    close(p_stdin[WRITE]);
    dup2(p_stdin[READ], READ);
    close(p_stdout[READ]);
    dup2(p_stdout[WRITE], WRITE);

    execl("/bin/sh", "sh", "-c", command, NULL);
    perror("execl");
    exit(1);
  }

  if (input_descriptor == NULL)
    close(p_stdin[WRITE]);
  else
    *input_descriptor = p_stdin[WRITE];

  if (output_descriptor == NULL)
    close(p_stdout[READ]);
  else
    *output_descriptor = p_stdout[READ];

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
  
  async_print_dispatcher.connect([this](){
    async_print_string_mutex.lock();
    if(async_print_string.size()>0) {
      print(async_print_string);
      async_print_string="";
    }
    async_print_string_mutex.unlock();
  });
  
  //Coppied from http://www.linuxprogrammingblog.com/code-examples/sigaction
  struct sigaction act;
	memset (&act, '\0', sizeof(act));
  act.sa_sigaction = &signal_execl_exit;
  act.sa_flags = SA_SIGINFO;
  sigaction(SIGCHLD, &act, NULL);
}

int Terminal::execute(const std::string &command, const std::string &path) {
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
    return -1;
  }
  else {
    char buffer[1024];
    while (fgets(buffer, 1024, p) != NULL) {
      print(buffer);
      while(gtk_events_pending())
        gtk_main_iteration();
    }
    async_and_sync_execute_mutex.lock();
    int exit_code=pclose(p);
    if(exit_code==-1)
      exit_code=execute_status;
    async_and_sync_execute_mutex.unlock();
    return exit_code;
  }
}

void Terminal::async_execute(const std::string &command, const std::string &path, std::function<void(int exit_code)> callback) {
  std::thread async_execute_thread([this, command, path, callback](){
    boost::filesystem::path boost_path;
    std::string cd_path_and_command;
    if(path!="") {
      boost_path=boost::filesystem::path(path);
    
      //TODO: Windows...
      cd_path_and_command="cd "+boost_path.string()+" 2>&1 && exec "+command;
    }
    else
      cd_path_and_command=command;
      
    int input_descriptor, output_descriptor;
    async_and_sync_execute_mutex.lock();
    auto pid=popen2(cd_path_and_command.c_str(), &input_descriptor, &output_descriptor);
    async_execute_descriptors[pid]={input_descriptor, output_descriptor};
    async_and_sync_execute_mutex.unlock();
    if (pid<=0)
      async_print("Error: Failed to run command" + command + "\n");
    else {
      char buffer[1024];
      ssize_t n;
      while ((n=read(output_descriptor, buffer, 1024)) > 0) {
        std::string message;
        for(int c=0;c<n;c++)
          message+=buffer[c];
        async_print(message);
      }
      async_and_sync_execute_mutex.lock();
      int exit_code=async_execute_status.at(pid);
      async_execute_status.erase(pid);
      async_and_sync_execute_mutex.unlock();
      if(callback)
        callback(exit_code);
    }
  });
  async_execute_thread.detach();
}

void Terminal::kill_executing() {
  async_and_sync_execute_mutex.lock();
  for(auto &pid: async_execute_descriptors)
    kill(pid.first, SIGTERM);
  async_and_sync_execute_mutex.unlock();
}

int Terminal::print(const std::string &message){
  INFO("Terminal: PrintMessage");
  text_view.get_buffer()->insert(text_view.get_buffer()->end(), message);
  return text_view.get_buffer()->end().get_line();
}

void Terminal::print(int line_nr, const std::string &message){
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

void Terminal::async_print(const std::string &message) {
  async_print_string_mutex.lock();
  bool dispatch=true;
  if(async_print_string.size()>0)
    dispatch=false;
  async_print_string+=message;
  async_print_string_mutex.unlock();
  if(dispatch)
    async_print_dispatcher();
}
