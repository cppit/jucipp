#include "model.h"

Model::Menu::Menu() {
    ui_string_ =
            "<ui>                                                   "
            "   <menubar name='MenuBar'>                            "
            "       <menu action='FileMenu'>                        "
            "           <menu action='FileNew'>                     "
            "               <menuitem action='FileNewStandard'/>    "
            "               <menuitem action='FileNewCC'/>          "
            "               <menuitem action='FileNewH'/>           "
            "           </menu>                                     "
           // "           <menuitem action='FileOpenFile'/>           "
            //"           <menuitem action='FileOpenFolder'/>         "
            "            <separator/>                               "
            "           <menuitem action='FileQuit'/>               "
            "       </menu>                                         "
            "       <menu action='PluginMenu'>                      "
            "           <menu action='PluginSnippet'>               "
            "               <menuitem action='PluginAddSnippet'/>   "
            "           </menu>                                     "
            "       </menu>                                         "
            "   </menubar>                                          "
            "</ui>                                                  ";

    // TODO(oyvang) legg til   <menuitem action='FileCloseTab'/>  under en meny Window
}

Model::Menu::~Menu() {
}