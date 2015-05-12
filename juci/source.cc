#include "source.h"
#include <iostream>
#include "sourcefile.h"
#include <boost/property_tree/json_parser.hpp>
#include <fstream>
#include <boost/timer/timer.hpp>
#include "notebook.h"
#include "logging.h"

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
    get_buffer()->create_tag(item.first)->property_foreground() = item.second;
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
InitSyntaxHighlighting(const std::string &filepath,
                       const std::string &project_path,
                       const std::map<std::string, std::string>
                       &buffers,
                       int start_offset,
                       int end_offset,
                       clang::Index *index) {
  set_project_path(project_path);
  std::vector<const char*> arguments = get_compilation_commands();
  tu_ = clang::TranslationUnit(index,
                               filepath,
                               arguments,
                               buffers);
}

// Source::View::UpdateLine
void Source::View::
OnLineEdit(const std::vector<Source::Range> &locations,
           const Source::Config &config) {
  OnUpdateSyntax(locations, config);
}

// Source::Model::UpdateLine
int Source::Model::
ReParse(const std::map<std::string, std::string> &buffer) {
  return tu_.ReparseTranslationUnit(file_path(), buffer);
}


// Source::Controller::OnLineEdit()
// fired when a line in the buffer is edited
void Source::Controller::OnLineEdit() { }

void Source::Controller::
GetAutoCompleteSuggestions(int line_number,
                           int column,
                           std::vector<Source::AutoCompleteData>
                           *suggestions) {
  parsing.lock();
  std::map<std::string, std::string> buffers;
  notebook_->MapBuffers(&buffers);
  model().GetAutoCompleteSuggestions(buffers,
                                     line_number,
                                     column,
                                     suggestions);
  parsing.unlock();
}

