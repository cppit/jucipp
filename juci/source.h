#ifndef JUCI_SOURCE_H_
#define JUCI_SOURCE_H_
#include <iostream>
#include <unordered_map>
#include <vector>
#include "gtkmm.h"
#include "clangmm.h"
#include <thread>
#include <mutex>
#include <string>
#include <atomic>
#include "gtksourceviewmm.h"

namespace Notebook {
  class Controller;
}

namespace Source {
  class Config {
  public:
    bool legal_extension(std::string e) const ;
    unsigned tab_size;
    bool show_line_numbers, highlight_current_line;
    std::string tab, background, font;
    std::vector<std::string> extensions;
    std::unordered_map<std::string, std::string> tags, types;
  };  // class Config

  class Location {
  public:
    Location(int line_number, int column_offset):
      line_number(line_number), column_offset(column_offset) {}
    int line_number;
    int column_offset;
  };

  class Range {
  public:
    Range(const Location &start, const Location &end, int kind):
      start(start), end(end), kind(kind) {}
    Location start;
    Location end;
    int kind;
  };

  class View : public Gsv::View {
  public:
    View();
    std::string get_line(size_t line_number);
    std::string get_line_before_insert();
  };  // class View

  class AutoCompleteChunk {
  public:
    explicit AutoCompleteChunk(const clang::CompletionChunk &clang_chunk) :
      chunk(clang_chunk.chunk()), kind(clang_chunk.kind()) { }
    std::string chunk;
    enum clang::CompletionChunkKind kind;
  };

  class AutoCompleteData {
  public:
    explicit AutoCompleteData(const std::vector<AutoCompleteChunk> &chunks) :
      chunks(chunks) { }
    std::vector<AutoCompleteChunk> chunks;
  };
  
  class Controller;

  class Parser{
  public:
    Parser(const std::vector<std::unique_ptr<Source::Controller> > &controllers):
      controllers(controllers) {}
    ~Parser();
    // inits the syntax highligthing on file open
    void init_syntax_highlighting(const std::string &filepath,
                                const std::string &project_path,
                                const std::map<std::string, std::string>
                                &buffers,
                                int start_offset,
                                int end_offset,
                                clang::Index *index);
    std::vector<Source::AutoCompleteData> get_autocomplete_suggestions(int line_number, int column);
    int reparse(const std::map<std::string, std::string> &buffers);
    std::vector<Range> extract_tokens(int, int);

    std::string file_path;
    std::string project_path;
    static clang::Index clang_index;
    std::map<std::string, std::string> get_buffer_map() const;
    std::mutex parsing_mutex;
  private:
    std::unique_ptr<clang::TranslationUnit> tu_; //use unique_ptr since it is not initialized in constructor
    void highlight_token(clang::Token *token,
                        std::vector<Range> *source_ranges,
                        int token_kind);
    void highlight_cursor(clang::Token *token,
                        std::vector<Range> *source_ranges);
    std::vector<std::string> get_compilation_commands();
    //controllers is needed here, no way around that I think
    const std::vector<std::unique_ptr<Source::Controller> > &controllers;
  };

  class Controller {
  public:
    Controller(const Source::Config &config,
               const std::vector<std::unique_ptr<Source::Controller> > &controllers);
    ~Controller();
    void update_syntax(const std::vector<Range> &locations);
    void on_new_empty_file();
    void on_open_file(const std::string &filename);
    Glib::RefPtr<Gsv::Buffer> buffer();
    bool on_key_press(GdkEventKey* key);
    
    bool is_saved = false; //TODO: Is never set to false in Notebook::Controller
    bool is_changed = false; //TODO: Is never set to true
    
    Parser parser;
    View view;
    
  private:
    Glib::Dispatcher parse_done;
    Glib::Dispatcher parse_start;
    std::thread parse_thread;
    std::map<std::string, std::string> parse_thread_buffer_map;
    std::mutex parse_thread_buffer_map_mutex;
    std::atomic<bool> parse_thread_go;
    std::atomic<bool> parse_thread_mapped;
    std::atomic<bool> parse_thread_stop;
    
    const Config& config;
  };  // class Controller
}  // namespace Source
#endif  // JUCI_SOURCE_H_
