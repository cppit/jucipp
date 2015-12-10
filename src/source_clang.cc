#include "source_clang.h"
#include "config.h"
#include "terminal.h"

#include <iostream> //TODO: remove
using namespace std; //TODO: remove

namespace sigc {
#ifndef SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
  template <typename Functor>
  struct functor_trait<Functor, false> {
    typedef decltype (::sigc::mem_fun(std::declval<Functor&>(),
                                      &Functor::operator())) _intermediate;
    typedef typename _intermediate::result_type result_type;
    typedef Functor functor_type;
  };
#else
  SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
#endif
}

clang::Index Source::ClangViewParse::clang_index(0, 0);

Source::ClangViewParse::ClangViewParse(const boost::filesystem::path &file_path, const boost::filesystem::path& project_path, Glib::RefPtr<Gsv::Language> language):
Source::View(file_path, project_path, language) {
  JDEBUG("start");
  
  auto tag_table=get_buffer()->get_tag_table();
  for (auto &item : Config::get().source.clang_types) {
    if(!tag_table->lookup(item.second)) {
      get_buffer()->create_tag(item.second);
    }
  }
  configure();
  
  parsing_in_progress=Terminal::get().print_in_progress("Parsing "+file_path.string());
  //GTK-calls must happen in main thread, so the parse_thread
  //sends signals to the main thread that it is to call the following functions:
  parse_preprocess_connection=parse_preprocess.connect([this]{
    auto expected=ParseProcessState::PREPROCESSING;
    if(parse_mutex.try_lock()) {
      if(parse_process_state.compare_exchange_strong(expected, ParseProcessState::PROCESSING))
        parse_thread_buffer=get_buffer()->get_text();
      parse_mutex.unlock();
    }
    else
      parse_process_state.compare_exchange_strong(expected, ParseProcessState::STARTING);
  });
  parse_postprocess_connection=parse_postprocess.connect([this](){
    if(parse_mutex.try_lock()) {
      auto expected=ParseProcessState::POSTPROCESSING;
      if(parse_process_state.compare_exchange_strong(expected, ParseProcessState::IDLE)) {
        update_syntax();
        update_diagnostics();
        parsed=true;
        set_status("");
      }
      parse_mutex.unlock();
    }
  });
  parse_error_connection=parse_error.connect([this](){
    Terminal::get().print("Error: failed to reparse "+this->file_path.string()+".\n", true);
    set_status("");
    set_info("");
    parsing_in_progress->cancel("failed");
  });
  parse_initialize();
  
  get_buffer()->signal_changed().connect([this]() {
    soft_reparse();
    type_tooltips.hide();
    diagnostic_tooltips.hide();
  });
  
  JDEBUG("end");
}

void Source::ClangViewParse::configure() {
  Source::View::configure();
  
  auto scheme = get_source_buffer()->get_style_scheme();
  auto tag_table=get_buffer()->get_tag_table();
  for (auto &item : Config::get().source.clang_types) {
    auto tag = get_buffer()->get_tag_table()->lookup(item.second);
    if(tag) {
      auto style = scheme->get_style(item.second);
      if (style) {
        if (style->property_foreground_set())
          tag->property_foreground()  = style->property_foreground();
        if (style->property_background_set())
          tag->property_background() = style->property_background();
        if (style->property_strikethrough_set())
          tag->property_strikethrough() = style->property_strikethrough();
        //   //    if (style->property_bold_set()) tag->property_weight() = style->property_bold();
        //   //    if (style->property_italic_set()) tag->property_italic() = style->property_italic();
        //   //    if (style->property_line_background_set()) tag->property_line_background() = style->property_line_background();
        //   // if (style->property_underline_set()) tag->property_underline() = style->property_underline();
      } else
        JINFO("Style " + item.second + " not found in " + scheme->get_name());
    }
  }
  
  bracket_regex=boost::regex("^([ \\t]*).*\\{ *$");
  no_bracket_statement_regex=boost::regex("^([ \\t]*)(if|for|else if|catch|while) *\\(.*[^;}] *$");
  no_bracket_no_para_statement_regex=boost::regex("^([ \\t]*)(else|try|do) *$");
}

void Source::ClangViewParse::parse_initialize() {
  type_tooltips.hide();
  diagnostic_tooltips.hide();
  parsed=false;
  if(parse_thread.joinable())
    parse_thread.join();
  parse_state=ParseState::PROCESSING;
  parse_process_state=ParseProcessState::STARTING;
  
  auto buffer=get_buffer()->get_text();
  //Remove includes for first parse for initial syntax highlighting
  std::size_t pos=0;
  while((pos=buffer.find("#include", pos))!=std::string::npos) {
    auto start_pos=pos;
    pos=buffer.find('\n', pos+8);
    if(pos==std::string::npos)
      break;
    if(start_pos==0 || buffer[start_pos-1]=='\n') {
      buffer.replace(start_pos, pos-start_pos, pos-start_pos, ' ');
    }
    pos++;
  }
  clang_tu = std::unique_ptr<clang::TranslationUnit>(new clang::TranslationUnit(clang_index, file_path.string(), get_compilation_commands(), buffer.raw()));
  clang_tokens=clang_tu->get_tokens(0, buffer.bytes()-1);
  update_syntax();
  
  set_status("parsing...");
  parse_thread=std::thread([this]() {
    while(true) {
      while(parse_state==ParseState::PROCESSING && parse_process_state!=ParseProcessState::STARTING && parse_process_state!=ParseProcessState::PROCESSING)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      if(parse_state!=ParseState::PROCESSING)
        break;
      auto expected=ParseProcessState::STARTING;
      if(parse_process_state.compare_exchange_strong(expected, ParseProcessState::PREPROCESSING))
        parse_preprocess();
      else if (parse_process_state==ParseProcessState::PROCESSING && parse_mutex.try_lock()) {
        auto status=clang_tu->ReparseTranslationUnit(parse_thread_buffer.raw());
        parsing_in_progress->done("done");
        if(status==0) {
          auto expected=ParseProcessState::PROCESSING;
          if(parse_process_state.compare_exchange_strong(expected, ParseProcessState::POSTPROCESSING)) {
            clang_tokens=clang_tu->get_tokens(0, parse_thread_buffer.bytes()-1);
            parse_mutex.unlock();
            parse_postprocess();
          }
          else
            parse_mutex.unlock();
        }
        else {
          parse_state=ParseState::STOP;
          parse_mutex.unlock();
          parse_error();
        }
      }
    }
  });
}

void Source::ClangViewParse::soft_reparse() {
  soft_reparse_needed=false;
  parsed=false;
  if(parse_state!=ParseState::PROCESSING)
    return;
  parse_process_state=ParseProcessState::IDLE;
  delayed_reparse_connection.disconnect();
  delayed_reparse_connection=Glib::signal_timeout().connect([this]() {
    parsed=false;
    auto expected=ParseProcessState::IDLE;
    if(parse_process_state.compare_exchange_strong(expected, ParseProcessState::STARTING))
      set_status("parsing...");
    return false;
  }, 1000);
}

