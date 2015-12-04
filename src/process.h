#ifndef JUCI_PROCESS_H_
#define JUCI_PROCESS_H_

#include <string>
#include <functional>
#include <vector>
#include <mutex>
#include <thread>
#ifdef _WIN32
#include <windows.h>
  typedef DWORD process_id_type;
  typedef HANDLE file_descriptor_type;
#else
#include <sys/wait.h>
  typedef pid_t process_id_type;
  typedef int file_descriptor_type;
#endif

class Process {
public:
  Process(const std::string &command, const std::string &path=std::string(),
          std::function<void(const char *bytes, size_t n)> read_stdout=nullptr,
          std::function<void(const char *bytes, size_t n)> read_stderr=nullptr,
          bool use_stdin=false,
          size_t buffer_size=131072);
  ~Process();
  
  ///Get the process id of the started process.
  process_id_type get_id() {return id;}
  ///Wait until process is finished, and return exit_code.
  int get_exit_code();
  bool write(const char *bytes, size_t n);
  
  ///Kill a given process id.
  static void kill(process_id_type id, bool force=false);
  
private:
  std::function<void(const char* bytes, size_t n)> read_stdout;
  std::function<void(const char* bytes, size_t n)> read_stderr;
  std::thread stdout_thread, stderr_thread;
  bool use_stdin;
  std::mutex stdin_mutex;
  const size_t buffer_size;
  
  std::unique_ptr<file_descriptor_type> stdout_fd, stderr_fd, stdin_fd;
  
  process_id_type open(const std::string &command, const std::string &path);
  process_id_type id;
  void async_read();
};

#endif  // JUCI_PROCESS_H_
