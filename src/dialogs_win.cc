#include "dialogs.h"
#include "singletons.h"

#include <iostream> //TODO: remove
using namespace std; //TODO: remove

#ifndef check
HRESULT __hr__;
#define check(__fun__, error_message)            \
  __hr__ = __fun__;                              \
  if (FAILED(__hr__)) {                          \
    Singleton::terminal->print(error_message); \
    throw std::runtime_error(error_message);	 \
}                                                
#endif  // CHECK

#undef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_VISTA
#undef _WIN32_WINNT
#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <windows.h>
#include <shobjidl.h>

#include <memory>
#include <sstream>
#include <codecvt>

class WinString {
public:
  WinString() : str(nullptr) { }
  WinString(const std::string &string);
  ~WinString() { CoTaskMemFree(static_cast<void*>(str)); }
  std::string operator()();
  wchar_t** operator&() { return &str; }
private:
  wchar_t* str;
  std::wstring s2ws(const std::string& str);
  std::string ws2s(const std::wstring& wstr);
};

class CommonDialog {
public:
  CommonDialog(CLSID type);
  void add_option(unsigned option);
  void set_title(const std::string &title);
  std::string show();

private:
  IFileDialog * dialog;
  DWORD options;
};

class OpenDialog : public CommonDialog {
public:
  OpenDialog(const std::string &title, unsigned option) : CommonDialog(CLSID_FileOpenDialog) {
    set_title(title);
    add_option(option);
  }
};

class SaveDialog : public CommonDialog {
public:
  SaveDialog(const std::string &title, unsigned option) : CommonDialog(CLSID_FileSaveDialog) {
    set_title(title);
    add_option(option);
  }
};

// { WINSTRING
WinString::WinString(const std::string &string) {
  std::wstringstream ss;
    ss << s2ws(string);
    ss >> str;
}

std::string WinString::operator()() {
  std::string res;
  if (str != nullptr) {
    std::wstring ss(str);
    res = ws2s(ss);
  }
  return res;
}

// http://stackoverflow.com/questions/4804298/how-to-convert-wstring-into-string
std::wstring WinString::s2ws(const std::string& str) {
  typedef std::codecvt_utf8<wchar_t> convert_typeX;
  std::wstring_convert<convert_typeX, wchar_t> converterX;
  return converterX.from_bytes(str);
}

std::string WinString::ws2s(const std::wstring& wstr) {
  typedef std::codecvt_utf8<wchar_t> convert_typeX;
  std::wstring_convert<convert_typeX, wchar_t> converterX;
  return converterX.to_bytes(wstr);
}

// WINSTRING }

// { COMMON_DIALOG
CommonDialog::CommonDialog(CLSID type) : dialog(nullptr) {
    check(CoCreateInstance(type, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)), "Failed to create instance");
    check(dialog->GetOptions(&options), "Failed to get options from instance");
}

void CommonDialog::set_title(const std::string &title) {
    auto tmp = std::wstring(title.begin(), title.end());
    auto t = tmp.data();
    check(dialog->SetTitle(t), "Failed to set dialog title");
}

void CommonDialog::add_option(unsigned option) {
  check(dialog->SetOptions(options | option), "Failed to set options");
}

std::string CommonDialog::show() {
  if(dialog->Show(nullptr)!=S_OK) {
    dialog->Release();
    return "";
  }
  IShellItem *result = nullptr;
  if(dialog->GetResult(&result)!=S_OK) {
    result->Release();
    return "";
  }
  WinString str;
  if(result->GetDisplayName(SIGDN_FILESYSPATH, &str)!=S_OK)
    return "";
  result->Release();
  dialog->Release();
  return str();
}
// COMMON_DIALOG }}
std::string Dialog::select_folder() {
  return (OpenDialog("Select folder", FOS_PICKFOLDERS)).show();
}

std::string Dialog::new_file() {
  return (SaveDialog("Please choose your destination", 0)).show();
}

std::string Dialog::new_folder() {
  return Dialog::select_folder();
}

std::string Dialog::select_file() {
  return (OpenDialog("Open file", 0)).show();
}

std::string Dialog::save_file(const boost::filesystem::path file_path) {
  //TODO: use file_path
  return (SaveDialog("Please choose your destination", 0)).show();
}
