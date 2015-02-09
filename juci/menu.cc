#include "menu.h"

/***********************************/
/*              MODEL              */
/***********************************/


Menu::Model::Model() {
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
                    "           <menuitem action='WindowSplitWindow'/>      "
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

}

Menu::Model::~Model() {
}

/***********************************/
/*              VIEW               */
/***********************************/
Menu::View::View(Gtk::Orientation orientation) :
        view_(orientation) {

    action_group_ = Gtk::ActionGroup::create();
    ui_manager_ = Gtk::UIManager::create();


}

void Menu::View::set_ui_manger_string(std::string ui_string) {
    try {
        ui_manager_->add_ui_from_string(ui_string);
    }
    catch (const Glib::Error &ex) {
        std::cerr << "building menus failed: " << ex.what();
    }
}

void Menu::View::set_ui_manager_action_group(Glib::RefPtr<Gtk::ActionGroup> action_group) {
    ui_manager_->insert_action_group(action_group);
}

Gtk::Box &Menu::View::view() {
    view_.pack_start(*ui_manager_->get_widget("/MenuBar"), Gtk::PACK_SHRINK);
    return view_;
}


Menu::View::~View() {
}

/***********************************/
/*            CONTROLLER           */
/***********************************/
Menu::Controller::Controller() :
        menu_view_(Gtk::ORIENTATION_VERTICAL),
        menu_model_() {
/* Add action to menues */
/* START file menu */
    menu_view_.action_group()->add(Gtk::Action::create("FileMenu", Gtk::Stock::FILE));
    /* File->New files */
    menu_view_.action_group()->add(Gtk::Action::create("FileNew", "New"));

    menu_view_.action_group()->add(Gtk::Action::create("FileNewStandard",
                    Gtk::Stock::NEW, "New empty file", "Create a new file"),
            [this]() {
                onFileNewEmptyfile();
            });
    menu_view_.action_group()->add(Gtk::Action::create("FileNewCC",
                    Gtk::Stock::NEW, "New cc file", "Create a new cc file"),
            Gtk::AccelKey("<control><alt>c"),
            [this]() {
                onFileNewCCFile();
            });
    menu_view_.action_group()->add(Gtk::Action::create("FileNewH",
                    Gtk::Stock::NEW, "New h file", "Create a new h file"),
            Gtk::AccelKey("<control><alt>h"),
            [this]() {
                onFileNewHeaderFile();
            });
    /* File-> New files end */
    menu_view_.action_group()->add(Gtk::Action::create("FileOpenFile", Gtk::Stock::OPEN),
            [this]() {
                onFileOpenFile();
            });
    menu_view_.action_group()->add(Gtk::Action::create("FileOpenFolder", "Open folder"),
            [this]() {
                onFileOpenFolder();
            });
    menu_view_.action_group()->add(Gtk::Action::create("FileQuit", Gtk::Stock::QUIT),
            [this]() {
                onSystemQuit();
            });
/* END file menu */
/* START edit menu */
    menu_view_.action_group()->add(Gtk::Action::create("EditMenu", Gtk::Stock::EDIT));
    menu_view_.action_group()->add(Gtk::Action::create("EditCopy", Gtk::Stock::COPY),
            [this]() {
                onEditCopy();
            });
    menu_view_.action_group()->add(Gtk::Action::create("EditCut", Gtk::Stock::CUT),
            [this]() {
                onEditCut();
            });
    menu_view_.action_group()->add(Gtk::Action::create("EditPaste", Gtk::Stock::PASTE),
            [this]() {
                onEditPaste();
            });
    menu_view_.action_group()->add(Gtk::Action::create("EditFind", Gtk::Stock::FIND),
            [this]() {
                onEditFind();
            });
/* END edit menu */
/* START window menu */
    menu_view_.action_group()->add(Gtk::Action::create("WindowMenu", "_Window"));
    menu_view_.action_group()->add(Gtk::Action::create("WindowCloseTab", "Close tab"),
            Gtk::AccelKey("<control>w"),
            [this]() {
                onWindowCloseTab();
            });
    menu_view_.action_group()->add(Gtk::Action::create("WindowSplitWindow", "Split window"),
            Gtk::AccelKey("<control><alt>S"),
            [this]() {
                onWindowSplitWindow();
            });
/* END window menu */
/* START Plugin menu */
    menu_view_.action_group()->add(Gtk::Action::create("PluginMenu", "_Plugins"));
    /*Plugin->snippet*/
    menu_view_.action_group()->add(Gtk::Action::create("PluginSnippet", "Snippet"));
    menu_view_.action_group()->add(Gtk::Action::create("PluginAddSnippet", "Add snippet"),
            Gtk::AccelKey("<alt>space"),
            [this]() {
                onPluginAddSnippet();
            });
    /* End snippet */
/* END plugin menu */
/* START help menu */
    menu_view_.action_group()->add(Gtk::Action::create("HelpMenu", Gtk::Stock::HELP));
    menu_view_.action_group()->add(Gtk::Action::create("HelpAbout", Gtk::Stock::ABOUT),
            [this]() {
                onHelpAbout();
            });
/* END help menu */


    menu_view_.set_ui_manager_action_group(menu_view_.action_group());
    menu_view_.set_ui_manger_string(menu_model_.ui_string());
}

