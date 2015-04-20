#include "terminal.h"
#include <iostream>
#include <thread>

Terminal::View::View(){
  scrolledwindow_.add(textview_);
  scrolledwindow_.set_size_request(-1,150);
  view_.add(scrolledwindow_);
  textview_.set_editable(false);
  //Pango::TabArray tabsize;
  //tabsize.set_tab(200,Pango::TAB_LEFT, 200);                                  
  //textview_.set_tabs(tabsize);
}


Terminal::Controller::Controller() {
  folder_command_ = "";
}

void Terminal::Controller::SetFolderCommand(std::string path) {
  int pos = path.find_last_of("/\\");
  path.erase(path.begin()+pos,path.end());
  folder_command_ = "cd "+ path + "; ";
}

void Terminal::Controller::CompileAndRun(std::string project_name) {
   if (folder_command_=="") {
     PrintMessage("juCi++ ERROR: Can not find project's CMakeList.txt\n");
   } else {
     if (running.try_lock()) {
       std::thread execute([=]() {
	   Terminal().get_buffer()->set_text("");
	   ExecuteCommand("cmake .");
	   if (ExistInConsole(cmake_sucsess)){
	     ExecuteCommand("make");
	     if (ExistInConsole(make_built)){
	       if (FindExecutable(project_name)) {
		 ExecuteCommand("./"+project_name);
	       } else {
		 PrintMessage("juCi++ ERROR: Can not find Executable\n");
	       }
	     }
	   }
	 });
       execute.detach();
       running.unlock();
     }
   }
}

void Terminal::Controller::PrintMessage(std::string message){
  Terminal().get_buffer()->
		   insert(Terminal().get_buffer()-> end(),"> "+message);
}

bool Terminal::Controller::FindExecutable(std::string executable) {
  std::string build = Terminal().get_buffer()->get_text();
  double pos = build.find(make_built);
  Gtk::TextIter start = Terminal().get_buffer()->get_iter_at_offset(pos);
  Gtk::TextIter end = Terminal().get_buffer()->get_iter_at_offset(pos);
  while (!end.ends_line())  {
    end.forward_char();
  }
  build = Terminal().get_buffer()->get_text(start, end);
  pos = build.find_last_of(" ");
    std::cout << "FINNER NY POS" << std::endl;
  build = build.substr(pos+1);
  std::cout <<"BUILD TARGET = "<< build << std::endl;
  std::cout << "EXECUTABLE FILE = "<< executable << std::endl;
  if(build != executable) return false;
  return true;
}

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
