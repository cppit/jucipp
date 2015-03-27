#ifndef JUCI_SOURCE_H_
#define JUCI_SOURCE_H_
#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>
#include "gtkmm.h"
#include "clangmm.h"

using std::string;

namespace Source {

  class Config {
  public:
    Config(const Config &original);
    Config();
    const std::unordered_map<string, string>& tagtable() const;
    const std::unordered_map<string, string>& typetable() const;
    void SetTagTable(const std::unordered_map<string, string> &tagtable);
    void InsertTag(const string &key, const string &value);
    void SetTypeTable(const std::unordered_map<string, string> &tagtable);
    void InsertType(const string &key, const string &value);

  private:
    std::unordered_map<string, string> tagtable_;
    std::unordered_map<string, string> typetable_;
    string background_;
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

  class View : public Gtk::TextView {
  public:
    View();
    string UpdateLine();
    void ApplyConfig(const Config &config);
    void OnOpenFile(const std::vector<Range> &locations,
                    const Config &config);
  private:
    string GetLine(const Gtk::TextIter &begin);
  };  // class View

  class Model{
  public:
    // constructor for Source::Model
    explicit Model(const Source::Config &config);
    // inits the syntax highligthing on file open
    void initSyntaxHighlighting(const std::string &filepath,
                           const std::string &project_path,
                           int start_offset,
                           int end_offset);
    // sets the filepath for this mvc
    void set_file_path(const string &file_path);
    // sets the project path for this mvc
    void set_project_path(const string &project_path);
    // sets the ranges for keywords in this mvc
    void set_ranges(const std::vector<Range> &ranges);

    // gets the file_path member
    const string& file_path() const;
    // gets the project_path member
    const string& project_path() const;
    // gets the ranges member
    const std::vector<Range>& source_ranges() const;
    // gets the config member
    const Config& config() const;
    ~Model() {
      std::cout << "SNOUTHN" << std::endl;
    }
  private:
    Source::Config config_;
    string file_path_;
    string project_path_;
    std::vector<Range> source_ranges_;
    clang::TranslationUnit tu_;
    void extractTokens(int, int);
    void LiteralToken(clang::Token *token);
    void CommentToken(clang::Token *token);
    void IdentifierToken(clang::Token *token);
    void KeywordToken(clang::Token *token);
    void PunctuationToken(clang::Token *token);
    std::vector<const char*> get_compilation_commands();
  };

  class Controller {
  public:
    explicit Controller(const Source::Config &config);
    Controller();
    View& view();
    Model& model();
    void OnNewEmptyFile();
    void OnOpenFile(const string &filename);
    Glib::RefPtr<Gtk::TextBuffer> buffer();

  private:
    void OnLineEdit();
    void OnSaveFile();

  protected:
    View view_;
    Model model_;
  };  // class Controller
}  // namespace Source
#endif  // JUCI_SOURCE_H_
