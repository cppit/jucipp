#include "menu.h"

// VIEW
Menu::View::View(Gtk::Orientation orientation) :
        view_(orientation) {

}// view controller

Gtk::Box &Menu::View::view(
        Glib::RefPtr<Gtk::UIManager> ui_manager) {
  view_.pack_start(*ui_manager->get_widget("/MenuBar"), Gtk::PACK_SHRINK);
  return view_;
}


// CONTROLLER
Menu::Controller::Controller(Keybindings::Controller keybindings) :
        menu_view_(Gtk::ORIENTATION_VERTICAL),
        keybindings_(keybindings) {
/* Add action to menues */
/* START file menu */
  keybindings_.action_group_menu()->add(Gtk::Action::create("FileMenu",
          Gtk::Stock::FILE));
  /* File->New files */
  keybindings_.action_group_menu()->add(Gtk::Action::create("FileNew", "New"));
  keybindings_.action_group_menu()->add(Gtk::Action::create("FileNewStandard",
                  Gtk::Stock::NEW, "New empty file", "Create a new file"),
          [this]() {
              OnFileNewEmptyfile();
          });
  keybindings_.action_group_menu()->add(Gtk::Action::create("FileNewCC",
                  "New cc file"),
          Gtk::AccelKey("<control><alt>c"),
          [this]() {
              OnFileNewCCFile();
          });
  keybindings_.action_group_menu()->add(Gtk::Action::create("FileNewH",
                  "New h file"),
          Gtk::AccelKey("<control><alt>h"),
          [this]() {
              OnFileNewHeaderFile();
          });
  /* File-> New files end */
  keybindings_.action_group_menu()->add(Gtk::Action::create("FileOpenFile",
                  Gtk::Stock::OPEN),
          [this]() {
              OnFileOpenFile();
          });
  keybindings_.action_group_menu()->add(Gtk::Action::create("FileOpenFolder",
                  "Open folder"),
          [this]() {
              OnFileOpenFolder();
          });
/* END file menu */
/* START edit menu */
  keybindings_.action_group_menu()->add(Gtk::Action::create("EditMenu",
          Gtk::Stock::EDIT));
  keybindings_.action_group_menu()->add(Gtk::Action::create("EditCopy",
                  Gtk::Stock::COPY),
          [this]() {
              OnEditCopy();
          });
  keybindings_.action_group_menu()->add(Gtk::Action::create("EditCut",
                  Gtk::Stock::CUT),
          [this]() {
              OnEditCut();
          });
  keybindings_.action_group_menu()->add(Gtk::Action::create("EditPaste",
                  Gtk::Stock::PASTE),
          [this]() {
              OnEditPaste();
          });
  keybindings_.action_group_menu()->add(Gtk::Action::create("EditFind",
                  Gtk::Stock::FIND),
          [this]() {
              OnEditFind();
          });
/* END edit menu */
/* START window menu */
  keybindings_.action_group_menu()->add(Gtk::Action::create("WindowMenu",
          "_Window"));
  keybindings_.action_group_menu()->add(Gtk::Action::create("WindowCloseTab",
                  "Close tab"),
          Gtk::AccelKey("<control>w"),
          [this]() {
              OnWindowCloseTab();
          });
  keybindings_.action_group_menu()->add(Gtk::Action::create("WindowSplitWindow",
                  "Split window"),
          Gtk::AccelKey("<control><alt>S"),
          [this]() {
              OnWindowSplitWindow();
          });
/* END window menu */
/* START Plugin menu */
  keybindings_.action_group_menu()->add(Gtk::Action::create("PluginMenu",
          "_Plugins"));
  /*Plugin->snippet*/
  keybindings_.action_group_menu()->add(Gtk::Action::create("PluginSnippet",
          "Snippet"));
  keybindings_.action_group_menu()->add(Gtk::Action::create("PluginAddSnippet",
                  "Add snippet"),
          Gtk::AccelKey("<alt>space"),
          [this]() {
              OnPluginAddSnippet();
          });
  /* End snippet */
/* END plugin menu */
/* START help menu */
  keybindings_.action_group_menu()->add(Gtk::Action::create("HelpMenu",
          Gtk::Stock::HELP));
  keybindings_.action_group_menu()->add(Gtk::Action::create("HelpAbout",
                  Gtk::Stock::ABOUT),
          [this]() {
              OnHelpAbout();
          });

// Hidden actions
  keybindings_.action_group_hidden()->add(Gtk::Action::create("Test"),
          Gtk::AccelKey("<control><alt>K"),
          [this]() {
              OnHelpAbout();
          });
  keybindings_.BuildMenu();
  keybindings_.BuildHiddenMenu();


/* END help menu */



}


Gtk::Box &Menu::Controller::view() {
  return menu_view_.view(keybindings_.ui_manager_menu());
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
  //TODO(Forgi add you snippet magic code)
  std::cout << "Add snipper" << std::endl;
  juci_api::py::LoadPlugin("snippet");
}


// void Menu::Controller::LoadPlugin() {
//  juci_api::py::LoadPlugin("plugin_name");
// }

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
