#include "api.h"
#include "logging.h"
#include "singletons.h"

Menu::Controller* PluginApi::menu_;
Notebook::Controller* PluginApi::notebook_;
/////////////////////////////
//// API ServiceProvider ////
/////////////////////////////
PluginApi::PluginApi() {
  DEBUG("Adding pointers for the API");
  notebook_ = Singleton::notebook();
  menu_ = Singleton::menu();
  DEBUG("Initiating plugins(from plugins.py)..");
#ifndef __APPLE__
  InitPlugins(); //TODO: fix this
#endif
  DEBUG("Plugins initiated..");
}

std::string PluginApi::ProjectPath() {
  int MAXPATHLEN = 50;
  char temp[MAXPATHLEN];
  return ( getcwd(temp, MAXPATHLEN) ? std::string( temp ) : std::string("") );
}

void PluginApi::ReplaceWord(std::string word) {
  Glib::RefPtr<Gtk::TextBuffer> buffer = libjuci::BufferFromNotebook();
  Gtk::TextIter word_start = libjuci::IterFromNotebook();
  Gtk::TextIter word_end = libjuci::IterFromNotebook();
  libjuci::IterToWordStart(word_start);
  libjuci::IterToWordEnd(word_end);
  if (word_start != word_end) {
    buffer->erase(word_start, word_end);
    Gtk::TextIter current = libjuci::IterFromNotebook();
    buffer->insert(current, word);
  }
}

void PluginApi::ReplaceLine(std::string line) {
  WARNING("use of unimplemented method");
}

std::string PluginApi::GetWord() {
  Glib::RefPtr<Gtk::TextBuffer> buffer = libjuci::BufferFromNotebook();
  Gtk::TextIter word_start = libjuci::IterFromNotebook();
  Gtk::TextIter word_end = libjuci::IterFromNotebook();

  libjuci::IterToWordStart(word_start);
  libjuci::IterToWordEnd(word_end);

  if (word_start < word_end) {
    std::string word = buffer->get_text(word_start, word_end);
    return word;
  }
  return "";
}

void PluginApi::InitPlugins() {
  libjuci::LoadPlugin(ProjectPath()+"/plugins.py");
}

void PluginApi::AddMenuElement(std::string plugin_name) {
  DEBUG("Adding menu element for "+plugin_name);
  AddMenuXml(plugin_name, "PluginMenu");
  std::string plugin_action_name = plugin_name+"Menu";
  Singleton::keybindings()->action_group_menu
    ->add(Gtk::Action::create(plugin_action_name, plugin_name));
}

void PluginApi::AddSubMenuElement(std::string parent_menu,
                                  std::string menu_name,
                                  std::string menu_func_name,
                                  std::string plugin_path,
                                  std::string menu_keybinding) {
  AddSubMenuXml(menu_func_name, parent_menu);
  Singleton::keybindings()->action_group_menu
    ->add(Gtk::Action::create(menu_func_name,
                              menu_name),
          Gtk::AccelKey(menu_keybinding),
          [=]() {
            libjuci::LoadPluginFunction(menu_func_name, plugin_path);
          });
}

void PluginApi::AddMenuXml(std::string plugin_name, std::string parent_menu) {
  std::string temp_menu = Singleton::keybindings()->menu_ui_string;
  std::size_t plugin_menu_pos = temp_menu.find(parent_menu);
  // +2 gets you outside of the tag:<'menu_name'> ref: keybindings.cc
  plugin_menu_pos+=parent_menu.size() +2;
  std::string menu_prefix = temp_menu.substr(0, plugin_menu_pos);
  std::string menu_suffix = temp_menu.substr(plugin_menu_pos);
  std::string menu_input =
    "           <menu action='"+plugin_name+"Menu'>               "
    "           </menu>                                           ";

  Singleton::keybindings()->menu_ui_string =
    menu_prefix + menu_input + menu_suffix;
}

