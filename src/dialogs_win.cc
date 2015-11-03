#ifdef _WIN32
#include "dialogs.h"
#include "singletons.h"
#include <boost/locale.hpp>
#ifndef check
HRESULT __hr__;
#define check(__fun__, error_message)            \
  __hr__ = __fun__;                              \
  if (FAILED(__hr__)) {                          \
    Singleton::terminal()->print(error_message); \
    throw std::runtime_error(error_message);	 \
}                                                
#endif  // CHECK

// { COMMON_DIALOG
CommonDialog::CommonDialog(CLSID type) : dialog(nullptr) {
    check(CoCreateInstance(type, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)), "Failed to create instance");
    check(dialog->GetOptions(&options), "Failed to get options from instance");
}
void CommonDialog::set_title(std::string &&title) {
  auto ptr = boost::locale::conv::utf_to_utf<wchar_t>(title).data();
  check(dialog->SetTitle(ptr), "Failed to set dialog title");
}
void CommonDialog::add_option(unsigned option) {
  check(dialog->SetOptions(options | option), "Failed to set options");
}
void CommonDialog::set_default_file_extension(std::string &&file_extension) {
  auto ptr = boost::locale::conv::utf_to_utf<wchar_t>(file_extension).data();
  check(dialog->SetDefaultExtension(ptr), "Failed to set file extension");
}
void CommonDialog::set_default_folder(const std::string &directory_path) {
  IShellItem * folder = nullptr;
  auto ptr = boost::locale::conv::utf_to_utf<wchar_t>(directory_path).data();
  check(SHCreateItemFromParsingName(ptr, nullptr, IID_PPV_ARGS(&folder)), "Failed to create string");
  check(dialog->SetDefaultFolder(folder), "Failed to set default folder");
  folder->Release();
}
std::string CommonDialog::show() {
  try {
    check(dialog->Show(nullptr), "Failed to show dialog");
    IShellItem *result = nullptr;
    check(dialog->GetResult(&result), "Failed to get result from dialog");
    LPWSTR str = nullptr; 
    check(result->GetDisplayName(SIGDN_FILESYSPATH, &str), "Failed to get display name from dialog");
    result->Release();
    auto res = boost::locale::conv::utf_to_utf<char>(str);
    CoTaskMemFree(str);
    return res;
  } catch (std::exception e) {
    return "";
  }
}

OpenDialog::OpenDialog(std::string &&title, unsigned option) : CommonDialog(CLSID_FileOpenDialog) {
  set_title(std::move(title));
  add_option(option);
  auto dirs = Singleton::directories()->current_path;
  set_default_folder(dirs.empty() ? boost::filesystem::current_path().string() : dirs.string());
}

SaveDialog::SaveDialog(std::string &&title, unsigned option) : CommonDialog(CLSID_FileSaveDialog) {
  set_title(std::move(title));
  add_option(option);
  auto dirs = Singleton::directories()->current_path;
  set_default_folder(dirs.empty() ? boost::filesystem::current_path().string() : dirs.string());
  extensions.emplace_back(COMDLG_FILTERSPEC{L"Default", L"*.h;*.cpp"});
  extensions.emplace_back(COMDLG_FILTERSPEC{L"GoogleStyle", L"*.cc;*.h"});
  extensions.emplace_back(COMDLG_FILTERSPEC{L"BoostStyle", L"*.hpp;*.cpp"});
  extensions.emplace_back(COMDLG_FILTERSPEC{L"Other", L"*.cxx;*.c"});
  check(dialog->SetFileTypes(extensions.size(), extensions.data()), "Failed to set extensions");
  set_default_file_extension("Default");
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

std::string Dialog::save_file() {
  return (SaveDialog("Please choose your destination", 0)).show();
}
#endif
