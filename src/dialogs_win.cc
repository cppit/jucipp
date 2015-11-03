#include "dialogs.h"
#include "singletons.h"
#include <boost/locale.hpp>

#undef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_VISTA
#undef _WIN32_WINNT
#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <windows.h>
#include <shobjidl.h>
#include <vector>

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

class CommonDialog {
public:
  CommonDialog(CLSID type);
  /** available options are listed https://msdn.microsoft.com/en-gb/library/windows/desktop/dn457282(v=vs.85).aspx */
  void add_option(unsigned option);
  void set_title(const std::string &title);
  /** Sets the extensions the browser can find */
  void set_default_file_extension(const std::string &file_extension);
  /** Sets the directory to start browsing */
  void set_default_folder(const std::string &directory_path);
  /** Returns the selected item's path as a string */
  std::string show();

protected:
  IFileDialog * dialog;
  DWORD options;
  bool error=false;
};

class OpenDialog : public CommonDialog {
public:
  OpenDialog(const std::string &title, unsigned option);
};

class SaveDialog : public CommonDialog {
public:
  SaveDialog(const std::string &title, unsigned option);
private:
  std::vector<COMDLG_FILTERSPEC> extensions;
};

// { COMMON_DIALOG
CommonDialog::CommonDialog(CLSID type) : dialog(nullptr) {
    if(CoCreateInstance(type, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog))!=S_OK) {
      error=true;
      return;
    }
    if(dialog->GetOptions(&options)!=S_OK)
      error=true;
}
void CommonDialog::set_title(const std::string &title) {
  if(error) return;
  auto ptr = boost::locale::conv::utf_to_utf<wchar_t>(title).data();
  if(dialog->SetTitle(ptr)!=S_OK)
    error=true;
}
void CommonDialog::add_option(unsigned option) {
  if(error) return;
  if(dialog->SetOptions(options | option)!=S_OK)
    error=true;
}
void CommonDialog::set_default_file_extension(const std::string &file_extension) {
  if(error) return;
  auto ptr = boost::locale::conv::utf_to_utf<wchar_t>(file_extension).data();
  if(dialog->SetDefaultExtension(ptr)!=S_OK)
    error=true;
}
void CommonDialog::set_default_folder(const std::string &directory_path) {
  if(error) return;
  IShellItem * folder = nullptr;
  auto ptr = boost::locale::conv::utf_to_utf<wchar_t>(directory_path).data();
  if(SHCreateItemFromParsingName(ptr, nullptr, IID_PPV_ARGS(&folder))!=S_OK) {
    error=true;
    return;
  }
  auto result=dialog->SetDefaultFolder(folder)
  folder->Release();
  if(result!=S_OK)
    error=true;
}
std::string CommonDialog::show() {
  if(error) return "";
  if(dialog->Show(nullptr)!=S_OK) {
    dialog->Release();
    return "";
  }
  IShellItem *result = nullptr;
  if(dialog->GetResult(&result)!=S_OK) {
    result->Release();
    dialog->Release();
    return "";
  }
  LPWSTR str = nullptr; 
  auto result=result->GetDisplayName(SIGDN_FILESYSPATH, &str);
  result->Release();
  dialog->Release();
  if(result!=S_OK)
    return "";
  auto res = boost::locale::conv::utf_to_utf<char>(str);
  CoTaskMemFree(str);
  return res;
}

OpenDialog::OpenDialog(const std::string &title, unsigned option) : CommonDialog(CLSID_FileOpenDialog) {
  set_title(title);
  add_option(option);
  auto dirs = Singleton::directories()->current_path;
  set_default_folder(dirs.empty() ? boost::filesystem::current_path().string() : dirs.string());
}

SaveDialog::SaveDialog(const std::string &title, unsigned option) : CommonDialog(CLSID_FileSaveDialog) {
  set_title(title);
  add_option(option);
  auto dirs = Singleton::directories()->current_path;
  set_default_folder(dirs.empty() ? boost::filesystem::current_path().string() : dirs.string());
  //extensions.emplace_back(COMDLG_FILTERSPEC{L"Default", L"*.h;*.cpp"});
  //extensions.emplace_back(COMDLG_FILTERSPEC{L"GoogleStyle", L"*.cc;*.h"});
  //extensions.emplace_back(COMDLG_FILTERSPEC{L"BoostStyle", L"*.hpp;*.cpp"});
  //extensions.emplace_back(COMDLG_FILTERSPEC{L"Other", L"*.cxx;*.c"});
  //check(dialog->SetFileTypes(extensions.size(), extensions.data()), "Failed to set extensions");
  //set_default_file_extension("Default");
}

// DIALOGS }}
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
