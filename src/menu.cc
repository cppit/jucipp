#include "menu.h"
#include <iostream>

Menu::Menu() : box(Gtk::ORIENTATION_VERTICAL) {
  action_group = Gtk::ActionGroup::create();
  ui_manager = Gtk::UIManager::create();

  action_group->add(Gtk::Action::create("FileMenu", "File"));
  action_group->add(Gtk::Action::create("EditMenu", "Edit"));
  action_group->add(Gtk::Action::create("WindowMenu", "_Window"));
  action_group->add(Gtk::Action::create("WindowSplitWindow", "Split window"), Gtk::AccelKey(key_map["split_window"]), [this]() {
  });
  action_group->add(Gtk::Action::create("ProjectMenu", "P_roject"));
  action_group->add(Gtk::Action::create("SourceMenu", "_Source"));
  action_group->add(Gtk::Action::create("PluginMenu", "_Plugins"));
  action_group->add(Gtk::Action::create("HelpMenu", "Help"));
  
  ui_xml =
  "<ui>\n"
  "  <menubar name=\"MenuBar\">\n"
  "    <menu action=\"FileMenu\">\n"
  "      <menuitem action=\"FileNewFile\"/>\n"
  "      <menuitem action=\"FileNewFolder\"/>\n"
  "      <menu action=\"FileNewProject\">\n"
  "        <menuitem action=\"FileNewProjectCpp\"/>\n"
  "      </menu>\n"
  "      <separator/>\n"
  "      <menuitem action=\"FileOpenFile\"/>\n"
  "      <menuitem action=\"FileOpenFolder\"/>\n"
  "      <separator/>\n"
  "      <menuitem action=\"FileSave\"/>\n"
  "      <menuitem action=\"FileSaveAs\"/>\n"
  "      <separator/>\n"
  "      <menuitem action=\"FileQuit\"/>\n"
  "    </menu>\n"
  "    <menu action=\"EditMenu\">\n"
  "      <menuitem action=\"EditUndo\"/>\n"
  "      <menuitem action=\"EditRedo\"/>\n"
  "      <separator/>\n"
  "      <menuitem action=\"EditCopy\"/>\n"
  "      <menuitem action=\"EditCut\"/>\n"
  "      <menuitem action=\"EditPaste\"/>\n"
  "      <separator/>\n"
  "      <menuitem action=\"EditFind\"/>\n"
  "    </menu>\n"
  "    <menu action=\"SourceMenu\">\n"
  "      <menuitem action=\"SourceGotoLine\"/>\n"
  "      <menuitem action=\"SourceCenterCursor\"/>\n"
  "      <separator/>\n"
  "      <menuitem action=\"SourceGotoDeclaration\"/>\n"
  "      <menuitem action=\"SourceGotoMethod\"/>\n"
  "      <menuitem action=\"SourceRename\"/>\n"
  "    </menu>\n"
  "    <menu action=\"ProjectMenu\">\n"
  "      <menuitem action=\"ProjectCompileAndRun\"/>\n"
  "      <menuitem action=\"ProjectCompile\"/>\n"
  "      <separator/>\n"
  "      <menuitem action=\"ProjectRunCommand\"/>\n"
  "      <menuitem action=\"ProjectKillLastRunning\"/>\n"
  "      <menuitem action=\"ProjectForceKillLastRunning\"/>\n"
  "    </menu>\n"
  "    <menu action=\"WindowMenu\">\n"
  "      <menuitem action=\"WindowCloseTab\"/>\n"
  "      <menuitem action=\"WindowSplitWindow\"/>\n"
  "    </menu>\n"
  "    <menu action=\"PluginMenu\">\n"
  "    </menu>\n"
  "    <menu action=\"HelpMenu\">\n"
  "      <menuitem action=\"HelpAbout\"/>\n"
  "    </menu>\n"
  "  </menubar>\n"
  "</ui>\n";
}

Gtk::Widget& Menu::get_widget() {
  return *ui_manager->get_widget("/MenuBar");
}

void Menu::build() {
  try {
    ui_manager->add_ui_from_string(ui_xml);
  }
  catch (const Glib::Error &ex) {
    std::cerr << "building menu failed" << ex.what();
  }
  ui_manager->insert_action_group(action_group);
}
