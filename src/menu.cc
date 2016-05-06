#include "menu.h"
#include "config.h"
#include <string>
#include <iostream>

//TODO: if Ubuntu ever gets fixed, cleanup the Ubuntu specific code
Menu::Menu() {
  auto accels=Config::get().menu.keys;
  for(auto &accel: accels) {
#ifdef JUCI_UBUNTU
    size_t pos=0;
    std::string second=accel.second;
    while((pos=second.find('<', pos))!=std::string::npos) {
      second.replace(pos, 1, "&lt;");
      pos+=4;
    }
    pos=0;
    while((pos=second.find('>', pos))!=std::string::npos) {
      second.replace(pos, 1, "&gt;");
      pos+=4;
    }
    if(second.size()>0)
      accel.second="<attribute name='accel'>"+second+"</attribute>";
    else
      accel.second="";
#else
    accel.second="";
#endif
  }
  
  ui_xml =
  "<interface>"
  "  <menu id='juci-menu'>"
  "    <section>"
  "      <item>"
  "        <attribute name='label' translatable='yes'>_About</attribute>"
  "        <attribute name='action'>app.about</attribute>"
           +accels["about"]+ //For Ubuntu...
  "      </item>"
  "    </section>"
  "    <section>"
  "      <item>"
  "        <attribute name='label' translatable='yes'>_Preferences</attribute>"
  "        <attribute name='action'>app.preferences</attribute>"
           +accels["preferences"]+ //For Ubuntu...
  "      </item>"  
  "    </section>"
  "    <section>"
  "      <item>"
  "        <attribute name='label' translatable='yes'>_Quit</attribute>"
  "        <attribute name='action'>app.quit</attribute>"
           +accels["quit"]+ //For Ubuntu...
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
             +accels["new_file"]+ //For Ubuntu...
  "        </item>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_New _Folder</attribute>"
  "          <attribute name='action'>app.new_folder</attribute>"
             +accels["new_folder"]+ //For Ubuntu...
  "        </item>"
  "        <submenu>"
  "          <attribute name='label' translatable='yes'>_New _Project</attribute>"
  "          <item>"
  "            <attribute name='label' translatable='no'>C++</attribute>"
  "            <attribute name='action'>app.new_project_cpp</attribute>"
               +accels["new_project_cpp"]+ //For Ubuntu...
  "          </item>"
  "        </submenu>"
  "      </section>"
  "      <section>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Open _File</attribute>"
  "          <attribute name='action'>app.open_file</attribute>"
             +accels["open_file"]+ //For Ubuntu...
  "        </item>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Open _Folder</attribute>"
  "          <attribute name='action'>app.open_folder</attribute>"
             +accels["open_folder"]+ //For Ubuntu...
  "        </item>"
  "      </section>"
  "      <section>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Save</attribute>"
  "          <attribute name='action'>app.save</attribute>"
             +accels["save"]+ //For Ubuntu...
  "        </item>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Save _As</attribute>"
  "          <attribute name='action'>app.save_as</attribute>"
             +accels["save_as"]+ //For Ubuntu...
  "        </item>"
  "      </section>"
  "      <section>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Print</attribute>"
  "          <attribute name='action'>app.print</attribute>"
             +accels["print"]+ //For Ubuntu...
  "        </item>"
  "      </section>"
  "    </submenu>"
  ""
  "    <submenu>"
  "      <attribute name='label' translatable='yes'>_Edit</attribute>"
  "      <section>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Undo</attribute>"
  "          <attribute name='action'>app.edit_undo</attribute>"
             +accels["edit_undo"]+ //For Ubuntu...
  "        </item>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Redo</attribute>"
  "          <attribute name='action'>app.edit_redo</attribute>"
             +accels["edit_redo"]+ //For Ubuntu...
  "        </item>"
  "      </section>"
  "      <section>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Cut</attribute>"
  "          <attribute name='action'>app.edit_cut</attribute>"
             +accels["edit_cut"]+ //For Ubuntu...
  "        </item>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Copy</attribute>"
  "          <attribute name='action'>app.edit_copy</attribute>"
             +accels["edit_copy"]+ //For Ubuntu...
  "        </item>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Paste</attribute>"
  "          <attribute name='action'>app.edit_paste</attribute>"
             +accels["edit_paste"]+ //For Ubuntu...
  "        </item>"
  "      </section>"
  "      <section>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Find</attribute>"
  "          <attribute name='action'>app.edit_find</attribute>"
             +accels["edit_find"]+ //For Ubuntu...
  "        </item>"
  "      </section>"
  "    </submenu>"
  ""
  "    <submenu>"
  "      <attribute name='label' translatable='yes'>_Source</attribute>"
  "      <section>"
  "        <submenu>"
  "          <attribute name='label' translatable='yes'>_Spell _Check</attribute>"
  "          <item>"
  "            <attribute name='label' translatable='yes'>_Spell _Check _Buffer</attribute>"
  "            <attribute name='action'>app.source_spellcheck</attribute>"
               +accels["source_spellcheck"]+ //For Ubuntu...
  "          </item>"
  "          <item>"
  "            <attribute name='label' translatable='yes'>_Clear _Spelling _Errors</attribute>"
  "            <attribute name='action'>app.source_spellcheck_clear</attribute>"
               +accels["source_spellcheck_clear"]+ //For Ubuntu...
  "          </item>"
  "          <item>"
  "            <attribute name='label' translatable='yes'>_Go _to _Next _Spelling _Error</attribute>"
  "            <attribute name='action'>app.source_spellcheck_next_error</attribute>"
               +accels["source_spellcheck_next_error"]+ //For Ubuntu...
  "          </item>"
  "        </submenu>"
  "      </section>"
  "      <section>"
  "        <submenu>"
  "          <attribute name='label' translatable='yes'>_Indentation</attribute>"
  "          <item>"
  "            <attribute name='label' translatable='yes'>_Set _Current _Buffer _Tab</attribute>"
  "            <attribute name='action'>app.source_indentation_set_buffer_tab</attribute>"
               +accels["source_indentation_set_buffer_tab"]+ //For Ubuntu...
  "          </item>"
  "          <item>"
  "            <attribute name='label' translatable='yes'>_Auto-Indent _Current _Buffer</attribute>"
  "            <attribute name='action'>app.source_indentation_auto_indent_buffer</attribute>"
               +accels["source_indentation_auto_indent_buffer"]+ //For Ubuntu...
  "          </item>"
  "        </submenu>"
  "      </section>"
  "      <section>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Go _to _Line</attribute>"
  "          <attribute name='action'>app.source_goto_line</attribute>"
             +accels["source_goto_line"]+ //For Ubuntu...
  "        </item>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Center _Cursor</attribute>"
  "          <attribute name='action'>app.source_center_cursor</attribute>"
             +accels["source_center_cursor"]+ //For Ubuntu...
  "        </item>"
  "      </section>"
  "      <section>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Find _Documentation</attribute>"
  "          <attribute name='action'>app.source_find_documentation</attribute>"
             +accels["source_find_documentation"]+ //For Ubuntu...
  "        </item>"
  "      </section>"
  "      <section>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Go to Declaration</attribute>"
  "          <attribute name='action'>app.source_goto_declaration</attribute>"
             +accels["source_goto_declaration"]+ //For Ubuntu...
  "        </item>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Go to Implementation</attribute>"
  "          <attribute name='action'>app.source_goto_implementation</attribute>"
             +accels["source_goto_implementation"]+ //For Ubuntu...
  "        </item>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Go to Usage</attribute>"
  "          <attribute name='action'>app.source_goto_usage</attribute>"
             +accels["source_goto_usage"]+ //For Ubuntu...
  "        </item>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Go to Method</attribute>"
  "          <attribute name='action'>app.source_goto_method</attribute>"
             +accels["source_goto_method"]+ //For Ubuntu...
  "        </item>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Rename</attribute>"
  "          <attribute name='action'>app.source_rename</attribute>"
             +accels["source_rename"]+ //For Ubuntu...
  "        </item>"
  "      </section>"
  "      <section>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Go to Next Diagnostic</attribute>"
  "          <attribute name='action'>app.source_goto_next_diagnostic</attribute>"
             +accels["source_goto_next_diagnostic"]+ //For Ubuntu...
  "        </item>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Apply Fix-Its</attribute>"
  "          <attribute name='action'>app.source_apply_fix_its</attribute>"
             +accels["source_apply_fix_its"]+ //For Ubuntu...
  "        </item>"
  "      </section>"
  "    </submenu>"
  ""
  "    <submenu>"
  "      <attribute name='label' translatable='yes'>_Project</attribute>"
  "      <section>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Set _Run _Arguments</attribute>"
  "          <attribute name='action'>app.project_set_run_arguments</attribute>"
             +accels["project_set_run_arguments"]+ //For Ubuntu...
  "        </item>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Compile _and _Run</attribute>"
  "          <attribute name='action'>app.compile_and_run</attribute>"
             +accels["compile_and_run"]+ //For Ubuntu...
  "        </item>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Compile</attribute>"
  "          <attribute name='action'>app.compile</attribute>"
             +accels["compile"]+ //For Ubuntu...
  "        </item>"
  "      </section>"
  "      <section>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Run _Command</attribute>"
  "          <attribute name='action'>app.run_command</attribute>"
             +accels["run_command"]+ //For Ubuntu...
  "        </item>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Kill _Last _Process</attribute>"
  "          <attribute name='action'>app.kill_last_running</attribute>"
             +accels["kill_last_running"]+ //For Ubuntu...
  "        </item>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Force _Kill _Last _Process</attribute>"
  "          <attribute name='action'>app.force_kill_last_running</attribute>"
             +accels["force_kill_last_running"]+ //For Ubuntu...
  "        </item>"
  "      </section>"
  "    </submenu>"
  ""
  "    <submenu>"
  "      <attribute name='label' translatable='yes'>_Debug</attribute>"
  "      <section>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Set _Run _Arguments</attribute>"
  "          <attribute name='action'>app.debug_set_run_arguments</attribute>"
             +accels["debug_set_run_arguments"]+ //For Ubuntu...
  "        </item>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Start/_Continue</attribute>"
  "          <attribute name='action'>app.debug_start_continue</attribute>"
             +accels["debug_start_continue"]+ //For Ubuntu...
  "        </item>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Stop</attribute>"
  "          <attribute name='action'>app.debug_stop</attribute>"
             +accels["debug_stop"]+ //For Ubuntu...
  "        </item>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Kill</attribute>"
  "          <attribute name='action'>app.debug_kill</attribute>"
             +accels["debug_kill"]+ //For Ubuntu...
  "        </item>"
  "      </section>"
  "      <section>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Step _Over</attribute>"
  "          <attribute name='action'>app.debug_step_over</attribute>"
             +accels["debug_step_over"]+ //For Ubuntu...
  "        </item>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Step _Into</attribute>"
  "          <attribute name='action'>app.debug_step_into</attribute>"
             +accels["debug_step_into"]+ //For Ubuntu...
  "        </item>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Step _Out</attribute>"
  "          <attribute name='action'>app.debug_step_out</attribute>"
             +accels["debug_step_out"]+ //For Ubuntu...
  "        </item>"
  "      </section>"
  "      <section>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Backtrace</attribute>"
  "          <attribute name='action'>app.debug_backtrace</attribute>"
             +accels["debug_backtrace"]+ //For Ubuntu...
  "        </item>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Show _Variables</attribute>"
  "          <attribute name='action'>app.debug_show_variables</attribute>"
             +accels["debug_show_variables"]+ //For Ubuntu...
  "        </item>"
  "      </section>"
  "      <section>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Run Command</attribute>"
  "          <attribute name='action'>app.debug_run_command</attribute>"
             +accels["debug_run_command"]+ //For Ubuntu...
  "        </item>"
  "      </section>"
  "      <section>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Toggle _Breakpoint</attribute>"
  "          <attribute name='action'>app.debug_toggle_breakpoint</attribute>"
             +accels["debug_toggle_breakpoint"]+ //For Ubuntu...
  "        </item>"
  "      </section>"
  "      <section>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Go _to _Stop</attribute>"
  "          <attribute name='action'>app.debug_goto_stop</attribute>"
             +accels["debug_goto_stop"]+ //For Ubuntu...
  "        </item>"
  "      </section>"
  "    </submenu>"
  ""
  "    <submenu>"
  "      <attribute name='label' translatable='yes'>_Window</attribute>"
  "      <section>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Next _Tab</attribute>"
  "          <attribute name='action'>app.next_tab</attribute>"
             +accels["next_tab"]+ //For Ubuntu...
  "        </item>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Previous _Tab</attribute>"
  "          <attribute name='action'>app.previous_tab</attribute>"
             +accels["previous_tab"]+ //For Ubuntu...
  "        </item>"
  "      </section>"
  "      <section>"
  "        <item>"
  "          <attribute name='label' translatable='yes'>_Close _Tab</attribute>"
  "          <attribute name='action'>app.close_tab</attribute>"
             +accels["close_tab"]+ //For Ubuntu...
  "        </item>"
  "      </section>"
  "    </submenu>"
  "  </menu>"
  "</interface>";
}