std::vector<std::string> Source::ClangViewParse::get_compilation_commands() {
  clang::CompilationDatabase db(project_path.string());
  clang::CompileCommands commands(file_path.string(), db);
  std::vector<clang::CompileCommand> cmds = commands.get_commands();
  std::vector<std::string> arguments;
  for (auto &i : cmds) {
    std::vector<std::string> lol = i.get_command_as_args();
    for (size_t a = 1; a < lol.size()-4; a++) {
      arguments.emplace_back(lol[a]);
    }
  }
  auto clang_version_string=clang::to_string(clang_getClangVersion());
  const boost::regex clang_version_regex("^[A-Za-z ]+([0-9.]+).*$");
  boost::smatch sm;
  if(boost::regex_match(clang_version_string, sm, clang_version_regex)) {
    auto clang_version=sm[1].str();
    arguments.emplace_back("-I/usr/lib/clang/"+clang_version+"/include");
    arguments.emplace_back("-I/usr/local/Cellar/llvm/"+clang_version+"/lib/clang/"+clang_version+"/include");
    arguments.emplace_back("-IC:/msys32/mingw32/lib/clang/"+clang_version+"/include");
    arguments.emplace_back("-IC:/msys32/mingw64/lib/clang/"+clang_version+"/include");
    arguments.emplace_back("-IC:/msys64/mingw32/lib/clang/"+clang_version+"/include");
    arguments.emplace_back("-IC:/msys64/mingw64/lib/clang/"+clang_version+"/include");
  }
  arguments.emplace_back("-fretain-comments-from-system-headers");
  if(file_path.extension()==".h") //TODO: temporary fix for .h-files (parse as c++)
    arguments.emplace_back("-xc++");

  return arguments;
}

void Source::ClangViewParse::update_syntax() {
  std::vector<TokenRange> ranges;
  for (auto &token : *clang_tokens) {
    //if(token.get_kind()==0) // PunctuationToken
      //ranges.emplace_back(token.offsets, (int) token.get_cursor().get_kind());
    if(token.get_kind()==1) // KeywordToken
      ranges.emplace_back(token.offsets, 702);
    else if(token.get_kind()==2) {// IdentifierToken 
      auto kind=(int)token.get_cursor().get_kind();
      if(kind==101 || kind==102)
        kind=(int)token.get_cursor().get_referenced().get_kind();
      if(kind!=500)
        ranges.emplace_back(token.offsets, kind);
    }
    else if(token.get_kind()==3) { // LiteralToken
      ranges.emplace_back(token.offsets, 109);
    }
    else if(token.get_kind()==4) // CommentToken
      ranges.emplace_back(token.offsets, 705);
  }
  if (ranges.empty() || ranges.size() == 0) {
    return;
  }
  auto buffer = get_source_buffer();
  for(auto &tag: last_syntax_tags)
    buffer->remove_tag_by_name(tag, buffer->begin(), buffer->end());
  last_syntax_tags.clear();
  for (auto &range : ranges) {
    auto type_it=Config::get().source.clang_types.find(std::to_string(range.kind));
    if(type_it!=Config::get().source.clang_types.end()) {
      last_syntax_tags.emplace(type_it->second);
      Gtk::TextIter begin_iter = buffer->get_iter_at_line_index(range.offsets.first.line-1, range.offsets.first.index-1);
      Gtk::TextIter end_iter  = buffer->get_iter_at_line_index(range.offsets.second.line-1, range.offsets.second.index-1);
      buffer->apply_tag_by_name(type_it->second, begin_iter, end_iter);
    }
  }
}

void Source::ClangViewParse::update_diagnostics() {
  diagnostic_offsets.clear();
  diagnostic_tooltips.clear();
  fix_its.clear();
  get_buffer()->remove_tag_by_name("def:warning_underline", get_buffer()->begin(), get_buffer()->end());
  get_buffer()->remove_tag_by_name("def:error_underline", get_buffer()->begin(), get_buffer()->end());
  auto diagnostics=clang_tu->get_diagnostics();
  size_t num_warnings=0;
  size_t num_errors=0;
  size_t num_fix_its=0;
  for(auto &diagnostic: diagnostics) {
    if(diagnostic.path==file_path.string()) {
      auto start_line=get_line(diagnostic.offsets.first.line-1); //index is sometimes off the line
      auto start_line_index=diagnostic.offsets.first.index-1;
      if(start_line_index>=start_line.size()) {
        if(start_line.size()==0)
          start_line_index=0;
        else
          start_line_index=start_line.size()-1;
      }
      auto end_line=get_line(diagnostic.offsets.second.line-1); //index is sometimes off the line
      auto end_line_index=diagnostic.offsets.second.index-1;
      if(end_line_index>end_line.size()) {
        if(end_line.size()==0)
          end_line_index=0;
        else
          end_line_index=end_line.size();
      }
      auto start=get_buffer()->get_iter_at_line_index(diagnostic.offsets.first.line-1, start_line_index);
      diagnostic_offsets.emplace(start.get_offset());
      auto end=get_buffer()->get_iter_at_line_index(diagnostic.offsets.second.line-1, end_line_index);
      std::string diagnostic_tag_name;
      if(diagnostic.severity<=CXDiagnostic_Warning) {
        diagnostic_tag_name="def:warning";
        num_warnings++;
      }
      else {
        diagnostic_tag_name="def:error";
        num_errors++;
      }
      
      auto spelling=diagnostic.spelling;
      auto severity_spelling=diagnostic.severity_spelling;
      
      std::string fix_its_string;
      unsigned fix_its_count=0;
      for(auto &fix_it: diagnostic.fix_its) {
        //Convert line index to line offset for correct output:
        auto clang_offsets=fix_it.offsets;
        std::pair<Offset, Offset> offsets;
        offsets.first.line=clang_offsets.first.line;
        offsets.first.index=clang_offsets.first.index;
        offsets.second.line=clang_offsets.second.line;
        offsets.second.index=clang_offsets.second.index;
        
        fix_its.emplace_back(fix_it.source, offsets);
        
        if(fix_its_string.size()>0)
          fix_its_string+='\n';
        fix_its_string+=fix_its.back().string(get_buffer());
        fix_its_count++;
        num_fix_its++;
      }
      
      if(fix_its_count==1)
        fix_its_string.insert(0, "Fix-it:\n");
      else if(fix_its_count>1)
        fix_its_string.insert(0, "Fix-its:\n");
      
      auto create_tooltip_buffer=[this, spelling, severity_spelling, diagnostic_tag_name, fix_its_string]() {
        auto tooltip_buffer=Gtk::TextBuffer::create(get_buffer()->get_tag_table());
        tooltip_buffer->insert_with_tag(tooltip_buffer->get_insert()->get_iter(), severity_spelling, diagnostic_tag_name);
        tooltip_buffer->insert_with_tag(tooltip_buffer->get_insert()->get_iter(), ":\n"+spelling, "def:note");
        if(fix_its_string.size()>0) {
          tooltip_buffer->insert_with_tag(tooltip_buffer->get_insert()->get_iter(), ":\n\n"+fix_its_string, "def:note");
        }
        return tooltip_buffer;
      };
      diagnostic_tooltips.emplace_back(create_tooltip_buffer, *this, get_buffer()->create_mark(start), get_buffer()->create_mark(end));
    
      get_buffer()->apply_tag_by_name(diagnostic_tag_name+"_underline", start, end);
      auto iter=get_buffer()->get_insert()->get_iter();
      if(iter.ends_line()) {
        auto next_iter=iter;
        if(next_iter.forward_char())
          get_buffer()->remove_tag_by_name(diagnostic_tag_name+"_underline", iter, next_iter);
      }
    }
  }
  std::string diagnostic_info;
  if(num_warnings>0) {
    diagnostic_info+=std::to_string(num_warnings)+" warning";
    if(num_warnings>1)
      diagnostic_info+='s';
  }
  if(num_errors>0) {
    if(num_warnings>0)
      diagnostic_info+=", ";
    diagnostic_info+=std::to_string(num_errors)+" error";
    if(num_errors>1)
      diagnostic_info+='s';
  }
  if(num_fix_its>0) {
    if(num_warnings>0 || num_errors>0)
      diagnostic_info+=", ";
    diagnostic_info+=std::to_string(num_fix_its)+" fix it";
    if(num_fix_its>1)
      diagnostic_info+='s';
  }
  set_info("  "+diagnostic_info);
}

