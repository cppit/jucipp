#include "singletons.h"
#include "config.h"
#include "logging.h"
#include <exception>
#include "files.h"
#include "sourcefile.h"

MainConfig::MainConfig() {
  find_or_create_config_files();
  boost::property_tree::json_parser::read_json(Singleton::config_dir() + "config.json", cfg);
  Singleton::Config::window()->keybindings = cfg.get_child("keybindings");
  GenerateSource();
  GenerateDirectoryFilter();
  
  Singleton::Config::window()->theme=cfg.get<std::string>("visual.gtk_theme");
  Singleton::Config::window()->theme_variant=cfg.get<std::string>("visual.gtk_theme_variant");
  
  boost::filesystem::create_directories(boost::filesystem::path(Singleton::style_dir()));
  Singleton::Config::source()->style=cfg.get<std::string>("visual.gtk_sourceview_style");
  
  Singleton::Config::terminal()->make_command=cfg.get<std::string>("project.make_command");
}

void MainConfig::find_or_create_config_files() {
  std::vector<std::string> files = {"config.json", "menu.xml", "plugins.py"};
  boost::filesystem::create_directories(boost::filesystem::path(Singleton::config_dir()));
  for (auto &file : files) {
    auto path = boost::filesystem::path(Singleton::config_dir() + file);
    if (!boost::filesystem::is_regular_file(path)) {
      if (file == "config.json") juci::filesystem::write(path, configjson);
      if (file == "plugins.py") juci::filesystem::write(path, pluginspy);
      if (file == "menu.xml") juci::filesystem::write(path, menuxml);
    }
  }
}

void MainConfig::GenerateSource() {
  auto source_cfg = Singleton::Config::source();
  auto source_json = cfg.get_child("source");
  
  source_cfg->tab_size = source_json.get<unsigned>("tab_size");
  source_cfg->tab_char = source_json.get<char>("tab_char");
  for(unsigned c=0;c<source_cfg->tab_size;c++)
    source_cfg->tab+=source_cfg->tab_char;
    
  source_cfg->highlight_current_line = source_json.get_value<bool>("highlight_current_line");
  source_cfg->show_line_numbers = source_json.get_value<bool>("show_line_numbers");
  
  
  for (auto &i : source_json.get_child("clang_types"))
    source_cfg->clang_types[i.first] = i.second.get_value<std::string>();
}

void MainConfig::GenerateDirectoryFilter() {
  auto dir_cfg=Singleton::Config::directories();
  DEBUG("Fetching directory filter");
  boost::property_tree::ptree dir_json = cfg.get_child("directoryfilter");
  boost::property_tree::ptree ignore_json = dir_json.get_child("ignore");
  boost::property_tree::ptree except_json = dir_json.get_child("exceptions");
  for ( auto &i : except_json )
    dir_cfg->exceptions.emplace_back(i.second.get_value<std::string>());
  for ( auto &i : ignore_json )
    dir_cfg->ignored.emplace_back(i.second.get_value<std::string>());
  DEBUG("Directory filter fetched");
}
