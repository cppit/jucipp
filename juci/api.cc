#include "api.h"

std::shared_ptr<Menu::Controller> libjuci::ApiServiceProvider::menu_;
std::shared_ptr<Notebook::Controller> libjuci::ApiServiceProvider::notebook_;

/////////////////////////////
//// API ServiceProvider ////
/////////////////////////////
libjuci::ApiServiceProvider::ApiServiceProvider( ){
  std::cout << "Apiservice std.ctor" << std::endl;
}
void libjuci::ApiServiceProvider::ReplaceWord(std::string word){
  std::cout << word << std::endl;
}
void libjuci::ApiServiceProvider::ReplaceLine(std::string line){}

void libjuci::ApiServiceProvider::AddKeybinding() {

  libjuci::ApiServiceProvider::menu_->keybindings_.action_group_menu()
          ->add(Gtk::Action::create("PluginAddSnippet",
                          "Add snippet"),
                  Gtk::AccelKey("<control><alt>space"),
                  []() {
                      std::cout << "ctrl alt space" << std::endl;
                      libjuci::LoadPlugin("juci_api_test");
                  });
  std::cout << "addkeybinding" << std::endl;
}

///////////////////////
//// Api to python ////
///////////////////////
void libjuci::ReplaceWord(const std::string word_) {
  //TODO implement libjuci::ReplaceWord / change string to iter?
  //some_namespace::controller::replaceWord(word_*);
  //std::cout << "unimplemented function: 'libjuci::ReplaceWord()' called"
  //    << std::endl;

  //libjuci::ApiServiceProvider::ReplaceWord(word);

  std::cout << "The string: "  << word_ << std::endl;
}

void libjuci::ReplaceLine(const std::string line) {
  //TODO implement libjuci::ReplaceLine / change string to iter?
  //some_namespace::controller::replaceLine(line_);
  std::cout << "unimplemented function: 'libjuci::ReplaceLine()' called"
      << std::endl;
}
//////////////////////////////
//// Boost.Python methods ////
//////////////////////////////

boost::python::api::object libjuci::openPythonScript(const std::string path,
            boost::python::api::object python_name_space) {
  std::string temp = g_project_root + path + ".py";
  boost::python::str the_path(temp);
  return boost::python::exec_file(the_path, python_name_space);//, python_name_space);
}

 void libjuci::LoadPlugin(const std::string& plugin_name) {
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
  }catch(boost::python::error_already_set const&) {
    PyErr_Print();
  }
}

