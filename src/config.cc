#include "config.h"
#include <exception>
#include "files.h"
#include <iostream>
#include "filesystem.h"
#include "terminal.h"
#include <algorithm>

Config::Config() {
  home_path=filesystem::get_home_path();
  if(home_path.empty())
    throw std::runtime_error("Could not find home path");
  home_juci_path=home_path/".juci";
}

void Config::load() {
  auto config_json = (home_juci_path/"config"/"config.json").string(); // This causes some redundant copies, but assures windows support
  boost::property_tree::ptree cfg;
  try {
    find_or_create_config_files();
    boost::property_tree::json_parser::read_json(config_json, cfg);
    update(cfg);
    read(cfg);
  }
  catch(const std::exception &e) {
    dispatcher.post([config_json, e_what=std::string(e.what())] {
      ::Terminal::get().print("Error: could not parse "+config_json+": "+e_what+"\n", true);
    });
    std::stringstream ss;
    ss << default_config_file;
    boost::property_tree::read_json(ss, cfg);
    read(cfg);
  }
}

void Config::find_or_create_config_files() {
  auto config_dir = home_juci_path/"config";
  auto config_json = config_dir/"config.json";

  boost::filesystem::create_directories(config_dir); // io exp captured by calling method

  if (!boost::filesystem::exists(config_json))
    filesystem::write(config_json, default_config_file);

  auto juci_style_path = home_juci_path/"styles";
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

void Config::update(boost::property_tree::ptree &cfg) {
  boost::property_tree::ptree default_cfg;
  bool cfg_ok=true;
  if(cfg.get<std::string>("version")!=JUCI_VERSION) {
    std::stringstream ss;
    ss << default_config_file;
    boost::property_tree::read_json(ss, default_cfg);
    cfg_ok=false;
    auto it_version=cfg.find("version");
    if(it_version!=cfg.not_found()) {
      make_version_dependent_corrections(cfg, default_cfg, it_version->second.data());
      it_version->second.data()=JUCI_VERSION;
    }
    
    auto style_path=home_juci_path/"styles";
    filesystem::write(style_path/"juci-light.xml", juci_light_style);
    filesystem::write(style_path/"juci-dark.xml", juci_dark_style);
    filesystem::write(style_path/"juci-dark-blue.xml", juci_dark_blue_style);
  }
  else
    return;
  cfg_ok&=add_missing_nodes(cfg, default_cfg);
  cfg_ok&=remove_deprecated_nodes(cfg, default_cfg);
  if(!cfg_ok)
    boost::property_tree::write_json((home_juci_path/"config"/"config.json").string(), cfg);
}

void Config::make_version_dependent_corrections(boost::property_tree::ptree &cfg, const boost::property_tree::ptree &default_cfg, const std::string &version) {
  auto &keybindings_cfg=cfg.get_child("keybindings");
  try {
    if(version<="1.2.4") {
      auto it_file_print=keybindings_cfg.find("print");
      if(it_file_print!=keybindings_cfg.not_found() && it_file_print->second.data()=="<primary>p") {
        dispatcher.post([] {
          ::Terminal::get().print("Preference change: keybindings.print set to \"\"\n");
        });
        it_file_print->second.data()="";
      }
    }
  }
  catch(const std::exception &e) {
    std::cerr << "Error correcting preferences: " << e.what() << std::endl;
  }
}

bool Config::add_missing_nodes(boost::property_tree::ptree &cfg, const boost::property_tree::ptree &default_cfg, std::string parent_path) {
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
    unchanged&=add_missing_nodes(cfg, node.second, path);
  }
  return unchanged;
}

bool Config::remove_deprecated_nodes(boost::property_tree::ptree &cfg, const boost::property_tree::ptree &default_cfg, std::string parent_path) {
  if(parent_path.size()>0)
    parent_path+=".";
  bool unchanged=true;
  for(auto it=cfg.begin();it!=cfg.end();) {
    auto path=parent_path+it->first;
    try {
      default_cfg.get<std::string>(path);
      unchanged&=remove_deprecated_nodes(it->second, default_cfg, path);
      ++it;
    }
    catch(const std::exception &e) {
      it=cfg.erase(it);
      unchanged=false;
    }
  }
  return unchanged;
}