void Source::ClangViewParse::show_diagnostic_tooltips(const Gdk::Rectangle &rectangle) {
  diagnostic_tooltips.show(rectangle);
}

void Source::ClangViewParse::show_type_tooltips(const Gdk::Rectangle &rectangle) {
  if(parsed) {
    Gtk::TextIter iter;
    int location_x, location_y;
    window_to_buffer_coords(Gtk::TextWindowType::TEXT_WINDOW_TEXT, rectangle.get_x(), rectangle.get_y(), location_x, location_y);
    location_x+=(rectangle.get_width()-1)/2;
    get_iter_at_location(iter, location_x, location_y);
    Gdk::Rectangle iter_rectangle;
    get_iter_location(iter, iter_rectangle);
    if(iter_rectangle.get_x()>location_x) {
      if(!iter.starts_line()) {
        if(!iter.backward_char())
          return;
      }
    }
    bool found_token=false;
    if(!((*iter>='a' && *iter<='z') || (*iter>='A' && *iter<='Z') || (*iter>='0' && *iter<='9') || *iter=='_')) {
      if(!iter.backward_char())
        return;
    }
      
    while((*iter>='a' && *iter<='z') || (*iter>='A' && *iter<='Z') || (*iter>='0' && *iter<='9') || *iter=='_') {
      if(!found_token)
        found_token=true;
      if(!iter.backward_char())
        return;
    }
    if(found_token && iter.forward_char()) {
      auto tokens=clang_tu->get_tokens(iter.get_line()+1, iter.get_line_index()+1, 
                                       iter.get_line()+1, iter.get_line_index()+1);
      
      type_tooltips.clear();
      for(auto &token: *tokens) {
        auto cursor=token.get_cursor();
        if(token.get_kind()==clang::Token_Identifier && cursor.has_type()) {
          if(static_cast<unsigned>(token.get_cursor().get_kind())==103) //These cursors are buggy
            continue;
          auto start=get_buffer()->get_iter_at_line_index(token.offsets.first.line-1, token.offsets.first.index-1);
          auto end=get_buffer()->get_iter_at_line_index(token.offsets.second.line-1, token.offsets.second.index-1);
          auto create_tooltip_buffer=[this, &token]() {
            auto tooltip_buffer=Gtk::TextBuffer::create(get_buffer()->get_tag_table());
            tooltip_buffer->insert_with_tag(tooltip_buffer->get_insert()->get_iter(), "Type: "+token.get_cursor().get_type(), "def:note");
            auto brief_comment=token.get_cursor().get_brief_comments();
            if(brief_comment!="")
              tooltip_buffer->insert_with_tag(tooltip_buffer->get_insert()->get_iter(), "\n\n"+brief_comment, "def:note");
            return tooltip_buffer;
          };
          
          type_tooltips.emplace_back(create_tooltip_buffer, *this, get_buffer()->create_mark(start), get_buffer()->create_mark(end));
        }
      }
      
      type_tooltips.show();
    }
  }
  
  //type_tooltips.show(rectangle);
}

