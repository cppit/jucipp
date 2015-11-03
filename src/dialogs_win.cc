#include "dialogs.h"
#include "singletons.h"

#undef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_VISTA
#undef _WIN32_WINNT
#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <windows.h>
#include <shobjidl.h>
#include <vector>

#include <iostream> //TODO: remove
using namespace std; //TODO: remove

class CommonDialog {
public:
  CommonDialog(CLSID type);
  ~CommonDialog() {
    if(dialog!=nullptr)
      dialog->Release();
  }
  /** available options are listed at https://msdn.microsoft.com/en-gb/library/windows/desktop/dn457282(v=vs.85).aspx */
  void add_option(unsigned option);
  void set_title(const std::wstring &title);
  /** Sets the extensions the browser can find */
  void set_default_file_extension(const std::wstring &file_extension);
  /** Sets the directory to start browsing */
  void set_folder(const std::wstring &directory_path);
  /** Returns the selected item's path as a string */
  std::string show();

protected:
  IFileDialog *dialog=nullptr;
  DWORD options;
  bool error=false;
};

class OpenDialog : public CommonDialog {
public:
  OpenDialog(const std::wstring &title, unsigned option=0);
};

class SaveDialog : public CommonDialog {
public:
  SaveDialog(const std::wstring &title, const boost::filesystem::path &file_path="", unsigned option=0);
private:
  std::vector<COMDLG_FILTERSPEC> extensions;
};

CommonDialog::CommonDialog(CLSID type) {
  if(CoCreateInstance(type, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog))!=S_OK) {
    error=true;
    return;
  }
  if(dialog->GetOptions(&options)!=S_OK)
    error=true;
}

void CommonDialog::set_title(const std::wstring &title) {
  if(error) return;
  if(dialog->SetTitle(title.c_str())!=S_OK)
    error=true;
}

void CommonDialog::add_option(unsigned option) {
  if(error) return;
  if(dialog->SetOptions(options | option)!=S_OK)
    error=true;
}

void CommonDialog::set_default_file_extension(const std::wstring &file_extension) {
  if(error) return;
  if(dialog->SetDefaultExtension(file_extension.c_str())!=S_OK)
    error=true;
}

void CommonDialog::set_folder(const std::wstring &directory_path) {
  if(error) return;
  
  std::wstring path=directory_path;
  size_t pos=0;
  while((pos=path.find(L'/', pos))!=std::wstring::npos) {//TODO: issue bug report on boost::filesystem::path::native on MSYS2
    path.replace(pos, 1, L"\\");
    pos++;
  }
  
  IShellItem * folder = nullptr;
  if(SHCreateItemFromParsingName(path.c_str(), nullptr, IID_PPV_ARGS(&folder))!=S_OK) {
    error=true;
    return;
  }
  auto hresult=dialog->SetFolder(folder);
  folder->Release();
  if(hresult!=S_OK)
    error=true;
}

std::string CommonDialog::show() {
  if(error) return "";
  if(dialog->Show(nullptr)!=S_OK) {
    return "";
  }
  IShellItem *result = nullptr;
  if(dialog->GetResult(&result)!=S_OK) {
    result->Release();
    return "";
  }
  LPWSTR str = nullptr; 
  auto hresult=result->GetDisplayName(SIGDN_FILESYSPATH, &str);
  result->Release();
  if(hresult!=S_OK)
    return "";
  std::wstring wstr(str);
  std::string res(wstr.begin(), wstr.end());
  CoTaskMemFree(str);
  return res;
}

OpenDialog::OpenDialog(const std::wstring &title, unsigned option) : CommonDialog(CLSID_FileOpenDialog) {
  set_title(title);
  add_option(option);
  auto dirs = Singleton::directories->current_path;
  set_folder(dirs.empty() ? boost::filesystem::current_path().native() : dirs.native());
}

SaveDialog::SaveDialog(const std::wstring &title, const boost::filesystem::path &file_path, unsigned option) : CommonDialog(CLSID_FileSaveDialog) {
  set_title(title);
  add_option(option);
  if(!file_path.empty()) {
    set_folder(file_path.parent_path().native());
    if(file_path.has_extension() && file_path.filename()!=file_path.extension()) {
      auto extension=(L"*"+file_path.extension().native()).c_str();
      extensions.emplace_back(COMDLG_FILTERSPEC{extension, extension});
      set_default_file_extension(extension);
    }
  }
  else {
    auto dirs = Singleton::directories->current_path;
    set_folder(dirs.empty() ? boost::filesystem::current_path().native() : dirs.native());
  }
  extensions.emplace_back(COMDLG_FILTERSPEC{L"All files", L"*.*"});
  if(dialog->SetFileTypes(extensions.size(), extensions.data())!=S_OK) {
    error=true;
    return;
  }
}

std::string Dialog::open_folder() {
  return (OpenDialog(L"Open Folder", FOS_PICKFOLDERS)).show();
}

std::string Dialog::new_file() {
  return (SaveDialog(L"New File")).show();
}

std::string Dialog::new_folder() {
  return (OpenDialog(L"New Folder", FOS_PICKFOLDERS)).show();
}

std::string Dialog::open_file() {
  return (OpenDialog(L"Open File")).show();
}

std::string Dialog::save_file_as(const boost::filesystem::path &file_path) {
  return (SaveDialog(L"Save File As", file_path)).show();
}
