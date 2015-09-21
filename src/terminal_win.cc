#include "terminal.h"
#include <iostream>
#include "logging.h"
#include "singletons.h"
#include <unistd.h>
#include <windows.h>

#include <iostream> //TODO: remove
using namespace std; //TODO: remove

#define BUFSIZE 1024

HANDLE popen3(const std::string &command, const std::string &path, HANDLE *stdin_h, HANDLE *stdout_h, HANDLE *stderr_h) {
  HANDLE g_hChildStd_IN_Rd = NULL;
  HANDLE g_hChildStd_IN_Wr = NULL;
  HANDLE g_hChildStd_OUT_Rd = NULL;
  HANDLE g_hChildStd_OUT_Wr = NULL;
  HANDLE g_hChildStd_ERR_Rd = NULL;
  HANDLE g_hChildStd_ERR_Wr = NULL;

  SECURITY_ATTRIBUTES saAttr;

  saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
  saAttr.bInheritHandle = TRUE; 
  saAttr.lpSecurityDescriptor = NULL; 

  if (!CreatePipe(&g_hChildStd_IN_Rd, &g_hChildStd_IN_Wr, &saAttr, 0)) 
    return NULL;
  if(!SetHandleInformation(g_hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0)) {
    CloseHandle(g_hChildStd_IN_Rd);
    CloseHandle(g_hChildStd_IN_Wr);
    return NULL;
  }
  if (!CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0)) {
    CloseHandle(g_hChildStd_IN_Rd);
    CloseHandle(g_hChildStd_IN_Wr);
    return NULL;
  }
  if(!SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) {
    CloseHandle(g_hChildStd_IN_Rd);
    CloseHandle(g_hChildStd_IN_Wr);
    CloseHandle(g_hChildStd_OUT_Rd);
    CloseHandle(g_hChildStd_OUT_Wr);
    return NULL;
  }
  if (!CreatePipe(&g_hChildStd_ERR_Rd, &g_hChildStd_ERR_Wr, &saAttr, 0)) {
    CloseHandle(g_hChildStd_IN_Rd);
    CloseHandle(g_hChildStd_IN_Wr);
    CloseHandle(g_hChildStd_OUT_Rd);
    CloseHandle(g_hChildStd_OUT_Wr);
    return NULL;
  }
  if(!SetHandleInformation(g_hChildStd_ERR_Rd, HANDLE_FLAG_INHERIT, 0)) {
    CloseHandle(g_hChildStd_IN_Rd);
    CloseHandle(g_hChildStd_IN_Wr);
    CloseHandle(g_hChildStd_OUT_Rd);
    CloseHandle(g_hChildStd_OUT_Wr);
    CloseHandle(g_hChildStd_ERR_Rd);
    CloseHandle(g_hChildStd_ERR_Wr);
    return NULL;
  }

  PROCESS_INFORMATION process_info; 
  STARTUPINFO siStartInfo;

  ZeroMemory(&process_info, sizeof(PROCESS_INFORMATION));

  ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
  siStartInfo.cb = sizeof(STARTUPINFO); 
  siStartInfo.hStdError = g_hChildStd_ERR_Wr;
  siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
  siStartInfo.hStdInput = g_hChildStd_IN_Rd;
  siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

  char* path_ptr;
  if(path=="")
    path_ptr=NULL;
  else {
    path_ptr=new char[path.size()+1];
    std::strcpy(path_ptr, path.c_str());
  }
  char* command_cstr=new char[command.size()+1];
  std::strcpy(command_cstr, command.c_str());
  BOOL bSuccess = CreateProcess(NULL, 
				command_cstr,  // command line
				NULL,          // process security attributes
				NULL,          // primary thread security attributes
				TRUE,          // handles are inherited
				0,             // creation flags
				NULL,          // use parent's environment
				path_ptr,      // use parent's current directory
				&siStartInfo,  // STARTUPINFO pointer
				&process_info);  // receives PROCESS_INFORMATION
   
  if(!bSuccess) {
    CloseHandle(process_info.hProcess);
    CloseHandle(process_info.hThread);
    CloseHandle(g_hChildStd_IN_Rd);
    CloseHandle(g_hChildStd_OUT_Wr);
    CloseHandle(g_hChildStd_ERR_Wr);
    return NULL;
  }
  else {
    // Close handles to the child process and its primary thread.
    // Some applications might keep these handles to monitor the status
    // of the child process, for example. 
    
    CloseHandle(process_info.hThread);
    CloseHandle(g_hChildStd_IN_Rd);
    CloseHandle(g_hChildStd_OUT_Wr);
    CloseHandle(g_hChildStd_ERR_Wr);
  }

  *stdin_h=g_hChildStd_IN_Wr;
  *stdout_h=g_hChildStd_OUT_Rd;
  *stderr_h=g_hChildStd_ERR_Rd;
  return process_info.hProcess;
}

