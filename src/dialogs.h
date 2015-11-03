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
#include <vector>

class CommonDialog {
public:
  CommonDialog(CLSID type);
  /** available options are listed https://msdn.microsoft.com/en-gb/library/windows/desktop/dn457282(v=vs.85).aspx */
  void add_option(unsigned option);
  void set_title(std::string &&title);
  /** Sets the extensions the browser can find */
  void set_default_file_extension(std::string &&file_extension);
  /** Sets the directory to start browsing */
  void set_default_folder(const std::string &directory_path);
  /** Returns the selected item's path as a string */
  std::string show();

protected:
  IFileDialog * dialog;
  DWORD options;
};

class OpenDialog : public CommonDialog {
public:
  OpenDialog(std::string &&title, unsigned option);
};
class SaveDialog : public CommonDialog {
public:
  SaveDialog(std::string &&title, unsigned option);
private:
  std::vector<COMDLG_FILTERSPEC> extensions;
};

#endif  // __WIN32
#endif  // JUCI_DIALOG_H_