void Source::Model::
GetAutoCompleteSuggestions(const std::map<std::string, std::string> &buffers,
                           int line_number,
                           int column,
                           std::vector<Source::AutoCompleteData>
                           *suggestions) {
  clang::CodeCompleteResults results(&tu_,
                                     file_path(),
                                     buffers,
                                     line_number,
                                     column);
  for (int i = 0; i < results.size(); i++) {
    const vector<clang::CompletionChunk> chunks_ = results.get(i).get_chunks();
    std::vector<AutoCompleteChunk> chunks;
    for (auto &chunk : chunks_) {
      chunks.emplace_back(chunk);
    }
    suggestions->emplace_back(chunks);
  }
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
// gets the file_path member
const std::string& Source::Model::file_path() const {
  return file_path_;
}
// gets the project_path member
const std::string& Source::Model::project_path() const {
  return project_path_;
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

std::vector<Source::Range> Source::Model::
ExtractTokens(int start_offset, int end_offset) {
  std::vector<Source::Range> ranges;
  clang::SourceLocation start(&tu_, file_path(), start_offset);
  clang::SourceLocation end(&tu_, file_path(), end_offset);
  clang::SourceRange range(&start, &end);
  clang::Tokens tokens(&tu_, &range);
  std::vector<clang::Token> tks = tokens.tokens();
  for (auto &token : tks) {
    switch (token.kind()) {
    case 0: HighlightCursor(&token, &ranges); break;  // PunctuationToken
    case 1: HighlightToken(&token, &ranges, 702); break;  // KeywordToken
    case 2: HighlightCursor(&token, &ranges); break;  // IdentifierToken
    case 3: HighlightToken(&token, &ranges, 109); break;  // LiteralToken
    case 4: HighlightToken(&token, &ranges, 705); break;  // CommentToken
    }
  }
  return ranges;
}

void Source::Model::
HighlightCursor(clang::Token *token,
                std::vector<Source::Range> *source_ranges) {
  clang::SourceLocation location = token->get_source_location(&tu_);
  clang::Cursor cursor(&tu_, &location);
  clang::SourceRange range(&cursor);
  clang::SourceLocation begin(&range, true);
  clang::SourceLocation end(&range, false);
  unsigned begin_line_num, begin_offset, end_line_num, end_offset;
  begin.get_location_info(NULL, &begin_line_num, &begin_offset, NULL);
  end.get_location_info(NULL, &end_line_num, &end_offset, NULL);
  source_ranges->emplace_back(Source::Location(begin_line_num,
                                               begin_offset),
                              Source::Location(end_line_num,
                                               end_offset), (int) cursor.kind());
}
void Source::Model::
HighlightToken(clang::Token *token,
               std::vector<Source::Range> *source_ranges,
               int token_kind) {
  clang::SourceRange range = token->get_source_range(&tu_);
  unsigned begin_line_num, begin_offset, end_line_num, end_offset;
  clang::SourceLocation begin(&range, true);
  clang::SourceLocation end(&range, false);
  begin.get_location_info(NULL, &begin_line_num, &begin_offset, NULL);
  end.get_location_info(NULL, &end_line_num, &end_offset, NULL);
  source_ranges->emplace_back(Source::Location(begin_line_num,
                                               begin_offset),
                              Source::Location(end_line_num,
                                               end_offset), token_kind);
}

////////////////////
//// Controller ////
////////////////////

// Source::Controller::Controller()
// Constructor for Controller
Source::Controller::Controller(const Source::Config &config,
                               Notebook::Controller *notebook) :
  model_(config), notebook_(notebook) {
  INFO("Source Controller with childs constructed");
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

void Source::View::OnUpdateSyntax(const std::vector<Source::Range> &ranges,
                                  const Source::Config &config) {
  if (ranges.empty() || ranges.size() == 0) {
    return;
  }
  Glib::RefPtr<Gtk::TextBuffer> buffer = get_buffer();
  buffer->remove_all_tags(buffer->begin(), buffer->end());
  for (auto &range : ranges) {
    std::string type = std::to_string(range.kind());
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
    Gtk::TextIter begin_iter =
      buffer->get_iter_at_line_offset(linum_start, begin);
    Gtk::TextIter end_iter  =
      buffer->get_iter_at_line_offset(linum_end, end);
      buffer->apply_tag_by_name(config.typetable().at(type),
                                begin_iter, end_iter);
  }
}

void Source::Controller::OnOpenFile(const string &filepath) {
  set_file_path(filepath);
  sourcefile s(filepath);
  std::map<std::string, std::string> buffers;
  notebook_->MapBuffers(&buffers);
  buffers[filepath] = s.get_content();
  buffer()->set_text(s.get_content());
  int start_offset = buffer()->begin().get_offset();
  int end_offset = buffer()->end().get_offset();
  if (check_extention(filepath)) {
    view().ApplyConfig(model().config());
    model().InitSyntaxHighlighting(filepath,
                                   extract_file_path(filepath),
                                   buffers,
                                   start_offset,
                                   end_offset,
                                   notebook_->index());
    view().OnUpdateSyntax(model().ExtractTokens(start_offset, end_offset),
                          model().config());
  }

  buffer()->signal_end_user_action().connect([this]() {
      if (!go) {
        std::thread parse([this]() {
            if (parsing.try_lock()) {
              while (true) {
                const std::string raw = buffer()->get_text().raw();
                std::map<std::string, std::string> buffers;
                notebook_->MapBuffers(&buffers);
                buffers[model().file_path()] = raw;
                if (model().ReParse(buffers) == 0 &&
                    raw == buffer()->get_text().raw()) {
                  syntax.lock();
                  go = true;
                  syntax.unlock();
                  break;
                }
              }
              parsing.unlock();
            }
          });
        parse.detach();
      }
    });

  buffer()->signal_begin_user_action().connect([this]() {
      if (go) {
        syntax.lock();
        view().
          OnUpdateSyntax(model().ExtractTokens(0, buffer()->get_text().size()),
                         model().config());
        go = false;
        syntax.unlock();
      }
    });
}
Glib::RefPtr<Gtk::TextBuffer> Source::Controller::buffer() {
  return view().get_buffer();
}
