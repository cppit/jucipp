#include "source.h"
#include "sourcefile.h"
#include <boost/property_tree/json_parser.hpp>
#include <fstream>
#include <boost/timer/timer.hpp>
#include "logging.h"
#include <algorithm>
#include <regex>

bool Source::Config::legal_extension(std::string e) const {
  std::transform(e.begin(), e.end(),e.begin(), ::tolower);
  if (find(extensions.begin(), extensions.end(), e) != extensions.end()) {
    DEBUG("Legal extension");
    return true;
  }
  DEBUG("Ilegal extension");
  return false;
}

//////////////
//// View ////
//////////////
Source::View::View() {
  Gsv::init();
}

string Source::View::get_line(size_t line_number) {
  Gtk::TextIter line_it = get_source_buffer()->get_iter_at_line(line_number);
  Gtk::TextIter line_end_it = line_it;
  while(!line_end_it.ends_line())
    line_end_it++;
  std::string line(get_source_buffer()->get_text(line_it, line_end_it));
  return line;
}

string Source::View::get_line_before_insert() {
  Gtk::TextIter insert_it = get_source_buffer()->get_insert()->get_iter();
  Gtk::TextIter line_it = get_source_buffer()->get_iter_at_line(insert_it.get_line());
  std::string line(get_source_buffer()->get_text(line_it, insert_it));
  return line;
}

///////////////
//// Parser ///
///////////////
clang::Index Source::Parser::clang_index(0, 1);

Source::Parser::~Parser() {
  parsing_mutex.lock(); //Be sure not to destroy while still parsing with libclang
  parsing_mutex.unlock();
}

