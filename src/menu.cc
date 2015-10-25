#include "menu.h"
#include "singletons.h"
#include <string>
#include <iostream>

using namespace std; //TODO: remove

Menu::Menu() {
  ui_xml =
  "<interface>"
  "  <menu id='juci-menu'>"
  "    <section>"
  "      <item>"
  "        <attribute name='label' translatable='yes'>_About</attribute>"
  "        <attribute name='action'>app.about</attribute>"
  "      </item>"
  "    </section>"
  "    <section>"
  "      <item>"
  "        <attribute name='label' translatable='yes'>_Preferences</attribute>"
  "        <attribute name='action'>app.preferences</attribute>"
  "      </item>"  
  "    </section>"
  "    <section>"
  "      <item>"
  "        <attribute name='label' translatable='yes'>_Quit</attribute>"
  "        <attribute name='action'>app.quit</attribute>"
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
  "          <attribute name='action'>app.new_file</attribute>"
  "        </item>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_New _Directory</attribute>"
  "          <attribute name='action'>app.new_folder</attribute>"
  "        </item>"
  "        <submenu>"
  "          <attribute name='label' translatable='yes'>_New _Project</attribute>"
  "          <item>"
  "            <attribute name='label' translatable='yes'>C++</attribute>"
  "            <attribute name='action'>app.new_project_cpp</attribute>"
  "          </item>"
  "        </submenu>"
  "      </section>"
  "      <section>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Open _File</attribute>"
  "          <attribute name='action'>app.open_file</attribute>"
  "        </item>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Open _Folder</attribute>"
  "          <attribute name='action'>app.open_folder</attribute>"
  "        </item>"
  "      </section>"
  "      <section>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Save</attribute>"
  "          <attribute name='action'>app.save</attribute>"
  "        </item>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Save _As</attribute>"
  "          <attribute name='action'>app.save_as</attribute>"
  "        </item>"
  "      </section>"
  "    </submenu>"
  "  </menu>"
  "</interface>";
}

void Menu::add_action(const std::string &name, std::function<void()> action) {
  auto application = builder->get_application();
  
  actions[name]=application->add_action(name, action);
  
  auto key=Singleton::Config::menu()->keys.find(name);
  if(key!=Singleton::Config::menu()->keys.end())
    application->set_accel_for_action("app."+name, key->second);
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