//Clang indentation.
bool Source::ClangViewParse::on_key_press_event(GdkEventKey* key) {
  if(spellcheck_suggestions_dialog_shown) {
    if(spellcheck_suggestions_dialog->on_key_press(key))
      return true;
  }
  
  auto iter=get_buffer()->get_insert()->get_iter();
  if(iter.backward_char() && (get_source_buffer()->iter_has_context_class(iter, "comment") || get_source_buffer()->iter_has_context_class(iter, "string")))
    return Source::View::on_key_press_event(key);
  
  if(get_buffer()->get_has_selection()) {
    return Source::View::on_key_press_event(key);
  }
  get_source_buffer()->begin_user_action();
  iter=get_buffer()->get_insert()->get_iter();
  //Indent depending on if/else/etc and brackets
  if(key->keyval==GDK_KEY_Return && !iter.starts_line()) {
    //First remove spaces or tabs around cursor
    auto start_blank_iter=iter;
    auto end_blank_iter=iter;
    while((*end_blank_iter==' ' || *end_blank_iter=='\t') &&
          !end_blank_iter.ends_line() && end_blank_iter.forward_char()) {}
    start_blank_iter.backward_char();
    while((*start_blank_iter==' ' || *start_blank_iter=='\t' || start_blank_iter.ends_line()) &&
          !start_blank_iter.starts_line() && start_blank_iter.backward_char()) {}
    if(!start_blank_iter.starts_line()) {
      start_blank_iter.forward_char();
      get_buffer()->erase(start_blank_iter, end_blank_iter);
    }
    else
      get_buffer()->erase(iter, end_blank_iter);
    iter=get_buffer()->get_insert()->get_iter();
    
    Gtk::TextIter start_of_sentence_iter;
    if(find_start_of_closed_expression(iter, start_of_sentence_iter)) {
      auto start_sentence_tabs_end_iter=get_tabs_end_iter(start_of_sentence_iter);
      auto tabs=get_line_before(start_sentence_tabs_end_iter);
      
      boost::smatch sm;
      if(iter.backward_char() && *iter=='{') {
        auto found_iter=iter;
        bool found_right_bracket=find_right_bracket_forward(iter, found_iter);
        
        bool has_bracket=false;
        if(found_right_bracket) {
          auto tabs_end_iter=get_tabs_end_iter(found_iter);
          auto line_tabs=get_line_before(tabs_end_iter);
          if(tabs.size()==line_tabs.size())
            has_bracket=true;
        }
        if(*get_buffer()->get_insert()->get_iter()=='}') {
          get_source_buffer()->insert_at_cursor("\n"+tabs+tab+"\n"+tabs);
          auto insert_it = get_source_buffer()->get_insert()->get_iter();
          if(insert_it.backward_chars(tabs.size()+1)) {
            scroll_to(get_source_buffer()->get_insert());
            get_source_buffer()->place_cursor(insert_it);
          }
          get_source_buffer()->end_user_action();
          return true;
        }
        else if(!has_bracket) {
          //Insert new lines with bracket end
          get_source_buffer()->insert_at_cursor("\n"+tabs+tab+"\n"+tabs+"}");
          auto insert_it = get_source_buffer()->get_insert()->get_iter();
          if(insert_it.backward_chars(tabs.size()+2)) {
            scroll_to(get_source_buffer()->get_insert());
            get_source_buffer()->place_cursor(insert_it);
          }
          get_source_buffer()->end_user_action();
          return true;
        }
        else {
          get_source_buffer()->insert_at_cursor("\n"+tabs+tab);
          scroll_to(get_buffer()->get_insert());
          get_source_buffer()->end_user_action();
          return true;
        }
      }
      auto line=get_line_before();
      iter=get_buffer()->get_insert()->get_iter();
      auto found_iter=iter;
      if(find_open_expression_symbol(iter, start_of_sentence_iter, found_iter)) {
        auto tabs_end_iter=get_tabs_end_iter(found_iter);
        tabs=get_line_before(tabs_end_iter);
        auto iter=tabs_end_iter;
        while(iter<=found_iter) {
          tabs+=' ';
          iter.forward_char();
        }
      }
      else if(boost::regex_match(line, sm, no_bracket_statement_regex)) {
        get_source_buffer()->insert_at_cursor("\n"+tabs+tab);
        scroll_to(get_source_buffer()->get_insert());
        get_source_buffer()->end_user_action();
        return true;
      }
      else if(boost::regex_match(line, sm, no_bracket_no_para_statement_regex)) {
        get_source_buffer()->insert_at_cursor("\n"+tabs+tab);
        scroll_to(get_source_buffer()->get_insert());
        get_source_buffer()->end_user_action();
        return true;
      }
      //Indenting after for instance if(...)\n...;\n
      else if(iter.backward_char() && *iter==';') {
        boost::smatch sm2;
        size_t line_nr=get_source_buffer()->get_insert()->get_iter().get_line();
        if(line_nr>0 && tabs.size()>=tab_size) {
          std::string previous_line=get_line(line_nr-1);
          if(!boost::regex_match(previous_line, sm2, bracket_regex)) {
            if(boost::regex_match(previous_line, sm2, no_bracket_statement_regex)) {
              get_source_buffer()->insert_at_cursor("\n"+sm2[1].str());
              scroll_to(get_source_buffer()->get_insert());
              get_source_buffer()->end_user_action();
              return true;
            }
            else if(boost::regex_match(previous_line, sm2, no_bracket_no_para_statement_regex)) {
              get_source_buffer()->insert_at_cursor("\n"+sm2[1].str());
              scroll_to(get_source_buffer()->get_insert());
              get_source_buffer()->end_user_action();
              return true;
            }
          }
        }
      }
      //Indenting after ':'
      else if(*iter==':') {
        Gtk::TextIter left_bracket_iter;
        if(find_left_bracket_backward(iter, left_bracket_iter)) {
          if(!left_bracket_iter.ends_line())
            left_bracket_iter.forward_char();
          Gtk::TextIter start_of_left_bracket_sentence_iter;
          if(find_start_of_closed_expression(left_bracket_iter, start_of_left_bracket_sentence_iter)) {
            boost::smatch sm;
            auto tabs_end_iter=get_tabs_end_iter(start_of_left_bracket_sentence_iter);
            auto tabs_start_of_sentence=get_line_before(tabs_end_iter);
            if(tabs.size()==(tabs_start_of_sentence.size()+tab_size)) {
              auto start_line_iter=get_buffer()->get_iter_at_line(iter.get_line());
              auto start_line_plus_tab_size=start_line_iter;
              for(size_t c=0;c<tab_size;c++)
                start_line_plus_tab_size.forward_char();
              get_buffer()->erase(start_line_iter, start_line_plus_tab_size);
            }
            else {
              get_source_buffer()->insert_at_cursor("\n"+tabs+tab);
              scroll_to(get_source_buffer()->get_insert());
              get_source_buffer()->end_user_action();
              return true;
            }
          }
        }
      }
      get_source_buffer()->insert_at_cursor("\n"+tabs);
      scroll_to(get_source_buffer()->get_insert());
      get_source_buffer()->end_user_action();
      return true;
    }
  }
  //Indent left when writing } on a new line
  else if(key->keyval==GDK_KEY_braceright) {
    std::string line=get_line_before();
    if(line.size()>=tab_size) {
      for(auto c: line) {
        if(c!=tab_char) {
          get_source_buffer()->insert_at_cursor("}");
          get_source_buffer()->end_user_action();
          return true;
        }
      }
      Gtk::TextIter insert_it = get_source_buffer()->get_insert()->get_iter();
      Gtk::TextIter line_it = get_source_buffer()->get_iter_at_line(insert_it.get_line());
      Gtk::TextIter line_plus_it=line_it;
      line_plus_it.forward_chars(tab_size);
      get_source_buffer()->erase(line_it, line_plus_it);
    }
    get_source_buffer()->insert_at_cursor("}");
    get_source_buffer()->end_user_action();
    return true;
  }
  
  get_source_buffer()->end_user_action();
  return Source::View::on_key_press_event(key);
}

//////////////////////////////
//// ClangViewAutocomplete ///
//////////////////////////////
Source::ClangViewAutocomplete::ClangViewAutocomplete(const boost::filesystem::path &file_path, const boost::filesystem::path& project_path, Glib::RefPtr<Gsv::Language> language):
Source::ClangViewParse(file_path, project_path, language), autocomplete_state(AutocompleteState::IDLE) {
  get_buffer()->signal_changed().connect([this](){
    if(autocomplete_state==AutocompleteState::SHOWN)
      delayed_reparse_connection.disconnect();
    else {
      if(!has_focus())
        return;
      if((last_keyval>='0' && last_keyval<='9') ||
         (last_keyval>='a' && last_keyval<='z') || (last_keyval>='A' && last_keyval<='Z') ||
         last_keyval=='_') {
        autocomplete_check();
      }
      else {
        if(autocomplete_state==AutocompleteState::STARTING)
          autocomplete_state=AutocompleteState::CANCELED;
        else {
          auto iter=get_buffer()->get_insert()->get_iter();
          if(last_keyval=='.' || last_keyval==':' || (last_keyval=='>' && iter.backward_char() && iter.backward_char() && *iter=='-'))
            autocomplete_check();
        }
      }
    }
  });
  get_buffer()->signal_mark_set().connect([this](const Gtk::TextBuffer::iterator& iterator, const Glib::RefPtr<Gtk::TextBuffer::Mark>& mark){
    if(mark->get_name()=="insert") {
      if(autocomplete_state==AutocompleteState::SHOWN)
        autocomplete_dialog->hide();
      if(autocomplete_state==AutocompleteState::STARTING)
        autocomplete_state=AutocompleteState::CANCELED;
    }
  });
  signal_scroll_event().connect([this](GdkEventScroll* event){
    if(autocomplete_state==AutocompleteState::SHOWN)
        autocomplete_dialog->hide();
    return false;
  }, false);
  signal_key_release_event().connect([this](GdkEventKey* key){
    if(autocomplete_state==AutocompleteState::SHOWN) {
      if(autocomplete_dialog->on_key_release(key))
        return true;
    }
    
    return false;
  }, false);

  signal_focus_out_event().connect([this](GdkEventFocus* event) {
    if(autocomplete_state==AutocompleteState::SHOWN)
      autocomplete_dialog->hide();
    if(autocomplete_state==AutocompleteState::STARTING)
      autocomplete_state=AutocompleteState::CANCELED;
    return false;
  });
  
  autocomplete_error_connection=autocomplete_done.connect([this](){
    if(autocomplete_state==AutocompleteState::CANCELED) {
      set_status("");
      soft_reparse();
      autocomplete_state=AutocompleteState::IDLE;
      autocomplete_restart();
    }
    else {
      autocomplete_dialog_setup();
      
      for (auto &data : autocomplete_data) {
        std::stringstream ss;
        std::string return_value;
        for (auto &chunk : data.chunks) {
          switch (chunk.kind) {
          case clang::CompletionChunk_ResultType:
            return_value = chunk.chunk;
            break;
          case clang::CompletionChunk_Informative: break;
          default: ss << chunk.chunk; break;
          }
        }
        auto row=ss.str();
        auto row_insert_on_selection=row;
        if (!row.empty()) {
          if(!return_value.empty())
            row+=" --> " + return_value;
          autocomplete_dialog_rows[row] = row_insert_on_selection;
          autocomplete_dialog->add_row(row, data.brief_comments);
        }
      }
      set_status("");
      if (!autocomplete_dialog_rows.empty()) {
        autocomplete_state=AutocompleteState::SHOWN;
        get_source_buffer()->begin_user_action();
        autocomplete_dialog->show();
      }
      else {
        autocomplete_state=AutocompleteState::IDLE;
        soft_reparse();
      }
    }
  });
  
  autocomplete_restart_connection=autocomplete_restart.connect([this]() {
    autocomplete_check();
  });
  autocomplete_error_connection=autocomplete_error.connect([this]() {
    Terminal::get().print("Error: autocomplete failed, reparsing "+this->file_path.string()+"\n", true);
    autocomplete_state=AutocompleteState::CANCELED;
    full_reparse();
  });
  
  do_delete_object_connection=do_delete_object.connect([this](){
    if(delete_thread.joinable())
      delete_thread.join();
    do_delete_object_connection.disconnect();
    delete this;
  });
  do_full_reparse.connect([this](){
    parse_initialize();
    full_reparse_running=false;
  });
}