void Source::Parser::
init_syntax_highlighting(const std::string &filepath,
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

std::map<std::string, std::string> Source::Parser::
get_buffer_map() const {
  std::map<std::string, std::string> buffer_map;
  for (auto &controller : controllers) {
    buffer_map.operator[](controller->parser.file_path) =
    controller->buffer()->get_text().raw();
  }
  return buffer_map;
}

// Source::Model::UpdateLine
int Source::Parser::
reparse(const std::map<std::string, std::string> &buffer) {
  return tu_->ReparseTranslationUnit(file_path, buffer);
}

std::vector<Source::AutoCompleteData> Source::Parser::
get_autocomplete_suggestions(int line_number,
                           int column) {
  INFO("Getting auto complete suggestions");
  std::vector<Source::AutoCompleteData> suggestions;
  auto buffer_map=get_buffer_map();
  parsing_mutex.lock();
  clang::CodeCompleteResults results(tu_.get(),
                                     file_path,
                                     buffer_map,
                                     line_number,
                                     column);
  for (int i = 0; i < results.size(); i++) {
    const vector<clang::CompletionChunk> chunks_ = results.get(i).get_chunks();
    std::vector<AutoCompleteChunk> chunks;
    for (auto &chunk : chunks_) {
      chunks.emplace_back(chunk);
    }
    suggestions.emplace_back(chunks);
  }
  parsing_mutex.unlock();
  DEBUG("Number of suggestions");
  DEBUG_VAR(suggestions.size());
  return suggestions;
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
extract_tokens(int start_offset, int end_offset) {
  std::vector<Source::Range> ranges;
  clang::SourceLocation start(tu_.get(), file_path, start_offset);
  clang::SourceLocation end(tu_.get(), file_path, end_offset);
  clang::SourceRange range(&start, &end);
  clang::Tokens tokens(tu_.get(), &range);
  std::vector<clang::Token> tks = tokens.tokens();
  for (auto &token : tks) {
    switch (token.kind()) {
    case 0: highlight_cursor(&token, &ranges); break;  // PunctuationToken
    case 1: highlight_token(&token, &ranges, 702); break;  // KeywordToken
    case 2: highlight_cursor(&token, &ranges); break;  // IdentifierToken
    case 3: highlight_token(&token, &ranges, 109); break;  // LiteralToken
    case 4: highlight_token(&token, &ranges, 705); break;  // CommentToken
    }
  }
  return ranges;
}

void Source::Parser::
highlight_cursor(clang::Token *token,
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
highlight_token(clang::Token *token,
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
                               const std::vector<std::unique_ptr<Source::Controller> > &controllers) :
  config(config), parser(controllers), parse_thread_go(true), parse_thread_mapped(false), parse_thread_stop(false) {
  INFO("Source Controller with childs constructed");
  view.signal_key_press_event().connect(sigc::mem_fun(*this, &Source::Controller::on_key_press), false);
  view.set_smart_home_end(Gsv::SMART_HOME_END_BEFORE);
  view.override_font(Pango::FontDescription(config.font));
  view.set_show_line_numbers(config.show_line_numbers);
  view.set_highlight_current_line(config.highlight_current_line);
  view.override_background_color(Gdk::RGBA(config.background));
  for (auto &item : config.tags) {
    buffer()->create_tag(item.first)->property_foreground() = item.second;
  }
}

Source::Controller::~Controller() {
  parse_thread_stop=true;
  if(parse_thread.joinable())
    parse_thread.join();
}

void Source::Controller::update_syntax(const std::vector<Source::Range> &ranges) {
  if (ranges.empty() || ranges.size() == 0) {
    return;
  }
  auto buffer = view.get_buffer();
  buffer->remove_all_tags(buffer->begin(), buffer->end());
  for (auto &range : ranges) {
    std::string type = std::to_string(range.kind);
    try {
      config.types.at(type);
    } catch (std::exception) {
      continue;
    }
    int linum_start = range.start.line_number-1;
    int linum_end = range.end.line_number-1;
    int begin = range.start.column_offset-1;
    int end = range.end.column_offset-1;

    if (end < 0) end = 0;
    if (begin < 0) begin = 0;
    Gtk::TextIter begin_iter =
      buffer->get_iter_at_line_offset(linum_start, begin);
    Gtk::TextIter end_iter  =
      buffer->get_iter_at_line_offset(linum_end, end);
    buffer->apply_tag_by_name(config.types.at(type),
                              begin_iter, end_iter);
  }
}

void Source::Controller::set_handlers() {
  buffer()->signal_changed().connect([this]() {
    if(signal_buffer_changed)
      signal_buffer_changed(is_saved);
    is_saved=false;
    parse_thread_mapped=false;
    parse_thread_go=true;
  });
}

void Source::Controller::on_new_empty_file() {
  string filename("/tmp/untitled");
  sourcefile s(filename);
  parser.file_path=filename;
  parser.project_path=filename;
  s.save("");
  set_handlers();
}

void Source::Controller::on_open_file(const string &filepath) {
  parser.file_path=filepath;
  sourcefile s(filepath);
  auto buffer_map=parser.get_buffer_map();
  buffer_map[filepath] = s.get_content();
  buffer()->get_undo_manager()->begin_not_undoable_action();
  buffer()->set_text(s.get_content());
  buffer()->get_undo_manager()->end_not_undoable_action();
  set_handlers();
  int start_offset = buffer()->begin().get_offset();
  int end_offset = buffer()->end().get_offset();
  if (config.legal_extension(filepath.substr(filepath.find_last_of(".") + 1))) {
    parser.init_syntax_highlighting(filepath,
                                   parser.file_path.substr(0, parser.file_path.find_last_of('/')),
                                   buffer_map,
                                   start_offset,
                                   end_offset,
                                   &Parser::clang_index);
    update_syntax(parser.extract_tokens(start_offset, end_offset));

    //GTK-calls must happen in main thread, so the parse_thread
    //sends signals to the main thread that it is to call the following functions:
    parse_start.connect([this]{
      if(parse_thread_buffer_map_mutex.try_lock()) {
        this->parse_thread_buffer_map=parser.get_buffer_map();
        parse_thread_mapped=true;
        parse_thread_buffer_map_mutex.unlock();
      }
      parse_thread_go=true;
    });

    parse_done.connect([this](){
      if(parse_thread_mapped) {
        INFO("Updating syntax");
        update_syntax(parser.extract_tokens(0, buffer()->get_text().size()));
        INFO("Syntax updated");
      }
      else {
        parse_thread_go=true;
      }
    });
    
    parse_thread=std::thread([this]() {
      while(true) {
        while(!parse_thread_go && !parse_thread_stop)
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if(parse_thread_stop)
          break;
        if(!parse_thread_mapped) {
          parse_thread_go=false;
          parse_start();
        }
        else if (parse_thread_mapped && parser.parsing_mutex.try_lock() && parse_thread_buffer_map_mutex.try_lock()) {
          parser.reparse(this->parse_thread_buffer_map);
          parse_thread_go=false;
          parser.parsing_mutex.unlock();
          parse_thread_buffer_map_mutex.unlock();
          parse_done();
        }
      }
    });
  }
}

Glib::RefPtr<Gsv::Buffer> Source::Controller::buffer() {
  return view.get_source_buffer();
}

//TODO: move indentation to Parser, replace indentation methods with a better implementation or
//maybe use libclang
bool Source::Controller::on_key_press(GdkEventKey* key) {
  const std::regex bracket_regex("^( *).*\\{ *$");
  const std::regex no_bracket_statement_regex("^( *)(if|for|else if|catch|while) *\\(.*[^;}] *$");
  const std::regex no_bracket_no_para_statement_regex("^( *)(else|try|do) *$");
  const std::regex spaces_regex("^( *).*$");

  //Indent as in previous line, and indent right after if/else/etc
  if(key->keyval==GDK_KEY_Return && key->state==0) {
    string line(view.get_line_before_insert());
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
        string previous_line=view.get_line(line_nr-1);
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
      string line=view.get_line(line_nr);
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
    string line=view.get_line_before_insert();
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
      string line=view.get_line(line_nr);
      string previous_line=view.get_line(line_nr-1);
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
