#include "source.h"
#include <iostream>
#include "sourcefile.h"
#include <boost/property_tree/json_parser.hpp>
#include <fstream>
#include <boost/timer/timer.hpp>
#include <thread>

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
  for (auto &item : config.tagtable()) {
    if (get_buffer()->get_tag_table()->lookup(item.first) == 0)
      get_buffer()->create_tag(item.first)->property_foreground() = item.second;
  }
}

void Source::View::OnUpdateSyntax(const std::vector<Source::Range> &ranges,
                              const Source::Config &config) {
  get_buffer()->remove_all_tags(get_buffer()->begin(),
                                get_buffer()->end());
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

    Gtk::TextIter begin_iter = buffer->get_iter_at_line_offset(linum_start,
                                                               begin);
    Gtk::TextIter end_iter  = buffer->get_iter_at_line_offset(linum_end, end);
    if (begin_iter.get_line() ==  end_iter.get_line()) {
      buffer->apply_tag_by_name(config.typetable().at(type),
                                begin_iter, end_iter);
    }
  }
}


// Source::View::Config::Config(Config &config)
// copy-constructor
Source::Config::Config(const Source::Config &original) {
  SetTagTable(original.tagtable());
  SetTypeTable(original.typetable());
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
                       const std::string &text,
                       int start_offset,
                       int end_offset) {
  set_file_path(filepath);
  set_project_path(project_path);
  std::vector<const char*> arguments = get_compilation_commands();
  tu_ = clang::TranslationUnit(true,
                               filepath,
                               arguments,
                               text);
  extractTokens(start_offset, end_offset);
}

// Source::View::UpdateLine
void Source::View::OnLineEdit(const std::vector<Source::Range> &locations,
                              const Source::Config &config) {
  OnUpdateSyntax(locations, config);
}

// Source::Model::UpdateLine
void Source::Model::OnLineEdit(const std::string &buffer) {
  int i = tu_.ReparseTranslationUnit(file_path(), buffer);
  set_ranges(std::vector<Source::Range>());
  extractTokens(0, buffer.size());
}


// Source::Controller::OnLineEdit()
// fired when a line in the buffer is edited
void Source::Controller::OnLineEdit() {
  model().OnLineEdit(buffer()->get_text());
  view().OnLineEdit(model().source_ranges(),
                    model().config());
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
  return arguments;
}

void Source::Model::
extractTokens(int start_offset, int end_offset) {
  clang::SourceLocation start(&tu_, file_path(), start_offset);
  clang::SourceLocation end(&tu_, file_path(), end_offset);
  clang::SourceRange range(&start, &end);
  clang::Tokens tokens(&tu_, &range);
  std::vector<clang::Token> tks = tokens.tokens();
  for (auto &token : tks) {
    switch (token.kind()) {
    case 0: PunctuationToken(&token); break;   // A tkn with punctation
    case 1: KeywordToken(&token); break;   // A language keyword.
    case 2: IdentifierToken(&token); break;  // An identifier, not a keyword
    case 3: LiteralToken(&token); break;  // A num, string, or char literal
    case 4: CommentToken(&token); break;  // A comment token
    }
  }
}

void Source::Model::LiteralToken(clang::Token *token) {
  HighlightToken(token, 109);;
}

void Source::Model::CommentToken(clang::Token *token) {
  HighlightToken(token, 705);
}

void Source::Model::IdentifierToken(clang::Token *token) {
  
}

void Source::Model::KeywordToken(clang::Token *token) {
  HighlightToken(token, 702);;
}

void Source::Model::PunctuationToken(clang::Token *token) {
}


void Source::Model::HighlightToken(clang::Token *token, int token_kind) {
  clang::SourceRange range = token->get_source_range(&tu_);
  unsigned begin_line_num, begin_offset, end_line_num, end_offset;
  clang::SourceLocation begin(&range, true);
  clang::SourceLocation end(&range, false);
  begin.get_location_info(NULL, &begin_line_num, &begin_offset, NULL);
  end.get_location_info(NULL, &end_line_num, &end_offset, NULL);
  source_ranges_.emplace_back(Source::Location(begin_line_num,
                                               begin_offset),
                              Source::Location(end_line_num,
                                               end_offset), token_kind);
}

////////////////////
//// Controller ////
////////////////////

// Source::Controller::Controller()
// Constructor for Controller
Source::Controller::Controller(const Source::Config &config) :
  model_(config) {
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

std::vector<std::string> extentions() {
  return {".h", ".cc", ".cpp", ".hpp"};
}

bool check_extention(const std::string &file_path) {
  std::string extention = file_path.substr(file_path.find_last_of('.'),
                                           file_path.size());
  for (auto &ex : extentions()) {
    if (extention == ex) {
      return true;
    }
  }
  return false;
}

void Source::Controller::OnOpenFile(const string &filepath) {
  sourcefile s(filepath);
  buffer()->set_text(s.get_content());
  int start_offset = buffer()->begin().get_offset();
  int end_offset = buffer()->end().get_offset();

  if (check_extention(filepath)) {
    model().initSyntaxHighlighting(filepath,
                                   extract_file_path(filepath),
                                   buffer()->get_text(),
                                   start_offset,
                                   end_offset);
    view().OnUpdateSyntax(model().source_ranges(), model().config());
  }
  view().get_buffer()->signal_changed().connect([this]() {
            OnLineEdit();
    });
}

Glib::RefPtr<Gtk::TextBuffer> Source::Controller::buffer() {
  return view().get_buffer();
}
