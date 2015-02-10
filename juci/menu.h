#ifndef JUCI_MENU_H_
#define JUCI_MENU_H_

#include <iostream>
#include "gtkmm.h"
namespace Menu {
  class Model {
  public:
    Model();
    virtual~Model();
    std::string ui_string() { return ui_string_; }
  private:
    std::string ui_string_;
  };  // class Model

  class View {
  public:
    explicit View(Gtk::Orientation orient);
    virtual ~View();
    Glib::RefPtr<Gtk::Box> view();
    Glib::RefPtr <Gtk::ActionGroup> action_group() {
      return action_group_;
    }
    Glib::RefPtr <Gtk::UIManager> ui_manager() {
      return ui_manager_;
    }
    void set_ui_manger_string(std::string ui_string);
    void set_ui_manager_action_group(
                           Glib::RefPtr<Gtk::ActionGroup> action_group);

  protected:
    Gtk::Box view_;
    Glib::RefPtr <Gtk::UIManager> ui_manager_;
    Glib::RefPtr <Gtk::ActionGroup> action_group_;
  };  // class View
  // controller
  class Controller {
  public:
    Controller();
    virtual ~Controller();
    Glib::RefPtr<Gtk::Box> view();
    Glib::RefPtr <Gtk::UIManager> ui_manager() {
      return menu_view_.ui_manager();
    }

  private:
    View menu_view_;
    Model menu_model_;
    void onFileNewEmptyfile();    /*Signal handlers*/
    void onFileNewCCFile();
    void onFileNewHeaderFile();
    void onFileOpenFile();
    void onFileOpenFolder();
    void onSystemQuit();
    void onPluginAddSnippet();
    void onWindowCloseTab();
    void onEditCopy();
    void onEditCut();
    void onEditPaste();
    void onEditFind();
    void onWindowSplitWindow();
    void onHelpAbout();
  };  // class Controller
}  // namespace Menu
#endif  // JUCI_MENU_H_
