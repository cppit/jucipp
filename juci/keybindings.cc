#include "keybindings.h"

Keybindings::Model::Model() {
  menu_ui_string_ =
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
  
  hidden_ui_string_ =
    "<ui>                                                   "
    "   <menubar name='MenuBar'>                            "
    "               <menuitem action='Test'/>               "
    "   </menubar>                                          "
    "</ui>                                                  ";
};

Keybindings::Model::~Model() {
}

Keybindings::Controller::Controller() {
  action_group_menu_ = Gtk::ActionGroup::create();
  ui_manager_menu_ = Gtk::UIManager::create();
  action_group_hidden_ = Gtk::ActionGroup::create();
  ui_manager_hidden_ = Gtk::UIManager::create();
}
Keybindings::Controller::~Controller(){
}
void Keybindings::Controller::BuildMenu() {
  try {
    ui_manager_menu_->add_ui_from_string(model_.menu_ui_string());
  }
  catch (const Glib::Error &ex) {
    std::cerr << "building menu failed" << ex.what();
  }
  ui_manager_menu_->insert_action_group(action_group_menu_);
}
void Keybindings::Controller::BuildHiddenMenu() {
  try {
    ui_manager_hidden_->add_ui_from_string(model_.hidden_ui_string());
  }
  catch (const Glib::Error &ex) {
    std::cerr << "building hidden menu failed" << ex.what();
  }
  ui_manager_hidden_->insert_action_group(action_group_hidden_);
}

