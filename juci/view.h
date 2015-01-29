/*juCi++ view header file*/
#ifndef VIEW_H
#define VIEW_H

#include <iostream>
#include "gtkmm.h"
#include "controller.h"

/* -------------------------- HOW TO -------------------------- */
/* All view shall be under View->private if possible            */
/* View objects under View->protected                           */
/* View controller object under View::class->private                                                             */
/* ------------------ Remove these comments ------------------- */


class View : public Gtk::Window {
public:
    View();
    virtual ~View();
protected:
    Gtk::Box window_box;
private:



};

#endif // VIEW_H