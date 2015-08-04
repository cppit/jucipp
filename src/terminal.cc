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
  
  change_folder_command = "";
  text_view.signal_size_allocate().connect([this](Gtk::Allocation& allocation){
    auto end=text_view.get_buffer()->create_mark(text_view.get_buffer()->end());
    text_view.scroll_to(end);
    text_view.get_buffer()->delete_mark(end);
  });
}

bool Terminal::execute(const std::string &path, const std::string &command) {
  boost::filesystem::path boost_path;
  if(path=="")
    boost_path=boost::filesystem::current_path();
  else
    boost_path=boost::filesystem::path(path);
  
  //TODO: Windows...
  auto cd_path_and_command="cd "+boost_path.string()+" 2>&1 && "+command;

  FILE* p = NULL;
  p = popen(cd_path_and_command.c_str(), "r");
  if (p == NULL) {
    print("Error: Failed to run command" + command + "\n");
    return false;
  }
  else {
    char buffer[1028];
    while (fgets(buffer, 1028, p) != NULL) {
      print(buffer);
    }
    int exit_code=pclose(p);
    if(exit_code==0)
      return true;
    else
      return false;
  }
}

void Terminal::async_execute(const std::string &path, const std::string &command) {
  
}

void Terminal::set_change_folder_command(boost::filesystem::path CMake_path) {
  INFO("Terminal: set_change_folder_command");
  path = CMake_path.string();
  change_folder_command = "cd "+ path + "; ";
}

void Terminal::compile() {
  INFO("Terminal: compile");
  text_view.get_buffer()->set_text("");
  DEBUG("Terminal: compile: running cmake command");
  std::vector<std::string> commands = Singleton::Config::terminal()->compile_commands;
  for (size_t it = 0; it < commands.size(); ++it) {
    execute_command(commands.at(it), "r");
  }
  print("\n");
  DEBUG("Terminal: compile: compile done");
}

void Terminal::run(std::string executable) {
  INFO("Terminal: run");
  print("juCi++ execute: " + executable + "\n");
  DEBUG("Terminal: compile: running run command: ");
  DEBUG_VAR(executable);
  execute_command("cd "+Singleton::Config::terminal()->run_command + "; ./"+executable, "r");
  print("\n");
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

void Terminal::execute_command(std::string command, std::string mode) {
  INFO("Terminal: execute_command");
  command = change_folder_command+command;
  DEBUG("Terminal: PrintMessage: running command");
  FILE* p = NULL;
  std::cout << command << std::endl;
  p = popen(command.c_str(), mode.c_str());
  if (p == NULL) {
    print("juCi++ ERROR: Failed to run command" + command + "\n");
  }
  else {
    char buffer[1028];
    while (fgets(buffer, 1028, p) != NULL) {
      print(buffer);
    }
    pclose(p); 
  }
}
