#include "dialogs.h"
#include <windows.h>
#include <shobjidl.h>
#include <memory>
#include <exception>
#include "singletons.h"
#include <sstream>

#ifndef check
#define MESSAGE "An error occurred when trying open windows dialog"
HRESULT __hr__;
#define check(__fun__) __hr__ = __fun__; if(FAILED(__hr__)) Singleton::terminal()->print(MESSAGE)
#endif

class win_string {
  public:
  win_string() : str(nullptr) { }
  ~win_string() { CoTaskMemFree(static_cast<void*>(str)); }
  std::string operator()(){
    std::string res;
    if (str != nullptr) {
      std::wstringstream ss;
      ss << str;
      res = std::string(ss.str().begin(), ss.str().end());
    }
    return res;
  }
  wchar_t** operator&() { return &str; }
  private:
    wchar_t* str;
};

class CommonDialog {
  public:
    CommonDialog() : dialog(nullptr) {
      check(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)));
      check(dialog->GetOptions(&options));
    }
    CommonDialog(const std::string &title, unsigned option) : CommonDialog() {
      set_title(title);
      add_option(option);
    }
    void set_title(const std::string &title) { check(dialog->SetTitle(title)); }
    void add_option(unsigned option) { check(dialog->SetOptions(options | option)); }
    std::string show() {
      check(dialog->Show(nullptr));
      IShellItem *result = nullptr;
      check(dialog->GetResult(&result));
      win_string str;
      check(result->GetDisplayName(SIGDN_FILESYSPATH, &str));;
      result->Release();
      return str();
    }
  
  private:
    IFileOpenDialog * dialog;
    DWORD options;
};

std::string Dialog::select_folder() {
  return (CommonDialog("Please select a folder", FOS_PICKFOLDERS)).show();
}

std::string Dialog::new_file() {
  return (CommonDialog("Please select a folder", FOS_PICKFOLDERS)).show();
}

std::string Dialog::new_folder() {
    return (CommonDialog("Please select a folder", FOS_PICKFOLDERS)).show();
}

std::string Dialog::select_file() {
  return (CommonDialog("Please select a folder", FOS_PICKFOLDERS)).show();
}

std::string Dialog::save_file() {
    return (CommonDialog("Please select a folder", FOS_PICKFOLDERS)).show();
}