bool Source::ClangViewAutocomplete::on_key_press_event(GdkEventKey *key) {
  last_keyval=key->keyval;
  if(autocomplete_state==AutocompleteState::SHOWN) {
    if(autocomplete_dialog->on_key_press(key))
      return true;
  }
  return ClangViewParse::on_key_press_event(key);
}

void Source::ClangViewAutocomplete::autocomplete_dialog_setup() {
  auto start_iter=get_buffer()->get_insert()->get_iter();
  if(prefix.size()>0 && !start_iter.backward_chars(prefix.size()))
    return;
  autocomplete_dialog=std::unique_ptr<CompletionDialog>(new CompletionDialog(*this, get_buffer()->create_mark(start_iter)));
  autocomplete_dialog_rows.clear();
  autocomplete_dialog->on_hide=[this](){
    get_source_buffer()->end_user_action();
    autocomplete_state=AutocompleteState::IDLE;
    parsed=false;
    soft_reparse();
  };
  autocomplete_dialog->on_select=[this](const std::string& selected, bool hide_window) {
    auto row = autocomplete_dialog_rows.at(selected);
    get_buffer()->erase(autocomplete_dialog->start_mark->get_iter(), get_buffer()->get_insert()->get_iter());
    auto iter=get_buffer()->get_insert()->get_iter();
    if(*iter=='<' || *iter=='(') {
      auto bracket_pos=row.find(*iter);
      if(bracket_pos!=std::string::npos) {
        row=row.substr(0, bracket_pos);
      }
    }
    get_buffer()->insert(autocomplete_dialog->start_mark->get_iter(), row);
    if(hide_window) {
      autocomplete_state=AutocompleteState::IDLE;
      auto para_pos=row.find('(');
      auto angle_pos=row.find('<');
      size_t start_pos=std::string::npos;
      size_t end_pos=std::string::npos;
      if(angle_pos<para_pos) {
        start_pos=angle_pos;
        end_pos=row.find('>');
      }
      else if(para_pos!=std::string::npos) {
        start_pos=para_pos;
        end_pos=row.size()-1;
      }
      if(start_pos==std::string::npos || end_pos==std::string::npos) {
        if((start_pos=row.find('\"'))!=std::string::npos)
          end_pos=row.find('\"', start_pos+1);
      }
      if(start_pos==std::string::npos || end_pos==std::string::npos) {
        if((start_pos=row.find(' '))!=std::string::npos) {
          if((start_pos=row.find("expression", start_pos+1))!=std::string::npos) {
            end_pos=start_pos+10;
            start_pos--;
          }
        }
      }
      if(start_pos!=std::string::npos && end_pos!=std::string::npos) {
        auto start_offset=autocomplete_dialog->start_mark->get_iter().get_offset()+start_pos+1;
        auto end_offset=autocomplete_dialog->start_mark->get_iter().get_offset()+end_pos;
        if(start_offset!=end_offset)
          get_buffer()->select_range(get_buffer()->get_iter_at_offset(start_offset), get_buffer()->get_iter_at_offset(end_offset));
      }
      else {
        //new autocomplete after for instance when selecting "std::"
        auto iter=get_buffer()->get_insert()->get_iter();
        if(iter.backward_char() && *iter==':') {
          autocomplete_state=AutocompleteState::IDLE;
          autocomplete_restart();
        }
      }
    }
  };
}

void Source::ClangViewAutocomplete::autocomplete_check() {
  auto iter=get_buffer()->get_insert()->get_iter();
  if(iter.backward_char() && iter.backward_char() && (get_source_buffer()->iter_has_context_class(iter, "string") ||
                                                      get_source_buffer()->iter_has_context_class(iter, "comment")))
    return;
  std::string line=" "+get_line_before();
  const boost::regex in_specified_namespace("^(.*[a-zA-Z0-9_\\)\\]\\>])(->|\\.|::)([a-zA-Z0-9_]*)$");
  const boost::regex within_namespace("^(.*)([^a-zA-Z0-9_]+)([a-zA-Z0-9_]{3,})$");
  boost::smatch sm;
  if(boost::regex_match(line, sm, in_specified_namespace)) {
    prefix_mutex.lock();
    prefix=sm[3].str();
    prefix_mutex.unlock();
    if(autocomplete_state==AutocompleteState::IDLE && (prefix.size()==0 || prefix[0]<'0' || prefix[0]>'9'))
      autocomplete();
  }
  else if(boost::regex_match(line, sm, within_namespace)) {
    prefix_mutex.lock();
    prefix=sm[3].str();
    prefix_mutex.unlock();
    if(autocomplete_state==AutocompleteState::IDLE && (prefix.size()==0 || prefix[0]<'0' || prefix[0]>'9'))
      autocomplete();
  }
  if(autocomplete_state!=AutocompleteState::IDLE)
    delayed_reparse_connection.disconnect();
}

void Source::ClangViewAutocomplete::autocomplete() {
  if(parse_state!=ParseState::PROCESSING)
    return;
  if(autocomplete_state==AutocompleteState::STARTING) {
    autocomplete_state=AutocompleteState::CANCELED;
    return;
  }
  if(autocomplete_state==AutocompleteState::CANCELED)
    return;
  autocomplete_state=AutocompleteState::STARTING;
  
  autocomplete_data.clear();
  
  set_status("autocomplete...");
  if(autocomplete_thread.joinable())
    autocomplete_thread.join();
  auto buffer=std::make_shared<Glib::ustring>(get_buffer()->get_text());
  auto iter=get_buffer()->get_insert()->get_iter();
  auto line_nr=iter.get_line()+1;
  auto column_nr=iter.get_line_offset()+1;
  auto pos=iter.get_offset()-1;
  while(pos>=0 && (((*buffer)[pos]>='a' && (*buffer)[pos]<='z') || ((*buffer)[pos]>='A' && (*buffer)[pos]<='Z') ||
                   ((*buffer)[pos]>='0' && (*buffer)[pos]<='9') || (*buffer)[pos]=='_')) {
    buffer->replace(pos, 1, " ");
    column_nr--;
    pos--;
  }
  autocomplete_thread=std::thread([this, line_nr, column_nr, buffer](){
    parse_mutex.lock();
    if(parse_state==ParseState::PROCESSING) {
      parse_process_state=ParseProcessState::IDLE;
      autocomplete_data=autocomplete_get_suggestions(buffer->raw(), line_nr, column_nr);
    }
    if(parse_state==ParseState::PROCESSING)
      autocomplete_done();
    else
      autocomplete_error();
    parse_mutex.unlock();
  });
}

