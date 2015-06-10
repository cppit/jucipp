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
    Config(const Config &original);
    Config();
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
  private:
    std::unordered_map<std::string, std::string> tagtable_;
    std::unordered_map<std::string, std::string> typetable_;

    std::string background_;
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
    void ApplyConfig(const Config &config);
    void OnLineEdit(const std::vector<Range> &locations,
                    const Config &config);
    void OnUpdateSyntax(const std::vector<Range> &locations,
                        const Config &config);

  private:
    std::string GetLine(const Gtk::TextIter &begin);
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

  class Model{
  public:
    // constructor for Source::Model
    explicit Model(const Source::Config &config);
    // inits the syntax highligthing on file open
    void InitSyntaxHighlighting(const std::string &filepath,
                                const std::string &project_path,
                                const std::map<std::string, std::string>
                                &buffers,
                                int start_offset,
                                int end_offset,
                                clang::Index *index);
    // sets the filepath for this mvc
    void set_file_path(const std::string &file_path);
    // sets the project path for this mvc
    void set_project_path(const std::string &project_path);

    // gets the file_path member
    const std::string& file_path() const;
    // gets the project_path member
    const std::string& project_path() const;
    // gets the config member
    const Config& config() const;
    void GetAutoCompleteSuggestions(const std::map<std::string, std::string>
                                     &buffers,
                                     int line_number,
                                     int column,
                                     std::vector<AutoCompleteData>
                                     *suggestions);
    ~Model() { }
    int ReParse(const std::map<std::string, std::string> &buffers);
    std::vector<Range> ExtractTokens(int, int);

  private:
    Config config_;
    std::string file_path_;
    std::string project_path_;
    clang::TranslationUnit tu_;
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
               Notebook::Controller *notebook);
    Controller();
    View& view();
    Model& model();
    void OnNewEmptyFile();
    void OnOpenFile(const std::string &filename);
    void GetAutoCompleteSuggestions(int line_number,
                                    int column,
                                    std::vector<AutoCompleteData>
                                    *suggestions);
    Glib::RefPtr<Gsv::Buffer> buffer();
    bool is_saved() { return is_saved_; }
    bool is_changed() { return is_changed_; }
    std::string path() { return model().file_path(); }
    void set_is_saved(bool isSaved) { is_saved_ = isSaved; }
    void set_is_changed(bool isChanged) { is_changed_ = isChanged; }
    void set_file_path(std::string path) { model().set_file_path(path); }

  private:
    void OnLineEdit();
    void OnSaveFile();
    std::mutex parsing;
    Glib::Dispatcher parsing_done;
    bool is_saved_ = false;
    bool is_changed_ = false;

  protected:
    View view_;
    Model model_;
    Notebook::Controller *notebook_;
  };  // class Controller
}  // namespace Source
#endif  // JUCI_SOURCE_H_
