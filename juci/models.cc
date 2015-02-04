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
            "           <menuitem action='FileOpenFile'/>           "
            "           <menuitem action='FileOpenFolder'/>         "
            "            <separator/>                               "
            "           <menuitem action='FileQuit'/>               "
            "       </menu>                                         "
            "       <menu action='EditMenu'>                        "
            "           <menuitem action='EditCopy'/>               "
            "           <menuitem action='EditCut'/>                "
            "           <menuitem action='EditPaste'/>              "
            "            <separator/>                               "
            "           <menuitem action='EditFind'/>               "
            "       </menu>                                         "
            "       <menu action='WindowMenu'>                      "
            "           <menuitem action='WindowCloseTab'/>         "
            "           <menuitem action='WindowSplitWindow'/>        "
            "       </menu>                                         "
            "       <menu action='PluginMenu'>                      "
            "           <menu action='PluginSnippet'>               "
            "               <menuitem action='PluginAddSnippet'/>   "
            "           </menu>                                     "
            "       </menu>                                         "
            "       <menu action='HelpMenu'>                        "
            "           <menuitem action='HelpAbout'/>              "
            "        </menu>                                        "
            "   </menubar>                                          "
            "</ui>                                                  ";

    // TODO(oyvang) legg til   <menuitem action='FileCloseTab'/>  under en meny Window
}

Model::Menu::~Menu() {
}