#include "api.h"

std::shared_ptr<Menu::Controller> libjuci::ApiServiceProvider::menu_;
std::shared_ptr<Notebook::Controller> libjuci::ApiServiceProvider::notebook_;

/////////////////////////////
//// API ServiceProvider ////
/////////////////////////////
libjuci::ApiServiceProvider::ApiServiceProvider( ) {
  std::cout << "Apiservice std.ctor" << std::endl;
}

void libjuci::ApiServiceProvider::ReplaceWord(std::string word) {
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

void libjuci::ApiServiceProvider::ReplaceLine(std::string line) {
  std::cout << "Unimplemented function: ApiServiceProvider::ReplaceLine(string)"
	    << std::endl;
}

std::string libjuci::ApiServiceProvider::GetWord() {
  Glib::RefPtr<Gtk::TextBuffer> buffer = libjuci::BufferFromNotebook();
  Gtk::TextIter word_start = libjuci::IterFromNotebook();
  Gtk::TextIter word_end = libjuci::IterFromNotebook();

  libjuci::IterToWordStart(word_start);
  libjuci::IterToWordEnd(word_end);

  if(word_start < word_end) {
    std::string word = buffer->get_text(word_start, word_end);
    return word;
  }
  return "";
}

void libjuci::ApiServiceProvider::AddKeybinding() {
  libjuci::LoadPlugin(g_project_root+"plugins.py");
  
  // libjuci::AddMenuElement("Snippet");
  // libjuci::AddMenuElement("Bobby");
    
  //TODO forgie: update views
}

///////////////////////
//// Api to python ////
///////////////////////
void libjuci::ReplaceWord(const std::string word) {
  libjuci::ApiServiceProvider::ReplaceWord(word);
}

void libjuci::ReplaceLine(const std::string line) {
  //TODO forgie: implement libjuci::ReplaceLine / change string to iter?
  //some_namespace::controller::replaceLine(line_);
  std::cout << "unimplemented function: 'libjuci::ReplaceLine()' called"
	    << std::endl;
}
std::string libjuci::GetWord() {
  return libjuci::ApiServiceProvider::GetWord();
}

void libjuci::AddMenuElement(std::string plugin_name){
  libjuci::AddMenuXml(plugin_name, "PluginMenu");
  std::string plugin_action_name = plugin_name+"Menu";
 
  libjuci::ApiServiceProvider::menu_->keybindings_.action_group_menu()
    ->add(Gtk::Action::create(plugin_action_name, plugin_name));
}

void libjuci::AddSubMenuElement(std::string parent_menu,
				std::string menu_name,
				std::string menu_func_name,
				std::string plugin_path,
				std::string menu_keybinding) {

  libjuci::AddSubMenuXml(menu_func_name, parent_menu);

  libjuci::ApiServiceProvider::menu_->keybindings_.action_group_menu()
    ->add(Gtk::Action::create(menu_func_name,
			      menu_name),
	  Gtk::AccelKey(menu_keybinding),
	  [=]() {
	    libjuci::LoadPluginFunction(menu_func_name, plugin_path);
	  });
}

void libjuci::AddMenuXml(std::string plugin_name, std::string parent_menu) {

  std::string temp_menu = libjuci::ApiServiceProvider::menu_
    ->keybindings_.model_.menu_ui_string();

  std::size_t plugin_menu_pos = temp_menu.find(parent_menu);
  // +2 gets you outside of the tag:<'menu_name'> ref: keybindings.cc 
  plugin_menu_pos+=parent_menu.size() +2; 
  std::string menu_prefix = temp_menu.substr(0, plugin_menu_pos);
  std::string menu_suffix = temp_menu.substr(plugin_menu_pos);
  
  std::string menu_input =
    "           <menu action='"+plugin_name+"Menu'>               "
    //    "               <menuitem action='PluginAdd"+plugin_name+"'/>   "
    "           </menu>                                             ";

  libjuci::ApiServiceProvider::menu_->keybindings_.model_.menu_ui_string_ =
    menu_prefix + menu_input + menu_suffix;
}

void libjuci::AddSubMenuXml(std::string plugin_name, std::string parent_menu){
  std::string temp_menu = libjuci::ApiServiceProvider::menu_
    ->keybindings_.model_.menu_ui_string();

  std::size_t parent_menu_pos = temp_menu.find(parent_menu);
  // +2 gets you outside of the tag:<'menu_name'> ref: keybindings.cc 
  parent_menu_pos+=parent_menu.size() +2; 
  std::string menu_prefix = temp_menu.substr(0, parent_menu_pos);
  std::string menu_suffix = temp_menu.substr(parent_menu_pos);
  
  std::string menu_input =
    "               <menuitem action='"+plugin_name+"'/>   ";
  

  libjuci::ApiServiceProvider::menu_->keybindings_.model_.menu_ui_string_ =
    menu_prefix + menu_input + menu_suffix;

  std::cout << libjuci::ApiServiceProvider::menu_
    ->keybindings_.model_.menu_ui_string_ << std::endl;
}
//////////////////////////////
//// Boost.Python methods ////
//////////////////////////////

boost::python::api::object libjuci::OpenPythonScript(const std::string path,
						     boost::python::api::object python_name_space) {
  boost::python::str str_path(path);
  return boost::python::exec_file(str_path, python_name_space);//, python_name_space);
}

void libjuci::LoadPlugin(const std::string& plugin_name) {
  std::cout << "libjuci::LoadPlugin " << plugin_name << std::endl; 
  try{
    Py_Initialize();
    boost::python::api::object main_module = boost::python::import("__main__");
    boost::python::api::object main_namespace =
      main_module.attr("__dict__");
    boost::python::api::object ignored2 =
      OpenPythonScript(plugin_name, main_namespace);
    
  }catch(boost::python::error_already_set const&) {
    PyErr_Print();
  }
}

void libjuci::LoadPluginFunction(const std::string &function_name,
				 const std::string &plugin_path){
     try{
    Py_Initialize();
    boost::python::api::object main_module = boost::python::import("__main__");
    boost::python::api::object main_namespace = main_module.attr("__dict__");
    boost::python::api::object ignored2 = OpenPythonScript(plugin_path,
							   main_namespace);

    boost::python::str func_name(function_name);
    // for extracting return value from python: 
    boost::python::object returnValue = boost::python::eval(func_name,
							    main_namespace);
    //std::string return_value = boost::python::extract<std::string>(pySnippet);
    //do something with return_value    
  }catch(boost::python::error_already_set const&) {
    PyErr_Print();
  }

  //std::cout << __func__ << " - " << function_name << std::endl;
}

void libjuci::InitPlugin(const std::string& plugin_path) {
    std::cout << "libjuci::InitPlugin " << plugin_path << std::endl; 
  try{
    Py_Initialize();
    boost::python::api::object main_module = boost::python::import("__main__");
    boost::python::api::object main_namespace = main_module.attr("__dict__");
    boost::python::api::object ignored2 = OpenPythonScript(plugin_path,
							   main_namespace);

    /* for extracting return value from python: */
    boost::python::object returnValue  =  boost::python::eval("initPlugin()",
							      main_namespace);
    //std::string return_value = boost::python::extract<std::string>(pySnippet);
    //do something with return_value    
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
  //finding focused view
  int i = 0;
  while(!libjuci::ApiServiceProvider::notebook_->source_vec_.at(i)
	->view().has_focus()) {
    i++;
  }
  return Glib::RefPtr<Gtk::TextBuffer>(libjuci::ApiServiceProvider::notebook_
				       ->source_vec_.at(i)
				       ->view().get_buffer());
}

Gtk::TextIter libjuci::IterFromNotebook() {
  return libjuci::BufferFromNotebook()->get_insert()->get_iter();
}