std::vector<Source::ClangViewAutocomplete::AutoCompleteData> Source::ClangViewAutocomplete::autocomplete_get_suggestions(const std::string &buffer, int line_number, int column) {
  std::vector<AutoCompleteData> suggestions;
  auto results=clang_tu->get_code_completions(buffer, line_number, column);
  if(results.cx_results==NULL) {
    auto expected=ParseState::PROCESSING;
    parse_state.compare_exchange_strong(expected, ParseState::RESTARTING);
    return suggestions;
  }
  
  if(autocomplete_state==AutocompleteState::STARTING) {
    prefix_mutex.lock();
    auto prefix_copy=prefix;
    prefix_mutex.unlock();
    for (unsigned i = 0; i < results.size(); i++) {
      auto result=results.get(i);
      if(result.available()) {
        auto chunks=result.get_chunks();
        bool match=false;
        for(auto &chunk: chunks) {
          if(chunk.kind!=clang::CompletionChunk_ResultType && chunk.kind!=clang::CompletionChunk_Informative) {
            if(chunk.chunk.size()>=prefix_copy.size() && chunk.chunk.compare(0, prefix_copy.size(), prefix_copy)==0)
              match=true;
            break;
          }
        }
        if(match) {
          suggestions.emplace_back(std::move(chunks));
          suggestions.back().brief_comments=result.get_brief_comments();
        }
      }
    }
  }
  return suggestions;
}

void Source::ClangViewAutocomplete::async_delete() {
  parsing_in_progress->cancel("canceled, freeing resources in the background");
  parse_state=ParseState::STOP;
  delete_thread=std::thread([this](){
    //TODO: Is it possible to stop the clang-process in progress?
    if(full_reparse_thread.joinable())
      full_reparse_thread.join();
    if(parse_thread.joinable())
      parse_thread.join();
    if(autocomplete_thread.joinable())
      autocomplete_thread.join();
    do_delete_object();
  });
}

bool Source::ClangViewAutocomplete::full_reparse() {
  full_reparse_needed=false;
  if(!full_reparse_running) {
    auto expected=ParseState::PROCESSING;
    if(!parse_state.compare_exchange_strong(expected, ParseState::RESTARTING)) {
      expected=ParseState::RESTARTING;
      if(!parse_state.compare_exchange_strong(expected, ParseState::RESTARTING))
        return false;
    }
    soft_reparse_needed=false;
    full_reparse_running=true;
    if(full_reparse_thread.joinable())
      full_reparse_thread.join();
    full_reparse_thread=std::thread([this](){
      if(parse_thread.joinable())
        parse_thread.join();
      if(autocomplete_thread.joinable())
        autocomplete_thread.join();
      do_full_reparse();
    });
    return true;
  }
  return false;
}

