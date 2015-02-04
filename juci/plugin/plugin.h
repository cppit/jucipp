#ifndef JUCI_PLUGIN_H_
#define JUCI_PLUGIN_H_

#include <boost/python.hpp>
#include <Python.h>

#include <string>
#include <iostream>

namespace bp = boost::python;

const std::string g_project_root("~/q6/testing/plugin/");
//TODO (forgie) get current working directory..

class Plugin{
 public:
  static void snippet(std::string& word);
  static void get_test_value();
  static std::string get_test_value2(); 
 private:
  static bp::object setPythonVar(const std::string varName,
				 const std::string varValue,
				 bp::object python_name_space);
  static bp::object openPythonScript(const std::string path,
				     bp::object python_name_space);
};

#endif // JUCI_PLUGIN_H_
