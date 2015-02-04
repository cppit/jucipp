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

        Gtk::Box &view();
        Glib::RefPtr<Gtk::ActionGroup> action_group() {
            return action_group_;
        };

        Glib::RefPtr<Gtk::UIManager> ui_manager() {
            return ui_manager_;
        };

        void set_ui_manger_string(std::string ui_string);

        void set_ui_manager_action_group(Glib::RefPtr<Gtk::ActionGroup> action_group);

    protected:
        Gtk::Box view_;
        Glib::RefPtr<Gtk::UIManager> ui_manager_;
        Glib::RefPtr<Gtk::ActionGroup> action_group_;
    };


};

#endif // VIEW_H