////////////////////////////
//// ClangViewRefactor /////
////////////////////////////
Source::ClangViewRefactor::ClangViewRefactor(const boost::filesystem::path &file_path, const boost::filesystem::path& project_path, Glib::RefPtr<Gsv::Language> language):
Source::ClangViewAutocomplete(file_path, project_path, language) {
  similar_tokens_tag=get_buffer()->create_tag();
  similar_tokens_tag->property_weight()=1000; //TODO: replace with Pango::WEIGHT_ULTRAHEAVY in 2016 or so (when Ubuntu 14 is history)
  
  get_buffer()->signal_changed().connect([this]() {
    if(!renaming && last_tagged_token) {
      for(auto &mark: similar_token_marks) {
        get_buffer()->remove_tag(similar_tokens_tag, mark.first->get_iter(), mark.second->get_iter());
        get_buffer()->delete_mark(mark.first);
        get_buffer()->delete_mark(mark.second);
      }
      similar_token_marks.clear();
      last_tagged_token=Token();
    }
  });
  
  auto_indent=[this]() {
    auto command=Config::get().terminal.clang_format_command;
    unsigned indent_width;
    std::string tab_style;
    if(tab_char=='\t') {
      indent_width=tab_size*8;
      tab_style="UseTab: Always";
    }
    else {
      indent_width=tab_size;
      tab_style="UseTab: Never";
    }
    command+=" -style=\"{IndentWidth: "+std::to_string(indent_width);
    command+=", "+tab_style;
    command+=", "+std::string("AccessModifierOffset: -")+std::to_string(indent_width);
    if(Config::get().source.clang_format_style!="")
      command+=", "+Config::get().source.clang_format_style;
    command+="}\"";
    
    std::stringstream stdin_stream(get_buffer()->get_text()), stdout_stream;
    
    auto exit_status=Terminal::get().process(stdin_stream, stdout_stream, command);
    if(exit_status==0) {
      get_source_buffer()->begin_user_action();
      auto iter=get_buffer()->get_insert()->get_iter();
      auto cursor_line_nr=iter.get_line();
      auto cursor_line_offset=iter.get_line_offset();
      
      get_buffer()->erase(get_buffer()->begin(), get_buffer()->end());
      get_buffer()->insert(get_buffer()->begin(), stdout_stream.str());
      
      cursor_line_nr=std::min(cursor_line_nr, get_buffer()->get_line_count()-1);
      if(cursor_line_nr>=0) {
        iter=get_buffer()->get_iter_at_line(cursor_line_nr);
        for(int c=0;c<cursor_line_offset;c++) {
          if(iter.ends_line())
            break;
          iter.forward_char();
        }
        get_buffer()->place_cursor(iter);
        while(g_main_context_pending(NULL)) //TODO: minor: might crash if the buffer is saved and closed really fast right after doing auto indent
          g_main_context_iteration(NULL, false);
        scroll_to(get_buffer()->get_insert(), 0.0, 1.0, 0.5);
      }
      get_source_buffer()->end_user_action();
    }
  };
  
  get_token=[this]() -> Token {
    if(parsed) {
      auto iter=get_buffer()->get_insert()->get_iter();
      auto line=(unsigned)iter.get_line();
      auto index=(unsigned)iter.get_line_index();
      for(auto &token: *clang_tokens) {
        auto cursor=token.get_cursor();
        if(token.get_kind()==clang::Token_Identifier && cursor.has_type()) {
          if(line==token.offsets.first.line-1 && index>=token.offsets.first.index-1 && index <=token.offsets.second.index-1) {
            if(static_cast<unsigned>(token.get_cursor().get_kind())==103) //These cursors are buggy
              continue;
            auto referenced=cursor.get_referenced();
            if(referenced)
              return Token(this->language, static_cast<int>(referenced.get_kind()), token.get_spelling(), referenced.get_usr());
          }
        }
      }
    }
    return Token();
  };
  
  rename_similar_tokens=[this](const Token &token, const std::string &text) {
    size_t number=0;
    if(parsed && token.language &&
       (token.language->get_id()=="chdr" || token.language->get_id()=="cpphdr" || token.language->get_id()=="c" || token.language->get_id()=="cpp" || token.language->get_id()=="objc")) {
      auto offsets=clang_tokens->get_similar_token_offsets(static_cast<clang::CursorKind>(token.type), token.spelling, token.usr);
      std::vector<std::pair<Glib::RefPtr<Gtk::TextMark>, Glib::RefPtr<Gtk::TextMark> > > marks;
      for(auto &offset: offsets) {
        marks.emplace_back(get_buffer()->create_mark(get_buffer()->get_iter_at_line_index(offset.first.line-1, offset.first.index-1)), get_buffer()->create_mark(get_buffer()->get_iter_at_line_index(offset.second.line-1, offset.second.index-1)));
        number++;
      }
      get_source_buffer()->begin_user_action();
      for(auto &mark: marks) {
        renaming=true;
        get_buffer()->erase(mark.first->get_iter(), mark.second->get_iter());
        get_buffer()->insert(mark.first->get_iter(), text);
        get_buffer()->delete_mark(mark.first);
        get_buffer()->delete_mark(mark.second);
      }
      get_source_buffer()->end_user_action();
      renaming=false;
    }
    return number;
  };
  
  get_buffer()->signal_mark_set().connect([this](const Gtk::TextBuffer::iterator& iterator, const Glib::RefPtr<Gtk::TextBuffer::Mark>& mark){
    if(mark->get_name()=="insert") {
      delayed_tag_similar_tokens_connection.disconnect();
      delayed_tag_similar_tokens_connection=Glib::signal_timeout().connect([this]() {
        auto token=get_token();
        tag_similar_tokens(token);
        return false;
      }, 100);
    }
  });
  
  get_declaration_location=[this](){
    Offset location;
    if(parsed) {
      auto iter=get_buffer()->get_insert()->get_iter();
      auto line=(unsigned)iter.get_line();
      auto index=(unsigned)iter.get_line_index();
      for(auto &token: *clang_tokens) {
        auto cursor=token.get_cursor();
        if(token.get_kind()==clang::Token_Identifier && cursor.has_type()) {
          if(line==token.offsets.first.line-1 && index>=token.offsets.first.index-1 && index <=token.offsets.second.index-1) {
            auto referenced=cursor.get_referenced();
            if(referenced) {
              location.file_path=referenced.get_source_location().get_path();
              auto clang_offset=referenced.get_source_location().get_offset();
              location.line=clang_offset.line;
              location.index=clang_offset.index;
              break;
            }
          }
        }
      }
    }
    return location;
  };
  
  get_usages=[this](const Token &token) {
    std::vector<std::pair<Offset, std::string> > usages;
    
    if(parsed && token.language &&
     (token.language->get_id()=="chdr" || token.language->get_id()=="cpphdr" || token.language->get_id()=="c" || token.language->get_id()=="cpp" || token.language->get_id()=="objc")) {
      auto offsets=clang_tokens->get_similar_token_offsets(static_cast<clang::CursorKind>(token.type), token.spelling, token.usr);
      for(auto &offset: offsets) {
        size_t whitespaces_removed=0;
        auto start_iter=get_buffer()->get_iter_at_line(offset.first.line-1);
        while(!start_iter.ends_line() && (*start_iter==' ' || *start_iter=='\t')) {
          start_iter.forward_char();
          whitespaces_removed++;
        }
        auto end_iter=start_iter;
        while(!end_iter.ends_line())
          end_iter.forward_char();
        std::string line=Glib::Markup::escape_text(get_buffer()->get_text(start_iter, end_iter));
        
        //markup token as bold
        size_t token_start_pos=offset.first.index-1-whitespaces_removed;
        size_t token_end_pos=offset.second.index-1-whitespaces_removed;
        size_t pos=0;
        while((pos=line.find('&', pos))!=std::string::npos) {
          size_t pos2=line.find(';', pos+2);
          if(token_start_pos>pos) {
            token_start_pos+=pos2-pos;
            token_end_pos+=pos2-pos;
          }
          else if(token_end_pos>pos)
            token_end_pos+=pos2-pos;
          else
            break;
          pos=pos2+1;
        }
        line.insert(token_end_pos, "</b>");
        line.insert(token_start_pos, "<b>");
        usages.emplace_back(Offset(offset.first.line-1, offset.first.index-1, this->file_path), line);
      }
    }
    
    return usages;
  };
  
  goto_method=[this](){    
    if(parsed) {
      auto iter=get_buffer()->get_insert()->get_iter();
      Gdk::Rectangle visible_rect;
      get_visible_rect(visible_rect);
      Gdk::Rectangle iter_rect;
      get_iter_location(iter, iter_rect);
      iter_rect.set_width(1);
      if(!visible_rect.intersects(iter_rect)) {
        get_iter_at_location(iter, 0, visible_rect.get_y()+visible_rect.get_height()/3);
      }
      selection_dialog=std::unique_ptr<SelectionDialog>(new SelectionDialog(*this, get_buffer()->create_mark(iter), true, true));
      auto rows=std::make_shared<std::unordered_map<std::string, clang::Offset> >();
      auto methods=clang_tokens->get_cxx_methods();
      if(methods.size()==0)
        return;
      for(auto &method: methods) {
        std::string row=std::to_string(method.second.line)+": "+Glib::Markup::escape_text(method.first);
        //Add bold method token
        size_t token_end_pos=row.find('(');
        if(token_end_pos==0 || token_end_pos==std::string::npos)
          continue;
        if(token_end_pos>8 && row.substr(token_end_pos-4, 4)=="&gt;") {
          token_end_pos-=8;
          size_t angle_bracket_count=1;
          do {
            if(row.substr(token_end_pos-4, 4)=="&gt;") {
              angle_bracket_count++;
              token_end_pos-=4;
            }
            else if(row.substr(token_end_pos-4, 4)=="&lt;") {
              angle_bracket_count--;
              token_end_pos-=4;
            }
            else
              token_end_pos--;
          } while(angle_bracket_count>0 && token_end_pos>4);
        }
        auto pos=token_end_pos;
        do {
          pos--;
        } while(((row[pos]>='a' && row[pos]<='z') ||
               (row[pos]>='A' && row[pos]<='Z') ||
               (row[pos]>='0' && row[pos]<='9') || row[pos]=='_' || row[pos]=='~') && pos>0);
        row.insert(token_end_pos, "</b>");
        row.insert(pos+1, "<b>");
        (*rows)[row]=method.second;
        selection_dialog->add_row(row);
      }
      selection_dialog->on_select=[this, rows](const std::string& selected, bool hide_window) {
        auto offset=rows->at(selected);
        get_buffer()->place_cursor(get_buffer()->get_iter_at_line_index(offset.line-1, offset.index-1));
        scroll_to(get_buffer()->get_insert(), 0.0, 1.0, 0.5);
        delayed_tooltips_connection.disconnect();
      };
      selection_dialog->show();
    }
  };
  
  get_token_data=[this]() {
    const auto find_non_word_char=[](const std::string &str, size_t start_pos) {
      for(size_t c=start_pos;c<str.size();c++) {
        if(!((str[c]>='a' && str[c]<='z') ||
             (str[c]>='A' && str[c]<='Z') ||
             (str[c]>='0' && str[c]<='9') ||
             str[c]=='_'))
        return c;
      }
      return std::string::npos;
    };
    
    std::vector<std::string> data;
    if(parsed) {
      auto iter=get_buffer()->get_insert()->get_iter();
      auto line=(unsigned)iter.get_line();
      auto index=(unsigned)iter.get_line_index();
      for(auto &token: *clang_tokens) {
        auto cursor=token.get_cursor();
        if(token.get_kind()==clang::Token_Identifier && cursor.has_type()) {
          if(line==token.offsets.first.line-1 && index>=token.offsets.first.index-1 && index <=token.offsets.second.index-1) {
            auto referenced=cursor.get_referenced();
            if(referenced) {
              auto usr=referenced.get_usr();
              boost::filesystem::path referenced_path=referenced.get_source_location().get_path();
              
              //Terminal::get().print(usr+'\n', true); //TODO: remove
              
              //Return empty if referenced is within project
              if(referenced_path.generic_string().substr(0, this->project_path.generic_string().size()+1)==this->project_path.generic_string()+'/')
                return data;
              
              data.emplace_back("clang");
              
              //namespace
              size_t pos1, pos2=0;
              while((pos1=usr.find('@', pos2))!=std::string::npos && pos1+1<usr.size() && usr[pos1+1]=='N') {
                pos1+=3;
                pos2=find_non_word_char(usr, pos1);
                if(pos2!=std::string::npos) {
                  auto ns=usr.substr(pos1, pos2-pos1);
                  if(ns=="__1")
                    break;
                  data.emplace_back(ns);
                }
                else
                  break;
              }
              if(data.size()==1)
                data.emplace_back("");
              
              //template arguments
              size_t template_pos=usr.find('$');
              
              bool found_type_or_function=false;
              //type
              pos2=0;
              while(((pos1=usr.find("T@", pos2))!=std::string::npos || (pos1=usr.find("S@", pos2))!=std::string::npos) && pos1<template_pos) {
                found_type_or_function=true;
                pos1+=2;
                pos2=find_non_word_char(usr, pos1);
                if(pos2!=std::string::npos)
                  data.emplace_back(usr.substr(pos1, pos2-pos1));
                else {
                  data.emplace_back(usr.substr(pos1));
                  break;
                }
              }
              
              //function/constant//variable
              pos1=usr.find("F@");
              if(pos1==std::string::npos)
                pos1=usr.find("C@");
              if(pos1==std::string::npos)
                pos1=usr.find("I@");
              if(pos1!=std::string::npos) {
                pos1+=2;
                pos2=find_non_word_char(usr, pos1);
              }
              if(pos1!=std::string::npos) {
                found_type_or_function=true;
                if(pos2!=std::string::npos)
                  data.emplace_back(usr.substr(pos1, pos2-pos1));
                else
                  data.emplace_back(usr.substr(pos1));
              }
              
              //Sometimes a method is at end without a identifier it seems:
              if(!found_type_or_function && (pos1=usr.rfind('@'))!=std::string::npos) {
                pos1++;
                pos2=find_non_word_char(usr, pos1);
                if(pos2!=std::string::npos)
                  data.emplace_back(usr.substr(pos1, pos2-pos1));
                else
                  data.emplace_back(usr.substr(pos1));
              }
              
              break;
            }
          }
        }
      }
    }
    return data;
  };
  
  goto_next_diagnostic=[this]() {
    if(parsed) {
      auto insert_offset=get_buffer()->get_insert()->get_iter().get_offset();
      for(auto offset: diagnostic_offsets) {
        if(offset>insert_offset) {
          get_buffer()->place_cursor(get_buffer()->get_iter_at_offset(offset));
          scroll_to(get_buffer()->get_insert(), 0.0, 1.0, 0.5);
          return;
        }
      }
      if(diagnostic_offsets.size()>0) {
        auto iter=get_buffer()->get_iter_at_offset(*diagnostic_offsets.begin());
        get_buffer()->place_cursor(iter);
        scroll_to(get_buffer()->get_insert(), 0.0, 1.0, 0.5);
      }
    }
  };
  
  apply_fix_its=[this]() {
    std::vector<std::pair<Glib::RefPtr<Gtk::TextMark>, Glib::RefPtr<Gtk::TextMark> > > fix_it_marks;
    if(parsed) {
      for(auto &fix_it: fix_its) {
        auto start_iter=get_buffer()->get_iter_at_line_index(fix_it.offsets.first.line-1, fix_it.offsets.first.index-1);
        auto end_iter=get_buffer()->get_iter_at_line_index(fix_it.offsets.second.line-1, fix_it.offsets.second.index-1);
        fix_it_marks.emplace_back(get_buffer()->create_mark(start_iter), get_buffer()->create_mark(end_iter));
      }
      size_t c=0;
      get_source_buffer()->begin_user_action();
      for(auto &fix_it: fix_its) {
        if(fix_it.type==FixIt::Type::INSERT) {
          get_buffer()->insert(fix_it_marks[c].first->get_iter(), fix_it.source);
        }
        if(fix_it.type==FixIt::Type::REPLACE) {
          get_buffer()->erase(fix_it_marks[c].first->get_iter(), fix_it_marks[c].second->get_iter());
          get_buffer()->insert(fix_it_marks[c].first->get_iter(), fix_it.source);
        }
        if(fix_it.type==FixIt::Type::ERASE) {
          get_buffer()->erase(fix_it_marks[c].first->get_iter(), fix_it_marks[c].second->get_iter());
        }
        c++;
      }
      for(auto &mark_pair: fix_it_marks) {
        get_buffer()->delete_mark(mark_pair.first);
        get_buffer()->delete_mark(mark_pair.second);
      }
      get_source_buffer()->end_user_action();
    }
  };
}

