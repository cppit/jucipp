#include "plugin.h"

////////////////////////////////////////////////////////////////////////////////
//
//    setPythonVar:
//    initiates a string value (k_var_value) to a declared variable name
//    (k_var_name) within a given namespace(python_name_space)
//
////////////////////////////////////////////////////////////////////////////////

bp::object Plugin::setPythonVar(const std::string k_var_name,
				const std::string k_var_value,
				bp::object python_name_space ) {  
  std::string temp = k_var_name + " = \"" + k_var_value + "\"";
  bp::str the_var(temp);
  return bp::exec(the_var, python_name_space);
}

////////////////////////////////////////////////////////////////////////////////
//
//   openPythonScript:
//   Opens a python plugin script within a file path and a python namespace
//   
////////////////////////////////////////////////////////////////////////////////

bp::object Plugin::openPythonScript(const std::string path,
				    bp::object python_name_space) {
  std::string temp = g_project_root + "plugins/" + path + "/" + path + ".py";
  bp::str the_path(temp);
  return bp::exec_file(the_path, python_name_space);
}

////////////////////////////////////////////////////////////////////////////////
//
// for testing purposes only
//
////////////////////////////////////////////////////////////////////////////////

std::string Plugin::get_test_value2(){
  return "STRING FROM WITHIN C++";
}

/* public functions */


////////////////////////////////////////////////////////////////////////////////
//
//    snippet:
//    takes a std::string and converts it into a matching snippet
//    if no matching snippet, returns the same string
//
////////////////////////////////////////////////////////////////////////////////

void Plugin::snippet(std::string& word){
  try{
    /* initialize python interpreter */
    Py_Initialize();
    bp::object main_module =  bp::import("__main__");
    bp::object main_namespace = main_module.attr("__dict__");
    
    /* runs script from python */
    bp::object ignored1 = setPythonVar("word", word, main_namespace);
    bp::object ignored2 = openPythonScript(__func__, main_namespace);
    
    /* extracts desired values */   
    bp::object pySnippet =  bp::eval("getSnippet()", main_namespace);
    word = bp::extract<std::string>(pySnippet);
    /* add snippet to textView */
    //TODO add snippet

  }catch(bp::error_already_set const&){
    PyErr_Print();
  }
}

////////////////////////////////////////////////////////////////////////////////
//
//    get_test_value:
//    for testing purposes
//    uses a python module generated from c++
//    calls c++ function from python
//    returns string to c++ from python
//    prints the string from python within c++
//
////////////////////////////////////////////////////////////////////////////////

 void Plugin::get_test_value(){
  try{
     /* initialize python interpreter */
    Py_Initialize();
    bp::object main_module =  bp::import("__main__");
    bp::object main_namespace = main_module.attr("__dict__");
    
    /* runs script from python */
    const std::string path("test");
    bp::object ignored2 = openPythonScript(path, main_namespace);

    /* extracts desired values */
    bp::object pySnippet =  bp::eval("get_test_value()", main_namespace);
    std::string mword = bp::extract<std::string>(pySnippet);
    /* add snippet to textView */
    std::cout << mword << std::endl;
    //TODO add snippet

  }catch(bp::error_already_set const&){
    PyErr_Print();
  }
 }

int main(int argc, char *argv[])
{
    std::string word("ifelse");
  Plugin::snippet(word);
  //Plugin::get_test_value();
  std::cout << word << std::endl;
  return 0;

}
