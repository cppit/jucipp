#include "view.h"

View::View() :
        window_box(Gtk::ORIENTATION_VERTICAL){

    set_title("juCi++");
    set_default_size(800, 600);
    maximize();

    add(window_box);

    /*Add default views here */

    show_all_children();

};
View::~View() {

}
