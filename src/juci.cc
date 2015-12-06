#include "juci.h"
#include "singletons.h"

using namespace std; //TODO: remove

void Application::init_logging() {
  boost::log::add_common_attributes();
  auto log_dir = Singleton::config->juci_home_path()/"log"/"juci.log";
  boost::log::add_file_log(boost::log::keywords::file_name = log_dir,
               boost::log::keywords::auto_flush = true);
  JINFO("Logging initalized");
}

int Application::on_command_line(const Glib::RefPtr<Gio::ApplicationCommandLine> &cmd) {
  Glib::set_prgname("juci");
  Glib::OptionContext ctx("[PATH ...]");
  Glib::OptionGroup gtk_group(gtk_get_option_group(true));
  ctx.add_group(gtk_group);
  int argc;
  char **argv = cmd->get_arguments(argc);
  ctx.parse(argc, argv);
  if(argc>=2) {
    for(int c=1;c<argc;c++) {
      boost::filesystem::path p(argv[c]);
      if(boost::filesystem::exists(p)) {
        p=boost::filesystem::canonical(p);
        if(boost::filesystem::is_regular_file(p))
          files.emplace_back(p);
        else if(boost::filesystem::is_directory(p))
          directories.emplace_back(p);
      }
      else { //Open new file if parent path exists
        auto parent_p=p.parent_path();
        boost::system::error_code ec;
        auto new_p=boost::filesystem::canonical(parent_p, ec);
        if(!ec && boost::filesystem::is_directory(new_p)) {
          new_p+="/";
          new_p+=p.filename();
          files.emplace_back(new_p);
        }
        else
          Singleton::terminal->print("Error: folder path "+parent_p.string()+" does not exist.\n", true);
      }
    }
  }
  activate();
  return 0;
}

void Application::on_activate() {
  add_window(*window);
  window->show();
  bool first_directory=true;
  for(auto &directory: directories) {
    if(first_directory) {
      Singleton::directories->open(directory);
      first_directory=false;
    }
    else {
      std::string files_in_directory;
      for(auto it=files.begin();it!=files.end();) {
        if(it->generic_string().substr(0, directory.generic_string().size()+1)==directory.generic_string()+'/') {
          files_in_directory+=" "+it->string();
          it=files.erase(it);
        }
        else
          it++;
      }
      std::thread another_juci_app([this, directory, files_in_directory](){
        Singleton::terminal->async_print("Executing: juci "+directory.string()+files_in_directory+"\n");
        Singleton::terminal->process("juci "+directory.string()+files_in_directory, "", false);
      });
      another_juci_app.detach();
    }
  }
  for(auto &file: files)
    window->notebook.open(file);
}

void Application::on_startup() {
  Gtk::Application::on_startup();
  
  Singleton::menu->init();
  Singleton::menu->build();

  auto object = Singleton::menu->builder->get_object("juci-menu");
  auto juci_menu = Glib::RefPtr<Gio::Menu>::cast_dynamic(object);
  object = Singleton::menu->builder->get_object("window-menu");
  auto window_menu = Glib::RefPtr<Gio::Menu>::cast_dynamic(object);
  if (!juci_menu || !window_menu) {
    std::cerr << "Menu not found." << std::endl;
  }
  else {
    set_app_menu(juci_menu);
    set_menubar(window_menu);
  }
}

Application::Application() : Gtk::Application("no.sout.juci", Gio::APPLICATION_NON_UNIQUE | Gio::APPLICATION_HANDLES_COMMAND_LINE) {
  Singleton::init();
  init_logging();
  
  Glib::set_application_name("juCi++");
  
  //Gtk::MessageDialog without buttons caused text to be selected, this prevents that
  Gtk::Settings::get_default()->property_gtk_label_select_on_focus()=false;
  
  window=std::unique_ptr<Window>(new Window());
}

int main(int argc, char *argv[]) {
  return Application().run(argc, argv);
}
