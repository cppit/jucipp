#ifndef JUCI_API_H_
#define JUCI_API_H_

#include <boost/python.hpp>
#include <Python.h>
#include <string>
#include "notebook.h"
#include "menu.h"

const std::string g_project_root("/home/forgie/app/juci/");

namespace libjuci {

  /////////////////////////////
  //// API ServiceProvider ////
  /////////////////////////////
  struct ApiServiceProvider {
  public:
    static std::shared_ptr<Menu::Controller> menu_;
    static std::shared_ptr<Notebook::Controller> notebook_;

    static std::string text;
    
    ApiServiceProvider();
    static std::string GetWord();
    static void ReplaceWord(const std::string word);
    static void ReplaceLine(const std::string line);

    static void AddKeybinding();
  };
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
  void ReplaceWord(const std::string word_);

  void ReplaceLine(const std::string line_);

  std::string GetWord();

  //////////////////////////////
  //// Boost.Python methods ////
  //////////////////////////////
  boost::python::api::object openPythonScript(const std::string path,
					      boost::python::api::object python_name_space);
    
  //void snippet(std::string& word);

  void LoadPlugin(const std::string& plugin_name);

}//api
#endif // JUCI_API_H
