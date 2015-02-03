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
        Gtk::Box &get_view();

        Glib::RefPtr<Gtk::ActionGroup> get_action_group() {
            return menu_view.get_action_group();
        };
        Glib::RefPtr<Gtk::UIManager> get_ui_manager() {
            return menu_view.get_ui_manager();
        };

    private:
        View::Menu menu_view;
        Model::Menu menu_model;
        void onNewEmptyfile();
        void onNewCCFile();
        void onNewHeaderFile();
    };
};


#endif //CONTROLLER_H