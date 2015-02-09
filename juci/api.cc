#include "api.h"

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
    std::string ReplaceWord(const std::string word_){
      //TODO implement api::ReplaceWord / change string to iter?
      //some_namespace::controller::replaceWord(word_*);
      std::cout << "unimplemented function: 'api::ReplaceWord()' called"
		<< std::endl;
      return "string from unimplemented method 'api::ReplaceWord'";
    }
    //
    //    ReplaceLine:
    //        Replaces a line in the editor with a string
    //
    std::string ReplaceLine(const std::string line_){
      //TODO implement api::ReplaceLine / change string to iter?
      //some_namespace::controller::replaceLine(line_);
      std::cout << "unimplemented function: 'api::ReplaceLine()' called"
		<< std::endl;
      return "string from unimplemented method 'api::ReplaceLine'";
    }
    //
    //     The module
    //
    namespace bp = boost::python;
    BOOST_PYTHON_MODULE(juci) {
      // text editing
      bp::def("replaceLine", &juci_plugin::cpp::ReplaceLine)
	.bp::def("replaceWord", &juci_plugin::cpp::ReplaceWord);
      //.bp::def("");
    }
    
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

