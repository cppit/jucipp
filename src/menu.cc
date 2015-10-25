#include "menu.h"
#include "singletons.h"
#include <string>
#include <iostream>

using namespace std; //TODO: remove

Menu::Menu() {
  auto &keys=Singleton::Config::menu()->keys;
  
  ui_xml =
  "<interface>"
  "  <menu id='juci-menu'>"
  "    <section>"
  "      <item>"
  "        <attribute name='label' translatable='yes'>_About</attribute>"
  "        <attribute name='action'>win.about</attribute>"
  "      </item>"
  "    </section>"
  "    <section>"
  "      <item>"
  "        <attribute name='label' translatable='yes'>_Preferences</attribute>"
  "        <attribute name='action'>win.preferences</attribute>"
  "      </item>"  
  "    </section>"
  "    <section>"
  "      <item>"
  "        <attribute name='label' translatable='yes'>_Quit</attribute>"
  "        <attribute name='action'>win.quit</attribute>"
  "        <attribute name='accel'>"+keys["quit"]+"</attribute>"
  "      </item>"  
  "    </section>"
  "  </menu>"
  ""
  "  <menu id='window-menu'>"
  "    <submenu>"
  "      <attribute name='label' translatable='yes'>_File</attribute>"
  "      <section>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_New _File</attribute>"
  "          <attribute name='action'>win.new_file</attribute>"
  "        </item>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_New _Directory</attribute>"
  "          <attribute name='action'>win.new_directory</attribute>"
  "        </item>"
  "      </section>"
  "      <section>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Open _File</attribute>"
  "          <attribute name='action'>win.open_file</attribute>"
  "        </item>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Open _Directory</attribute>"
  "          <attribute name='action'>win.open_directory</attribute>"
  "        </item>"
  "      </section>"
  "    </submenu>"
  "  </menu>"
  "</interface>";
  
  /*action_group = Gtk::ActionGroup::create();
  ui_manager = Gtk::UIManager::create();
  ui_manager->insert_action_group(action_group);
  
  action_group->add(Gtk::Action::create("FileMenu", "File"));
  action_group->add(Gtk::Action::create("EditMenu", "Edit"));
  action_group->add(Gtk::Action::create("WindowMenu", "_Window"));
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
  "      <menuitem action=\"Preferences\"/>\n"
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
  "      <menu action=\"SourceSpellCheck\">\n"
  "        <menuitem action=\"SourceSpellCheckBuffer\"/>\n"
  "        <menuitem action=\"SourceSpellCheckClear\"/>\n"
  "        <menuitem action=\"SourceSpellCheckNextError\"/>\n"
  "      </menu>\n"
  "      <separator/>\n"
  "      <menu action=\"SourceIndentation\">\n"
  "        <menuitem action=\"SourceIndentationSetBufferTab\"/>\n"
  "        <menuitem action=\"SourceIndentationAutoIndentBuffer\"/>\n"
  "      </menu>\n"
  "      <separator/>\n"
  "      <menuitem action=\"SourceGotoLine\"/>\n"
  "      <menuitem action=\"SourceCenterCursor\"/>\n"
  "      <separator/>\n"
  "      <menuitem action=\"SourceFindDocumentation\"/>\n"
  "      <separator/>\n"
  "      <menuitem action=\"SourceGotoDeclaration\"/>\n"
  "      <menuitem action=\"SourceGotoMethod\"/>\n"
  "      <menuitem action=\"SourceRename\"/>\n"
  "      <separator/>\n"
  "      <menuitem action=\"SourceGotoNextDiagnostic\"/>\n"
  "      <menuitem action=\"SourceApplyFixIts\"/>\n"
  "      <separator/>\n"
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
  "      <menuitem action=\"WindowNextTab\"/>\n"
  "      <menuitem action=\"WindowPreviousTab\"/>\n"
  "      <separator/>\n"
  "      <menuitem action=\"WindowCloseTab\"/>\n"
  "    </menu>\n"
  "    <menu action=\"PluginMenu\">\n"
  "    </menu>\n"
  "    <menu action=\"HelpMenu\">\n"
  "      <menuitem action=\"HelpAbout\"/>\n"
  "    </menu>\n"
  "  </menubar>\n"
  "</ui>\n";*/
}

Gtk::Widget& Menu::get_widget() {
  //return *ui_manager->get_widget("/MenuBar");
}

void Menu::build() {
  builder = Gtk::Builder::create();
  
  try {
    builder->add_from_string(ui_xml);
  }
  catch (const Glib::Error &ex) {
    std::cerr << "building menu failed: " << ex.what();
  }
}
