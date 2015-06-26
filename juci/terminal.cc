#include "terminal.h"
#include <iostream>
#include <thread>
#include <atomic>
#include "logging.h"

Terminal::View::View(){
  text_view.set_editable(false);
  scrolled_window.add(text_view);
  add(scrolled_window);
}


Terminal::Controller::Controller(Terminal::Config& cfg) :
  config(cfg) {  
  folder_command_ = "";
}

void Terminal::Controller::SetFolderCommand( boost::filesystem::path
					     CMake_path) {
  INFO("Terminal: SetFolderCommand");
  path_ = CMake_path.string();
  folder_command_ = "cd "+ path_ + "; ";
}

void Terminal::Controller::Compile(){
  INFO("Terminal: Compile");

  view.text_view.get_buffer()->set_text("");
  DEBUG("Terminal: Compile: running cmake command");
  std::vector<std::string> commands = config.compile_commands;
  for (size_t it = 0; it < commands.size(); ++it) {
    ExecuteCommand(commands.at(it), "r");
    
  }
  PrintMessage("\n");
  DEBUG("Terminal: Compile: compile done");
}

void Terminal::Controller::Run(std::string executable) {
  INFO("Terminal: Run");
  PrintMessage("juCi++ execute: " + executable + "\n");
  DEBUG("Terminal: Compile: running run command: ");
  DEBUG_VAR(executable);
  ExecuteCommand("cd "+config.run_command + "; ./"+executable, "r");
  PrintMessage("\n");
}

int Terminal::Controller::PrintMessage(std::string message){
  INFO("Terminal: PrintMessage");
  view.text_view.get_buffer()->insert(view.text_view.get_buffer()->end(), "> "+message);
  auto mark_end=view.text_view.get_buffer()->create_mark(view.text_view.get_buffer()->end());
  view.text_view.scroll_to(mark_end);
  return mark_end->get_iter().get_line();
}

void Terminal::Controller::PrintMessage(int line_nr, std::string message){
  INFO("Terminal: PrintMessage at line " << line_nr);
  auto iter=view.text_view.get_buffer()->get_iter_at_line(line_nr);
  while(!iter.ends_line())
    iter++;
  view.text_view.get_buffer()->insert(iter, message);
}

std::function<void()> Terminal::Controller::PrintMessage(std::string start_msg, std::string done_msg) {
  int line_nr=PrintMessage(start_msg+"...\n");
  
  std::shared_ptr<std::atomic<bool> > stop(new std::atomic<bool>(false));
  std::shared_ptr<Glib::Dispatcher> waiting_print(new Glib::Dispatcher());
  waiting_print->connect([this, line_nr](){
    PrintMessage(line_nr-1, ".");
  });
  
  std::shared_ptr<std::thread> wait_thread(new std::thread([this, stop, waiting_print](){
    size_t c=0;
    while(!*stop) {
      if(c%100==0)
        (*waiting_print)();
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      c++;
    }
  }));
  
  std::function<void()> done=[this, line_nr, stop, done_msg, waiting_print, wait_thread](){
    *stop=true;
    PrintMessage(line_nr-1, done_msg);
    if(wait_thread->joinable())
      wait_thread->join(); //waiting_print has to be destroyed in main thread
  };
  return done;
}

bool Terminal::Controller::ExistInConsole(std::string string) {
    INFO("Terminal: ExistInConsole");
    DEBUG("Terminal: PrintMessage: finding string in buffer");
  double pos = view.text_view.get_buffer()->
    get_text().find(string);
  if (pos == std::string::npos) return false;
  return true;
}

void Terminal::Controller::ExecuteCommand(std::string command, std::string mode) {
  INFO("Terminal: ExecuteCommand");
  command = folder_command_+command;
  DEBUG("Terminal: PrintMessage: Running command");
  FILE* p = NULL;
  std::cout << command << std::endl;
  p = popen(command.c_str(), mode.c_str());
  if (p == NULL) {
    PrintMessage("juCi++ ERROR: Failed to run command" + command + "\n");
  }else {
    char buffer[1028];
    while (fgets(buffer, 1028, p) != NULL) {
      PrintMessage(buffer);
    }
    pclose(p); 
  }

}
