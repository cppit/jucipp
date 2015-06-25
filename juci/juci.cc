#include "juci.h"

void init_logging() {
  add_common_attributes();
  add_file_log(keywords::file_name = "juci.log",
               keywords::auto_flush = true);
  INFO("Logging initalized");
}

int Juci::on_command_line(const Glib::RefPtr<Gio::ApplicationCommandLine> &cmd) {
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

void Juci::on_activate() {
  window = std::unique_ptr<Window>(new Window());
  add_window(*window);
  window->show();
  if(directory!="") {
    //TODO: use the following instead, window->notebook_.open_directory(directory);
    window->notebook_.project_path=directory;
    window->notebook_.directories.open_folder(directory);
  }
  for(auto &f: files)
    window->notebook_.OnOpenFile(f);
}

int main(int argc, char *argv[]) {
  init_logging();
  return Juci().run(argc, argv);
}
