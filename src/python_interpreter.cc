#include "python_interpreter.h"
#include "notebook.h"
#include "config.h"
#include <iostream>
#include <pygobject.h>
#include "menu.h"
#include "directories.h"

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
    return pybind11::module(pygobject_new(obj), false);
  return pybind11::module(Py_None, false);
}

Python::Interpreter::Interpreter(){
#ifdef _WIN32
  auto root_path=Config::get().terminal.msys2_mingw_path;
  append_path(root_path/"include/python3.5m");
  append_path(root_path/"lib/python3.5");
  long long unsigned size = 0L;
#else
  long unsigned size = 0L;
#endif
  auto init_juci_api=[](){
    pybind11::module(pygobject_init(-1,-1,-1),false);
    pybind11::module api("jucpp","Python bindings for juCi++");
    api
    .def("get_juci_home",[](){return Config::get().juci_home_path().string();})
    .def("get_plugin_folder",[](){return Config::get().python.plugin_directory;});
    api
    .def_submodule("editor")
      .def("get_current_gtk_source_view",[](){
        auto view=Notebook::get().get_current_view();
        if(view)
          return pyobject_from_gobj(view->gobj());
        return pybind11::module(Py_None,false);
      })
      .def("get_file_path",[](){
        auto view=Notebook::get().get_current_view();
        if(view)
          return view->file_path.string();
        return std::string();
      });
    api
    .def("get_gio_plugin_menu",[](){
      auto &plugin_menu=Menu::get().plugin_menu;
      if(!plugin_menu){
        plugin_menu=Gio::Menu::create();
        plugin_menu->append("<empty>");
        Menu::get().window_menu->append_submenu("_Plugins",plugin_menu);
      }
      return pyobject_from_gobj(plugin_menu->gobj());
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
  auto plugin_path=Config::get().python.plugin_directory;
  add_path(Config::get().python.site_packages);
  add_path(plugin_path);
  Py_Initialize();
  argv=DecodeLocale("",&size);
  PySys_SetArgv(0,&argv);
  auto sys=get_loaded_module("sys");
  auto exc_func=[](pybind11::object type,pybind11::object value,pybind11::object traceback){
    if(!given_exception_matches(type,PyExc_SyntaxError))
      Terminal::get().print(Error(type,value,traceback),true);
    else
      Terminal::get().print(SyntaxError(type,value,traceback),true);
  };
  sys.attr("excepthook")=pybind11::cpp_function(exc_func);
  boost::filesystem::directory_iterator end_it;
  for(boost::filesystem::directory_iterator it(plugin_path);it!=end_it;it++){
    auto module_name=it->path().stem().string();
    if(module_name.empty())
      break;
    auto is_directory=boost::filesystem::is_directory(it->path());
    auto has_py_extension=it->path().extension()==".py";
    auto is_pycache=module_name=="__pycache__";
    if((is_directory && !is_pycache)||has_py_extension){
      auto module=import(module_name);
      if(!module){
        auto msg="Error loading plugin `"+module_name+"`:\n";
        auto err=std::string(Error());
        Terminal::get().print(msg+err+"\n");
      }
    }
  }
}

pybind11::module Python::get_loaded_module(const std::string &module_name){
  return pybind11::module(PyImport_AddModule(module_name.c_str()), true);
}

pybind11::module Python::import(const std::string &module_name){
  return pybind11::module(PyImport_ImportModule(module_name.c_str()), false);
}

pybind11::module Python::reload(pybind11::module &module){
  return pybind11::module(PyImport_ReloadModule(module.ptr()),false);
}

Python::SyntaxError::SyntaxError(pybind11::object type,pybind11::object value,pybind11::object traceback)
: Error(type,value,traceback){}

Python::Error::Error(pybind11::object type,pybind11::object value,pybind11::object traceback){
  exp=type;
  val=value;
  trace=traceback;
}

void Python::Interpreter::add_path(const boost::filesystem::path &path){
  if(path.empty())
    return;
  std::wstring sys_path(Py_GetPath());
  if(!sys_path.empty())
#ifdef _WIN32
    sys_path += ';';
#else
    sys_path += ':';
#endif
  sys_path += path.generic_wstring();
  Py_SetPath(sys_path.c_str());
}

Python::Interpreter::~Interpreter(){
  auto err=Error();
  if(Py_IsInitialized())
    Py_Finalize();
  if(err)
    std::cerr << std::string(err) << std::endl;
}

pybind11::object Python::error_occured(){
  return pybind11::object(PyErr_Occurred(),true);
}

bool Python::thrown_exception_matches(pybind11::handle exception_type){
  return PyErr_ExceptionMatches(exception_type.ptr());
}

bool Python::given_exception_matches(const pybind11::object &exception, pybind11::handle exception_type){
  return PyErr_GivenExceptionMatches(exception.ptr(),exception_type.ptr());
}

Python::Error::Error(){
  if(error_occured()){
    try{
      PyErr_Fetch(&exp.ptr(),&val.ptr(),&trace.ptr());
      PyErr_NormalizeException(&exp.ptr(),&val.ptr(),&trace.ptr());
    }catch(const std::exception &e) {
      Terminal::get().print(e.what(),true);
    }
  }
}

Python::Error::operator std::string(){
  return std::string(exp.str())+"\n"+std::string(val.str())+"\n";
}

Python::SyntaxError::SyntaxError():Error(){
  if(val){
    _Py_IDENTIFIER(msg);
    _Py_IDENTIFIER(lineno);
    _Py_IDENTIFIER(offset);
    _Py_IDENTIFIER(text);
    exp=std::string(pybind11::str(_PyObject_GetAttrId(val.ptr(),&PyId_msg),false));
    text=std::string(pybind11::str(_PyObject_GetAttrId(val.ptr(),&PyId_text),false));
    pybind11::object py_line_number(_PyObject_GetAttrId(val.ptr(),&PyId_lineno),false);
    pybind11::object py_line_offset(_PyObject_GetAttrId(val.ptr(),&PyId_offset),false);
    line_number=pybind11::cast<int>(py_line_number);
    line_offset=pybind11::cast<int>(py_line_offset);
  }
}

Python::SyntaxError::operator std::string(){
  return exp+" ("+std::to_string(line_number)+":"+std::to_string(line_offset)+"):\n"+text;
}

Python::Error::operator bool(){
  return exp || trace || val;
}
