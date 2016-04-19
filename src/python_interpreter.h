#ifndef JUCI_PYTHON_INTERPRETER_H_
#define JUCI_PYTHON_INTERPRETER_H_

#include <pybind11/pybind11.h>
#include <boost/filesystem/path.hpp>

#include <iostream>
using namespace std;

namespace Python {
  class Interpreter {
  public:
    pybind11::module static find_module(const std::string &module_name);
    pybind11::module static reload(pybind11::module &module);
    pybind11::object static error();
  private:
    Interpreter();
    ~Interpreter();
    wchar_t *argv;
    void configure_path();
  public:
    static Interpreter& get(){
      static Interpreter singleton;
      return singleton;
    }
  };
};

#endif  // JUCI_PYTHON_INTERPRETER_H_
