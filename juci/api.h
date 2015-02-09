#include <boost/python.hpp>
#include <Python.h>
#include <string>
//
//    Plugin API
//
namespace juci_plugin{
  //
  //   calls from python to C++
  //
  namespace cpp{

    //
    //    ReplaceWord:
    //        replaces a word in the editor with a string
    //
    std::string ReplaceWord(const std::string word_);

    //
    //    ReplaceLine:
    //        Replaces a line in the editor with a string
    //
    std::string ReplaceLine(const std::string line_);

    //
    //     The module
    //
    namespace bp = boost::python;
    BOOST_PYTHON_MODULE(juci) {
      // text editing
      bp::def("replaceLine", &juci_plugin::cpp::ReplaceLine);
      bp::def("replaceWord", &juci_plugin::cpp::ReplaceWord);
    }// module::juci
    
  }//namespace #include "api.h"

  //
  //   calls from C++ to Python
  //
  namespace py{

    
  }// py 
  
}//juci_plugin


int main(int argc, char *argv[])
{
  
  return 0;
}

