#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <fstream>
#include <string>
#include "keybindings.h"
#include "source.h"
#include "directories.h"

class MainConfig {
public:
  MainConfig();
  Source::Config& source_cfg() { return source_cfg_; }
  Keybindings::Config& keybindings_cfg() { return keybindings_cfg_; }
  Directories::Config& dir_cfg() { return dir_cfg_; } 
  void PrintMenu();
  void GenerateSource();
  void GenerateKeybindings();
  void GenerateDirectoryFilter();
private:
  boost::property_tree::ptree cfg_;
  boost::property_tree::ptree key_tree_;
  Source::Config source_cfg_;
  Keybindings::Config keybindings_cfg_;
  Directories::Config dir_cfg_;
};
