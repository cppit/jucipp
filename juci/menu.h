#include <iostream>
#include "gtkmm.h"
#include "keybindings.h"
namespace Menu {
    class Model {
    public:
      Model();

      virtual~Model();

      std::string ui_string() {
        return ui_string_;
      };
    private:
      std::string ui_string_;
    };

    class View {
    public:
      View(Gtk::Orientation orient);
      virtual ~View();
      Gtk::Box &view(Glib::RefPtr<Gtk::UIManager> ui_manager);

    protected:
      Gtk::Box view_;

    };

    class Controller {
    public:
      Controller(Keybindings::Controller keybindings);
      virtual ~Controller();
      Gtk::Box &view();
    private:
      Keybindings::Controller keybindings_;
      Menu::View menu_view_;
      Menu::Model menu_model_;

      /*Signal handlers*/
      void OnFileNewEmptyfile();

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

    };
}