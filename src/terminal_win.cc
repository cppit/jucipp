#include "terminal.h"
#include <iostream>
#include "logging.h"
#include "singletons.h"
#include <unistd.h>
#include <windows.h>

#include <iostream> //TODO: remove
using namespace std; //TODO: remove

const size_t buffer_size=131072;

//Based on the example at https://msdn.microsoft.com/en-us/library/windows/desktop/ms682499(v=vs.85).aspx
//Note: on Windows it seems impossible to specify which pipes to use
//Thus, if stdin_h, stdout_h and stderr all are NULL, the out,err,in is sent to the parent process instead
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

  if(stdin_h!=nullptr) {
    if (!CreatePipe(&g_hChildStd_IN_Rd, &g_hChildStd_IN_Wr, &saAttr, 0)) 
      return NULL;
    if(!SetHandleInformation(g_hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0)) {
      CloseHandle(g_hChildStd_IN_Rd);
      CloseHandle(g_hChildStd_IN_Wr);
      return NULL;
    }
  }
  if(stdout_h!=nullptr) {
    if (!CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0)) {
      if(stdin_h!=nullptr) CloseHandle(g_hChildStd_IN_Rd);
      if(stdin_h!=nullptr) CloseHandle(g_hChildStd_IN_Wr);
      return NULL;
    }
    if(!SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) {
      if(stdin_h!=nullptr) CloseHandle(g_hChildStd_IN_Rd);
      if(stdin_h!=nullptr) CloseHandle(g_hChildStd_IN_Wr);
      CloseHandle(g_hChildStd_OUT_Rd);
      CloseHandle(g_hChildStd_OUT_Wr);
      return NULL;
    }
  }
  if(stderr_h!=nullptr) {
    if (!CreatePipe(&g_hChildStd_ERR_Rd, &g_hChildStd_ERR_Wr, &saAttr, 0)) {
      if(stdin_h!=nullptr) CloseHandle(g_hChildStd_IN_Rd);
      if(stdin_h!=nullptr) CloseHandle(g_hChildStd_IN_Wr);
      if(stdout_h!=nullptr) CloseHandle(g_hChildStd_OUT_Rd);
      if(stdout_h!=nullptr) CloseHandle(g_hChildStd_OUT_Wr);
      return NULL;
    }
    if(!SetHandleInformation(g_hChildStd_ERR_Rd, HANDLE_FLAG_INHERIT, 0)) {
      if(stdin_h!=nullptr) CloseHandle(g_hChildStd_IN_Rd);
      if(stdin_h!=nullptr) CloseHandle(g_hChildStd_IN_Wr);
      if(stdout_h!=nullptr) CloseHandle(g_hChildStd_OUT_Rd);
      if(stdout_h!=nullptr) CloseHandle(g_hChildStd_OUT_Wr);
      CloseHandle(g_hChildStd_ERR_Rd);
      CloseHandle(g_hChildStd_ERR_Wr);
      return NULL;
    }
  }

  PROCESS_INFORMATION process_info; 
  STARTUPINFO siStartInfo;

  ZeroMemory(&process_info, sizeof(PROCESS_INFORMATION));

  ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
  siStartInfo.cb = sizeof(STARTUPINFO);
  if(stdin_h!=nullptr) siStartInfo.hStdInput = g_hChildStd_IN_Rd;
  if(stdout_h!=nullptr) siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
  if(stderr_h!=nullptr) siStartInfo.hStdError = g_hChildStd_ERR_Wr;
  if(stdin_h!=nullptr || stdout_h!=nullptr || stderr_h!=nullptr)
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
    if(stdin_h!=nullptr) CloseHandle(g_hChildStd_IN_Rd);
    if(stdout_h!=nullptr) CloseHandle(g_hChildStd_OUT_Wr);
    if(stderr_h!=nullptr) CloseHandle(g_hChildStd_ERR_Wr);
    return NULL;
  }
  else {
    // Close handles to the child process and its primary thread.
    // Some applications might keep these handles to monitor the status
    // of the child process, for example. 
    
    CloseHandle(process_info.hThread);
    if(stdin_h!=nullptr) CloseHandle(g_hChildStd_IN_Rd);
    if(stdout_h!=nullptr) CloseHandle(g_hChildStd_OUT_Wr);
    if(stderr_h!=nullptr) CloseHandle(g_hChildStd_ERR_Wr);
  }

  if(stdin_h!=NULL) *stdin_h=g_hChildStd_IN_Wr;
  if(stdout_h!=NULL) *stdout_h=g_hChildStd_OUT_Rd;
  if(stderr_h!=NULL) *stderr_h=g_hChildStd_ERR_Rd;
  return process_info.hProcess;
}

Terminal::InProgress::InProgress(const std::string& start_msg): stop(false) {
  waiting_print.connect([this](){
    Singleton::terminal->async_print(line_nr-1, ".");
  });
  start(start_msg);
}

Terminal::InProgress::~InProgress() {
  stop=true;
  if(wait_thread.joinable())
    wait_thread.join();
}

void Terminal::InProgress::start(const std::string& msg) {
  line_nr=Singleton::terminal->print(msg+"...\n");
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
    Singleton::terminal->async_print(line_nr-1, msg);
  }
}

