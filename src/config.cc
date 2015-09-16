#include "singletons.h"
#include "config.h"
#include "logging.h"
#include <exception>
#include "files.h"
#include "sourcefile.h"

#include <iostream> //TODO: remove
using namespace std; //TODO: remove

MainConfig::MainConfig() {
  find_or_create_config_files();
  boost::property_tree::json_parser::read_json(Singleton::config_dir() + "config.json", cfg);
  update_config_file();
  
  Singleton::Config::window()->keybindings = cfg.get_child("keybindings");
  GenerateSource();
  GenerateDirectoryFilter();
  
  Singleton::Config::window()->theme_name=cfg.get<std::string>("gtk_theme.name");
  Singleton::Config::window()->theme_variant=cfg.get<std::string>("gtk_theme.variant");
  Singleton::Config::window()->version = cfg.get<std::string>("version");
  Singleton::Config::terminal()->make_command=cfg.get<std::string>("project.make_command");
  Singleton::Config::terminal()->cmake_command=cfg.get<std::string>("project.cmake_command");
}

void MainConfig::find_or_create_config_files() {
  std::vector<std::string> files = {"config.json", "plugins.py"};
  boost::filesystem::create_directories(boost::filesystem::path(Singleton::config_dir()));
  for (auto &file : files) {
    auto path = boost::filesystem::path(Singleton::config_dir() + file);
    if (!boost::filesystem::is_regular_file(path)) {
      if (file == "config.json") juci::filesystem::write(path, configjson);
      if (file == "plugins.py") juci::filesystem::write(path, pluginspy);
    }
  }
  
  boost::filesystem::create_directories(boost::filesystem::path(Singleton::style_dir()));
  boost::filesystem::path juci_style_path=Singleton::style_dir();
  juci_style_path+="juci-light.xml";
  if(!boost::filesystem::exists(juci_style_path))
    juci::filesystem::write(juci_style_path, juci_light_style);
  juci_style_path=Singleton::style_dir();
  juci_style_path+="juci-dark.xml";
  if(!boost::filesystem::exists(juci_style_path))
    juci::filesystem::write(juci_style_path, juci_dark_style);
}

bool MainConfig::check_config_file(const boost::property_tree::ptree &default_cfg, std::string parent_path) {
  if(parent_path.size()>0)
    parent_path+=".";
  bool exists=true;
  for(auto &node: default_cfg) {
    auto path=parent_path+node.first;
    try {
      cfg.get<std::string>(path);
    }
    catch(const std::exception &e) {
      cfg.add(path, node.second.data());
      exists=false;        
    }
    try {
      exists&=check_config_file(default_cfg.get_child(node.first), path);
    }
    catch(const std::exception &e) {        
    }
  }
  return exists;
}

void MainConfig::update_config_file() {
  boost::property_tree::ptree default_cfg;
  bool cfg_ok=true;
  try {
    if(cfg.get<std::string>("version")!=JUCI_VERSION) {
      std::stringstream ss;
      ss << configjson;
      boost::property_tree::read_json(ss, default_cfg);
      cfg_ok=false;
      if(cfg.count("version")>0)
        cfg.find("version")->second.data()=default_cfg.get<std::string>("version");
    }
    else
      return;
  }
  catch(const std::exception &e) {
    cfg_ok=false;
  }
  cfg_ok&=check_config_file(default_cfg);
  if(!cfg_ok)
    boost::property_tree::write_json(Singleton::config_dir()+"config.json", cfg);
}

void MainConfig::GenerateSource() {
  auto source_cfg = Singleton::Config::source();
  auto source_json = cfg.get_child("source");
  
  source_cfg->spellcheck_language = source_json.get<std::string>("spellcheck_language");
  
  source_cfg->default_tab_char = source_json.get<char>("default_tab_char");
  source_cfg->default_tab_size = source_json.get<unsigned>("default_tab_size");
  source_cfg->auto_tab_char_and_size = source_json.get<bool>("auto_tab_char_and_size");
  
  source_cfg->wrap_lines = source_json.get<bool>("wrap_lines");
  
  source_cfg->highlight_current_line = source_json.get<bool>("highlight_current_line");
  source_cfg->show_line_numbers = source_json.get<bool>("show_line_numbers");
  
  for (auto &i : source_json.get_child("clang_types"))
    source_cfg->clang_types[i.first] = i.second.get_value<std::string>();
  
  Singleton::Config::source()->style=source_json.get<std::string>("style");
  source_cfg->font=source_json.get<std::string>("font");
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
