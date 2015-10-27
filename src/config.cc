#include "singletons.h"
#include "config.h"
#include "logging.h"
#include <exception>
#include "files.h"
#include <iostream>

using namespace std; //TODO: remove

MainConfig::MainConfig() {
  auto search_envs = init_home_path();
  auto config_json = (home/"config"/"config.json").string(); // This causes some redundant copies, but assures windows support
  try {
    find_or_create_config_files();
    if(home.empty()) {
      std::string searched_envs = "[";
      for(auto &env : search_envs)
        searched_envs+=env+", ";
      searched_envs.erase(searched_envs.end()-2, searched_envs.end());
      searched_envs+="]";
      throw std::runtime_error("One of these environment variables needs to point to a writable directory to save configuration: " + searched_envs);
    }
    boost::property_tree::json_parser::read_json(config_json, cfg);
    update_config_file();
    retrieve_config();
  }
  catch(const std::exception &e) {
    std::stringstream ss;
    ss << configjson;
    boost::property_tree::read_json(ss, cfg);
    retrieve_config();
    JERROR("Error reading "+ config_json + ": "+e.what()+"\n"); // logs will print to cerr when init_log haven't been run yet
  }
  cfg.clear();
}

void MainConfig::find_or_create_config_files() {
  auto config_dir = home/"config";
  auto config_json = config_dir/"config.json";
  auto plugins_py = config_dir/"plugins.py";

  boost::filesystem::create_directories(config_dir); // io exp captured by calling method

  if (!boost::filesystem::exists(config_json))
    filesystem::write(config_json, configjson); // vars configjson and pluginspy
  if (!boost::filesystem::exists(plugins_py))   // live in files.h
    filesystem::write(plugins_py, pluginspy);

  auto juci_style_path = home/"styles";
  boost::filesystem::create_directories(juci_style_path); // io exp captured by calling method

  juci_style_path/="juci-light.xml";
  if(!boost::filesystem::exists(juci_style_path))
    filesystem::write(juci_style_path, juci_light_style);
  juci_style_path=juci_style_path.parent_path();
  juci_style_path/="juci-dark.xml";
  if(!boost::filesystem::exists(juci_style_path))
    filesystem::write(juci_style_path, juci_dark_style);
  juci_style_path=juci_style_path.parent_path();
  juci_style_path/="juci-dark-blue.xml";
  if(!boost::filesystem::exists(juci_style_path))
    filesystem::write(juci_style_path, juci_dark_blue_style);
}

void MainConfig::retrieve_config() {
  Singleton::Config::window()->keybindings = cfg.get_child("keybindings");
  GenerateSource();
  GenerateDirectoryFilter();

  Singleton::Config::window()->theme_name=cfg.get<std::string>("gtk_theme.name");
  Singleton::Config::window()->theme_variant=cfg.get<std::string>("gtk_theme.variant");
  Singleton::Config::window()->version = cfg.get<std::string>("version");
  Singleton::Config::window()->default_size = {cfg.get<int>("default_window_size.width"), cfg.get<int>("default_window_size.height")};
  Singleton::Config::terminal()->make_command=cfg.get<std::string>("project.make_command");
  Singleton::Config::terminal()->cmake_command=cfg.get<std::string>("project.cmake_command");
  Singleton::Config::terminal()->history_size=cfg.get<int>("terminal_history_size");
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
      exists&=check_config_file(node.second, path);
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
    std::cerr << "Error reading json-file: " << e.what() << std::endl;
    cfg_ok=false;
  }
  cfg_ok&=check_config_file(default_cfg);
  if(!cfg_ok) {
    boost::property_tree::write_json((home/"config"/"config.json").string(), cfg);
  }
}

void MainConfig::GenerateSource() {
  auto source_cfg = Singleton::Config::source();
  auto source_json = cfg.get_child("source");

  Singleton::Config::source()->style=source_json.get<std::string>("style");
  source_cfg->font=source_json.get<std::string>("font");

  source_cfg->show_map = source_json.get<bool>("show_map");
  source_cfg->map_font_size = source_json.get<std::string>("map_font_size");

  source_cfg->spellcheck_language = source_json.get<std::string>("spellcheck_language");

  source_cfg->default_tab_char = source_json.get<char>("default_tab_char");
  source_cfg->default_tab_size = source_json.get<unsigned>("default_tab_size");
  source_cfg->auto_tab_char_and_size = source_json.get<bool>("auto_tab_char_and_size");

  source_cfg->wrap_lines = source_json.get<bool>("wrap_lines");

  source_cfg->highlight_current_line = source_json.get<bool>("highlight_current_line");
  source_cfg->show_line_numbers = source_json.get<bool>("show_line_numbers");

  for (auto &i : source_json.get_child("clang_types"))
    source_cfg->clang_types[i.first] = i.second.get_value<std::string>();

  auto pt_doc_search=cfg.get_child("documentation_searches");
  for(auto &pt_doc_search_lang: pt_doc_search) {
    source_cfg->documentation_searches[pt_doc_search_lang.first].separator=pt_doc_search_lang.second.get<std::string>("separator");
    auto &queries=source_cfg->documentation_searches.find(pt_doc_search_lang.first)->second.queries;
    for(auto &i: pt_doc_search_lang.second.get_child("queries")) {
      queries[i.first]=i.second.get_value<std::string>();
    }
  }
}

void MainConfig::GenerateDirectoryFilter() {
  auto dir_cfg=Singleton::Config::directories();
  boost::property_tree::ptree dir_json = cfg.get_child("directoryfilter");
  boost::property_tree::ptree ignore_json = dir_json.get_child("ignore");
  boost::property_tree::ptree except_json = dir_json.get_child("exceptions");
  for ( auto &i : except_json )
    dir_cfg->exceptions.emplace_back(i.second.get_value<std::string>());
  for ( auto &i : ignore_json )
    dir_cfg->ignored.emplace_back(i.second.get_value<std::string>());
}
std::vector<std::string> MainConfig::init_home_path(){
  std::vector<std::string> locations = JUCI_ENV_SEARCH_LOCATIONS;
  char *ptr = nullptr;
  for (auto &env : locations) {
    ptr=std::getenv(env.c_str());
    if (ptr==nullptr)
      continue;
    else
      if (boost::filesystem::exists(ptr)) {
        home /= ptr;
        home /= ".juci";
        return locations;
      }
  }
  home="";
  return locations;
}
