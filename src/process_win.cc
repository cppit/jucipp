#include "process.h"
#include <cstring>
#include "TlHelp32.h"

#include <iostream> //TODO: remove
using namespace std; //TODO: remove

//Based on the example at https://msdn.microsoft.com/en-us/library/windows/desktop/ms682499(v=vs.85).aspx.
//Note: on Windows it seems impossible to specify which pipes to use.
//Thus, if read_stdout=nullptr, read_stderr=nullptr and open_stdin=false, the stdout, stderr and stdin are sent to the parent process instead.
Process::id_type Process::open(const std::string &command, const std::string &path) {
  if(open_stdin)
    stdin_fd=std::unique_ptr<fd_type>(new fd_type);
  if(read_stdout)
    stdout_fd=std::unique_ptr<fd_type>(new fd_type);
  if(read_stderr)
    stderr_fd=std::unique_ptr<fd_type>(new fd_type);

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

  if(stdin_fd) {
    if (!CreatePipe(&g_hChildStd_IN_Rd, &g_hChildStd_IN_Wr, &saAttr, 0))
      return 0;
    if(!SetHandleInformation(g_hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0)) {
      CloseHandle(g_hChildStd_IN_Rd);
      CloseHandle(g_hChildStd_IN_Wr);
      return 0;
    }
  }
  if(stdout_fd) {
    if (!CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0)) {
      if(stdin_fd) CloseHandle(g_hChildStd_IN_Rd);
      if(stdin_fd) CloseHandle(g_hChildStd_IN_Wr);
      return 0;
    }
    if(!SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) {
      if(stdin_fd) CloseHandle(g_hChildStd_IN_Rd);
      if(stdin_fd) CloseHandle(g_hChildStd_IN_Wr);
      CloseHandle(g_hChildStd_OUT_Rd);
      CloseHandle(g_hChildStd_OUT_Wr);
      return 0;
    }
  }
  if(stderr_fd) {
    if (!CreatePipe(&g_hChildStd_ERR_Rd, &g_hChildStd_ERR_Wr, &saAttr, 0)) {
      if(stdin_fd) CloseHandle(g_hChildStd_IN_Rd);
      if(stdin_fd) CloseHandle(g_hChildStd_IN_Wr);
      if(stdout_fd) CloseHandle(g_hChildStd_OUT_Rd);
      if(stdout_fd) CloseHandle(g_hChildStd_OUT_Wr);
      return 0;
    }
    if(!SetHandleInformation(g_hChildStd_ERR_Rd, HANDLE_FLAG_INHERIT, 0)) {
      if(stdin_fd) CloseHandle(g_hChildStd_IN_Rd);
      if(stdin_fd) CloseHandle(g_hChildStd_IN_Wr);
      if(stdout_fd) CloseHandle(g_hChildStd_OUT_Rd);
      if(stdout_fd) CloseHandle(g_hChildStd_OUT_Wr);
      CloseHandle(g_hChildStd_ERR_Rd);
      CloseHandle(g_hChildStd_ERR_Wr);
      return 0;
    }
  }
  
  PROCESS_INFORMATION process_info;
  STARTUPINFO siStartInfo;
  
  ZeroMemory(&process_info, sizeof(PROCESS_INFORMATION));
  
  ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
  siStartInfo.cb = sizeof(STARTUPINFO);
  if(stdin_fd) siStartInfo.hStdInput = g_hChildStd_IN_Rd;
  if(stdout_fd) siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
  if(stderr_fd) siStartInfo.hStdError = g_hChildStd_ERR_Wr;
  if(stdin_fd || stdout_fd || stderr_fd)
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;
  
  char* path_ptr;
  if(path=="")
    path_ptr=NULL;
  else {
    path_ptr=new char[path.size()+1];
    std::strcpy(path_ptr, path.c_str());
  }

  char* command_cstr;
#ifdef MSYS_PROCESS_USE_SH
  size_t pos=0;
  std::string sh_command=command;
  while((pos=sh_command.find('\"', pos))!=std::string::npos) {
    if(pos>0 && sh_command[pos-1]!='\\') {
      sh_command.replace(pos, 1, "\\\"");
      pos++;
    }
    pos++;
  }
  sh_command.insert(0, "sh -c \"");
  sh_command+="\"";
  command_cstr=new char[sh_command.size()+1];
  std::strcpy(command_cstr, sh_command.c_str());
