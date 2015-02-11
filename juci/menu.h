#ifndef JUCI_MENU_H_
#define JUCI_MENU_H_

#include <iostream>
#include "gtkmm.h"
#include "keybindings.h"

namespace Menu {
  class Model {
  public:
    Model();
    virtual ~Model();
    std::string ui_string() { return ui_string_; }
  private:
    std::string ui_string_;
  };  // class Model

  class View {
  public:
    explicit View(Gtk::Orientation orient);
    virtual ~View();
    Gtk::Box& view(Glib::RefPtr<Gtk::UIManager> ui_manager);

  protected:
    Gtk::Box view_;
  };  // class View
  // controller
  class Controller {
  public:
    explicit Controller(Keybindings::Controller keybindings);
    virtual ~Controller();
    Gtk::Box& view();
  private:
    Keybindings::Controller keybindings_;
    View menu_view_;
    Model menu_model_;
    void OnFileNewEmptyfile();    /*Signal handlers*/
    void OnFileNewCCFile();
    void OnFileNewHeaderFile();
    void OnFileOpenFile();
    void OnFileOpenFolder();
    void OnSystemQuit();
    void OnPluginAddSnippet();
    void OnWindowCloseTab();
    void OnEditCopy();
    void OnEditCut();
    void OnEditPaste();
    void OnEditFind();
    void OnWindowSplitWindow();
    void OnHelpAbout();
  };  // class Controller
}  // namespace Menu
#endif  // JUCI_MENU_H_
