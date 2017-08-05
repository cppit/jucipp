#ifndef JUCI_CONFIG_H_
#define JUCI_CONFIG_H_
#include <boost/property_tree/json_parser.hpp>
#include <boost/filesystem.hpp>
#include <unordered_map>
#include <string>
#include <utility>
#include <vector>
#include "dispatcher.h"

class Config {
public:
  class Menu {
  public:
    std::unordered_map<std::string, std::string> keys;
  };
  
  class Window {
  public:
    std::string theme_name;
    std::string theme_variant;
    std::string version;
    std::pair<int, int> default_size;
  };
  
  class Terminal {
  public:
    std::string clang_format_command;
    int history_size;
    std::string font;
    bool show_progress;
  };
  
  class Project {
  public:
    class CMake {
    public:
      std::string command;
      std::string compile_command;
    };
    class Meson {
    public:
      std::string command;
      std::string compile_command;
    };
    
    std::string default_build_path;
    std::string debug_build_path;
    CMake cmake;
    Meson meson;
    bool save_on_compile_or_run;
    bool clear_terminal_on_compile;
    std::string ctags_command;
  };
  
  class Source {
  public:
    class DocumentationSearch {
    public:
      std::string separator;
      std::unordered_map<std::string, std::string> queries;
    };
    
    std::string style;
    std::string font;
    std::string spellcheck_language;
    
    bool cleanup_whitespace_characters;
    std::string show_whitespace_characters;
    
    bool format_style_on_save;
    bool format_style_on_save_if_style_file_found;
    
    bool smart_brackets;
    bool smart_inserts;
    
    bool show_map;
    std::string map_font_size;
    bool show_git_diff;
    bool show_background_pattern;
    
    bool auto_tab_char_and_size;
    char default_tab_char;
    unsigned default_tab_size;
    bool tab_indents_line;
    bool wrap_lines;
    bool highlight_current_line;
    bool show_line_numbers;
    std::unordered_map<int, std::string> clang_types;
    std::string clang_format_style;
    
    std::unordered_map<std::string, DocumentationSearch> documentation_searches;
  };
private:
  Config();
public:
  static Config &get() {
    static Config singleton;
    return singleton;
  }
  
  void load();
  
  Menu menu;
  Window window;
  Terminal terminal;
  Project project;
  Source source;
  
  boost::filesystem::path home_path;
  boost::filesystem::path home_juci_path;

private:
  /// Used to dispatch Terminal outputs after juCi++ GUI setup and configuration
  Dispatcher dispatcher;
  
  void find_or_create_config_files();
  void update(boost::property_tree::ptree &cfg);
  void make_version_dependent_corrections(boost::property_tree::ptree &cfg, const boost::property_tree::ptree &default_cfg, const std::string &version);
  bool add_missing_nodes(boost::property_tree::ptree &cfg, const boost::property_tree::ptree &default_cfg, std::string parent_path="");
  bool remove_deprecated_nodes(boost::property_tree::ptree &cfg, const boost::property_tree::ptree &default_cfg, std::string parent_path="");
  void read(const boost::property_tree::ptree &cfg);
};
#endif