#else
  command_cstr=new char[command.size()+1];
  std::strcpy(command_cstr, command.c_str());
#endif

  BOOL bSuccess = CreateProcess(NULL, command_cstr, NULL, NULL, TRUE, 0,
                                NULL, path_ptr, &siStartInfo, &process_info);

  if(!bSuccess) {
    CloseHandle(process_info.hProcess);
    CloseHandle(process_info.hThread);
    if(stdin_fd) CloseHandle(g_hChildStd_IN_Rd);
    if(stdout_fd) CloseHandle(g_hChildStd_OUT_Wr);
    if(stderr_fd) CloseHandle(g_hChildStd_ERR_Wr);
    return 0;
  }
  else {
    CloseHandle(process_info.hThread);
    if(stdin_fd) CloseHandle(g_hChildStd_IN_Rd);
    if(stdout_fd) CloseHandle(g_hChildStd_OUT_Wr);
    if(stderr_fd) CloseHandle(g_hChildStd_ERR_Wr);
  }

  if(stdin_fd) *stdin_fd=g_hChildStd_IN_Wr;
  if(stdout_fd) *stdout_fd=g_hChildStd_OUT_Rd;
  if(stderr_fd) *stderr_fd=g_hChildStd_ERR_Rd;
  
  return process_info.dwProcessId;
}

void Process::async_read() {
  if(stdout_fd) {
    stdout_thread=std::thread([this](){
      DWORD n;
      char buffer[buffer_size];
      for (;;) {
        BOOL bSuccess = ReadFile(*stdout_fd, static_cast<CHAR*>(buffer), static_cast<DWORD>(buffer_size), &n, NULL);
        if(!bSuccess || n == 0)
          break;
        read_stdout(buffer, static_cast<size_t>(n));
      }
    });
  }
  if(stderr_fd) {
    stderr_thread=std::thread([this](){
      DWORD n;
      char buffer[buffer_size];
      for (;;) {
        BOOL bSuccess = ReadFile(*stderr_fd, static_cast<CHAR*>(buffer), static_cast<DWORD>(buffer_size), &n, NULL);
        if(!bSuccess || n == 0)
          break;
        read_stderr(buffer, static_cast<size_t>(n));
      }
    });
  }
}

int Process::get_exit_code() {
  if(id==0)
    return -1;
  DWORD exit_code;
  HANDLE process_handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, id);
  if(process_handle) {
    WaitForSingleObject(process_handle, INFINITE);
    GetExitCodeProcess(process_handle, &exit_code);
    CloseHandle(process_handle);
  }
  else
    exit_code=-1;
  
  if(stdout_thread.joinable())
    stdout_thread.join();
  if(stderr_thread.joinable())
    stderr_thread.join();
  
  close_stdin();
  if(stdout_fd) {
    CloseHandle(*stdout_fd);
    stdout_fd.reset();
  }
  if(stderr_fd) {
    CloseHandle(*stderr_fd);
    stderr_fd.reset();
  }
  
  return static_cast<int>(exit_code);
}

bool Process::write(const char *bytes, size_t n) {
  stdin_mutex.lock();
  if(stdin_fd) {
    DWORD written;
    BOOL bSuccess=WriteFile(*stdin_fd, bytes, static_cast<DWORD>(n), &written, NULL);
    if(!bSuccess || written==0) {
      stdin_mutex.unlock();
      return false;
    }
    else {
      stdin_mutex.unlock();
      return true;
    }
  }
  stdin_mutex.unlock();
  return false;
}

void Process::close_stdin() {
  stdin_mutex.lock();
  if(stdin_fd) {
    CloseHandle(*stdin_fd);
    stdin_fd.reset();
  }
  stdin_mutex.unlock();
}

//Based on http://stackoverflow.com/a/1173396
void Process::kill(id_type id, bool force) {
  if(id==0)
    return;
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if(snapshot) {
    PROCESSENTRY32 process;
    ZeroMemory(&process, sizeof(process));
    process.dwSize = sizeof(process);
    if(Process32First(snapshot, &process)) {
      do {
        if(process.th32ParentProcessID==id) {
          HANDLE process_handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, process.th32ProcessID);
          if(process_handle) TerminateProcess(process_handle, 2);
        }
      } while (Process32Next(snapshot, &process));
    }
  }
  HANDLE process_handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, id);
  if(process_handle) TerminateProcess(process_handle, 2);
}
