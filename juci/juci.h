/*juCi++ main header file*/
#ifndef JUCI_H
#define JUCI_H

#include <iostream>
#include "gtkmm.h"
#include "menu.h"


class Window : public Gtk::Window {
public:
    Window();

    virtual ~Window();

    Gtk::Box window_box_;

private:
    Menu::Controller menu;
    /*signal handler*/
    void onSystemQuit();

};

#endif // JUCI_H