void Menu::add_action(const std::string &name, std::function<void()> action) {
  auto g_application=g_application_get_default();
  auto gio_application=Glib::wrap(g_application, true);
  auto application=Glib::RefPtr<Gtk::Application>::cast_static(gio_application);
  
  actions[name]=application->add_action(name, action);
}

void Menu::set_keys() {
  auto g_application=g_application_get_default();
  auto gio_application=Glib::wrap(g_application, true);
  auto application=Glib::RefPtr<Gtk::Application>::cast_static(gio_application);
           
  for(auto &key: Config::get().menu.keys) {
    if(key.second.size()>0 && actions.find(key.first)!=actions.end()) {
#if GTK_VERSION_GE(3, 12)
      application->set_accel_for_action("app."+key.first, key.second); 
#else
      application->add_accelerator(key.second, "app."+key.first); //For Ubuntu 14...
#endif
    }
  }
}

void Menu::build() {
  builder = Gtk::Builder::create();
  
  try {
    builder->add_from_string(ui_xml);
    auto object = Menu::get().builder->get_object("juci-menu");
    juci_menu = Glib::RefPtr<Gio::Menu>::cast_dynamic(object);
    object = Menu::get().builder->get_object("window-menu");
    window_menu = Glib::RefPtr<Gio::Menu>::cast_dynamic(object);
  }
  catch (const Glib::Error &ex) {
    std::cerr << "building menu failed: " << ex.what();
  }
}
