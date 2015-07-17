#ifndef JUCI_MENU_H_
#define JUCI_MENU_H_

#include <iostream>
#include <gtkmm.h>

namespace Menu {
  class View {
  public:
    explicit View(Gtk::Orientation orient);
    Gtk::Box &view(Glib::RefPtr<Gtk::UIManager> ui_manager);
  protected:
    Gtk::Box view_;
  };  // class View
  class Controller {
  public:
    Controller();
    Gtk::Box &view();

    View menu_view_;
    void OnFileNewEmptyfile();
    void OnFileNewCCFile();
    void OnFileNewHeaderFile();
    void OnFileOpenFile();
    void OnFileOpenFolder();
    void OnPluginAddSnippet();
    void OnWindowCloseTab();
    void OnEditCut();
    void OnEditFind();
    void OnWindowSplitWindow();
    void OnHelpAbout();
  };  // class Controller
}  // namespace Menu
#endif  // JUCI_MENU_H_
