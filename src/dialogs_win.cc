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

class Win32Dialog {
public:
  Win32Dialog() {};
  
  bool init(CLSID type) {
    if(CoCreateInstance(type, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog))!=S_OK)
      return false;
    if(dialog->GetOptions(&options)!=S_OK)
      return false;
    return true;
  }
  
  ~Win32Dialog() {
    if(dialog!=nullptr)
      dialog->Release();
  }
  
  /** available options are listed at https://msdn.microsoft.com/en-gb/library/windows/desktop/dn457282(v=vs.85).aspx */
  bool add_option(unsigned option) {
    if(dialog->SetOptions(options | option)!=S_OK)
      return false;
    return true;
  }
  
  bool set_title(const std::wstring &title) {
    if(dialog->SetTitle(title.c_str())!=S_OK)
      return false;
    return true;
  }
  
  /** Sets the extensions the browser can find */
  bool set_default_file_extension(const std::wstring &file_extension) {
    if(dialog->SetDefaultExtension(file_extension.c_str())!=S_OK)
      return false;
    return true;
  }
  /** Sets the directory to start browsing */
  bool set_folder(const std::wstring &directory_path) {
    std::wstring path=directory_path;
    size_t pos=0;
    while((pos=path.find(L'/', pos))!=std::wstring::npos) {//TODO: issue bug report on boost::filesystem::path::native on MSYS2
      path.replace(pos, 1, L"\\");
      pos++;
    }
    
    IShellItem *folder = nullptr;
    if(SHCreateItemFromParsingName(path.c_str(), nullptr, IID_PPV_ARGS(&folder))!=S_OK)
      return false;
    if(dialog->SetFolder(folder)!=S_OK)
      return false;
    folder->Release();
    return true;
  }
  
  /** Returns the selected item's path as a string */
  std::string open(const std::wstring &title, unsigned option=0) {
    if(!init(CLSID_FileOpenDialog))
      return "";
    
    if(!set_title(title) || !add_option(option))
      return "";
    auto dirs = Singleton::directories->current_path;
    if(!set_folder(dirs.empty() ? boost::filesystem::current_path().native() : dirs.native()))
      return "";
    
    return show();
  }
  
  std::string save(const std::wstring &title, const boost::filesystem::path &file_path="", unsigned option=0) {
    if(!init(CLSID_FileSaveDialog))
      return "";
    
    if(!set_title(title) || !add_option(option))
      return "";
    std::vector<COMDLG_FILTERSPEC> extensions;
    if(!file_path.empty()) {
      if(!set_folder(file_path.parent_path().native()))
        return "";
      if(file_path.has_extension() && file_path.filename()!=file_path.extension()) {
        auto extension=(L"*"+file_path.extension().native()).c_str();
        extensions.emplace_back(COMDLG_FILTERSPEC{extension, extension});
        if(!set_default_file_extension(extension))
          return "";
      }
    }
    else {
      auto dirs = Singleton::directories->current_path;
      if(!set_folder(dirs.empty() ? boost::filesystem::current_path().native() : dirs.native()))
        return "";
    }
    extensions.emplace_back(COMDLG_FILTERSPEC{L"All files", L"*.*"});
    if(dialog->SetFileTypes(extensions.size(), extensions.data())!=S_OK)
      return "";
    
    return show();
  }

private:
  std::string show() {
    if(dialog->Show(nullptr)!=S_OK)
      return "";
    IShellItem *result = nullptr;
    if(dialog->GetResult(&result)!=S_OK)
      return "";
    LPWSTR file_path = nullptr; 
    auto hresult=result->GetDisplayName(SIGDN_FILESYSPATH, &file_path);
    result->Release();
    if(hresult!=S_OK)
      return "";
    std::wstring file_path_wstring(file_path);
    std::string file_path_string(file_path_wstring.begin(), file_path_wstring.end());
    CoTaskMemFree(file_path);
    return file_path_string;
  }
  
  IFileDialog *dialog=nullptr;
  DWORD options;
};

std::string Dialog::open_folder() {
  return Win32Dialog().open(L"Open Folder", FOS_PICKFOLDERS);
}

std::string Dialog::new_file() {
  return Win32Dialog().save(L"New File");
}

std::string Dialog::new_folder() {
  return  Win32Dialog().open(L"New Folder", FOS_PICKFOLDERS); //TODO: this is not working correctly yet
}

std::string Dialog::open_file() {
  return  Win32Dialog().open(L"Open File");
}

std::string Dialog::save_file_as(const boost::filesystem::path &file_path) {
  return Win32Dialog().save(L"Save File As", file_path);
}
