#ifndef JUCI_API_H_
#define JUCI_API_H_

#include <boost/python.hpp>
#include <Python.h>
#include <string>
#include "notebook.h"
#include "menu.h"

////////////////////
//// Plugin Api ////
////////////////////
class PluginApi {
public:
  static Menu::Controller* menu_;
  static Notebook::Controller* notebook_;
  static void InitPlugins();
  static std::string ProjectPath();
  // for Python module:
  static std::string GetWord();
  // menu management
  static void AddMenuElement(const std::string plugin_name);
  static void AddSubMenuElement(const std::string parent_menu,
                                const std::string menu_name,
                                const std::string menu_func_name,
                                const std::string plugin_path,
                                const std::string menu_keybinding);
  static void ReplaceWord(const std::string word);
  static void ReplaceLine(const std::string line);
protected:
  static void AddMenuXml(std::string plugin_name, std::string parent_menu);
  static void AddSubMenuXml(std::string plugin_name, std::string parent_menu);
};  // PluginApi
namespace libjuci {
  ///////////////////////
  //// Glib wrappers ////
  ///////////////////////
  void IterToWordStart(Gtk::TextIter &iter);
  void IterToWordEnd(Gtk::TextIter &iter);
  Gtk::TextIter IterFromNotebook();
  Glib::RefPtr<Gtk::TextBuffer> BufferFromNotebook();
  ///////////////////////
  //// Api to python ////
  ///////////////////////
  void ReplaceWord(const std::string word);
  void ReplaceLine(const std::string line);
  std::string GetWord();

  void AddMenuElement(const std::string plugin_name);

  void AddSubMenuElement(const std::string parent_menu,
                         const std::string menu_name,
                         const std::string menu_func_name,
                         const std::string plugin_path,
                         const std::string menu_keybinding);
  void AddMenuXml(const std::string plugin_name,
                  const std::string parent_menu);
  void AddSubMenuXml(const std::string plugin_name,
                     const std::string parent_menu);
  //////////////////////////////
  //// Boost.Python methods ////
  //////////////////////////////
  namespace bp = boost::python;
  bp::api::object OpenPythonScript(const std::string path,
                                   bp::api::object python_name_space);
  void LoadPlugin(const std::string& plugin_name);
  void LoadPluginFunction(const std::string &function_name,
                          const std::string &plugin_path);

  void InitPlugin(const std::string& plugin_path);
}  // namespace libjuci
#endif  // JUCI_API_H_
