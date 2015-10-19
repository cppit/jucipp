#ifdef _WIN32
#include "dialogs.h"

// { WIN_STRING
WinString::WinString(const std::string &string) {
  std::wstringstream ss;
    ss << s2ws(string);
    ss >> str;
}

WinString::operator()() {
  std::string res;
  if (str != nullptr) {
    std::wstring ss(str);
    res = ws2s(ss);
  }
  return res;
}
// WIN_STRING }

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
  try {
    check(dialog->Show(nullptr), "Failed to show dialog");
    IShellItem *result = nullptr;
    check(dialog->GetResult(&result), "Failed to get result from dialog");
    win_string str;
    check(result->GetDisplayName(SIGDN_FILESYSPATH, &str), "Failed to get display name from dialog");
    result->Release();
    return str();
  } catch (std::exception e) {
    return "";
  }
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

std::string Dialog::save_file() {
  return (SaveDialog("Please choose your destination", 0)).show();
}
#endif