void PluginApi::AddSubMenuXml(std::string plugin_name,
                              std::string parent_menu) {
  std::string temp_menu = Singleton::keybindings()->menu_ui_string;

  std::size_t parent_menu_pos = temp_menu.find(parent_menu);
  // +2 gets you outside of the tag:<'menu_name'> ref: keybindings.cc
  parent_menu_pos+=parent_menu.size() +2;
  std::string menu_prefix = temp_menu.substr(0, parent_menu_pos);
  std::string menu_suffix = temp_menu.substr(parent_menu_pos);

  std::string menu_input ="<menuitem action='"+plugin_name+"'/>";
  Singleton::keybindings()->menu_ui_string =
    menu_prefix + menu_input + menu_suffix;
}

///////////////////////
//// Api to python ////
///////////////////////
void libjuci::ReplaceWord(const std::string word) {
  PluginApi::ReplaceWord(word);
}

void libjuci::ReplaceLine(const std::string line) {
  std::cout << "unimplemented: " << __func__ << " called"
            << std::endl;
}
std::string libjuci::GetWord() {
  return PluginApi::GetWord();
}

void libjuci::AddMenuElement(std::string plugin_name) {
  PluginApi::AddMenuElement(plugin_name);
}

void libjuci::AddSubMenuElement(std::string parent_menu,
                                std::string menu_name,
                                std::string menu_func_name,
                                std::string plugin_path,
                                std::string menu_keybinding) {
  PluginApi::AddSubMenuElement(parent_menu,
                               menu_name,
                               menu_func_name,
                               plugin_path,
                               menu_keybinding);
}

//////////////////////////////
//// Boost.Python methods ////
//////////////////////////////
namespace bp = boost::python;

bp::api::object libjuci::OpenPythonScript(const std::string path,
                                          bp::api::object python_name_space) {
  bp::str str_path(path);
  return bp::exec_file(str_path, python_name_space);
}

void libjuci::LoadPlugin(const std::string& plugin_name) {
  try {
    Py_Initialize();
    bp::api::object main_module = bp::import("__main__");
    bp::api::object main_namespace =
      main_module.attr("__dict__");
    bp::api::object ignored2 =
      OpenPythonScript(plugin_name, main_namespace);
  }catch (bp::error_already_set const&) {
    PyErr_Print();
  }
}

void libjuci::LoadPluginFunction(const std::string &function_name,
                                 const std::string &plugin_path) {
  try {
    Py_Initialize();
    bp::api::object main_module = bp::import("__main__");
    bp::api::object main_namespace = main_module.attr("__dict__");
    bp::api::object ignored2 = OpenPythonScript(plugin_path, main_namespace);
    bp::str func_name(function_name);
    bp::api::object returnValue = bp::eval(func_name, main_namespace);
  }catch (bp::error_already_set const&) {
    PyErr_Print();
  }
}

void libjuci::InitPlugin(const std::string& plugin_path) {
  try {
    Py_Initialize();
    bp::api::object main_module = bp::import("__main__");
    bp::api::object main_namespace = main_module.attr("__dict__");
    bp::api::object ignored2 = OpenPythonScript(plugin_path, main_namespace);
    bp::object returnValue  =  bp::eval("initPlugin()", main_namespace);
    // do something with return_value
  }catch (bp::error_already_set const&) {
    PyErr_Print();
  }
}

///////////////////////
//// Glib wrappers ////
///////////////////////
void libjuci::IterToWordStart(Gtk::TextIter &iter) {
  if (!iter.starts_line()) {
    while (!iter.starts_word()) {
      iter.backward_char();
    }
  }
}

void libjuci::IterToWordEnd(Gtk::TextIter &iter) {
  if (!iter.ends_line()) {
    while (!iter.ends_word()) {
      iter.forward_char();
    }
  }
}

Glib::RefPtr<Gtk::TextBuffer> libjuci::BufferFromNotebook() {
  return Glib::RefPtr<Gtk::TextBuffer>(PluginApi::notebook_
                                       ->CurrentSourceView()->get_buffer());
}

Gtk::TextIter libjuci::IterFromNotebook() {
  return libjuci::BufferFromNotebook()->get_insert()->get_iter();
}