void Terminal::InProgress::cancel(const std::string& msg) {
  if(!stop) {
    stop=true;
    Singleton::terminal->async_print(line_nr-1, msg);
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

int Terminal::execute(const std::string &command, const boost::filesystem::path &path, bool use_pipes) {
  HANDLE stdin_h, stdout_h, stderr_h;
  
  HANDLE process;
  if(use_pipes)
    process=popen3(command, path.string(), &stdin_h, &stdout_h, &stderr_h);
  else
    process=popen3(command, path.string(), nullptr, nullptr, nullptr);
  if(process==NULL) {
    async_print("Error: Failed to run command: " + command + "\n", true);
    return -1;
  }
  if(use_pipes) {
    std::thread stderr_thread([this, stderr_h](){
      DWORD n;
      CHAR buffer[buffer_size];
      for (;;) {
        BOOL bSuccess = ReadFile(stderr_h, buffer, static_cast<DWORD>(buffer_size), &n, NULL);
        if(!bSuccess || n == 0)
  	break;
  
        std::string message;
        message.reserve(n);
        for(DWORD c=0;c<n;c++)
  	message+=buffer[c];
        async_print(message, true);
      }
    });
    stderr_thread.detach();
  
    std::thread stdout_thread([this, stdout_h](){
      DWORD n;
      CHAR buffer[buffer_size];
      for (;;) {
        BOOL bSuccess = ReadFile(stdout_h, buffer, static_cast<DWORD>(buffer_size), &n, NULL);
        if(!bSuccess || n == 0)
  	break;
  
        std::string message;
        message.reserve(n);
        for(DWORD c=0;c<n;c++)
  	message+=buffer[c];
        async_print(message);
      }
    });
    stdout_thread.detach();
  }

  unsigned long exit_code;
  WaitForSingleObject(process, INFINITE);
  GetExitCodeProcess(process, &exit_code);
  
  CloseHandle(process);
  if(use_pipes) {
    CloseHandle(stdin_h);
    CloseHandle(stdout_h);
    CloseHandle(stderr_h);
  }
  return exit_code;
}

int Terminal::execute(std::istream &stdin_stream, std::ostream &stdout_stream, const std::string &command, const boost::filesystem::path &path) {
  HANDLE stdin_h, stdout_h, stderr_h;

  auto process=popen3(command, path.string(), &stdin_h, &stdout_h, &stderr_h);
  if(process==NULL) {
    async_print("Error: Failed to run command: " + command + "\n", true);
    return -1;
  }
  std::thread stderr_thread([this, stderr_h](){
    DWORD n;
    CHAR buffer[buffer_size];
    for (;;) {
      BOOL bSuccess = ReadFile(stderr_h, buffer, static_cast<DWORD>(buffer_size), &n, NULL);
      if(!bSuccess || n == 0)
	break;

      std::string message;
      message.reserve(n);
      for(DWORD c=0;c<n;c++)
	message+=buffer[c];
      async_print(message, true);
    }
  });
  stderr_thread.detach();

  std::thread stdout_thread([this, &stdout_stream, stdout_h](){
    DWORD n;
    CHAR buffer[buffer_size];
    for (;;) {
      BOOL bSuccess = ReadFile(stdout_h, buffer, static_cast<DWORD>(buffer_size), &n, NULL);
      if(!bSuccess || n == 0)
        break;
      Glib::ustring umessage;
      umessage.reserve(n);
      for(DWORD c=0;c<n;c++)
        umessage+=buffer[c];
      Glib::ustring::iterator iter;
      while(!umessage.validate(iter)) {
        auto next_char_iter=iter;
        next_char_iter++;
        umessage.replace(iter, next_char_iter, "?");
      }
      stdout_stream.write(umessage.data(), static_cast<ssize_t>(n));
    }
  });
  stdout_thread.detach();
    
  CHAR buffer[buffer_size];
  for(;;) {
    stdin_stream.readsome(buffer, buffer_size);
    auto read_n=stdin_stream.gcount();
    if(read_n==0)
      break;
    DWORD write_n;
    BOOL bSuccess = WriteFile(stdin_h, buffer, static_cast<DWORD>(read_n), &write_n, NULL);
    if(!bSuccess || write_n==0)
      break;
  }
  CloseHandle(stdin_h);
  
  unsigned long exit_code;
  WaitForSingleObject(process, INFINITE);
  GetExitCodeProcess(process, &exit_code);
  
  CloseHandle(process);
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
      async_print("Error: Failed to run command: " + command + "\n", true);
      if(callback)
	callback(-1);
      return;
    }
    async_executes.emplace_back(process, stdin_h);
    async_executes_mutex.unlock();
    
    std::thread stderr_thread([this, stderr_h](){
      DWORD n;
      CHAR buffer[buffer_size];
      for (;;) {
	BOOL bSuccess = ReadFile(stderr_h, buffer, static_cast<DWORD>(buffer_size), &n, NULL);
	if(!bSuccess || n == 0)
	  break;

	std::string message;
	message.reserve(n);
	for(DWORD c=0;c<n;c++)
	  message+=buffer[c];
	async_print(message, true);
      }
    });
    stderr_thread.detach();

    std::thread stdout_thread([this, stdout_h](){
      DWORD n;
      CHAR buffer[buffer_size];
      for (;;) {
	BOOL bSuccess = ReadFile(stdout_h, buffer, static_cast<DWORD>(buffer_size), &n, NULL);
	if(!bSuccess || n == 0)
	  break;
	
	std::string message;
	message.reserve(n);
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

size_t Terminal::print(const std::string &message, bool bold){
  Glib::ustring umessage=message;
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
  
  if(get_buffer()->get_line_count()>Singleton::config->terminal.history_size) {
    int lines=get_buffer()->get_line_count()-Singleton::config->terminal.history_size;
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
  async_executes_mutex.lock();
  if(async_executes.size()>0) {
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
      DWORD written;
      WriteFile(async_executes.back().second, stdin_buffer.c_str(), stdin_buffer.size(), &written, NULL);
      //TODO: is this line needed?
      get_buffer()->insert_at_cursor(stdin_buffer.substr(stdin_buffer.size()-1));
      stdin_buffer.clear();
    }
  }
  async_executes_mutex.unlock();
  return true;
}
