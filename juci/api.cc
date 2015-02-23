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
  Glib::RefPtr<Gtk::TextBuffer> buffer = libjuci::BufferFromNotebook();
  Gtk::TextIter word_start = libjuci::IterFromNotebook();
  Gtk::TextIter word_end = libjuci::IterFromNotebook();

  libjuci::IterToWordStart(word_start);
  libjuci::IterToWordEnd(word_end);
  if(word_start != word_end) {
    buffer->erase(word_start, word_end);
    Gtk::TextIter current = libjuci::IterFromNotebook();
    buffer->insert(current, word);
  }
}
void libjuci::ApiServiceProvider::ReplaceLine(std::string line){}

std::string libjuci::ApiServiceProvider::GetWord(){
  Glib::RefPtr<Gtk::TextBuffer> buffer = libjuci::BufferFromNotebook();
  Gtk::TextIter word_start = libjuci::IterFromNotebook();
  Gtk::TextIter word_end = libjuci::IterFromNotebook();

  libjuci::IterToWordStart(word_start);
  libjuci::IterToWordEnd(word_end);

  std::cout <<"start: " << word_start << std::endl << "end: " << word_end << std::endl;
  if(word_start <  word_end) {
    std::string test = buffer->get_text(word_start, word_end);
    std::cout << "test: " << test << std::endl;
    return test;
  }
  return "no word, sry";
}

void libjuci::ApiServiceProvider::AddKeybinding() {

  libjuci::ApiServiceProvider::menu_->keybindings_.action_group_menu()
    ->add(Gtk::Action::create("PluginAddSnippet",
			      "Add snippet"),
	  Gtk::AccelKey("<control><alt>space"),
	  []() {
	    std::cout << "ctrl alt space" << std::endl;
	    libjuci::LoadPlugin("snippet");
	  });
  std::cout << "addkeybinding" << std::endl;
}

///////////////////////
//// Api to python ////
///////////////////////
void libjuci::ReplaceWord(const std::string word_) {
  libjuci::ApiServiceProvider::ReplaceWord(word_);
}

void libjuci::ReplaceLine(const std::string line) {
  //TODO forgie: implement libjuci::ReplaceLine / change string to iter?
  //some_namespace::controller::replaceLine(line_);
  std::cout << "unimplemented function: 'libjuci::ReplaceLine()' called"
	    << std::endl;
}
std::string libjuci::GetWord() {
  //  boost::python::str converted(libjuci::ApiServiceProvider::GetWord() );
  return libjuci::ApiServiceProvider::GetWord();
  //  return converted;
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

///////////////////////
//// Glib wrappers ////
///////////////////////
void libjuci::IterToWordStart(Gtk::TextIter &iter) {
  if(!iter.starts_line()) {
    while(!iter.starts_word()) {
      iter.backward_char();
    }
  }
}
void libjuci::IterToWordEnd(Gtk::TextIter &iter) {
  if(!iter.ends_line()) {
    while(!iter.ends_word()) {
      iter.forward_char();
    }
  }
}

Glib::RefPtr<Gtk::TextBuffer> libjuci::BufferFromNotebook() {
  return Glib::RefPtr<Gtk::TextBuffer>(libjuci::ApiServiceProvider::notebook_->source_vec_.back()->view().get_buffer());
}

Gtk::TextIter libjuci::IterFromNotebook() {
  // Glib::RefPtr<Gtk::TextBuffer> buffer = libjuci::ApiServiceProvider::notebook_
  //->source_vec_.back()->view().get_buffer();  
  return libjuci::BufferFromNotebook()->get_insert()->get_iter();
}
 