Menu::Controller::~Controller() {

}

Gtk::Box &Menu::Controller::view() {
    return menu_view_.view();
}

void Menu::Controller::onFileNewEmptyfile() {
    std::cout << "New file clicked" << std::endl;
    //TODO(Oyvang) Legg til funksjon
}
void Menu::Controller::onFileNewCCFile() {
    std::cout << "New cc file clicked" << std::endl;
    //TODO(Oyvang) Legg til funksjon
}

void Menu::Controller::onFileNewHeaderFile() {
    std::cout << "New cc file clicked" << std::endl;
    //TODO(Oyvang) Legg til funksjon
}
void Menu::Controller::onSystemQuit(){
    //TODO(Oyvang, Zalox, Forgie) Add everything that needs to be done before quiting
    /*Quit the system*/
    Gtk::Main::quit(); //TODO(Oyvang, Zalox, Forgie) methode is depricated, find a better solution.
}
void Menu::Controller::onPluginAddSnippet() {
    std::cout << "Add snipper" << std::endl; //TODO(Forgi add you snippet magic code)
}
void Menu::Controller::onFileOpenFile() {
    std::cout << "Open file clicked" << std::endl;
    //TODO(Oyvang) Legg til funksjon
}
void Menu::Controller::onFileOpenFolder() {
    std::cout << "Open folder clicked" << std::endl;
    //TODO(Oyvang) Legg til funksjon
}
void Menu::Controller::onWindowCloseTab() {
    std::cout << "Closing tab clicked" << std::endl;
    //TODO(Oyvang) Legg til funksjon
}
void Menu::Controller::onEditCopy() {
    std::cout << "Clicked copy" << std::endl;
    //TODO(Oyvang) Legg til funksjon
}
void Menu::Controller::onEditCut() {
    std::cout << "Clicked cut" << std::endl;
    //TODO(Oyvang) Legg til funksjon
}
void Menu::Controller::onEditPaste() {
    std::cout << "Clicked paste" << std::endl;
    //TODO(Oyvang) Legg til funksjon
}
void Menu::Controller::onEditFind() {
    std::cout << "Clicked find" << std::endl;
    //TODO(Oyvang) Legg til funksjon
}
void Menu::Controller::onWindowSplitWindow() {
    std::cout << "Clicked split window" << std::endl;
    //TODO(Oyvang) Legg til funksjon
}
void Menu::Controller::onHelpAbout() {
    std::cout << "Clicked about" << std::endl;
    //TODO(Oyvang) Legg til funksjon
}