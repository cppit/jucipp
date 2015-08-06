#include "juci.h"
#include "singletons.h"

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
      boost::filesystem::path p=boost::filesystem::canonical(argv[c]);
      if(boost::filesystem::exists(p)) {
        if(boost::filesystem::is_regular_file(p))
          files.emplace_back(p.string());
        else if(directory=="" && boost::filesystem::is_directory(p))
          directory=p.string();
      }
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
    window->notebook.project_path=directory;
    window->directories.open_folder(directory);
  }
  for(auto &f: files)
    window->notebook.open(f);
}

app::app() : Gtk::Application("no.sout.juci", Gio::APPLICATION_HANDLES_COMMAND_LINE) {
  auto css_provider = Gtk::CssProvider::create();
  if (css_provider->load_from_path(Singleton::style_dir() + "juci.css")) {
    auto style_context = Gtk::StyleContext::create();
    style_context->add_provider(css_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    style_context->add_provider_for_screen(window->get_screen(), css_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  }
}

int main(int argc, char *argv[]) {
  init_logging();
  return app().run(argc, argv);
}
