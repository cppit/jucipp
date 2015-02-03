#include "model.h"

Model::Menu::Menu() {
    ui_string =
            "<ui>                                                   "
            "   <menubar name='MenuBar'>                            "
            "       <menu action='FileMenu'>                        "
            "               <menuitem action='FileNewStandard'/>    "
            "           <menuitem action='FileQuit'/>               "
            "       </menu>                                         "
            "   </menubar>                                          "
            "</ui>                                                  ";
}

Model::Menu::~Menu() {
}