Terminal::InProgress::InProgress(const std::string& start_msg): stop(false) {
  waiting_print.connect([this](){
    Singleton::terminal()->async_print(line_nr-1, ".");
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
    Singleton::terminal()->async_print(line_nr-1, msg);
  }
}

void Terminal::InProgress::cancel(const std::string& msg) {
  if(!stop) {
    stop=true;
    Singleton::terminal()->async_print(line_nr-1, msg);
  }
}

Terminal::Terminal() {
  bold_tag=get_buffer()->create_tag();
  bold_tag->property_weight()=PANGO_WEIGHT_BOLD;
  
  signal_size_allocate().connect([this](Gtk::Allocation& allocation){
    auto iter=get_buffer()->end();
    if(iter.backward_char()) {
      auto mark=get_buffer()->create_mark(iter);
      scroll_to(mark, 0.0, 1.0, 1.0);
      get_buffer()->delete_mark(mark);
    }
  });
  
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

//Based on the example at https://msdn.microsoft.com/en-us/library/windows/desktop/ms682499(v=vs.85).aspx
int Terminal::execute(const std::string &command, const boost::filesystem::path &path) {
  HANDLE stdin_h, stdout_h, stderr_h;

  auto process=popen3(command, path.string(), &stdin_h, &stdout_h, &stderr_h);
  if(process==NULL) {
    async_print("Error: Failed to run command: " + command + "\n");
    return -1;
  }
  std::thread stderr_thread([this, stderr_h](){
    DWORD n;
    CHAR buffer[BUFSIZE];
    for (;;) {
      BOOL bSuccess = ReadFile(stderr_h, buffer, BUFSIZE, &n, NULL);
      if(!bSuccess || n == 0)
	break;

      std::string message;
      for(DWORD c=0;c<n;c++)
	message+=buffer[c];
      async_print(message, true);
    }
  });
  stderr_thread.detach();

  std::thread stdout_thread([this, stdout_h](){
    DWORD n;
    CHAR buffer[BUFSIZE];
    for (;;) {
      BOOL bSuccess = ReadFile(stdout_h, buffer, BUFSIZE, &n, NULL);
      if(!bSuccess || n == 0)
	break;

      std::string message;
      for(DWORD c=0;c<n;c++)
	message+=buffer[c];
      async_print(message);
    }
  });
  stdout_thread.detach();

  unsigned long exit_code;
  WaitForSingleObject(process, INFINITE);
  GetExitCodeProcess(process, &exit_code);
  
  CloseHandle(process);
  CloseHandle(stdin_h);
  CloseHandle(stdout_h);
  CloseHandle(stderr_h);
  return exit_code;
}

void Terminal::async_execute(const std::string &command, const boost::filesystem::path &path, std::function<void(int exit_code)> callback) {
  std::thread async_execute_thread([this, command, path, callback](){
    HANDLE stdin_h, stdout_h, stderr_h;

    async_executes_mutex.lock();
    stdin_buffer.clear();
    auto process=popen3(command, path.string(), &stdin_h, &stdout_h, &stderr_h);
    if(process==NULL) {
      async_executes_mutex.unlock();
      async_print("Error: Failed to run command: " + command + "\n");
      if(callback)
	callback(-1);
      return;
    }
    async_executes.emplace_back(process, stdin_h);
    async_executes_mutex.unlock();
    
    std::thread stderr_thread([this, stderr_h](){
      DWORD n;
      CHAR buffer[BUFSIZE];
      for (;;) {
	BOOL bSuccess = ReadFile(stderr_h, buffer, BUFSIZE, &n, NULL);
	if(!bSuccess || n == 0)
	  break;

	std::string message;
	for(DWORD c=0;c<n;c++)
	  message+=buffer[c];
	async_print(message, true);
      }
    });
    stderr_thread.detach();

    std::thread stdout_thread([this, stdout_h](){
      DWORD n;
      CHAR buffer[BUFSIZE];
      for (;;) {
	BOOL bSuccess = ReadFile(stdout_h, buffer, BUFSIZE, &n, NULL);
	if(!bSuccess || n == 0)
	  break;
	
	std::string message;
	for(DWORD c=0;c<n;c++)
	  message+=buffer[c];
	async_print(message);
      }
    });
    stdout_thread.detach();

    unsigned long exit_code;
    WaitForSingleObject(process, INFINITE);
    GetExitCodeProcess(process, &exit_code);
  
    async_executes_mutex.lock();
    for(auto it=async_executes.begin();it!=async_executes.end();it++) {
      if(it->first==process) {
	async_executes.erase(it);
	break;
      }
    }
    stdin_buffer.clear();
    CloseHandle(process);
    CloseHandle(stdin_h);
    CloseHandle(stdout_h);
    CloseHandle(stderr_h);
    async_executes_mutex.unlock();
      
    if(callback)
      callback(exit_code);
  });
  async_execute_thread.detach();
}

void Terminal::kill_last_async_execute(bool force) {
  async_executes_mutex.lock();
  if(async_executes.size()>0) {
    TerminateProcess(async_executes.back().first, 2);
  }
  async_executes_mutex.unlock();
}

void Terminal::kill_async_executes(bool force) {
  async_executes_mutex.lock();
  for(auto &async_execute: async_executes) {
    TerminateProcess(async_execute.first, 2);
  }
  async_executes_mutex.unlock();
}

int Terminal::print(const std::string &message, bool bold){
 JINFO("Terminal: PrintMessage");
  if(bold)
    get_buffer()->insert_with_tag(get_buffer()->end(), message, bold_tag);
  else
    get_buffer()->insert(get_buffer()->end(), message);
    
  auto iter=get_buffer()->end();
  if(iter.backward_char()) {
    auto mark=get_buffer()->create_mark(iter);
    scroll_to(mark, 0.0, 1.0, 1.0);
    get_buffer()->delete_mark(mark);
  }
  
  return get_buffer()->end().get_line();
}

void Terminal::print(int line_nr, const std::string &message){
 JINFO("Terminal: PrintMessage at line " << line_nr);
  auto iter=get_buffer()->get_iter_at_line(line_nr);
  while(!iter.ends_line())
    iter++;
  get_buffer()->insert(iter, message);
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
  async_executes_mutex.lock();
  if(async_executes.size()>0) {
    get_buffer()->place_cursor(get_buffer()->end());
    auto unicode=gdk_keyval_to_unicode(event->keyval);
    char chr=(char)unicode;
    if(unicode>=32 && unicode<=126) {
      stdin_buffer+=chr;
      get_buffer()->insert_at_cursor(stdin_buffer.substr(stdin_buffer.size()-1));
      scroll_to(get_buffer()->get_insert());
    }
    else if(event->keyval==GDK_KEY_BackSpace) {
      if(stdin_buffer.size()>0 && get_buffer()->get_char_count()>0) {
        auto iter=get_buffer()->end();
        iter--;
        stdin_buffer.pop_back();
        get_buffer()->erase(iter, get_buffer()->end());
        scroll_to(get_buffer()->get_insert());
      }
    }
    else if(event->keyval==GDK_KEY_Return) {
      stdin_buffer+='\n';
      DWORD written;
      WriteFile(async_executes.back().second, stdin_buffer.c_str(), stdin_buffer.size(), &written, NULL);
      //TODO: is this line needed?
      get_buffer()->insert_at_cursor(stdin_buffer.substr(stdin_buffer.size()-1));
      scroll_to(get_buffer()->get_insert());
      stdin_buffer.clear();
    }
  }
  async_executes_mutex.unlock();
  return true;
}
