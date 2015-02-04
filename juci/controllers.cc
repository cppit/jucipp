#include "controller.h"

Controller::Menu::Menu() :
        menu_view_(Gtk::ORIENTATION_VERTICAL),
        menu_model_() {
/*Add action to menues*/
/*File menu*/
    menu_view_.action_group()->add(Gtk::Action::create("FileMenu", Gtk::Stock::FILE));
    /*File->New files*/
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
                onFileNewEmptyfile();
            });
    menu_view_.action_group()->add(Gtk::Action::create("FileOpenFolder", "Open folder"),
            [this]() {
                onFileNewEmptyfile();
            });
    menu_view_.action_group()->add(Gtk::Action::create("FileQuit", Gtk::Stock::QUIT),
            [this]() {
                onSystemQuit();
            });
    menu_view_.action_group()->add(Gtk::Action::create("PluginMenu", "Plugins"));
    menu_view_.action_group()->add(Gtk::Action::create("PluginSnippet", "Snippet"));
    menu_view_.action_group()->add(Gtk::Action::create("PluginAddSnippet", "Add snippet"),
            Gtk::AccelKey("<alt>space"),
            [this]() {
                onPluginAddSnippet();
            });


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
}
void Controller::Menu::onFileNewCCFile() {
    std::cout << "New cc file clicked" << std::endl;
}

void Controller::Menu::onFileNewHeaderFile() {
    std::cout << "New cc file clicked" << std::endl;
}
void Controller::Menu::onSystemQuit(){
    //TODO(Oyvang, Zalox, Forgie) Add everything that needs to be done before quiting
    /*Quit the system*/
    Gtk::Main::quit(); //TODO(Oyvang, Zalox, Forgie) methode is depricated, find a better solution.
}
void Controller::Menu::onPluginAddSnippet() {
    std::cout << "Add snipper" << std::endl; //TODO(Forgi add you snippet magic code)
}
