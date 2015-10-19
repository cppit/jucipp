#ifndef JUCI_DIALOG_H_
#define JUCI_DIALOG_H_
#include <string>

class Dialog {
  public:
    static std::string select_folder();
    static std::string select_file();
    static std::string new_file();
    static std::string new_folder();
    static std::string save_file();
}; // namespace Dialog

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NTDDI_VERSION NTDDI_VISTA
#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <windows.h>
#include <shobjidl.h>

#include <memory>
#include <sstream>
#include <codecvt>
#include "singletons.h"

#ifndef check
HRESULT __hr__;
#define check(__fun__, error_message)            \
  __hr__ = __fun__;                              \
  if (FAILED(__hr__)) {                          \
    Singleton::terminal()->print(error_message); \
    throw std::exception(error_message);
  }
#endif


// http://stackoverflow.com/questions/4804298/how-to-convert-wstring-into-string
std::wstring s2ws(const std::string& str) {
  typedef std::codecvt_utf8<wchar_t> convert_typeX;
  std::wstring_convert<convert_typeX, wchar_t> converterX;
  return converterX.from_bytes(str);
}

std::string ws2s(const std::wstring& wstr) {
  typedef std::codecvt_utf8<wchar_t> convert_typeX;
  std::wstring_convert<convert_typeX, wchar_t> converterX;
  return converterX.to_bytes(wstr);
}

class WinString {
public:
  WinString() : str(nullptr) { }
  WinString(const std::string &string);
  ~WinString() { CoTaskMemFree(static_cast<void*>(str)); }
  std::string operator()();
  wchar_t** operator&() { return &str; }
private:
  wchar_t* str;
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
#endif
#endif  // JUCI_DIALOG_H_
