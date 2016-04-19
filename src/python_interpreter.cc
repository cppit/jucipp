#include "python_interpreter.h"
#include "notebook.h"
#include "config.h"
#include <iostream>
#include <pygobject.h>
#include "menu.h"
#include "directories.h"
#include "terminal.h"

static wchar_t* DecodeLocale(const char* arg, size_t *size)
{
#ifndef PY_VERSION_HEX
#error Python not included
#elif PY_VERSION_HEX < 0x03050000
  return _Py_char2wchar(arg, size);
#else
  return Py_DecodeLocale(arg, size);
#endif
}

inline pybind11::module pyobject_from_gobj(gpointer ptr){
  auto obj=G_OBJECT(ptr);
  if(obj)
    return pybind11::reinterpret_steal<pybind11::module>(pygobject_new(obj));
  return pybind11::reinterpret_steal<pybind11::module>(Py_None);
}

Python::Interpreter::Interpreter(){
  
  auto init_juci_api=[](){
    auto module = pybind11::reinterpret_steal<pybind11::module>(pygobject_init(-1,-1,-1));
    pybind11::module api("jucpp","Python bindings for juCi++");
    api
    .def("get_juci_home",[](){return Config::get().home_juci_path.string();})
    .def("get_plugin_folder",[](){return Config::get().python.plugin_directory;});
    api
    .def_submodule("editor")
      .def("get_current_gtk_source_view",[](){
        auto view=Notebook::get().get_current_view();
        if(view)
          return pyobject_from_gobj(view->gobj());
        return pybind11::reinterpret_steal<pybind11::module>(Py_None);
      })
      .def("get_file_path",[](){
        auto view=Notebook::get().get_current_view();
        if(view)
          return view->file_path.string();
        return std::string();
      });
    api
    .def("get_gio_plugin_menu",[](){
      if(!Menu::get().plugin_menu){
        Menu::get().plugin_menu=Gio::Menu::create();
        Menu::get().plugin_menu->append("<empty>");
        Menu::get().window_menu->append_submenu("_Plugins",Menu::get().plugin_menu);
      }
      return pyobject_from_gobj(Menu::get().plugin_menu->gobj());
    })
    .def("get_gio_window_menu",[](){return pyobject_from_gobj(Menu::get().window_menu->gobj());})
    .def("get_gio_juci_menu",[](){return pyobject_from_gobj(Menu::get().juci_menu->gobj());})
    .def("get_gtk_notebook",[](){return pyobject_from_gobj(Notebook::get().gobj());})
    .def_submodule("terminal")
      .def("get_gtk_text_view",[](){return pyobject_from_gobj(Terminal::get().gobj());})
      .def("println", [](const std::string &message){ Terminal::get().print(message +"\n"); });
    api.def_submodule("directories")
    .def("get_gtk_treeview",[](){return pyobject_from_gobj(Directories::get().gobj());})
    .def("open",[](const std::string &directory){Directories::get().open(directory);})
    .def("update",[](){Directories::get().update();});
    return api.ptr();
  };
  PyImport_AppendInittab("jucipp", init_juci_api);

  Config::get().load();
  configure_path();
  Py_Initialize();
  #ifdef _WIN32
    long long unsigned size = 0L;
  #else
    long unsigned size = 0L;
  #endif
  argv=DecodeLocale("",&size);
  PySys_SetArgv(0,&argv);
  boost::filesystem::directory_iterator end_it;
  for(boost::filesystem::directory_iterator it(Config::get().python.plugin_directory);it!=end_it;it++){
    auto module_name=it->path().stem().string();
    if(module_name.empty())
      continue;
    auto is_directory=boost::filesystem::is_directory(it->path());
    auto has_py_extension=it->path().extension()==".py";
    auto is_pycache=module_name=="__pycache__";
    if((is_directory && !is_pycache)||has_py_extension){
      try {
        pybind11::module::import(module_name.c_str());
      } catch (pybind11::error_already_set &error) {
        Terminal::get().print("Error loading plugin `"+module_name+"`:\n"+error.what()+"\n");
      }
    }
  }
  auto sys=find_module("sys");
  if(sys){
    auto exc_func=[](pybind11::object type,pybind11::object value,pybind11::object traceback){
      std::cerr << "ERROR FUNCTION";
    };
    sys.attr("excepthook")=pybind11::cpp_function(exc_func);
  } else {
    std::cerr << "Failed to set exception hook\n";
  }
}

pybind11::module Python::Interpreter::find_module(const std::string &module_name){
  return pybind11::reinterpret_borrow<pybind11::module>(PyImport_AddModule(module_name.c_str()));
}

pybind11::module Python::Interpreter::reload(pybind11::module &module){
  auto reload=pybind11::reinterpret_steal<pybind11::module>(PyImport_ReloadModule(module.ptr()));
  if(!reload)
    throw pybind11::error_already_set();
  return reload;
}

void Python::Interpreter::configure_path(){
  const std::vector<boost::filesystem::path> python_path = {
    "/usr/lib/python3.6",
    "/usr/lib/python3.6/lib-dynload",
    "/usr/lib/python3.6/site-packages",
    Config::get().python.site_packages,
    Config::get().python.plugin_directory
  };
  std::wstring sys_path;
  for(auto &path:python_path){
    if(path.empty())
      continue;
    if(!sys_path.empty()){
    #ifdef _WIN32
        sys_path += ';';
    #else
        sys_path += ':';
    #endif
    }
    sys_path += path.generic_wstring();
  }
  Py_SetPath(sys_path.c_str());
}

Python::Interpreter::~Interpreter(){
  if(Py_IsInitialized())
    Py_Finalize();
  if(error())
    std::cerr << pybind11::error_already_set().what() << std::endl;
}

pybind11::object Python::Interpreter::error(){
  return pybind11::reinterpret_borrow<pybind11::object>(PyErr_Occurred());
}

