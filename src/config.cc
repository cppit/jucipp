#include "config.h"
#include <exception>
#include "files.h"
#include <iostream>
#include "filesystem.h"
#include "terminal.h"
#include <algorithm>

Config::Config() {
  std::vector<std::string> environment_variables = {"JUCI_HOME", "HOME", "AppData"};
  char *ptr = nullptr;
  for (auto &variable : environment_variables) {
    ptr=std::getenv(variable.c_str());
    if (ptr!=nullptr && boost::filesystem::exists(ptr)) {
      home /= ptr;
      home /= ".juci";
      break;
    }
  }
  if(home.empty()) {
    std::string searched_envs = "[";
    for(auto &variable : environment_variables)
      searched_envs+=variable+", ";
    searched_envs.erase(searched_envs.end()-2, searched_envs.end());
    searched_envs+="]";
    throw std::runtime_error("One of these environment variables needs to point to a writable directory to save configuration: " + searched_envs);
  }
  
#ifdef _WIN32
    auto env_WD=std::getenv("WD");
    auto env_MSYSTEM=std::getenv("MSYSTEM");
    if(env_WD!=NULL && env_MSYSTEM!=NULL) {
      terminal.msys2_mingw_path=boost::filesystem::path(env_WD).parent_path().parent_path().parent_path();
      std::string msystem=env_MSYSTEM;
      std::transform(msystem.begin(), msystem.end(), msystem.begin(), ::tolower);
      terminal.msys2_mingw_path/=msystem;
    }
#endif
}

void Config::load() {
  auto config_json = (home/"config"/"config.json").string(); // This causes some redundant copies, but assures windows support
  try {
    find_or_create_config_files();
    boost::property_tree::json_parser::read_json(config_json, cfg);
    update_config_file();
    retrieve_config();
  }
  catch(const std::exception &e) {
    ::Terminal::get().print("Error: could not parse "+config_json+": "+e.what()+"\n", true);
    std::stringstream ss;
    ss << configjson;
    boost::property_tree::read_json(ss, cfg);
    retrieve_config();
  }
  cfg.clear();
}

void Config::find_or_create_config_files() {
  auto config_dir = home/"config";
  auto config_json = config_dir/"config.json";

  boost::filesystem::create_directories(config_dir); // io exp captured by calling method

  if (!boost::filesystem::exists(config_json))
    filesystem::write(config_json, configjson);

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

void Config::retrieve_config() {
  auto keybindings_pt = cfg.get_child("keybindings");
  for (auto &i : keybindings_pt) {
    menu.keys[i.first] = i.second.get_value<std::string>();
  }
  get_source();

  window.theme_name=cfg.get<std::string>("gtk_theme.name");
  window.theme_variant=cfg.get<std::string>("gtk_theme.variant");
  window.version = cfg.get<std::string>("version");
  window.default_size = {cfg.get<int>("default_window_size.width"), cfg.get<int>("default_window_size.height")};
  
  project.default_build_path=cfg.get<std::string>("project.default_build_path");
  project.debug_build_path=cfg.get<std::string>("project.debug_build_path");
  project.make_command=cfg.get<std::string>("project.make_command");
  project.cmake_command=cfg.get<std::string>("project.cmake_command");
  project.save_on_compile_or_run=cfg.get<bool>("project.save_on_compile_or_run");
  project.clear_terminal_on_compile=cfg.get<bool>("project.clear_terminal_on_compile");
  
  terminal.history_size=cfg.get<int>("terminal.history_size");
  terminal.font=cfg.get<std::string>("terminal.font");
  terminal.clang_format_command="clang-format";
#ifdef __linux
  if(terminal.clang_format_command=="clang-format" &&
     !boost::filesystem::exists("/usr/bin/clang-format") && !boost::filesystem::exists("/usr/local/bin/clang-format")) {
    if(boost::filesystem::exists("/usr/bin/clang-format-3.8"))
      terminal.clang_format_command="/usr/bin/clang-format-3.8";
    else if(boost::filesystem::exists("/usr/bin/clang-format-3.7"))
      terminal.clang_format_command="/usr/bin/clang-format-3.7";
    else if(boost::filesystem::exists("/usr/bin/clang-format-3.6"))
      terminal.clang_format_command="/usr/bin/clang-format-3.6";
    else if(boost::filesystem::exists("/usr/bin/clang-format-3.5"))
      terminal.clang_format_command="/usr/bin/clang-format-3.5";
  }
#endif
}

bool Config::add_missing_nodes(const boost::property_tree::ptree &default_cfg, std::string parent_path) {
  if(parent_path.size()>0)
    parent_path+=".";
  bool unchanged=true;
  for(auto &node: default_cfg) {
    auto path=parent_path+node.first;
    try {
      cfg.get<std::string>(path);
    }
    catch(const std::exception &e) {
      cfg.add(path, node.second.data());
      unchanged=false;
    }
    unchanged&=add_missing_nodes(node.second, path);
  }
  return unchanged;
}

bool Config::remove_deprecated_nodes(const boost::property_tree::ptree &default_cfg, boost::property_tree::ptree &config_cfg, std::string parent_path) {
  if(parent_path.size()>0)
    parent_path+=".";
  bool unchanged=true;
  for(auto &node: config_cfg) {
    auto path=parent_path+node.first;
    try {
      default_cfg.get<std::string>(path);
      unchanged&=remove_deprecated_nodes(default_cfg, node.second, path);
    }
    catch(const std::exception &e) {
      config_cfg.erase(node.first);
      unchanged=false;
    }
  }
  return unchanged;
}

void Config::update_config_file() {
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
  cfg_ok&=add_missing_nodes(default_cfg);
  cfg_ok&=remove_deprecated_nodes(default_cfg, cfg);
  if(!cfg_ok)
    boost::property_tree::write_json((home/"config"/"config.json").string(), cfg);
}

void Config::get_source() {
  auto source_json = cfg.get_child("source");

  source.style=source_json.get<std::string>("style");
  source.font=source_json.get<std::string>("font");

  source.cleanup_whitespace_characters=source_json.get<bool>("cleanup_whitespace_characters");
  source.show_whitespace_characters=source_json.get<std::string>("show_whitespace_characters");

  source.show_map = source_json.get<bool>("show_map");
  source.map_font_size = source_json.get<std::string>("map_font_size");

  source.spellcheck_language = source_json.get<std::string>("spellcheck_language");

  source.default_tab_char = source_json.get<char>("default_tab_char");
  source.default_tab_size = source_json.get<unsigned>("default_tab_size");
  source.auto_tab_char_and_size = source_json.get<bool>("auto_tab_char_and_size");

  source.wrap_lines = source_json.get<bool>("wrap_lines");

  source.highlight_current_line = source_json.get<bool>("highlight_current_line");
  source.show_line_numbers = source_json.get<bool>("show_line_numbers");

  for (auto &i : source_json.get_child("clang_types"))
    source.clang_types[i.first] = i.second.get_value<std::string>();
  
  source.clang_format_style = source_json.get<std::string>("clang_format_style");
  
  auto pt_doc_search=cfg.get_child("documentation_searches");
  for(auto &pt_doc_search_lang: pt_doc_search) {
    source.documentation_searches[pt_doc_search_lang.first].separator=pt_doc_search_lang.second.get<std::string>("separator");
    auto &queries=source.documentation_searches.find(pt_doc_search_lang.first)->second.queries;
    for(auto &i: pt_doc_search_lang.second.get_child("queries")) {
      queries[i.first]=i.second.get_value<std::string>();
    }
  }
}
