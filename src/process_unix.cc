#include "process.h"
#include <cstdlib>
#include <unistd.h>
#include <signal.h>

#include <iostream> //TODO: remove
using namespace std; //TODO: remove

Process::id_type Process::open(const std::string &command, const std::string &path) {
  if(open_stdin)
    stdin_fd=std::unique_ptr<fd_type>(new fd_type);
  if(read_stdout)
    stdout_fd=std::unique_ptr<fd_type>(new fd_type);
  if(read_stderr)
    stderr_fd=std::unique_ptr<fd_type>(new fd_type);
  
  int stdin_p[2], stdout_p[2], stderr_p[2];

  if(stdin_fd && pipe(stdin_p)!=0) {
    close(stdin_p[0]);
    close(stdin_p[1]);
    return -1;
  }
  if(stdout_fd && pipe(stdout_p)!=0) {
    if(stdin_fd) close(stdin_p[0]);
    if(stdin_fd) close(stdin_p[1]);
    close(stdout_p[0]);
    close(stdout_p[1]);
    return -1;
  }
  if(stderr_fd && pipe(stderr_p)!=0) {
    if(stdin_fd) close(stdin_p[0]);
    if(stdin_fd) close(stdin_p[1]);
    if(stdout_fd) close(stdout_p[0]);
    if(stdout_fd) close(stdout_p[1]);
    close(stderr_p[0]);
    close(stderr_p[1]);
    return -1;
  }
  
  id_type pid = fork();

  if (pid < 0) {
    if(stdin_fd) close(stdin_p[0]);
    if(stdin_fd) close(stdin_p[1]);
    if(stdout_fd) close(stdout_p[0]);
    if(stdout_fd) close(stdout_p[1]);
    if(stderr_fd) close(stderr_p[0]);
    if(stderr_fd) close(stderr_p[1]);
    return pid;
  }
  else if (pid == 0) {
    if(stdin_fd) close(stdin_p[1]);
    if(stdout_fd) close(stdout_p[0]);
    if(stderr_fd) close(stderr_p[0]);
    if(stdin_fd) dup2(stdin_p[0], 0);
    if(stdout_fd) dup2(stdout_p[1], 1);
    if(stderr_fd) dup2(stderr_p[1], 2);

    setpgid(0, 0);
    //TODO: See here on how to emulate tty for colors: http://stackoverflow.com/questions/1401002/trick-an-application-into-thinking-its-stdin-is-interactive-not-a-pipe
    //TODO: One solution is: echo "command;exit"|script -q /dev/null
    
    if(!path.empty())
      execl("/bin/sh", "sh", "-c", ("cd \""+path+"\" && "+command).c_str(), NULL);
    else
      execl("/bin/sh", "sh", "-c", command.c_str(), NULL);
    
    perror("execl");
    exit(EXIT_FAILURE);
  }

  if(stdin_fd) close(stdin_p[0]);
  if(stdout_fd) close(stdout_p[1]);
  if(stderr_fd) close(stderr_p[1]);
  
  if(stdin_fd) *stdin_fd = stdin_p[1];
  if(stdout_fd) *stdout_fd = stdout_p[0];
  if(stderr_fd) *stderr_fd = stderr_p[0];

  return pid;
}

void Process::async_read() {
  if(stdout_fd) {
    stdout_thread=std::thread([this](){
      char buffer[buffer_size];
      ssize_t n;
      while ((n=read(*stdout_fd, buffer, buffer_size)) > 0)
        read_stdout(buffer, static_cast<size_t>(n));
    });
  }
  if(stderr_fd) {
    stderr_thread=std::thread([this](){
      char buffer[buffer_size];
      ssize_t n;
      while ((n=read(*stderr_fd, buffer, buffer_size)) > 0)
        read_stderr(buffer, static_cast<size_t>(n));
    });
  }
}

int Process::get_exit_code() {
  if(id<=0)
    return -1;
  int exit_code;
  waitpid(id, &exit_code, 0);
  
  if(stdout_thread.joinable())
    stdout_thread.join();
  if(stderr_thread.joinable())
    stderr_thread.join();
  
  close_stdin();
  if(stdout_fd) {
    close(*stdout_fd);
    stdout_fd.reset();
  }
  if(stderr_fd) {
    close(*stderr_fd);
    stderr_fd.reset();
  }
  
  return exit_code;
}

bool Process::write(const char *bytes, size_t n) {
  stdin_mutex.lock();
  if(stdin_fd) {
    if(::write(*stdin_fd, bytes, n)>=0) {
      stdin_mutex.unlock();
      return true;
    }
    else {
      stdin_mutex.unlock();
      return false;
    }
  }
  stdin_mutex.unlock();
  return false;
}

void Process::close_stdin() {
  stdin_mutex.lock();
  if(stdin_fd) {
    close(*stdin_fd);
    stdin_fd.reset();
  }
  stdin_mutex.unlock();
}

void Process::kill(id_type id, bool force) {
  if(id<=0)
    return;
  if(force)
    ::kill(-id, SIGTERM);
  else
    ::kill(-id, SIGINT);
}
