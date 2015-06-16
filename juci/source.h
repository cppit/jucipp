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
#include "gtksourceviewmm.h"

namespace Notebook {
  class Controller;
}

namespace Source {
  class Config {
  public:
    const std::unordered_map<std::string, std::string>& tagtable() const;
    const std::unordered_map<std::string, std::string>& typetable() const;
     std::vector<std::string>& extensiontable();
    void SetTagTable(const std::unordered_map<std::string, std::string>
                     &tagtable);
    void InsertTag(const std::string &key, const std::string &value);
    void SetTypeTable(const std::unordered_map<std::string, std::string>
                      &tagtable);
    void InsertType(const std::string &key, const std::string &value);
    void InsertExtension(const std::string &ext);
        std::vector<std::string> extensiontable_;
    // TODO: Have to clean away all the simple setter and getter methods at some point. It creates too much unnecessary code
    unsigned tab_size;
    bool show_line_numbers, highlight_current_line;
    std::string tab, background, font;
  private:
    std::unordered_map<std::string, std::string> tagtable_, typetable_;
  };  // class Config

  class Location {
  public:
    Location(const Location &location);
    Location(int line_number, int column_offset);
    int line_number() const { return line_number_; }
    int column_offset() const { return column_offset_; }
  private:
    int line_number_;
    int column_offset_;
  };

  class Range {
  public:
    Range(const Location &start, const Location &end, int kind);
    Range(const Range &org);
    const Location& start() const { return start_; }
    const Location& end() const { return end_; }
    int kind() const { return kind_; }
  private:
    Location start_;
    Location end_;
    int kind_;
  };

  class View : public Gsv::View {
  public:
    View();
    virtual ~View() { }
    void OnLineEdit(const std::vector<Range> &locations,
                    const Config &config);
    void OnUpdateSyntax(const std::vector<Range> &locations,
                        const Config &config);
    std::string GetLine(size_t line_number);
    std::string GetLineBeforeInsert();
  };  // class View

  class AutoCompleteChunk {
  public:
    explicit AutoCompleteChunk(const clang::CompletionChunk &chunk) :
      chunk_(chunk.chunk()), kind_(chunk.kind()) { }
    const std::string& chunk() const { return chunk_; }
    const clang::CompletionChunkKind& kind() const { return kind_; }
  private:
    std::string chunk_;
    enum clang::CompletionChunkKind kind_;
  };

  class AutoCompleteData {
  public:
    explicit AutoCompleteData(const std::vector<AutoCompleteChunk> &chunks) :
      chunks_(chunks) { }
    std::vector<AutoCompleteChunk> chunks_;
  };

  class Parser{
  public:
    // inits the syntax highligthing on file open
    void InitSyntaxHighlighting(const std::string &filepath,
                                const std::string &project_path,
                                const std::map<std::string, std::string>
                                &buffers,
                                int start_offset,
                                int end_offset,
                                clang::Index *index);
    void GetAutoCompleteSuggestions(const std::map<std::string, std::string>
                                     &buffers,
                                     int line_number,
                                     int column,
                                     std::vector<AutoCompleteData>
                                     *suggestions);
    int ReParse(const std::map<std::string, std::string> &buffers);
    std::vector<Range> ExtractTokens(int, int);

    std::string file_path;
    std::string project_path;
  private:
    std::unique_ptr<clang::TranslationUnit> tu_; //use unique_ptr since it is not initialized in constructor
    void HighlightToken(clang::Token *token,
                        std::vector<Range> *source_ranges,
                        int token_kind);
    void HighlightCursor(clang::Token *token,
                        std::vector<Range> *source_ranges);
    std::vector<std::string> get_compilation_commands();
  };

  class Controller {
  public:
    Controller(const Source::Config &config,
               Notebook::Controller &notebook);
    Controller();
    ~Controller();
    void OnNewEmptyFile();
    void OnOpenFile(const std::string &filename);
    void GetAutoCompleteSuggestions(int line_number,
                                    int column,
                                    std::vector<AutoCompleteData>
                                    *suggestions);
    Glib::RefPtr<Gsv::Buffer> buffer();
    bool OnKeyPress(GdkEventKey* key);
    
    bool is_saved = false; //TODO: Is never set to false in Notebook::Controller
    bool is_changed = false; //TODO: Is never set to true
    
    Parser parser;
    View view;
    
  private:
    void OnLineEdit();
    void OnSaveFile();
    std::mutex parsing;
    Glib::Dispatcher parsing_done;
    
    const Config& config;
    Notebook::Controller& notebook; //TODO: should maybe be const, but that involves a small change in libclangmm
  };  // class Controller
}  // namespace Source
#endif  // JUCI_SOURCE_H_