void Source::ClangViewRefactor::tag_similar_tokens(const Token &token) {
  if(parsed) {
    if(token && last_tagged_token!=token) {
      for(auto &mark: similar_token_marks) {
        get_buffer()->remove_tag(similar_tokens_tag, mark.first->get_iter(), mark.second->get_iter());
        get_buffer()->delete_mark(mark.first);
        get_buffer()->delete_mark(mark.second);
      }
      similar_token_marks.clear();
      auto offsets=clang_tokens->get_similar_token_offsets(static_cast<clang::CursorKind>(token.type), token.spelling, token.usr);
      for(auto &offset: offsets) {
        auto start_iter=get_buffer()->get_iter_at_line_index(offset.first.line-1, offset.first.index-1);
        auto end_iter=get_buffer()->get_iter_at_line_index(offset.second.line-1, offset.second.index-1);
        get_buffer()->apply_tag(similar_tokens_tag, start_iter, end_iter);
        similar_token_marks.emplace_back(get_buffer()->create_mark(start_iter), get_buffer()->create_mark(end_iter)); 
      }
      last_tagged_token=token;
    }
  }
  if(!token && last_tagged_token) {
    for(auto &mark: similar_token_marks) {
      get_buffer()->remove_tag(similar_tokens_tag, mark.first->get_iter(), mark.second->get_iter());
      get_buffer()->delete_mark(mark.first);
      get_buffer()->delete_mark(mark.second);
    }
    similar_token_marks.clear();
    last_tagged_token=Token();
  }
}

Source::ClangView::ClangView(const boost::filesystem::path &file_path, const boost::filesystem::path& project_path, Glib::RefPtr<Gsv::Language> language): ClangViewRefactor(file_path, project_path, language) {
  if(language) {
    get_source_buffer()->set_highlight_syntax(true);
    get_source_buffer()->set_language(language);
  }
}

void Source::ClangView::async_delete() {
  delayed_reparse_connection.disconnect();
  parse_postprocess_connection.disconnect();
  parse_preprocess_connection.disconnect();
  parse_error_connection.disconnect();
  autocomplete_error_connection.disconnect();
  autocomplete_restart_connection.disconnect();
  autocomplete_error_connection.disconnect();
  do_restart_parse_connection.disconnect();
  delayed_tag_similar_tokens_connection.disconnect();
  ClangViewAutocomplete::async_delete();
}

