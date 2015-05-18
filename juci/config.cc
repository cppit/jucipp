#include "config.h"
#include "logging.h"

MainConfig::MainConfig() :
  keybindings_cfg_(), source_cfg_() {
  boost::property_tree::json_parser::read_json("config.json", cfg_);
  
  GenerateSource();
  GenerateKeybindings();
  GenerateDirectoryFilter();
  GenerateTerminalCommands();
}

void MainConfig::GenerateSource() {
  DEBUG("Fetching source cfg");
  boost::property_tree::ptree source_json = cfg_.get_child("source");
  boost::property_tree::ptree syntax_json = source_json.get_child("syntax");
  boost::property_tree::ptree colors_json = source_json.get_child("colors");
  boost::property_tree::ptree extensions_json = source_json.get_child("extensions");
  for (auto &i : colors_json) {
    source_cfg_.InsertTag(i.first, i.second.get_value<std::string>());
  }
  for (auto &i : syntax_json) {
    source_cfg_.InsertType(i.first, i.second.get_value<std::string>());
  }
  for (auto &i : extensions_json) {
    source_cfg_.InsertExtension(i.second.get_value<std::string>());
  }
  DEBUG("Source cfg fetched");
}

void MainConfig::GenerateTerminalCommands() {
  boost::property_tree::ptree source_json = cfg_.get_child("project");
  boost::property_tree::ptree commands_json = source_json.get_child("compile_commands");
   for (auto &i : commands_json) {
     terminal_cfg_.InsertCompileCommand(i.second.get_value<std::string>());
  }
}

void MainConfig::GenerateKeybindings() {
  DEBUG("Fetching keybindings");
  std::string line;
  std::ifstream menu_xml("menu.xml");
  if (menu_xml.is_open()) {
    while (getline(menu_xml, line))
      keybindings_cfg_.AppendXml(line);   
  }
  boost::property_tree::ptree keys_json = cfg_.get_child("keybindings");
  for (auto &i : keys_json)
    keybindings_cfg_.key_map()[i.first] = i.second.get_value<std::string>();
  DEBUG("Keybindings fetched");
}

void MainConfig::GenerateDirectoryFilter() {
  DEBUG("Fetching directory filter");
  boost::property_tree::ptree dir_json = cfg_.get_child("directoryfilter");
  boost::property_tree::ptree ignore_json = dir_json.get_child("ignore");
  boost::property_tree::ptree except_json = dir_json.get_child("exceptions");
  for ( auto &i : except_json )
    dir_cfg_.AddException(i.second.get_value<std::string>());
  for ( auto &i : ignore_json )
    dir_cfg_.AddIgnore(i.second.get_value<std::string>());
  DEBUG("Directory filter fetched");
}
