#ifndef JUCI_API_H_
#define JUCI_API_H_

#include <boost/python.hpp>
#include <Python.h>
#include <string>

//    Plugin API
//
const std::string g_project_root("/home/forgie/bachelor/app/juci/");
static std::string g_test_string("test");
namespace juci_api {
  //
  //   calls from python to C++
  //
  namespace cpp {

    
    //    Replaceword:
    //        replaces a word in the editor with a string
    
    void ReplaceWord(const std::string word_);
        
    //
    //    ReplaceLine:
    //        Replaces a line in the editor with a string
    //
    void ReplaceLine(const std::string line_);
    
  }//namespace cpp

  //
  //   calls from C++ to Python
  //
  namespace py {
    

    //helpers
    boost::python::api::object openPythonScript(const std::string path,
						boost::python::api::object python_name_space);
    
    //void snippet(std::string& word);

    void LoadPlugin(const std::string& plugin_name);
  }// py   
}//juci_api
#endif // JUCI_API_H
