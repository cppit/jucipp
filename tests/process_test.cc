#include <glib.h>
#include "process.hpp"

int main() {
  auto output=std::make_shared<std::string>();
  auto error=std::make_shared<std::string>();
  {
    TinyProcessLib::Process process("echo Test", "", [output](const char *bytes, size_t n) {
      *output+=std::string(bytes, n);
    });
    g_assert(process.get_exit_status()==0);
    g_assert(output->substr(0, 4)=="Test");
    output->clear();
  }
  
  {
    TinyProcessLib::Process process("echo Test && ls an_incorrect_path", "", [output](const char *bytes, size_t n) {
      *output+=std::string(bytes, n);
    }, [error](const char *bytes, size_t n) {
      *error+=std::string(bytes, n);
    });
    g_assert(process.get_exit_status()>0);
    g_assert(output->substr(0, 4)=="Test");
    g_assert(!error->empty());
    output->clear();
    error->clear();
  }
  
  {
    TinyProcessLib::Process process("bash", "", [output](const char *bytes, size_t n) {
      *output+=std::string(bytes, n);
    }, nullptr, true);
    process.write("echo Test\n");
    process.write("exit\n");
    g_assert(process.get_exit_status()==0);
    g_assert(output->substr(0, 4)=="Test");
    output->clear();
  }
  
  {
    TinyProcessLib::Process process("cat", "", [output](const char *bytes, size_t n) {
      *output+=std::string(bytes, n);
    }, nullptr, true);
    process.write("Test\n");
    process.close_stdin();
    g_assert(process.get_exit_status()==0);
    g_assert(output->substr(0, 4)=="Test");
    output->clear();
  }
}
