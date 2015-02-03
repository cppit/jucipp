/*juCi++ main header file*/
#ifndef JUCI_H
#define JUCI_H

#include <iostream>
#include "gtkmm.h"
#include "view.h"
#include "model.h"
#include "controller.h"


class Window : public Gtk::Window {
public:
    Window();

    virtual ~Window();

    Gtk::Box window_box;

private:
    Controller::Menu menu;
    /*signal handler*/
    void onSystemQuit();

};

#endif // JUCI_H
