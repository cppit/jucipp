/*juCi++ controller header file*/
#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "view.h"
#include "model.h"

/* -------------------------- HOW TO ----------------------------- */
/* Controll classes under Controller->public                       */
/* Model object under Controller::class->private  name:class_model */
/* View object under Controller::class->private   name:class_view  */
/* ------------------ Remove these comments ---------------------- */

class Controller {
public:
    class Menu {
    public:
        Menu();

        virtual ~Menu();
        Gtk::Box &view();

        Glib::RefPtr<Gtk::UIManager> ui_manager() {
            return menu_view_.ui_manager();
        };

    private:
        View::Menu menu_view_;
        Model::Menu menu_model_;
        /*Signal handlers*/
        void onFileNewEmptyfile();
        void onFileNewCCFile();
        void onFileNewHeaderFile();
        void onFileOpenFile();
        void onFileOpenFolder();
        void onSystemQuit();
        void onPluginAddSnippet();

    };
};


#endif //CONTROLLER_H