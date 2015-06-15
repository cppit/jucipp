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
  MainConfig();
  Keybindings::Config& keybindings_cfg() { return keybindings_cfg_; }
  Directories::Config& dir_cfg() { return dir_cfg_; }
  Terminal::Config& terminal_cfg() { return terminal_cfg_; }
  void PrintMenu();
  void GenerateSource();
  void GenerateKeybindings();
  void GenerateDirectoryFilter();
  void GenerateTerminalCommands();
private:
  boost::property_tree::ptree cfg_;
  boost::property_tree::ptree key_tree_;
  Keybindings::Config keybindings_cfg_;
  Directories::Config dir_cfg_;
  Terminal::Config terminal_cfg_;
};
#endif
