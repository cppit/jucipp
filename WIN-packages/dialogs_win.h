extern "C" {
#ifndef JUCI_C_DIALOGS
#define JUCI_C_DIALOGS
	__declspec(dllimport) const char* c_select_folder();
	__declspec(dllimport) const char* c_select_file();
	__declspec(dllimport) const char* c_new_file();
	__declspec(dllimport) const char* c_new_folder();
	__declspec(dllimport) const char* c_save_file();
#endif  // JUCI_C_DIALOGS
};

