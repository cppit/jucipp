#include "source.h"
#include "sourcefile.h"
#include <boost/property_tree/json_parser.hpp>
#include <fstream>
#include <boost/timer/timer.hpp>
#include "notebook.h"
#include "logging.h"
#include <algorithm>
#include <regex>

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
  Gsv::init();
}

string Source::View::GetLine(size_t line_number) {
  Gtk::TextIter line_it = get_source_buffer()->get_iter_at_line(line_number);
  Gtk::TextIter line_end_it = line_it;
  while(!line_end_it.ends_line())
    line_end_it++;
  std::string line(get_source_buffer()->get_text(line_it, line_end_it));
  return line;
}

string Source::View::GetLineBeforeInsert() {
  Gtk::TextIter insert_it = get_source_buffer()->get_insert()->get_iter();
  Gtk::TextIter line_it = get_source_buffer()->get_iter_at_line(insert_it.get_line());
  std::string line(get_source_buffer()->get_text(line_it, insert_it));
  return line;
}

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

std::vector<string>& Source::Config::extensiontable(){
  return extensiontable_;
}

void Source::Config::InsertTag(const string &key, const string &value) {
  tagtable_[key] = value;
}

void Source::Config::InsertExtension(const string &ext) {
  extensiontable_.push_back(ext);
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
void Source::Parser::
InitSyntaxHighlighting(const std::string &filepath,
                       const std::string &project_path,
                       const std::map<std::string, std::string>
                       &buffers,
                       int start_offset,
                       int end_offset,
                       clang::Index *index) {
  this->project_path=project_path;
  std::vector<string> arguments = get_compilation_commands();
  tu_ = std::unique_ptr<clang::TranslationUnit>(new clang::TranslationUnit(index,
                               filepath,
                               arguments,
                               buffers));
}

// Source::View::UpdateLine
void Source::View::
OnLineEdit(const std::vector<Source::Range> &locations,
           const Source::Config &config) {
  OnUpdateSyntax(locations, config);
}

// Source::Model::UpdateLine
int Source::Parser::
ReParse(const std::map<std::string, std::string> &buffer) {
  return tu_->ReparseTranslationUnit(file_path, buffer);
}


// Source::Controller::OnLineEdit()
// fired when a line in the buffer is edited
void Source::Controller::OnLineEdit() { }

void Source::Controller::
GetAutoCompleteSuggestions(int line_number,
                           int column,
                           std::vector<Source::AutoCompleteData>
                           *suggestions) {
  INFO("Getting auto complete suggestions");
  parsing.lock();
  std::map<std::string, std::string> buffers;
  notebook.MapBuffers(&buffers);
  parser.GetAutoCompleteSuggestions(buffers,
                                     line_number,
                                     column,
                                     suggestions);
  DEBUG("Number of suggestions");
  DEBUG_VAR(suggestions->size());
  parsing.unlock();
}

void Source::Parser::
GetAutoCompleteSuggestions(const std::map<std::string, std::string> &buffers,
                           int line_number,
                           int column,
                           std::vector<Source::AutoCompleteData>
                           *suggestions) {
  clang::CodeCompleteResults results(tu_.get(),
                                     file_path,
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

std::vector<std::string> Source::Parser::
get_compilation_commands() {
  clang::CompilationDatabase db(project_path+"/");
  clang::CompileCommands commands(file_path, &db);
  std::vector<clang::CompileCommand> cmds = commands.get_commands();
  std::vector<std::string> arguments;
  for (auto &i : cmds) {
    std::vector<std::string> lol = i.get_command_as_args();
    for (int a = 1; a < lol.size()-4; a++) {
      arguments.emplace_back(lol[a]);
    }
  }
  return arguments;
}

std::vector<Source::Range> Source::Parser::
ExtractTokens(int start_offset, int end_offset) {
  std::vector<Source::Range> ranges;
  clang::SourceLocation start(tu_.get(), file_path, start_offset);
  clang::SourceLocation end(tu_.get(), file_path, end_offset);
  clang::SourceRange range(&start, &end);
  clang::Tokens tokens(tu_.get(), &range);
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

void Source::Parser::
HighlightCursor(clang::Token *token,
                std::vector<Source::Range> *source_ranges) {
  clang::SourceLocation location = token->get_source_location(tu_.get());
  clang::Cursor cursor(tu_.get(), &location);
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
void Source::Parser::
HighlightToken(clang::Token *token,
               std::vector<Source::Range> *source_ranges,
               int token_kind) {
  clang::SourceRange range = token->get_source_range(tu_.get());
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
                               Notebook::Controller &notebook) :
  config(config), notebook(notebook), parse_thread_go(false), parse_thread_mapped(false) {
  INFO("Source Controller with childs constructed");
  view.signal_key_press_event().connect(sigc::mem_fun(*this, &Source::Controller::OnKeyPress), false);
  view.set_smart_home_end(Gsv::SMART_HOME_END_BEFORE);
  view.override_font(Pango::FontDescription(config.font));
  view.set_show_line_numbers(config.show_line_numbers);
  view.set_highlight_current_line(config.highlight_current_line);
  view.override_background_color(Gdk::RGBA(config.background));
  for (auto &item : config.tagtable()) {
    buffer()->create_tag(item.first)->property_foreground() = item.second;
  }
}

Source::Controller::~Controller() {
  parsing.lock(); //Be sure not to destroy while still parsing with libclang
  parsing.unlock();
}

void Source::Controller::OnNewEmptyFile() {
  string filename("/tmp/juci_t");
  sourcefile s(filename);
  parser.file_path=filename;
  parser.project_path=filename;
  s.save("");
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
  parser.file_path=filepath;
  sourcefile s(filepath);
  std::map<std::string, std::string> buffers;
  notebook.MapBuffers(&buffers);
  buffers[filepath] = s.get_content();
  buffer()->get_undo_manager()->begin_not_undoable_action();
  buffer()->set_text(s.get_content());
  buffer()->get_undo_manager()->end_not_undoable_action();
  int start_offset = buffer()->begin().get_offset();
  int end_offset = buffer()->end().get_offset();
  if (notebook.LegalExtension(filepath.substr(filepath.find_last_of(".") + 1))) {
    parser.InitSyntaxHighlighting(filepath,
                                   parser.file_path.substr(0, parser.file_path.find_last_of('/')),
                                   buffers,
                                   start_offset,
                                   end_offset,
                                   notebook.index());
    view.OnUpdateSyntax(parser.ExtractTokens(start_offset, end_offset), config);

    //GTK-calls must happen in main thread, so the parse_thread
    //sends signals to the main thread that it is to call the following functions:
    parse_start.connect([this]{
      if(parse_thread_buffers_mutex.try_lock()) {
        notebook.MapBuffers(&this->parse_thread_buffers);
        parse_thread_mapped=true;
        parse_thread_buffers_mutex.unlock();
      }
      parse_thread_go=true;
    });

    parse_done.connect([this](){
      if(parse_thread_mapped) {
        INFO("Updating syntax");
        view.
          OnUpdateSyntax(parser.ExtractTokens(0, buffer()->get_text().size()), config);
        INFO("Syntax updated");
      }
      else {
        parse_thread_go=true;
      }
    });
    
    std::thread parse_thread([this]() {
      while(true) {
        while(!parse_thread_go) std::this_thread::yield();
        if(!parse_thread_mapped) {
          parse_thread_go=false;
          parse_start();
        }
        else if (parse_thread_mapped && parsing.try_lock() && parse_thread_buffers_mutex.try_lock()) {
          parser.ReParse(this->parse_thread_buffers);
          parse_thread_go=false;
          parsing.unlock();
          parse_thread_buffers_mutex.unlock();
          parse_done();
        }
      }
    });
    parse_thread.detach();
    
    buffer()->signal_changed().connect([this]() {
      parse_thread_mapped=false;
      parse_thread_go=true;
    });
  }
}

Glib::RefPtr<Gsv::Buffer> Source::Controller::buffer() {
  return view.get_source_buffer();
}

bool Source::Controller::OnKeyPress(GdkEventKey* key) {
  const std::regex bracket_regex("^( *).*\\{ *$");
  const std::regex no_bracket_statement_regex("^( *)(if|for|else if|catch|while) *\\(.*[^;}] *$");
  const std::regex no_bracket_no_para_statement_regex("^( *)(else|try|do) *$");
  const std::regex spaces_regex("^( *).*$");

  //Indent as in previous line, and indent right after if/else/etc
  if(key->keyval==GDK_KEY_Return && key->state==0) {
    string line(view.GetLineBeforeInsert());
    std::smatch sm;
    if(std::regex_match(line, sm, bracket_regex)) {
      buffer()->insert_at_cursor("\n"+sm[1].str()+config.tab+"\n"+sm[1].str()+"}");
      auto insert_it = buffer()->get_insert()->get_iter();
      for(size_t c=0;c<config.tab_size+sm[1].str().size();c++)
        insert_it--;
      view.scroll_to(buffer()->get_insert());
      buffer()->place_cursor(insert_it);
    }
    else if(std::regex_match(line, sm, no_bracket_statement_regex)) {
      buffer()->insert_at_cursor("\n"+sm[1].str()+config.tab);
      view.scroll_to(buffer()->get_insert());
    }
    else if(std::regex_match(line, sm, no_bracket_no_para_statement_regex)) {
      buffer()->insert_at_cursor("\n"+sm[1].str()+config.tab);
      view.scroll_to(buffer()->get_insert());
    }
    else if(std::regex_match(line, sm, spaces_regex)) {
      std::smatch sm2;
      size_t line_nr=buffer()->get_insert()->get_iter().get_line();
      if(line_nr>0 && sm[1].str().size()>=config.tab_size) {
        string previous_line=view.GetLine(line_nr-1);
        if(!std::regex_match(previous_line, sm2, bracket_regex)) {
          if(std::regex_match(previous_line, sm2, no_bracket_statement_regex)) {
            buffer()->insert_at_cursor("\n"+sm2[1].str());
            view.scroll_to(buffer()->get_insert());
            return true;
          }
          else if(std::regex_match(previous_line, sm2, no_bracket_no_para_statement_regex)) {
            buffer()->insert_at_cursor("\n"+sm2[1].str());
            view.scroll_to(buffer()->get_insert());
            return true;
          }
        }
      }
      buffer()->insert_at_cursor("\n"+sm[1].str());
      view.scroll_to(buffer()->get_insert());
    }
    return true;
  }
  //Indent right when clicking tab, no matter where in the line the cursor is. Also works on selected text.
  if(key->keyval==GDK_KEY_Tab && key->state==0) {
    Gtk::TextIter selection_start, selection_end;
    buffer()->get_selection_bounds(selection_start, selection_end);
    int line_start=selection_start.get_line();
    int line_end=selection_end.get_line();
    for(int line=line_start;line<=line_end;line++) {
      Gtk::TextIter line_it = buffer()->get_iter_at_line(line);
      buffer()->insert(line_it, config.tab);
    }
    return true;
  }
  //Indent left when clicking shift-tab, no matter where in the line the cursor is. Also works on selected text.
  else if((key->keyval==GDK_KEY_ISO_Left_Tab || key->keyval==GDK_KEY_Tab) && key->state==GDK_SHIFT_MASK) {
    Gtk::TextIter selection_start, selection_end;
    buffer()->get_selection_bounds(selection_start, selection_end);
    int line_start=selection_start.get_line();
    int line_end=selection_end.get_line();
    
    for(int line_nr=line_start;line_nr<=line_end;line_nr++) {
      string line=view.GetLine(line_nr);
      if(!(line.size()>=config.tab_size && line.substr(0, config.tab_size)==config.tab))
        return true;
    }
    
    for(int line_nr=line_start;line_nr<=line_end;line_nr++) {
      Gtk::TextIter line_it = buffer()->get_iter_at_line(line_nr);
      Gtk::TextIter line_plus_it=line_it;
      
      for(unsigned c=0;c<config.tab_size;c++)
        line_plus_it++;
      buffer()->erase(line_it, line_plus_it);
    }
    return true;
  }
  //Indent left when writing } on a new line
  else if(key->keyval==GDK_KEY_braceright) {
    string line=view.GetLineBeforeInsert();
    if(line.size()>=config.tab_size) {
      for(auto c: line) {
        if(c!=' ')
          return false;
      }
      Gtk::TextIter insert_it = buffer()->get_insert()->get_iter();
      Gtk::TextIter line_it = buffer()->get_iter_at_line(insert_it.get_line());
      Gtk::TextIter line_plus_it=line_it;
      for(unsigned c=0;c<config.tab_size;c++)
        line_plus_it++;
      
      buffer()->erase(line_it, line_plus_it);
    }
    return false;
  }
  //"Smart" backspace key
  else if(key->keyval==GDK_KEY_BackSpace) {
    Gtk::TextIter insert_it=buffer()->get_insert()->get_iter();
    int line_nr=insert_it.get_line();
    if(line_nr>0) {
      string line=view.GetLine(line_nr);
      string previous_line=view.GetLine(line_nr-1);
      smatch sm;
      if(std::regex_match(previous_line, sm, spaces_regex)) {
        if(line==sm[1]) {
          Gtk::TextIter line_it = buffer()->get_iter_at_line(line_nr);
          buffer()->erase(line_it, insert_it);
        }
      }
    }
  }
  return false;
}
