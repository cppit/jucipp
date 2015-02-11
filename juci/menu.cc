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


// VIEW

Menu::View::View(Gtk::Orientation orientation) :
        view_(orientation) {

}

Gtk::Box &Menu::View::view(
        Glib::RefPtr<Gtk::UIManager> ui_manager) {
  view_.pack_start(*ui_manager->get_widget("/MenuBar"), Gtk::PACK_SHRINK);
  return view_;
}

Menu::View::~View() {
}

/***********************************/
/*            CONTROLLER           */
/***********************************/
Menu::Controller::Controller(Keybindings::Controller keybindings) :
        menu_view_(Gtk::ORIENTATION_VERTICAL),
        menu_model_(),
        keybindings_(keybindings) {
/* Add action to menues */
/* START file menu */
  keybindings_.action_group()->add(Gtk::Action::create("FileMenu", Gtk::Stock::FILE));
  /* File->New files */
  keybindings_.action_group()->add(Gtk::Action::create("FileNew", "New"));
  keybindings_.action_group()->add(Gtk::Action::create("FileNewStandard",
                  Gtk::Stock::NEW, "New empty file", "Create a new file"),
          [this]() {
              OnFileNewEmptyfile();
          });
  keybindings_.action_group()->add(Gtk::Action::create("FileNewCC",
                  Gtk::Stock::NEW, "New cc file", "Create a new cc file"),
          Gtk::AccelKey("<control><alt>c"),
          [this]() {
              OnFileNewCCFile();
          });
  keybindings_.action_group()->add(Gtk::Action::create("FileNewH",
                  Gtk::Stock::NEW, "New h file", "Create a new h file"),
          Gtk::AccelKey("<control><alt>h"),
          [this]() {
              OnFileNewHeaderFile();
          });
  /* File-> New files end */
  keybindings_.action_group()->add(Gtk::Action::create("FileOpenFile", Gtk::Stock::OPEN),
          [this]() {
              OnFileOpenFile();
          });
  keybindings_.action_group()->add(Gtk::Action::create("FileOpenFolder", "Open folder"),
          [this]() {
              OnFileOpenFolder();
          });
/* END file menu */
/* START edit menu */
  keybindings_.action_group()->add(Gtk::Action::create("EditMenu", Gtk::Stock::EDIT));
  keybindings_.action_group()->add(Gtk::Action::create("EditCopy", Gtk::Stock::COPY),
          [this]() {
              OnEditCopy();
          });
  keybindings_.action_group()->add(Gtk::Action::create("EditCut", Gtk::Stock::CUT),
          [this]() {
              OnEditCut();
          });
  keybindings_.action_group()->add(Gtk::Action::create("EditPaste", Gtk::Stock::PASTE),
          [this]() {
              OnEditPaste();
          });
  keybindings_.action_group()->add(Gtk::Action::create("EditFind", Gtk::Stock::FIND),
          [this]() {
              OnEditFind();
          });
/* END edit menu */
/* START window menu */
  keybindings_.action_group()->add(Gtk::Action::create("WindowMenu", "_Window"));
  keybindings_.action_group()->add(Gtk::Action::create("WindowCloseTab", "Close tab"),
          Gtk::AccelKey("<control>w"),
          [this]() {
              OnWindowCloseTab();
          });
  keybindings_.action_group()->add(Gtk::Action::create("WindowSplitWindow", "Split window"),
          Gtk::AccelKey("<control><alt>S"),
          [this]() {
              OnWindowSplitWindow();
          });
/* END window menu */
/* START Plugin menu */
  keybindings_.action_group()->add(Gtk::Action::create("PluginMenu", "_Plugins"));
  /*Plugin->snippet*/
  keybindings_.action_group()->add(Gtk::Action::create("PluginSnippet", "Snippet"));
  keybindings_.action_group()->add(Gtk::Action::create("PluginAddSnippet", "Add snippet"),
          Gtk::AccelKey("<alt>space"),
          [this]() {
              OnPluginAddSnippet();
          });
  /* End snippet */
/* END plugin menu */
/* START help menu */
  keybindings_.action_group()->add(Gtk::Action::create("HelpMenu", Gtk::Stock::HELP));
  keybindings_.action_group()->add(Gtk::Action::create("HelpAbout", Gtk::Stock::ABOUT),
          [this]() {
              OnHelpAbout();
          });


/* END help menu */


  keybindings_.ui_manager()->add_ui_from_string(menu_model_.ui_string());
  keybindings_.ui_manager()->insert_action_group(keybindings_.action_group());
}

Menu::Controller::~Controller() {
}

Gtk::Box &Menu::Controller::view() {
  return menu_view_.view(keybindings_.ui_manager());
}

void Menu::Controller::OnFileNewEmptyfile() {
  std::cout << "New file clicked" << std::endl;
  //TODO(Oyvang) Legg til funksjon
}

void Menu::Controller::OnFileNewCCFile() {
  std::cout << "New cc file clicked" << std::endl;
  //TODO(Oyvang) Legg til funksjon
}

void Menu::Controller::OnFileNewHeaderFile() {
  std::cout << "New header file clicked" << std::endl;
  //TODO(Oyvang) Legg til funksjon
}

void Menu::Controller::OnPluginAddSnippet() {
  std::cout << "Add snipper" << std::endl; //TODO(Forgi add you snippet magic code)
}

void Menu::Controller::OnFileOpenFile() {
  std::cout << "Open file clicked" << std::endl;
  //TODO(Oyvang) Legg til funksjon
}

void Menu::Controller::OnFileOpenFolder() {
  std::cout << "Open folder clicked" << std::endl;
  //TODO(Oyvang) Legg til funksjon
}

void Menu::Controller::OnWindowCloseTab() {
  std::cout << "Closing tab clicked" << std::endl;
  //TODO(Oyvang) Legg til funksjon
}

void Menu::Controller::OnEditCopy() {
  std::cout << "Clicked copy" << std::endl;
  //TODO(Oyvang) Legg til funksjon
}

void Menu::Controller::OnEditCut() {
  std::cout << "Clicked cut" << std::endl;
  //TODO(Oyvang) Legg til funksjon
}

void Menu::Controller::OnEditPaste() {
  std::cout << "Clicked paste" << std::endl;
  //TODO(Oyvang) Legg til funksjon
}

void Menu::Controller::OnEditFind() {
  std::cout << "Clicked find" << std::endl;
  //TODO(Oyvang) Legg til funksjon
}

void Menu::Controller::OnWindowSplitWindow() {
  std::cout << "Clicked split window" << std::endl;
  //TODO(Oyvang) Legg til funksjon
}

void Menu::Controller::OnHelpAbout() {
  std::cout << "Clicked about" << std::endl;
  //TODO(Oyvang) Legg til funksjon
}
