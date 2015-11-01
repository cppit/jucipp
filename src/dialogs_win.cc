#ifdef _WIN32
#include "dialogs.h"
#include "singletons.h"

#ifndef check
HRESULT __hr__;
#define check(__fun__, error_message)            \
  __hr__ = __fun__;                              \
  if (FAILED(__hr__)) {                          \
    Singleton::terminal()->print(error_message); \
    throw std::runtime_error(error_message);	 \
}                                                
#endif  // CHECK


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

void CommonDialog::set_file_extensions(const std::vector<std::string> &file_extensions) {
  return;
}

void CommonDialog::set_default_folder(const std::string &directory_path) {
  IShellItem * folder = nullptr;
  WinString str;
  &str = (str.s2ws(directory_path)).data();
  check(SHCreateItemFromParsingName(&str, NULL, IID_PPV_ARGS(&folder)), "Failed to create string");
  check(dialog->SetDefaultFolder(folder), "Failed to set default folder");
  folder->Release();
}

std::string CommonDialog::show() {
  try {
    check(dialog->Show(nullptr), "Failed to show dialog");
    IShellItem *result = nullptr;
    check(dialog->GetResult(&result), "Failed to get result from dialog");
    WinString str;
    check(result->GetDisplayName(SIGDN_FILESYSPATH, &str), "Failed to get display name from dialog");
    result->Release();
    return str();
  } catch (std::exception e) {
    return "";
  }
}

OpenDialog::OpenDialog(const std::string &title, unsigned option) : CommonDialog(CLSID_FileOpenDialog); {
  set_title(title);
  add_option(option);
  auto dirs = Singleton::directories()->current_path;
  set_default_folder(dirs.empty() ? boost::filesystem::current_path() : dirs);
}

SaveDialog::SaveDialog(const std::string &title, unsigned option) : CommonDialog(CLSID_FileSaveDialog) {
  set_title(title);
  add_option(option);
  auto dirs = Singleton::directories()->current_path;
  set_default_folder(dirs.empty() ? boost::filesystem::current_path() : dirs);
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
