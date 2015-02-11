#include <boost/python.hpp>
#include <Python.h>
#include <string>
//
//    Plugin API
//
namespace juci_api{
  //
  //   calls from python to C++
  //
  namespace cpp{

    //
    //    Replaceword:
    //        replaces a word in the editor with a string
    //
    void ReplaceWord(const std::string word_){
      //TODO implement api::ReplaceWord / change string to iter?
      //some_namespace::controller::replaceWord(word_*);
      std::cout << "unimplemented function: 'api::ReplaceWord()' called"
		<< std::endl;
    }
        
    //
    //    ReplaceLine:
    //        Replaces a line in the editor with a string
    //
    void ReplaceLine(const std::string line_){
      //TODO implement api::ReplaceLine / change string to iter?
      //some_namespace::controller::replaceLine(line_);
      std::cout << "unimplemented function: 'api::ReplaceLine()' called"
		<< std::endl;
    }    
    
  }//namespace cpp

  //
  //   calls from C++ to Python
  //
  namespace py{
    const std::string g_project_root("~/bachelor/app/juci/");
    //helpers
    boost::python::api::object openPythonScript(const std::string path,
						boost::python::api::object python_name_space) {
      std::string temp = g_project_root + path + ".py";
      boost::python::str the_path(temp);
      return boost::python::exec_file(the_path, python_name_space);//, python_name_space);
    }
    
    //void snippet(std::string& word);

    void plugin(const std::string& plugin_name){
      try{
	/* initialize python interpreter */
	Py_Initialize();
	boost::python::api::object main_module = boost::python::import("__main__");
	boost::python::api::object main_namespace = main_module.attr("__dict__");
    
	/* runs script from python */
	//boost::python::object ignored1 = setPythonVar("word", word, main_namespace);
	boost::python::api::object ignored2 = openPythonScript(plugin_name, main_namespace);   
	/* extracts desired values */   
	//boost::python::object pySnippet =  boost::python::eval("getSnippet()", main_namespace);
	//word = boost::python::extract<std::string>(pySnippet);
	/* add snippet to textView */
	//TODO add snippet
      }catch(boost::python::error_already_set const&){
	PyErr_Print();
      }
    }
    
  }// py 
  
}//juci_api


   
