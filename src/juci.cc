#include "juci.h"
#include "singletons.h"
#include "config.h"
#include <iostream>

void init_logging() {
  add_common_attributes();
  add_file_log(keywords::file_name = Singleton::log_dir() + "juci.log",
               keywords::auto_flush = true);
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
        else if(directory=="" && boost::filesystem::is_directory(p))
          directory=p.string();
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
  if(directory!="") {
    window->directories.open_folder(directory);
  }
  for(auto &f: files)
    window->notebook.open(f);
}

app::app() : Gtk::Application("no.sout.juci", Gio::APPLICATION_NON_UNIQUE | Gio::APPLICATION_HANDLES_COMMAND_LINE) {
  MainConfig(); // Read the configs here
  auto css_provider = Gtk::CssProvider::get_default();
  auto style_context = Gtk::StyleContext::create();
  auto screen = Gdk::Screen::get_default();
  style_context->add_provider_for_screen(screen, css_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  css_provider->load_from_path(Singleton::theme_dir() + Singleton::Config::theme()->current_theme());
}

int main(int argc, char *argv[]) {
  init_logging();
  return app().run(argc, argv);
}
