#include "singletons.h"
#include "config.h"
#include "logging.h"
#include <fstream>
#include <string>

MainConfig::MainConfig() {
  INFO("Reading config file");
  std::string path(getenv("HOME")); // TODO WINDOWS
  boost::property_tree::json_parser::read_json(path + "/.juci/config/config.json", cfg_);
  INFO("Config file read");
  GenerateSource();
  GenerateKeybindings();
  GenerateDirectoryFilter();
  GenerateTerminalCommands();
}

void MainConfig::GenerateSource() {
  auto source_cfg=Singleton::Config::source();
  DEBUG("Fetching source cfg");
  // boost::property_tree::ptree
  auto source_json = cfg_.get_child("source");
  auto syntax_json = source_json.get_child("syntax");
  auto colors_json = source_json.get_child("colors");
  auto extensions_json = source_json.get_child("extensions");
  auto visual_json = source_json.get_child("visual");
  for (auto &i : visual_json) {
    if (i.first == "background") {
	     source_cfg->background = i.second.get_value<std::string>();
    }
    else if (i.first == "background_selected") {
       source_cfg->background_selected = i.second.get_value<std::string>();
    }
    else if (i.first == "background_tooltips") {
	     source_cfg->background_tooltips = i.second.get_value<std::string>();
    }
    else if (i.first == "show_line_numbers") {
      source_cfg->show_line_numbers = i.second.get_value<std::string>() == "1" ? true : false;
    }
    else if (i.first == "highlight_current_line") {
      source_cfg->highlight_current_line = i.second.get_value<std::string>() == "1" ? true : false;
    }
    else if (i.first == "font") {
      source_cfg->font = i.second.get_value<std::string>();
    }
  }
  source_cfg->tab_size = source_json.get<unsigned>("tab_size");
  for (unsigned c = 0; c < source_cfg->tab_size; c++) {
    source_cfg->tab+=" ";
  }
  for (auto &i : colors_json) {
    source_cfg->tags[i.first]=i.second.get_value<std::string>();
  }
  for (auto &i : syntax_json) {
    source_cfg->types[i.first]=i.second.get_value<std::string>();
  }
  for (auto &i : extensions_json) {
    source_cfg->extensions.emplace_back(i.second.get_value<std::string>());
  }
  DEBUG("Source cfg fetched");
}

void MainConfig::GenerateTerminalCommands() {
  auto terminal_cfg=Singleton::Config::terminal();
  boost::property_tree::ptree source_json = cfg_.get_child("project");
  boost::property_tree::ptree compile_commands_json = source_json.get_child("compile_commands");
  boost::property_tree::ptree run_commands_json = source_json.get_child("run_commands");
  for (auto &i : compile_commands_json) {
    terminal_cfg->compile_commands.emplace_back(i.second.get_value<std::string>());
  }
  for (auto &i : run_commands_json) {
    terminal_cfg->run_command=(i.second.get_value<std::string>()); //TODO: run_commands array->one run_command?
  }
}

void MainConfig::GenerateKeybindings() {
  auto menu=Singleton::menu();
  DEBUG("Fetching keybindings");
  std::string line;
  std::ifstream menu_xml("menu.xml");
  if (menu_xml.is_open()) {
    while (getline(menu_xml, line))
      menu->ui+=line;   
  }
  boost::property_tree::ptree keys_json = cfg_.get_child("keybindings");
  for (auto &i : keys_json) {
    auto key=i.second.get_value<std::string>();
    menu->key_map[i.first] = key;
  }
  DEBUG("Keybindings fetched");
}

void MainConfig::GenerateDirectoryFilter() {
  auto dir_cfg=Singleton::Config::directories();
  DEBUG("Fetching directory filter");
  boost::property_tree::ptree dir_json = cfg_.get_child("directoryfilter");
  boost::property_tree::ptree ignore_json = dir_json.get_child("ignore");
  boost::property_tree::ptree except_json = dir_json.get_child("exceptions");
  for ( auto &i : except_json )
    dir_cfg->AddException(i.second.get_value<std::string>());
  for ( auto &i : ignore_json )
    dir_cfg->AddIgnore(i.second.get_value<std::string>());
  DEBUG("Directory filter fetched");
}
