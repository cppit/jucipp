#include "terminal.h"
#include <iostream>
#include <thread>


Terminal::View::View(){
  scrolledwindow_.add(textview_);
  scrolledwindow_.set_size_request(-1,150);
  view_.add(scrolledwindow_);
  textview_.set_editable(false);
}


Terminal::Controller::Controller() {
  folder_command_ = "";
}

void Terminal::Controller::SetFolderCommand( boost::filesystem::path
					     CMake_path) {
  path_ = CMake_path.string();
  folder_command_ = "cd "+ path_ + "; ";
}

bool Terminal::Controller::Compile(){
  if (running.try_lock()) {
    std::thread execute([=]() {
	Terminal().get_buffer()->set_text("");
	ExecuteCommand("cmake .");
	if (ExistInConsole(cmake_sucsess)){
	  ExecuteCommand("make");
	}
      });
    execute.detach();
    running.unlock();
    if (ExistInConsole(make_built)) return true;
  }
  PrintMessage("juCi++ ERROR: Failed to compile project in directory"
	       + path + "\n");
  return false;
}

void Terminal::Controller::Run(std::string executable) {
  if (running.try_lock()) {
    std::thread execute([=]() {
	ExecuteCommand("./"+executable);
      });
    execute.detach();
    running.unlock();
  }
}

void Terminal::Controller::PrintMessage(std::string message){
  Terminal().get_buffer()->
y		   insert(Terminal().get_buffer()-> end(),"> "+message);
}

// bool Terminal::Controller::FindExecutable(std::string executable) {
//   std::string build = Terminal().get_buffer()->get_text();
//   double pos = build.find(make_built);
//   Gtk::TextIter start = Terminal().get_buffer()->get_iter_at_offset(pos);
//   Gtk::TextIter end = Terminal().get_buffer()->get_iter_at_offset(pos);
//   while (!end.ends_line())  {
//     end.forward_char();
//   }
//   build = Terminal().get_buffer()->get_text(start, end);
//   pos = build.find_last_of(" ");
//   build = build.substr(pos+1);
//   std::cout << "DEBUG: BUILD TARGET = "<< build << std::endl;
//   std::cout << "EDEBUG: ECUTABLE FILE = "<< executable << std::endl;
//   if(build != executable) return false;
//   return true;
// }

bool Terminal::Controller::ExistInConsole(std::string string) {
  double pos = Terminal().get_buffer()->
    get_text().find(string);
  if (pos == std::string::npos) return false;
  return true;
}

void Terminal::Controller::ExecuteCommand(std::string command) {
  command = folder_command_+command;
  std::cout << "EXECUTE COMMAND: "<< command << std::endl;
  FILE* p = popen(command.c_str(), "r");
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
