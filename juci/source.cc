#include "source.h"
#include <iostream>
#include "sourcefile.h"
#include <boost/property_tree/json_parser.hpp>
#include <fstream>
#include <boost/timer/timer.hpp>

#define DBGVAR( os, var )                                       \
  (os) << "DBG: " << __FILE__ << "(" << __LINE__ << ") "        \
  << #var << " = [" << (var) << "]" << std::endl

Source::Location::
Location(int line_number, int column_offset) :
  line_number_(line_number), column_offset_(column_offset) { }

Source::Location::
Location(const Source::Location &org) :
  line_number_(org.line_number_), column_offset_(org.column_offset_) { }

Source::Range::
Range(const Location &start, const Location &end, int kind) :
  start_(start), end_(end), kind_(kind) { }

Source::Range::
Range(const Source::Range &org) :
  start_(org.start_), end_(org.end_), kind_(org.kind_) { }


//////////////
//// View ////
//////////////
Source::View::View() {
  override_font(Pango::FontDescription("Monospace"));
  DBGVAR(std::cout, "View Constructor");
}
// Source::View::UpdateLine
// returns the new line
string Source::View::UpdateLine() {
  Gtk::TextIter line(get_buffer()->get_insert()->get_iter());
  DBGVAR(std::cout, line.get_line());
  DBGVAR(std::cout, line.get_line_offset());
  return "";
}

string Source::View::GetLine(const Gtk::TextIter &begin) {
  Gtk::TextIter end(begin);
  while (!end.ends_line())
    end++;
  return begin.get_text(end);
}

// Source::View::ApplyTheme()
// Applies theme in textview
void Source::View::ApplyConfig(const Source::Config &config) {
  //  DBGVAR(std::cout, "Start of apply config");
  for (auto &item : config.tagtable())
    get_buffer()->create_tag(item.first)->property_foreground() = item.second;
  std::cout << "End of apply config" << std::endl;
}

void Source::View::OnOpenFile(const std::vector<Source::Range> &ranges,
                              const Source::Config &config) {
  ApplyConfig(config);
  Glib::RefPtr<Gtk::TextBuffer> buffer = get_buffer();
  for (auto &range : ranges) {
    string type = std::to_string(range.kind());
    try {
      config.typetable().at(type);
    } catch (std::exception) {
      continue;
    }

    int linum_start = range.start().line_number()-1;
    int linum_end = range.end().line_number()-1;

    int begin = range.start().column_offset()-1;
    int end = range.end().column_offset()-1;

    if (end < 0) end = 0;
    if (begin < 0) begin = 0;

    std::cout << "Libc: ";
    std::cout << "type: " << type;
    std::cout << " linum_s: " << linum_start;
    std::cout << " linum_e: " << linum_end;
    std::cout << ", begin: " << begin << ", end: " << end  << std::endl;

    Gtk::TextIter begin_iter = buffer->get_iter_at_line_offset(linum_start,
                                                               begin);
    Gtk::TextIter end_iter  = buffer->get_iter_at_line_offset(linum_end, end);
    std::cout << get_buffer()->get_text(begin_iter, end_iter) << std::endl;
    if (begin_iter.get_line() ==  end_iter.get_line()) {
      buffer->apply_tag_by_name(config.typetable().at(type),
                                begin_iter, end_iter);
    }
  }
  //  DBGVAR(std::cout, "End of OnOpenFile");
}


// Source::View::Config::Config(Config &config)
// copy-constructor
Source::Config::Config(const Source::Config &original) {
  //  DBGVAR(std::cout, "Start of Config constructor");
  SetTagTable(original.tagtable());
  SetTypeTable(original.typetable());
  //  DBGVAR(std::cout, "End of Config constructor");
}

Source::Config::Config() {}

// Source::View::Config::tagtable()
// returns a const refrence to the tagtable
const std::unordered_map<string, string>& Source::Config::tagtable() const {
  return tagtable_;
}

// Source::View::Config::tagtable()
// returns a const refrence to the tagtable
const std::unordered_map<string, string>& Source::Config::typetable() const {
  return typetable_;
}

void Source::Config::InsertTag(const string &key, const string &value) {
  tagtable_[key] = value;
}
// Source::View::Config::SetTagTable()
// sets the tagtable for the view
void Source::Config::
SetTypeTable(const std::unordered_map<string, string> &typetable) {
  typetable_ = typetable;
}

void Source::Config::InsertType(const string &key, const string &value) {
  typetable_[key] = value;
}
// Source::View::Config::SetTagTable()
// sets the tagtable for the view
void Source::Config::
SetTagTable(const std::unordered_map<string, string> &tagtable) {
  tagtable_ = tagtable;
}

///////////////
//// Model ////
///////////////
Source::Model::Model(const Source::Config &config) :
  config_(config) {
}

