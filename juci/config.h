#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <fstream>
#include <string>
#include "keybindings.h"
#include "source.h"

class MainConfig {
public:
  MainConfig();
  const Source::Config& source_cfg();
  Keybindings::Config& keybindings_cfg();
  void PrintMenu();
  void GenerateSource();
  void GenerateKeybindings();
private:
  boost::property_tree::ptree cfg_;
  boost::property_tree::ptree key_tree_;
  Source::Config source_cfg_;
  Keybindings::Config keybindings_cfg_;
};
