#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <fstream>
#include "source.h"
#include "keybindings.h"
  class MainConfig {
  public: 
    MainConfig();
    const Source::Config& source_cfg() {return source_cfg_; }
    const Keybindings::Config& keybindings_cfg() {return keybindings_cfg_; }

    void PrintMenu();
    //boost::property_tree::ptree& source_cfg();
    //boost::property_tree::ptree& keybindings_cfg();
    //boost::property_tree::ptree& notebookk_cfg();
    //boost::property_tree::ptree& menu_cfg();
  
    boost::property_tree::ptree cfg_;
    // boost::property_tree::ptree sourcecfg_;
    // boost::property_tree::ptree keybindings_cfg_;
    // boost::property_tree::ptree notebook_cfg_;
    // boost::property_tree::ptree menu_cfg_;

    Source::Config source_cfg_;
    Keybindings::Config keybindings_cfg_;
    
    void GenerateSource();
    void GenerateKeybindings();
  };

