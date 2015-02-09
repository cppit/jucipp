#include <iostream>
#include "gtkmm.h"
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

        Gtk::Box &view();

        Glib::RefPtr <Gtk::ActionGroup> action_group() {
            return action_group_;
        };

        Glib::RefPtr <Gtk::UIManager> ui_manager() {
            return ui_manager_;
        };

        void set_ui_manger_string(std::string ui_string);

        void set_ui_manager_action_group(Glib::RefPtr <Gtk::ActionGroup> action_group);

    protected:
        Gtk::Box view_;
        Glib::RefPtr <Gtk::UIManager> ui_manager_;
        Glib::RefPtr <Gtk::ActionGroup> action_group_;
    };

    class Controller {
    public:
        Controller();

        virtual ~Controller();

        Gtk::Box &view();

        Glib::RefPtr <Gtk::UIManager> ui_manager() {
            return menu_view_.ui_manager();
        };

    private:
        Menu::View menu_view_;
        Menu::Model menu_model_;

        /*Signal handlers*/
        void onFileNewEmptyfile();

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

    };
}