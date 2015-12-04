#include "process.h"

#include <iostream> //TODO: remove
using namespace std; //TODO: remove

Process::Process(const std::string &command, const std::string &path,
                 std::function<void(const char* bytes, size_t n)> read_stdout,
                 std::function<void(const char* bytes, size_t n)> read_stderr,
                 bool use_stdin, size_t buffer_size):
                 read_stdout(read_stdout), read_stderr(read_stderr), use_stdin(use_stdin), buffer_size(buffer_size) {
  id=open(command, path);
  if(id!=0)
    async_read();
}

Process::~Process() {
  if(stdout_thread.joinable())
    stdout_thread.join();
  if(stderr_thread.joinable())
    stderr_thread.join();
}