void Config::read(const boost::property_tree::ptree &cfg) {
  auto keybindings_pt = cfg.get_child("keybindings");
  for (auto &i : keybindings_pt) {
    menu.keys[i.first] = i.second.get_value<std::string>();
  }
  
  auto source_json = cfg.get_child("source");
  source.style=source_json.get<std::string>("style");
  source.font=source_json.get<std::string>("font");
  source.cleanup_whitespace_characters=source_json.get<bool>("cleanup_whitespace_characters");
  source.show_whitespace_characters=source_json.get<std::string>("show_whitespace_characters");
  source.format_style_on_save=source_json.get<bool>("format_style_on_save");
  source.format_style_on_save_if_style_file_found=source_json.get<bool>("format_style_on_save_if_style_file_found");
  source.smart_brackets=source_json.get<bool>("smart_brackets");
  source.smart_inserts=source_json.get<bool>("smart_inserts");
  if(source.smart_inserts)
    source.smart_brackets=true;
  source.show_map = source_json.get<bool>("show_map");
  source.map_font_size = source_json.get<std::string>("map_font_size");
  source.show_git_diff = source_json.get<bool>("show_git_diff");
  source.show_background_pattern = source_json.get<bool>("show_background_pattern");
  source.show_right_margin = source_json.get<bool>("show_right_margin");
  source.right_margin_position = source_json.get<unsigned>("right_margin_position");
  source.spellcheck_language = source_json.get<std::string>("spellcheck_language");
  source.default_tab_char = source_json.get<char>("default_tab_char");
  source.default_tab_size = source_json.get<unsigned>("default_tab_size");
  source.auto_tab_char_and_size = source_json.get<bool>("auto_tab_char_and_size");
  source.tab_indents_line = source_json.get<bool>("tab_indents_line");
  source.wrap_lines = source_json.get<bool>("wrap_lines");
  source.highlight_current_line = source_json.get<bool>("highlight_current_line");
  source.show_line_numbers = source_json.get<bool>("show_line_numbers");
  source.enable_multiple_cursors = source_json.get<bool>("enable_multiple_cursors");
  source.auto_reload_changed_files = source_json.get<bool>("auto_reload_changed_files");
  source.clang_format_style = source_json.get<std::string>("clang_format_style");
  source.clang_usages_threads = static_cast<unsigned>(source_json.get<int>("clang_usages_threads"));
  auto pt_doc_search=cfg.get_child("documentation_searches");
  for(auto &pt_doc_search_lang: pt_doc_search) {
    source.documentation_searches[pt_doc_search_lang.first].separator=pt_doc_search_lang.second.get<std::string>("separator");
    auto &queries=source.documentation_searches.find(pt_doc_search_lang.first)->second.queries;
    for(auto &i: pt_doc_search_lang.second.get_child("queries")) {
      queries[i.first]=i.second.get_value<std::string>();
    }
  }

  window.theme_name=cfg.get<std::string>("gtk_theme.name");
  window.theme_variant=cfg.get<std::string>("gtk_theme.variant");
  window.version = cfg.get<std::string>("version");
  
  project.default_build_path=cfg.get<std::string>("project.default_build_path");
  project.debug_build_path=cfg.get<std::string>("project.debug_build_path");
  project.cmake.command=cfg.get<std::string>("project.cmake.command");
  project.cmake.compile_command=cfg.get<std::string>("project.cmake.compile_command");
  project.meson.command=cfg.get<std::string>("project.meson.command");
  project.meson.compile_command=cfg.get<std::string>("project.meson.compile_command");
  project.save_on_compile_or_run=cfg.get<bool>("project.save_on_compile_or_run");
  project.clear_terminal_on_compile=cfg.get<bool>("project.clear_terminal_on_compile");
  project.ctags_command=cfg.get<std::string>("project.ctags_command");
  project.python_command=cfg.get<std::string>("project.python_command");
  
  terminal.history_size=cfg.get<int>("terminal.history_size");
  terminal.font=cfg.get<std::string>("terminal.font");
}
