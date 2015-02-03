/*juCi++ view header file*/
#ifndef VIEW_H
#define VIEW_H

#include "gtkmm.h"
#include "iostream"

/* -------------------------- HOW TO -------------------------- */
/* All view shall be under View if possible                     */
/* ------------------ Remove these comments ------------------- */


class View {
public:
    class Menu {
    public:
        Menu(Gtk::Orientation orient);

        virtual ~Menu();

        Gtk::Box &get_view();

        Glib::RefPtr<Gtk::ActionGroup> get_action_group() {
            return action_group;
        };

        Glib::RefPtr<Gtk::UIManager> get_ui_manager() {
            return ui_manager;
        };

        void set_ui_manger_string(std::string ui_string);

        void set_ui_manager_action_group(Glib::RefPtr<Gtk::ActionGroup> action_group);

    protected:
        Gtk::Box view;
        Glib::RefPtr<Gtk::UIManager> ui_manager;
        Glib::RefPtr<Gtk::ActionGroup> action_group;
    };


};

#endif // VIEW_H