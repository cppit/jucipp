#include "controller.h"

Controller::Menu::Menu() :
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

Controller::Menu::~Menu() {

}

Gtk::Box &Controller::Menu::view() {
    return menu_view_.view();
}

void Controller::Menu::onFileNewEmptyfile() {
    std::cout << "New file clicked" << std::endl;
    //TODO(Oyvang) Legg til funksjon
}
void Controller::Menu::onFileNewCCFile() {
    std::cout << "New cc file clicked" << std::endl;
    //TODO(Oyvang) Legg til funksjon
}

void Controller::Menu::onFileNewHeaderFile() {
    std::cout << "New cc file clicked" << std::endl;
    //TODO(Oyvang) Legg til funksjon
}
void Controller::Menu::onSystemQuit(){
    //TODO(Oyvang, Zalox, Forgie) Add everything that needs to be done before quiting
    /*Quit the system*/
    Gtk::Main::quit(); //TODO(Oyvang, Zalox, Forgie) methode is depricated, find a better solution.
}
void Controller::Menu::onPluginAddSnippet() {
    std::cout << "Add snipper" << std::endl; //TODO(Forgi add you snippet magic code)
}
void Controller::Menu::onFileOpenFile() {
    std::cout << "Open file clicked" << std::endl;
    //TODO(Oyvang) Legg til funksjon
}
void Controller::Menu::onFileOpenFolder() {
    std::cout << "Open folder clicked" << std::endl;
    //TODO(Oyvang) Legg til funksjon
}
void Controller::Menu::onWindowCloseTab() {
    std::cout << "Closing tab clicked" << std::endl;
    //TODO(Oyvang) Legg til funksjon
}
void Controller::Menu::onEditCopy() {
    std::cout << "Clicked copy" << std::endl;
    //TODO(Oyvang) Legg til funksjon
}
void Controller::Menu::onEditCut() {
    std::cout << "Clicked cut" << std::endl;
    //TODO(Oyvang) Legg til funksjon
}
void Controller::Menu::onEditPaste() {
    std::cout << "Clicked paste" << std::endl;
    //TODO(Oyvang) Legg til funksjon
}
void Controller::Menu::onEditFind() {
    std::cout << "Clicked find" << std::endl;
    //TODO(Oyvang) Legg til funksjon
}
void Controller::Menu::onWindowSplitWindow() {
    std::cout << "Clicked split window" << std::endl;
    //TODO(Oyvang) Legg til funksjon
}
void Controller::Menu::onHelpAbout() {
    std::cout << "Clicked about" << std::endl;
    //TODO(Oyvang) Legg til funksjon
}
