#ifndef JUCI_DIALOG_H_
#define JUCI_DIALOG_H_
#include <string>
#include <boost/filesystem.hpp>

class Dialog {
public:
  static std::string select_folder();
  static std::string select_file();
  static std::string new_file();
  static std::string new_folder();
  static std::string save_file(const boost::filesystem::path file_path);
}; // Dialog

#ifdef _WIN32
#define NTDDI_VERSION NTDDI_VISTA
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
#endif  // __WIN32
#endif  // JUCI_DIALOG_H_
