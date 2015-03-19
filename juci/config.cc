#include "config.h"

MainConfig::MainConfig() {
  boost::property_tree::json_parser::read_json("config.json", cfg_);
  GenerateSource();
  GenerateKeybindings();
    
    //    keybindings_cfg_ = cfg_.get_child("keybindings");
    //    notebook_cfg_ = cfg_.get_child("notebook");
    //    menu_cfg_ = cfg_.get_child("menu");
}

void MainConfig::GenerateSource(){
  boost::property_tree::ptree source_json = cfg_.get_child("source");
  boost::property_tree::ptree syntax_json = source_json.get_child("syntax");
  boost::property_tree::ptree colors_json = source_json.get_child("colors");
  for ( auto &i : syntax_json )
    source_cfg_.InsertTag(i.first, i.second.get_value<std::string>());

  for ( auto &i : colors_json )
    source_cfg_.InsertType(i.first, i.second.get_value<std::string>());	    
}

void MainConfig::GenerateKeybindings(){
}
