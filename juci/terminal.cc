#include "terminal.h"
#include <iostream>

Terminal::View::View(){
  scrolledwindow_.add(textview_);
  scrolledwindow_.set_size_request(-1,150);
  view_.add(scrolledwindow_);
}


Terminal::Controller::Controller() {
  root = "";
  Terminal().signal_key_release_event().
    connect(sigc::mem_fun(*this,&Terminal::Controller::OnButtonRealeaseEvenet),false);
}

bool Terminal::Controller::OnButtonRealeaseEvenet(GdkEventKey *key) {
  if(key->keyval == 65421 || key->keyval == 65293){
    ExecuteCommand();
  }
  return false;
}
void Terminal::Controller::ExecuteCommand() {

  std::cout << "EXECUTE COMMAND ALGORITHM "<< std::endl;
  std::string temp = getCommand();
  if(temp != ""){
    std::cout << "EXECUTE COMMAND: "<<temp << std::endl;  
    FILE* p = popen(temp.c_str(), "r"); 
    if (p == NULL) 
      { 
	Terminal().get_buffer()->insert(Terminal().get_buffer()->end(),"Command Failed\n");

      } else{
      char buffer[1028]; 
   
      while (fgets(buffer, 1028, p) != NULL) 
   	{ 
   	  Terminal().get_buffer()->insert(Terminal().get_buffer()->end(),buffer);
   	} 
      pclose(p); 

     
    }
    Terminal().get_buffer()->insert(Terminal().get_buffer()->end(),"Command Executed\n");
  }else{
    std::cout << "NO COMMAND TO RUN"<< std::endl;
  }
  
}

std::string Terminal::Controller::getCommand(){
  std::string command = "";
  Gtk::TextIter start,end;
  int a = Terminal().get_buffer()->get_insert()->get_iter().get_line()-1;
  if(a==-1)a=0;
  start = Terminal().get_buffer()->get_iter_at_line(a);
  end  =Terminal().get_buffer()->get_iter_at_line(a);
  while(!end.ends_line()) {
    end++;
  }
  while(!start.starts_line()) {
    start--;
  }
  
  command = Terminal().get_buffer()->get_text(start,end);
  return command;
}


