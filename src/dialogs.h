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
  /** available options are listed https://msdn.microsoft.com/en-gb/library/windows/desktop/dn457282(v=vs.85).aspx */
  void add_option(unsigned option);
  void set_title(const std::string &title);
  /** Sets the extensions the browser can find */
  void set_file_extensions(const std::vector<std::string> file_extensions);
  /** Sets the directory to start browsing */
  void set_default_folder(const std::string &directory_path);
  /** Returns the selected item's path as a string */
  std::string show();

private:
  IFileDialog * dialog;
  DWORD options;
};

class OpenDialog : public CommonDialog {
public:
  OpenDialog(const std::string &title, unsigned option) : CommonDialog(CLSID_FileOpenDialog);
};
class SaveDialog : public CommonDialog {
public:
  SaveDialog(const std::string &title, unsigned option) : CommonDialog(CLSID_FileSaveDialog);
};

#endif  // __WIN32
#endif  // JUCI_DIALOG_H_
