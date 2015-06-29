#ifndef JUCI_CONFIG_H_
#define JUCI_CONFIG_H_
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <fstream>
#include <string>
#include "keybindings.h"
#include "source.h"
#include "directories.h"
#include "terminal.h"

class MainConfig {
public:
  Source::Config source_cfg;
  Terminal::Config terminal_cfg;
  Keybindings::Config keybindings_cfg;
  Directories::Config dir_cfg;
  MainConfig();
  void PrintMenu();
  void GenerateSource();
  void GenerateKeybindings();
  void GenerateDirectoryFilter();
  void GenerateTerminalCommands();
private:
  boost::property_tree::ptree cfg_;
  boost::property_tree::ptree key_tree_;
};
#endif