void Source::Model::
initSyntaxHighlighting(const std::string &filepath,
                       const std::string &project_path,
                       int start_offset,
                       int end_offset) {
  //  DBGVAR(std::cout, "Start of initsntax");
  set_file_path(filepath);
  set_project_path(project_path);
  std::vector<const char*> arguments = get_compilation_commands();
  clang::TranslationUnit tu(true, filepath, arguments);
  //  DBGVAR(std::cout, &tu);
  //  DBGVAR(std::cout, "Start of extract tokens");
  //  DBGVAR(std::cout, file_path());
  //  DBGVAR(std::cout, &tu);
  clang::SourceLocation start(&tu, file_path(), start_offset);
  //  DBGVAR(std::cout, "After start");
  clang::SourceLocation end(&tu, file_path(), end_offset);
  //  DBGVAR(std::cout, "After end");
  clang::SourceRange range(&start, &end);
  //  DBGVAR(std::cout, "After range");
  clang::Tokens tokens(&tu, &range);

  
  
  //  DBGVAR(std::cout, "After tokens");
  std::vector<clang::Token> tks = tokens.tokens();
  //  DBGVAR(std::cout, "After tokens tokens");
  //  DBGVAR(std::cout, "before for loop");
  for (auto &token : tks) {
    //    std::cout << token.kind() << std::endl;
    switch (token.kind()) {
      // case 0: PunctuationToken(&token); break;  // A tkn with punctation
      // case 1: KeywordToken(&token); break;  // A language keyword.
      // case 2: IdentifierToken(&token); break;  // An identifier, not a keyword
      // case 3: LiteralToken(&token); break;  // A num, string, or char literal
    case 4:
      //  std::cout << "Comment token" << std::endl;
      clang::SourceRange range = token.get_source_range(&tu);
      //  DBGVAR(std::cout, "etter range");
      unsigned begin_line_num, begin_offset, end_line_num, end_offset;
      clang::SourceLocation begin(&range, true);
      clang::SourceLocation end(&range, false);
      //  DBGVAR(std::cout, "Herre krasje etter begin");
      begin.get_location_info(NULL, &begin_line_num, &begin_offset, NULL);
      //  DBGVAR(std::cout, "etter utpressing av begin");
      end.get_location_info(NULL, &end_line_num, &end_offset, NULL);
      //  DBGVAR(std::cout, "etter utpressing av end");
      source_ranges_.emplace_back(Source::Location(begin_line_num, begin_offset),
                                  Source::Location(end_line_num, end_offset), 705);
      std::string k = token.get_token_spelling(&tu);
      break;  // A comment.
    }
  }
  //  DBGVAR(std::cout, "End of extract tokens");
  extractTokens(start_offset, end_offset);
  //  DBGVAR(std::cout, "End of of initsntax");
}

// sets the filepath for this mvc
void Source::Model::
set_file_path(const std::string &file_path) {
  file_path_ = file_path;
}
// sets the project path for this mvc
void Source::Model::
set_project_path(const std::string &project_path) {
  project_path_ = project_path;
}
// sets the ranges for keywords in this mvc
void Source::Model::
set_ranges(const std::vector<Source::Range> &source_ranges) {
  source_ranges_ = source_ranges;
}
// gets the file_path member
const std::string& Source::Model::file_path() const {
  return file_path_;
}
// gets the project_path member
const std::string& Source::Model::project_path() const {
  return project_path_;
}
// gets the ranges member
const std::vector<Source::Range>& Source::Model::source_ranges() const {
  return source_ranges_;
}
// gets the config member
const Source::Config& Source::Model::config() const {
  return config_;
}

std::vector<const char*> Source::Model::
get_compilation_commands() {
  //  DBGVAR(std::cout, "Start of get compilation commands");
  clang::CompilationDatabase db(project_path()+"/");
  clang::CompileCommands commands(file_path(), &db);
  std::vector<clang::CompileCommand> cmds = commands.get_commands();
  std::vector<const char*> arguments;
  for (auto &i : cmds) {
    std::vector<std::string> lol = i.get_command_as_args();
    for (int a = 1; a < lol.size()-4; a++) {
      arguments.emplace_back(lol[a].c_str());
    }
  }
  //  DBGVAR(std::cout, "End of get compilation commands");
  return arguments;
}

void Source::Model::
extractTokens(int start_offset, int end_offset) {
 
}

void Source::Model::LiteralToken(clang::Token *token) {
  std::cout << "Literal token" << std::endl;
}

void Source::Model::CommentToken(clang::Token *token) {
}

void Source::Model::IdentifierToken(clang::Token *token) {
  std::cout << "Identifyer token" << std::endl;
}

void Source::Model::KeywordToken(clang::Token *token) {
  std::cout << "Keyword token" << std::endl;
}

void Source::Model::PunctuationToken(clang::Token *token) {
  std::cout << "Punctuation token" << std::endl;
}

////////////////////
//// Controller ////
////////////////////

// Source::Controller::Controller()
// Constructor for Controller
Source::Controller::Controller(const Source::Config &config) :
  model_(config) {
  view().get_buffer()->signal_changed().connect([this](){
      this->OnLineEdit();
    });
}

// Source::Controller::view()
// return shared_ptr to the view
Source::View& Source::Controller::view() {
  return view_;
}
// Source::Controller::model()
// return shared_ptr to the model()
Source::Model& Source::Controller::model() {
  return model_;
}
// Source::Controller::OnLineEdit()
// fired when a line in the buffer is edited
void Source::Controller::OnLineEdit() {
  view().UpdateLine();
}

void Source::Controller::OnNewEmptyFile() {
  string filename("/tmp/juci_t");
  sourcefile s(filename);
  model().set_file_path(filename);
  model().set_project_path(filename);
  s.save("");
}

string extract_file_path(const std::string &file_path) {
  return file_path.substr(0, file_path.find_last_of('/'));
}

void Source::Controller::OnOpenFile(const string &filepath) {
  sourcefile s(filepath);
  buffer()->set_text(s.get_content());
  int start_offset = buffer()->begin().get_offset();
  int end_offset = buffer()->end().get_offset();
  model().initSyntaxHighlighting(filepath,
                                 extract_file_path(filepath),
                                 start_offset,
                                 end_offset);
  view().OnOpenFile(model().source_ranges(), model().config());
}

Glib::RefPtr<Gtk::TextBuffer> Source::Controller::buffer() {
  return view().get_buffer();
}
