#include "juci.h"
#include "singletons.h"
#include "config.h"
#include <iostream>

using namespace std; //TODO: remove

void init_logging() {
  boost::log::add_common_attributes();
  boost::log::add_file_log(boost::log::keywords::file_name = Singleton::log_dir() + "juci.log",
               boost::log::keywords::auto_flush = true);
  INFO("Logging initalized");
}

int app::on_command_line(const Glib::RefPtr<Gio::ApplicationCommandLine> &cmd) {
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
          files.emplace_back(p.string());
        else if(boost::filesystem::is_directory(p))
          directories.emplace_back(p.string());
      }
      else
        std::cerr << "Path " << p << " does not exist." << std::endl;
    }
  }
  activate();
  return 0;
}

void app::on_activate() {
  window = std::unique_ptr<Window>(new Window());
  add_window(*window);
  window->show();
  bool first_directory=true;
  for(auto &directory: directories) {
    if(first_directory) {
      Singleton::directories()->open(directory);
      first_directory=false;
    }
    else {
      std::string files_in_directory;
      for(size_t c=0;c<files.size();c++) {
        if(files[c].substr(0, directory.size())==directory) {
          files_in_directory+=" "+files[c];
          files.erase(files.begin()+c);
          c--;
        }
      }
      std::thread another_juci_app([this, directory, files_in_directory](){
        Singleton::terminal()->async_print("Executing: juci "+directory+files_in_directory);
        Singleton::terminal()->execute("juci "+directory+files_in_directory, ""); //TODO: do not open pipes here, doing this after Juci compiles on Windows
      });
      another_juci_app.detach();
    }
  }
  for(auto &file: files)
    window->notebook.open(file);
}

app::app() : Gtk::Application("no.sout.juci", Gio::APPLICATION_NON_UNIQUE | Gio::APPLICATION_HANDLES_COMMAND_LINE) {
  MainConfig(); // Read the configs here
  auto style_context = Gtk::StyleContext::create();
  auto screen = Gdk::Screen::get_default();
  auto css_provider = Gtk::CssProvider::get_named(Singleton::Config::window()->theme_name, Singleton::Config::window()->theme_variant);
  //TODO: add check if theme exists, or else write error to Singleton::terminal()
  style_context->add_provider_for_screen(screen, css_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

int main(int argc, char *argv[]) {
  init_logging();
  return app().run(argc, argv);
}
