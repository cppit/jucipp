#include "source_clang.h"
#include "config.h"
#include "terminal.h"
#include "project_build.h"
#ifdef JUCI_ENABLE_DEBUG
#include "debug_lldb.h"
#endif
#include "info.h"
#include "dialogs.h"
#include "ctags.h"
#include "selection_dialog.h"
#include "filesystem.h"
#include "compile_commands.h"
#include "usages_clang.h"

clangmm::Index Source::ClangViewParse::clang_index(0, 0);

Source::ClangViewParse::ClangViewParse(const boost::filesystem::path &file_path, Glib::RefPtr<Gsv::Language> language):
    Source::View(file_path, language) {
  Usages::Clang::erase_cache(file_path);
  
  auto tag_table=get_buffer()->get_tag_table();
  for (auto &item : Config::get().source.clang_types) {
    if(!tag_table->lookup(item.second)) {
      get_buffer()->create_tag(item.second);
    }
  }
  configure();
  
  parsing_in_progress=Terminal::get().print_in_progress("Parsing "+file_path.string());
  parse_initialize();
  
  get_buffer()->signal_changed().connect([this]() {
    soft_reparse(true);
  });
}

bool Source::ClangViewParse::save() {
  if(!Source::View::save())
     return false;
  
  if(language->get_id()=="chdr" || language->get_id()=="cpphdr") {
    for(auto &view: views) {
      if(auto clang_view=dynamic_cast<Source::ClangView*>(view)) {
        if(this!=clang_view)
          clang_view->soft_reparse_needed=true;
      }
    }
  }
  return true;
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
      }
    }
  }
}

void Source::ClangViewParse::parse_initialize() {
  hide_tooltips();
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
  auto &buffer_raw=const_cast<std::string&>(buffer.raw());
  if(language && (language->get_id()=="chdr" || language->get_id()=="cpphdr"))
    clangmm::remove_include_guard(buffer_raw);
  
  auto build=Project::Build::create(file_path);
  if(build->project_path.empty())
    Info::get().print(file_path.filename().string()+": could not find a supported build system");
  build->update_default();
  auto arguments=CompileCommands::get_arguments(build->get_default_path(), file_path);
  clang_tu = std::make_unique<clangmm::TranslationUnit>(clang_index, file_path.string(), arguments, buffer_raw);
  clang_tokens=clang_tu->get_tokens();
  clang_tokens_offsets.clear();
  clang_tokens_offsets.reserve(clang_tokens->size());
  for(auto &token: *clang_tokens)
    clang_tokens_offsets.emplace_back(token.get_source_range().get_offsets());
  update_syntax();
  
  status_state="parsing...";
  if(update_status_state)
    update_status_state(this);
  parse_thread=std::thread([this]() {
    while(true) {
      while(parse_state==ParseState::PROCESSING && parse_process_state!=ParseProcessState::STARTING && parse_process_state!=ParseProcessState::PROCESSING)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      if(parse_state!=ParseState::PROCESSING)
        break;
      auto expected=ParseProcessState::STARTING;
      std::unique_lock<std::mutex> parse_lock(parse_mutex, std::defer_lock);
      if(parse_process_state.compare_exchange_strong(expected, ParseProcessState::PREPROCESSING)) {
        dispatcher.post([this] {
          auto expected=ParseProcessState::PREPROCESSING;
          std::unique_lock<std::mutex> parse_lock(parse_mutex, std::defer_lock);
          if(parse_lock.try_lock()) {
            if(parse_process_state.compare_exchange_strong(expected, ParseProcessState::PROCESSING))
              parse_thread_buffer=get_buffer()->get_text();
            parse_lock.unlock();
          }
          else
            parse_process_state.compare_exchange_strong(expected, ParseProcessState::STARTING);
        });
      }
      else if (parse_process_state==ParseProcessState::PROCESSING && parse_lock.try_lock()) {
        auto &parse_thread_buffer_raw=const_cast<std::string&>(parse_thread_buffer.raw());
        if(this->language && (this->language->get_id()=="chdr" || this->language->get_id()=="cpphdr"))
          clangmm::remove_include_guard(parse_thread_buffer_raw);
        auto status=clang_tu->reparse(parse_thread_buffer_raw);
        parsing_in_progress->done("done");
        if(status==0) {
          auto expected=ParseProcessState::PROCESSING;
          if(parse_process_state.compare_exchange_strong(expected, ParseProcessState::POSTPROCESSING)) {
            clang_tokens=clang_tu->get_tokens();
            clang_tokens_offsets.clear();
            clang_tokens_offsets.reserve(clang_tokens->size());
            for(auto &token: *clang_tokens)
              clang_tokens_offsets.emplace_back(token.get_source_range().get_offsets());
            clang_diagnostics=clang_tu->get_diagnostics();
            parse_lock.unlock();
            dispatcher.post([this] {
              std::unique_lock<std::mutex> parse_lock(parse_mutex, std::defer_lock);
              if(parse_lock.try_lock()) {
                auto expected=ParseProcessState::POSTPROCESSING;
                if(parse_process_state.compare_exchange_strong(expected, ParseProcessState::IDLE)) {
                  update_syntax();
                  update_diagnostics();
                  parsed=true;
                  status_state="";
                  if(update_status_state)
                    update_status_state(this);
                }
                parse_lock.unlock();
              }
            });
          }
          else
            parse_lock.unlock();
        }
        else {
          parse_state=ParseState::STOP;
          parse_lock.unlock();
          dispatcher.post([this] {
            Terminal::get().print("Error: failed to reparse "+this->file_path.string()+".\n", true);
            status_state="";
            if(update_status_state)
              update_status_state(this);
            status_diagnostics=std::make_tuple(0, 0, 0);
            if(update_status_diagnostics)
              update_status_diagnostics(this);
            parsing_in_progress->cancel("failed");
          });
        }
      }
    }
  });
}

void Source::ClangViewParse::soft_reparse(bool delayed) {
  soft_reparse_needed=false;
  parsed=false;
  if(parse_state!=ParseState::PROCESSING)
    return;
  parse_process_state=ParseProcessState::IDLE;
  delayed_reparse_connection.disconnect();
  delayed_reparse_connection=Glib::signal_timeout().connect([this]() {
    parsed=false;
    auto expected=ParseProcessState::IDLE;
    if(parse_process_state.compare_exchange_strong(expected, ParseProcessState::STARTING)) {
      status_state="parsing...";
      if(update_status_state)
        update_status_state(this);
    }
    return false;
  }, delayed?1000:0);
}

void Source::ClangViewParse::update_syntax() {
  auto buffer=get_buffer();
  const auto apply_tag=[this, buffer](const std::pair<clangmm::Offset, clangmm::Offset> &offsets, int type) {
    auto type_it=Config::get().source.clang_types.find(type);
    if(type_it!=Config::get().source.clang_types.end()) {
      last_syntax_tags.emplace(type_it->second);
      Gtk::TextIter begin_iter = buffer->get_iter_at_line_index(offsets.first.line-1, offsets.first.index-1);
      Gtk::TextIter end_iter  = buffer->get_iter_at_line_index(offsets.second.line-1, offsets.second.index-1);
      buffer->apply_tag_by_name(type_it->second, begin_iter, end_iter);
    }
  };
  
  for(auto &tag: last_syntax_tags)
    buffer->remove_tag_by_name(tag, buffer->begin(), buffer->end());
  last_syntax_tags.clear();
  
  for(size_t c=0;c<clang_tokens->size();++c) {
    auto &token=(*clang_tokens)[c];
    auto &token_offsets=clang_tokens_offsets[c];
    //if(token.get_kind()==clangmm::Token::Kind::Token_Punctuation)
      //ranges.emplace_back(token_offset, static_cast<int>(token.get_cursor().get_kind()));
    auto token_kind=token.get_kind();
    if(token_kind==clangmm::Token::Kind::Keyword)
      apply_tag(token_offsets, 702);
    else if(token_kind==clangmm::Token::Kind::Identifier) {
      auto cursor_kind=token.get_cursor().get_kind();
      if(cursor_kind==clangmm::Cursor::Kind::DeclRefExpr || cursor_kind==clangmm::Cursor::Kind::MemberRefExpr)
        cursor_kind=token.get_cursor().get_referenced().get_kind();
      if(cursor_kind!=clangmm::Cursor::Kind::PreprocessingDirective)
        apply_tag(token_offsets, static_cast<int>(cursor_kind));
    }
    else if(token_kind==clangmm::Token::Kind::Literal)
      apply_tag(token_offsets, static_cast<int>(clangmm::Cursor::Kind::StringLiteral));
    else if(token_kind==clangmm::Token::Kind::Comment)
      apply_tag(token_offsets, 705);
  }
}

void Source::ClangViewParse::update_diagnostics() {
  diagnostic_offsets.clear();
  diagnostic_tooltips.clear();
  fix_its.clear();
  get_buffer()->remove_tag_by_name("def:warning_underline", get_buffer()->begin(), get_buffer()->end());
  get_buffer()->remove_tag_by_name("def:error_underline", get_buffer()->begin(), get_buffer()->end());
  size_t num_warnings=0;
  size_t num_errors=0;
  size_t num_fix_its=0;
  for(auto &diagnostic: clang_diagnostics) {
    if(diagnostic.path==file_path.string()) {      
      int line=diagnostic.offsets.first.line-1;
      if(line<0 || line>=get_buffer()->get_line_count())
        line=get_buffer()->get_line_count()-1;
      auto start=get_iter_at_line_end(line);
      int index=diagnostic.offsets.first.index-1;
      if(index>=0 && index<start.get_line_index())
        start=get_buffer()->get_iter_at_line_index(line, index);
      if(start.ends_line()) {
        while(!start.is_start() && start.ends_line())
          start.backward_char();
      }
      diagnostic_offsets.emplace(start.get_offset());
      
      line=diagnostic.offsets.second.line-1;
      if(line<0 || line>=get_buffer()->get_line_count())
        line=get_buffer()->get_line_count()-1;
      auto end=get_iter_at_line_end(line);
      index=diagnostic.offsets.second.index-1;
      if(index>=0 && index<end.get_line_index())
        end=get_buffer()->get_iter_at_line_index(line, index);
            
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
        auto clang_offsets=fix_it.offsets;
        std::pair<Offset, Offset> offsets;
        offsets.first.line=clang_offsets.first.line-1;
        offsets.first.index=clang_offsets.first.index-1;
        offsets.second.line=clang_offsets.second.line-1;
        offsets.second.index=clang_offsets.second.index-1;
        
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
      diagnostic_tooltips.emplace_back(create_tooltip_buffer, this, get_buffer()->create_mark(start), get_buffer()->create_mark(end));
    
      get_buffer()->apply_tag_by_name(diagnostic_tag_name+"_underline", start, end);
      auto iter=get_buffer()->get_insert()->get_iter();
      if(iter.ends_line()) {
        auto next_iter=iter;
        if(next_iter.forward_char())
          get_buffer()->remove_tag_by_name(diagnostic_tag_name+"_underline", iter, next_iter);
      }
    }
  }
  status_diagnostics=std::make_tuple(num_warnings, num_errors, num_fix_its);
  if(update_status_diagnostics)
    update_status_diagnostics(this);
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
    if(iter.ends_line() && location_x>iter_rectangle.get_x())
      return;
    
    auto line=static_cast<unsigned>(iter.get_line());
    auto index=static_cast<unsigned>(iter.get_line_index());
    type_tooltips.clear();
    for(size_t c=clang_tokens->size()-1;c!=static_cast<size_t>(-1);--c) {
      auto &token=(*clang_tokens)[c];
      auto &token_offsets=clang_tokens_offsets[c];
      if(token.is_identifier()) {
        if(line==token_offsets.first.line-1 && index>=token_offsets.first.index-1 && index <=token_offsets.second.index-1) {
          auto cursor=token.get_cursor();
          auto referenced=cursor.get_referenced();
          if(referenced) {
            auto start=get_buffer()->get_iter_at_line_index(token_offsets.first.line-1, token_offsets.first.index-1);
            auto end=get_buffer()->get_iter_at_line_index(token_offsets.second.line-1, token_offsets.second.index-1);
            auto create_tooltip_buffer=[this, &token, &token_offsets]() {
              auto tooltip_buffer=Gtk::TextBuffer::create(get_buffer()->get_tag_table());
              tooltip_buffer->insert_with_tag(tooltip_buffer->get_insert()->get_iter(), "Type: "+token.get_cursor().get_type_description(), "def:note");
              auto brief_comment=token.get_cursor().get_brief_comments();
              if(brief_comment!="")
                tooltip_buffer->insert_with_tag(tooltip_buffer->get_insert()->get_iter(), "\n\n"+brief_comment, "def:note");
  
#ifdef JUCI_ENABLE_DEBUG
              if(Debug::LLDB::get().is_stopped()) {
                auto location=token.get_cursor().get_referenced().get_source_location();
                Glib::ustring value_type="Value";
                
                auto start=get_buffer()->get_iter_at_line_index(token_offsets.first.line-1, token_offsets.first.index-1);
                auto end=get_buffer()->get_iter_at_line_index(token_offsets.second.line-1, token_offsets.second.index-1);
                auto iter=start;
                while((*iter>='a' && *iter<='z') || (*iter>='A' && *iter<='Z') || (*iter>='0' && *iter<='9') || *iter=='_' || *iter=='.') {
                  start=iter;
                  if(!iter.backward_char())
                    break;
                  if(*iter=='>') {
                    if(!(iter.backward_char() && *iter=='-' && iter.backward_char()))
                      break;
                  }
                  else if(*iter==':') {
                    if(!(iter.backward_char() && *iter==':' && iter.backward_char()))
                      break;
                  }
                }
                auto spelling=get_buffer()->get_text(start, end).raw();
                
                Glib::ustring debug_value=Debug::LLDB::get().get_value(spelling, location.get_path(), location.get_offset().line, location.get_offset().index);
                if(debug_value.empty()) {
                  value_type="Return value";
                  auto cursor=token.get_cursor();
                  auto offsets=cursor.get_source_range().get_offsets();
                  debug_value=Debug::LLDB::get().get_return_value(cursor.get_source_location().get_path(), offsets.first.line, offsets.first.index);
                }
                if(!debug_value.empty()) {
                  size_t pos=debug_value.find(" = ");
                  if(pos!=Glib::ustring::npos) {
                    Glib::ustring::iterator iter;
                    while(!debug_value.validate(iter)) {
                      auto next_char_iter=iter;
                      next_char_iter++;
                      debug_value.replace(iter, next_char_iter, "?");
                    }
                    tooltip_buffer->insert_with_tag(tooltip_buffer->get_insert()->get_iter(), "\n\n"+value_type+": "+debug_value.substr(pos+3, debug_value.size()-(pos+3)-1), "def:note");
                  }
                }
              }
#endif
              
              return tooltip_buffer;
            };
            
            type_tooltips.emplace_back(create_tooltip_buffer, this, get_buffer()->create_mark(start), get_buffer()->create_mark(end));
            type_tooltips.show();
            return;
          }
        }
      }
    }
  }
}


Source::ClangViewAutocomplete::ClangViewAutocomplete(const boost::filesystem::path &file_path, Glib::RefPtr<Gsv::Language> language):
    Source::ClangViewParse(file_path, language), autocomplete(this, interactive_completion, last_keyval, true) {
  non_interactive_completion=[this] {
    if(CompletionDialog::get() && CompletionDialog::get()->is_visible())
      return;
    autocomplete.run();
  };
  
  autocomplete.is_processing=[this] {
    return parse_state==ParseState::PROCESSING;
  };
  
  autocomplete.reparse=[this] {
    soft_reparse(true);
  };
  
  autocomplete.cancel_reparse=[this] {
    delayed_reparse_connection.disconnect();
  };
  
  autocomplete.get_parse_lock=[this]() {
    return std::make_unique<std::lock_guard<std::mutex>>(parse_mutex);
  };
  
  autocomplete.stop_parse=[this]() {
    parse_process_state=ParseProcessState::IDLE;
  };
  
  // Activate argument completions
  get_buffer()->signal_changed().connect([this] {
    if(!interactive_completion)
      return;
    if(CompletionDialog::get() && CompletionDialog::get()->is_visible())
      return;
    if(!has_focus())
      return;
    if(show_arguments)
      autocomplete.stop();
    show_arguments=false;
    delayed_show_arguments_connection.disconnect();
    delayed_show_arguments_connection=Glib::signal_timeout().connect([this]() {
      if(get_buffer()->get_has_selection())
        return false;
      if(CompletionDialog::get() && CompletionDialog::get()->is_visible())
        return false;
      if(!has_focus())
        return false;
      if(is_possible_parameter()) {
        autocomplete.stop();
        autocomplete.run();
      }
      return false;
    }, 500);
  }, false);

  // Remove argument completions
  signal_key_press_event().connect([this](GdkEventKey *key) {
    if(show_arguments && CompletionDialog::get() && CompletionDialog::get()->is_visible() &&
       key->keyval != GDK_KEY_Down && key->keyval != GDK_KEY_Up &&
       key->keyval != GDK_KEY_Return && key->keyval != GDK_KEY_KP_Enter &&
       key->keyval != GDK_KEY_ISO_Left_Tab && key->keyval != GDK_KEY_Tab &&
       (key->keyval < GDK_KEY_Shift_L || key->keyval > GDK_KEY_Hyper_R)) {
      get_buffer()->erase(CompletionDialog::get()->start_mark->get_iter(), get_buffer()->get_insert()->get_iter());
      CompletionDialog::get()->hide();
    }
    return false;
  }, false);

  autocomplete.is_continue_key=[](guint keyval) {
    if((keyval>='0' && keyval<='9') || (keyval>='a' && keyval<='z') || (keyval>='A' && keyval<='Z') || keyval=='_')
      return true;
    
    return false;
  };
  
  autocomplete.is_restart_key=[this](guint keyval) {
    auto iter=get_buffer()->get_insert()->get_iter();
    iter.backward_chars(2);
    if(keyval=='.' || (keyval==':' && *iter==':') || (keyval=='>' && *iter=='-'))
      return true;
    return false;
  };
  
  autocomplete.run_check=[this]() {
    auto iter=get_buffer()->get_insert()->get_iter();
    iter.backward_char();
    if(!is_code_iter(iter))
      return false;
    
    show_arguments=false;
    
    std::string line=" "+get_line_before();
    const static std::regex in_specified_namespace("^.*[a-zA-Z0-9_\\)\\]\\>](->|\\.|::)([a-zA-Z0-9_]*)$");
    const static std::regex within_namespace("^.*[^a-zA-Z0-9_]+([a-zA-Z0-9_]{3,})$");
    std::smatch sm;
    if(std::regex_match(line, sm, in_specified_namespace)) {
      {
        std::unique_lock<std::mutex> lock(autocomplete.prefix_mutex);
        autocomplete.prefix=sm[2].str();
      }
      if(autocomplete.prefix.size()==0 || autocomplete.prefix[0]<'0' || autocomplete.prefix[0]>'9')
        return true;
    }
    else if(std::regex_match(line, sm, within_namespace)) {
      {
        std::unique_lock<std::mutex> lock(autocomplete.prefix_mutex);
        autocomplete.prefix=sm[1].str();
      }
      if(autocomplete.prefix.size()==0 || autocomplete.prefix[0]<'0' || autocomplete.prefix[0]>'9')
        return true;
    }
    else if(is_possible_parameter()) {
      show_arguments=true;
      std::unique_lock<std::mutex> lock(autocomplete.prefix_mutex);
      autocomplete.prefix="";
      return true;
    }
    else if(!interactive_completion) {
      auto end_iter=get_buffer()->get_insert()->get_iter();
      auto iter=end_iter;
      while(iter.backward_char() && autocomplete.is_continue_key(*iter)) {}
      if(iter!=end_iter)
        iter.forward_char();
      std::unique_lock<std::mutex> lock(autocomplete.prefix_mutex);
      autocomplete.prefix=get_buffer()->get_text(iter, end_iter);
      return true;
    }
    
    return false;
  };
  
  autocomplete.before_add_rows=[this] {
    status_state="autocomplete...";
    if(update_status_state)
      update_status_state(this);
  };
  
  autocomplete.after_add_rows=[this] {
    status_state="";
    if(update_status_state)
      update_status_state(this);
  };
  
  autocomplete.on_add_rows_error=[this] {
    Terminal::get().print("Error: autocomplete failed, reparsing "+this->file_path.string()+"\n", true);
    full_reparse();
  };
  
  autocomplete.add_rows=[this](std::string &buffer, int line_number, int column) {
    if(this->language && (this->language->get_id()=="chdr" || this->language->get_id()=="cpphdr"))
      clangmm::remove_include_guard(buffer);
    auto results=clang_tu->get_code_completions(buffer, line_number, column);
    if(results.cx_results==nullptr) {
      auto expected=ParseState::PROCESSING;
      parse_state.compare_exchange_strong(expected, ParseState::RESTARTING);
      return;
    }
    
    if(autocomplete.state==Autocomplete::State::STARTING) {
      std::string prefix_copy;
      {
        std::lock_guard<std::mutex> lock(autocomplete.prefix_mutex);
        prefix_copy=autocomplete.prefix;
      }
      
      for (unsigned i = 0; i < results.size(); ++i) {
        auto result=results.get(i);
        if(result.available()) {
          std::string text;
          if(show_arguments) {
            class Recursive {
            public:
              static void f(const clangmm::CompletionString &completion_string, std::string &text) {
                for(unsigned i = 0; i < completion_string.get_num_chunks(); ++i) {
                  auto kind=static_cast<clangmm::CompletionChunkKind>(clang_getCompletionChunkKind(completion_string.cx_completion_sting, i));
                  if(kind==clangmm::CompletionChunk_Optional)
                    f(clangmm::CompletionString(clang_getCompletionChunkCompletionString(completion_string.cx_completion_sting, i)), text);
                  else if(kind==clangmm::CompletionChunk_CurrentParameter) {
                    auto chunk_cstr=clangmm::String(clang_getCompletionChunkText(completion_string.cx_completion_sting, i));
                    text+=chunk_cstr.c_str;
                  }
                }
              }
            };
            Recursive::f(result, text);
            if(!text.empty()) {
              bool already_added=false;
              for(auto &pair: autocomplete.rows) {
                if(pair.first==text) {
                  already_added=true;
                  break;
                }
              }
              if(!already_added)
                autocomplete.rows.emplace_back(std::move(text), result.get_brief_comment());
            }
          }
          else {
            std::string return_text;
            bool match=false;
            for(unsigned i = 0; i < result.get_num_chunks(); ++i) {
              auto kind=static_cast<clangmm::CompletionChunkKind>(clang_getCompletionChunkKind(result.cx_completion_sting, i));
              if(kind!=clangmm::CompletionChunk_Informative) {
                auto chunk_cstr=clangmm::String(clang_getCompletionChunkText(result.cx_completion_sting, i));
                if(kind==clangmm::CompletionChunk_TypedText) {
                  if(strlen(chunk_cstr.c_str)>=prefix_copy.size() && prefix_copy.compare(0, prefix_copy.size(), chunk_cstr.c_str, prefix_copy.size())==0)
                    match = true;
                  else
                    break;
                }
                if(kind==clangmm::CompletionChunk_ResultType)
                  return_text=std::string("  →  ")+chunk_cstr.c_str;
                else
                  text+=chunk_cstr.c_str;
              }
            }
            if(match && !text.empty()) {
              if(!return_text.empty())
                text+=return_text;
              autocomplete.rows.emplace_back(std::move(text), result.get_brief_comment());
            }
          }
        }
      }
    }
  };
  
  autocomplete.setup_dialog=[this] {
    CompletionDialog::get()->on_show=[this] {
      hide_tooltips();
    };
    
    CompletionDialog::get()->on_select=[this](unsigned int index, const std::string &text, bool hide_window) {
      if(index>=autocomplete.rows.size())
        return;
      std::string row;
      auto pos=text.find("  →  ");
      if(pos!=std::string::npos)
        row=text.substr(0, pos);
      else
        row=text;
      //erase existing variable or function before insert iter
      get_buffer()->erase(CompletionDialog::get()->start_mark->get_iter(), get_buffer()->get_insert()->get_iter());
      //do not insert template argument or function parameters if they already exist
      auto iter=get_buffer()->get_insert()->get_iter();
      if(*iter=='<' || *iter=='(') {
        auto bracket_pos=row.find(*iter);
        if(bracket_pos!=std::string::npos) {
          row=row.substr(0, bracket_pos);
        }
      }
      //Fixes for the most commonly used stream manipulators
      auto manipulators_map=autocomplete_manipulators_map();
      auto it=manipulators_map.find(row);
      if(it!=manipulators_map.end())
        row=it->second;
      get_buffer()->insert(CompletionDialog::get()->start_mark->get_iter(), row);
      //if selection is finalized, select text inside template arguments or function parameters
      if(hide_window) {
        size_t start_pos=std::string::npos;
        size_t end_pos=std::string::npos;
        if(show_arguments) {
          start_pos=0;
          end_pos=row.size();
        }
        else {
          auto para_pos=row.find('(');
          auto angle_pos=row.find('<');
          if(angle_pos<para_pos) {
            start_pos=angle_pos+1;
            end_pos=row.find('>');
          }
          else if(para_pos!=std::string::npos) {
            start_pos=para_pos+1;
            end_pos=row.size()-1;
          }
          if(start_pos==std::string::npos || end_pos==std::string::npos) {
            if((start_pos=row.find('\"'))!=std::string::npos) {
              end_pos=row.find('\"', start_pos+1);
              ++start_pos;
            }
          }
        }
        if(start_pos==std::string::npos || end_pos==std::string::npos) {
          if((start_pos=row.find(' '))!=std::string::npos) {
            std::vector<std::string> parameters={"expression", "arguments", "identifier", "type name", "qualifier::name", "macro", "condition"};
            for(auto &parameter: parameters) {
              if((start_pos=row.find(parameter, start_pos+1))!=std::string::npos) {
                end_pos=start_pos+parameter.size();
                break;
              }
            }
          }
        }
        
        if(start_pos!=std::string::npos && end_pos!=std::string::npos) {
          int start_offset=CompletionDialog::get()->start_mark->get_iter().get_offset()+start_pos;
          int end_offset=CompletionDialog::get()->start_mark->get_iter().get_offset()+end_pos;
          auto size=get_buffer()->size();
          if(start_offset!=end_offset && start_offset<size && end_offset<size)
            get_buffer()->select_range(get_buffer()->get_iter_at_offset(start_offset), get_buffer()->get_iter_at_offset(end_offset));
        }
        else {
          //new autocomplete after for instance when selecting "std::"
          auto iter=get_buffer()->get_insert()->get_iter();
          if(iter.backward_char() && *iter==':') {
            autocomplete.run();
            return;
          }
        }
      }
    };
  };
}

bool Source::ClangViewAutocomplete::is_possible_parameter() {
  auto iter=get_buffer()->get_insert()->get_iter();
  if(iter.backward_char() && (!interactive_completion || last_keyval=='(' || last_keyval==',' || last_keyval==' ' ||
                              last_keyval==GDK_KEY_Return || last_keyval==GDK_KEY_KP_Enter)) {
    while((*iter==' ' || *iter=='\t' || *iter=='\n' || *iter=='\r') && iter.backward_char()) {}
    if(*iter=='(' || *iter==',')
      return true;
  }
  return false;
}

const std::unordered_map<std::string, std::string> &Source::ClangViewAutocomplete::autocomplete_manipulators_map() {
  //TODO: feel free to add more
  static std::unordered_map<std::string, std::string> map={
    {"endl(basic_ostream<_CharT, _Traits> &__os)", "endl"},
    {"flush(basic_ostream<_CharT, _Traits> &__os)", "flush"},
    {"hex(std::ios_base &__str)", "hex"}, //clang++ headers
    {"hex(std::ios_base &__base)", "hex"}, //g++ headers
    {"dec(std::ios_base &__str)", "dec"},
    {"dec(std::ios_base &__base)", "dec"}
  };
  return map;
}


Source::ClangViewRefactor::ClangViewRefactor(const boost::filesystem::path &file_path, Glib::RefPtr<Gsv::Language> language) :
    Source::ClangViewParse(file_path, language) {
  similar_identifiers_tag=get_buffer()->create_tag();
  similar_identifiers_tag->property_weight()=Pango::WEIGHT_ULTRAHEAVY;
  
  get_buffer()->signal_changed().connect([this]() {
    if(last_tagged_identifier) {
      for(auto &mark: similar_identifiers_marks) {
        get_buffer()->remove_tag(similar_identifiers_tag, mark.first->get_iter(), mark.second->get_iter());
        get_buffer()->delete_mark(mark.first);
        get_buffer()->delete_mark(mark.second);
      }
      similar_identifiers_marks.clear();
      last_tagged_identifier=Identifier();
    }
  });
  
  get_token_spelling=[this]() {
    if(!parsed) {
      Info::get().print("Buffer is parsing");
      return std::string();
    }
    auto identifier=get_identifier();
    if(identifier.spelling.empty() ||
       identifier.spelling=="::" || identifier.spelling=="," || identifier.spelling=="=" ||
       identifier.spelling=="(" || identifier.spelling==")" ||
       identifier.spelling=="[" || identifier.spelling=="]") {
      Info::get().print("No valid symbol found at current cursor location");
      return std::string();
    }
    return identifier.spelling;
  };
  
  rename_similar_tokens=[this](const std::string &text) {
    if(!parsed) {
      Info::get().print("Buffer is parsing");
      return;
    }
    auto identifier=get_identifier();
    if(identifier) {
      wait_parsing();
      
      std::vector<clangmm::TranslationUnit*> translation_units;
      translation_units.emplace_back(clang_tu.get());
      for(auto &view: views) {
        if(view!=this) {
          if(auto clang_view=dynamic_cast<Source::ClangView*>(view))
            translation_units.emplace_back(clang_view->clang_tu.get());
        }
      }
      
      auto build=Project::Build::create(this->file_path);
      auto usages=Usages::Clang::get_usages(build->project_path, build->get_default_path(), build->get_debug_path(), identifier.spelling, identifier.cursor, translation_units);
      
      std::vector<Source::View*> renamed_views;
      std::vector<Usages::Clang::Usages*> usages_renamed;
      for(auto &usage: usages) {
        size_t line_c=usage.lines.size()-1;
        auto view_it=views.end();
        for(auto it=views.begin();it!=views.end();++it) {
          if((*it)->file_path==usage.path) {
            view_it=it;
            break;
          }
        }
        if(view_it!=views.end()) {
          (*view_it)->get_buffer()->begin_user_action();
          for(auto offset_it=usage.offsets.rbegin();offset_it!=usage.offsets.rend();++offset_it) {
            auto start_iter=(*view_it)->get_buffer()->get_iter_at_line_index(offset_it->first.line-1, offset_it->first.index-1);
            auto end_iter=(*view_it)->get_buffer()->get_iter_at_line_index(offset_it->second.line-1, offset_it->second.index-1);
            (*view_it)->get_buffer()->erase(start_iter, end_iter);
            start_iter=(*view_it)->get_buffer()->get_iter_at_line_index(offset_it->first.line-1, offset_it->first.index-1);
            (*view_it)->get_buffer()->insert(start_iter, text);
            if(offset_it->first.index-1<usage.lines[line_c].size())
              usage.lines[line_c].replace(offset_it->first.index-1, offset_it->second.index-offset_it->first.index, text);
            --line_c;
          }
          (*view_it)->get_buffer()->end_user_action();
          (*view_it)->save();
          renamed_views.emplace_back(*view_it);
          usages_renamed.emplace_back(&usage);
        }
        else {
          std::string buffer;
          {
            std::ifstream stream(usage.path.string(), std::ifstream::binary);
            if(stream)
              buffer.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
          }
          std::ofstream stream(usage.path.string(), std::ifstream::binary);
          if(!buffer.empty() && stream) {
            std::vector<size_t> lines_start_pos={0};
            for(size_t c=0;c<buffer.size();++c) {
              if(buffer[c]=='\n')
                lines_start_pos.emplace_back(c+1);
            }
            for(auto offset_it=usage.offsets.rbegin();offset_it!=usage.offsets.rend();++offset_it) {
              auto start_line=offset_it->first.line-1;
              auto end_line=offset_it->second.line-1;
              if(start_line<lines_start_pos.size() && end_line<lines_start_pos.size()) {
                auto start=lines_start_pos[start_line]+offset_it->first.index-1;
                auto end=lines_start_pos[end_line]+offset_it->second.index-1;
                if(start<buffer.size() && end<=buffer.size())
                  buffer.replace(start, end-start, text);
              }
              if(offset_it->first.index-1<usage.lines[line_c].size())
                usage.lines[line_c].replace(offset_it->first.index-1, offset_it->second.index-offset_it->first.index, text);
              --line_c;
            }
            stream.write(buffer.data(), buffer.size());
            usages_renamed.emplace_back(&usage);
          }
          else
            Terminal::get().print("Error: could not write to file "+usage.path.string()+'\n', true);
        }
      }
      
      if(!usages_renamed.empty()) {
        Terminal::get().print("Renamed ");
        Terminal::get().print(identifier.spelling, true);
        Terminal::get().print(" to ");
        Terminal::get().print(text, true);
        Terminal::get().print(" at:\n");
      }
      for(auto &usage: usages_renamed) {
        size_t line_c=0;
        for(auto &offset: usage->offsets) {
          Terminal::get().print(filesystem::get_short_path(usage->path).string()+':'+std::to_string(offset.first.line)+':'+std::to_string(offset.first.index)+": ");
          auto &line=usage->lines[line_c];
          auto index=offset.first.index-1;
          unsigned start=0;
          for(auto &chr: line) {
            if(chr!=' ' && chr!='\t')
              break;
            ++start;
          }
          if(start<line.size() && index+text.size()<line.size()) {
            Terminal::get().print(line.substr(start, index-start));
            Terminal::get().print(line.substr(index, text.size()), true);
            Terminal::get().print(line.substr(index+text.size()));
          }
          Terminal::get().print("\n");
          ++line_c;
        }
      }
      
      for(auto &view: renamed_views)
        view->soft_reparse_needed=false;
    }
  };
  
  get_buffer()->signal_mark_set().connect([this](const Gtk::TextBuffer::iterator& iterator, const Glib::RefPtr<Gtk::TextBuffer::Mark>& mark){
    if(mark->get_name()=="insert") {
      delayed_tag_similar_identifiers_connection.disconnect();
      delayed_tag_similar_identifiers_connection=Glib::signal_timeout().connect([this]() {
        auto identifier=get_identifier();
        tag_similar_identifiers(identifier);
        return false;
      }, 100);
    }
  });
    
  auto declaration_location=[this]() {
    auto identifier=get_identifier();
    if(identifier) {
      auto source_location=identifier.cursor.get_canonical().get_source_location();
      auto offset=source_location.get_offset();
      return Offset(offset.line-1, offset.index-1, source_location.get_path());
    }
    else {
      // If cursor is at an include line, return offset to included file
      const static std::regex include_regex("^[ \t]*#[ \t]*include[ \t]*[<\"]([^<>\"]+)[>\"].*$");
      std::smatch sm;
      auto line=get_line();
      if(std::regex_match(line, sm, include_regex)) {
        struct ClientData {
          boost::filesystem::path &file_path;
          std::string found_include;
          int line_nr;
          std::string sm_str;
        };
        ClientData client_data{this->file_path, std::string(), get_buffer()->get_insert()->get_iter().get_line(), sm[1].str()};
        
        // Attempt to find the 100% correct include file first
        clang_getInclusions(clang_tu->cx_tu, [](CXFile included_file, CXSourceLocation *inclusion_stack, unsigned include_len, CXClientData client_data_) {
          auto client_data=static_cast<ClientData*>(client_data_);
          if(client_data->found_include.empty() && include_len>0) {
            auto source_location=clangmm::SourceLocation(inclusion_stack[0]);
            if(static_cast<int>(source_location.get_offset().line)-1==client_data->line_nr &&
               filesystem::get_normal_path(source_location.get_path())==client_data->file_path)
              client_data->found_include=clangmm::to_string(clang_getFileName(included_file));
          }
        }, &client_data);
        
        if(!client_data.found_include.empty())
          return Offset(0, 0, client_data.found_include);
        
        // Find a matching include file if no include was found previously
        clang_getInclusions(clang_tu->cx_tu, [](CXFile included_file, CXSourceLocation *inclusion_stack, unsigned include_len, CXClientData client_data_) {
          auto client_data=static_cast<ClientData*>(client_data_);
          if(client_data->found_include.empty()) {
            for(unsigned c=1;c<include_len;++c) {
              auto source_location=clangmm::SourceLocation(inclusion_stack[c]);
              if(static_cast<int>(source_location.get_offset().line)-1<=client_data->line_nr &&
                 filesystem::get_normal_path(source_location.get_path())==client_data->file_path) {
                auto included_file_str=clangmm::to_string(clang_getFileName(included_file));
                if(included_file_str.size()>=client_data->sm_str.size() &&
                   included_file_str.compare(included_file_str.size()-client_data->sm_str.size(), client_data->sm_str.size(), client_data->sm_str)==0) {
                  client_data->found_include=included_file_str;
                  break;
                }
              }
            }
          }
        }, &client_data);
        
        if(!client_data.found_include.empty())
          return Offset(0, 0, client_data.found_include);
      }
    }
    return Offset();
  };
  
  get_declaration_location=[this, declaration_location](){
    if(!parsed) {
      Info::get().print("Buffer is parsing");
      return Offset();
    }
    auto offset=declaration_location();
    if(!offset)
      Info::get().print("No declaration found");
    
    // Workaround for bug in ArchLinux's clang_getFileName()
    // TODO: remove the workaround when this is fixed
    auto include_path=filesystem::get_normal_path(offset.file_path);
    boost::system::error_code ec;
    if(!boost::filesystem::exists(include_path, ec))
      offset.file_path="/usr/include"/include_path;
    
    return offset;
  };
  
  auto implementation_locations=[this]() {
    std::vector<Offset> offsets;
    auto identifier=get_identifier();
    if(identifier) {
      wait_parsing();
      
      //First, look for a definition cursor that is equal
      auto identifier_usr=identifier.cursor.get_usr();
      for(auto &view: views) {
        if(auto clang_view=dynamic_cast<Source::ClangView*>(view)) {
          for(auto &token: *clang_view->clang_tokens) {
            auto cursor=token.get_cursor();
            auto cursor_kind=cursor.get_kind();
            if((cursor_kind==clangmm::Cursor::Kind::FunctionDecl || cursor_kind==clangmm::Cursor::Kind::CXXMethod ||
                cursor_kind==clangmm::Cursor::Kind::Constructor || cursor_kind==clangmm::Cursor::Kind::Destructor ||
                cursor_kind==clangmm::Cursor::Kind::FunctionTemplate || cursor_kind==clangmm::Cursor::Kind::ConversionFunction) &&
               token.is_identifier()) {
              auto token_spelling=token.get_spelling();
              if(identifier.kind==cursor.get_kind() && identifier.spelling==token_spelling && identifier_usr==cursor.get_usr()) {
                if(clang_isCursorDefinition(cursor.cx_cursor)) {
                  Offset offset;
                  auto location=cursor.get_source_location();
                  auto clang_offset=location.get_offset();
                  offset.file_path=location.get_path();
                  offset.line=clang_offset.line-1;
                  offset.index=clang_offset.index-1;
                  offsets.emplace_back(offset);
                }
              }
            }
          }
        }
      }
      if(!offsets.empty())
        return offsets;
      
      //If no implementation was found, try using clang_getCursorDefinition
      auto definition=identifier.cursor.get_definition();
      if(definition) {
        auto location=definition.get_source_location();
        Offset offset;
        offset.file_path=location.get_path();
        auto clang_offset=location.get_offset();
        offset.line=clang_offset.line-1;
        offset.index=clang_offset.index-1;
        offsets.emplace_back(offset);
        return offsets;
      }
      
      //If no implementation was found, use declaration if it is a function template
      auto canonical=identifier.cursor.get_canonical();
      auto cursor=clang_tu->get_cursor(canonical.get_source_location());
      if(cursor && cursor.get_kind()==clangmm::Cursor::Kind::FunctionTemplate) {
        auto location=cursor.get_source_location();
        Offset offset;
        offset.file_path=location.get_path();
        auto clang_offset=location.get_offset();
        offset.line=clang_offset.line-1;
        offset.index=clang_offset.index-1;
        offsets.emplace_back(offset);
        return offsets;
      }
      
      //If no implementation was found, try using Ctags
      auto name=identifier.cursor.get_spelling();
      auto parent=identifier.cursor.get_semantic_parent();
      while(parent && parent.get_kind()!=clangmm::Cursor::Kind::TranslationUnit) {
        auto spelling=parent.get_spelling()+"::";
        name.insert(0, spelling);
        parent=parent.get_semantic_parent();
      }
      auto ctags_locations=Ctags::get_locations(this->file_path, name, identifier.cursor.get_type_description());
      if(!ctags_locations.empty()) {
        for(auto &ctags_location: ctags_locations) {
          Offset offset;
          offset.file_path=ctags_location.file_path;
          offset.line=ctags_location.line;
          offset.index=ctags_location.index;
          offsets.emplace_back(offset);
        }
        return offsets;
      }
    }
    return offsets;
  };
  
  get_implementation_locations=[this, implementation_locations](){
    if(!parsed) {
      Info::get().print("Buffer is parsing");
      return std::vector<Offset>();
    }
    auto offsets=implementation_locations();
    if(offsets.empty())
      Info::get().print("No implementation found");
    
    // Workaround for bug in ArchLinux's clang_getFileName()
    // TODO: remove the workaround when this is fixed
    for(auto &offset: offsets) {
      auto include_path=filesystem::get_normal_path(offset.file_path);
      boost::system::error_code ec;
      if(!boost::filesystem::exists(include_path, ec))
        offset.file_path="/usr/include"/include_path;
    }
    
    return offsets;
  };
  
  get_declaration_or_implementation_locations=[this, declaration_location, implementation_locations]() {
    if(!parsed) {
      Info::get().print("Buffer is parsing");
      return std::vector<Offset>();
    }
    
    std::vector<Offset> offsets;
    
    bool is_implementation=false;
    auto iter=get_buffer()->get_insert()->get_iter();
    auto line=static_cast<unsigned>(iter.get_line());
    auto index=static_cast<unsigned>(iter.get_line_index());
    for(size_t c=0;c<clang_tokens->size();++c) {
      auto &token=(*clang_tokens)[c];
      if(token.is_identifier()) {
        auto &token_offsets=clang_tokens_offsets[c];
        if(line==token_offsets.first.line-1 && index>=token_offsets.first.index-1 && index<=token_offsets.second.index-1) {
          if(clang_isCursorDefinition(token.get_cursor().cx_cursor)>0)
            is_implementation=true;
          break;
        }
      }
    }
    // If cursor is at implementation, return declaration_location
    if(is_implementation) {
      auto offset=declaration_location();
      if(offset)
        offsets.emplace_back(offset);
    }
    else {
      auto implementation_offsets=implementation_locations();
      if(!implementation_offsets.empty()) {
        offsets=std::move(implementation_offsets);
      }
      else {
        auto offset=declaration_location();
        if(offset)
          offsets.emplace_back(offset);
      }
    }
    
    if(offsets.empty())
      Info::get().print("No declaration or implementation found");
    
    // Workaround for bug in ArchLinux's clang_getFileName()
    // TODO: remove the workaround when this is fixed
    for(auto &offset: offsets) {
      auto include_path=filesystem::get_normal_path(offset.file_path);
      boost::system::error_code ec;
      if(!boost::filesystem::exists(include_path, ec))
        offset.file_path="/usr/include"/include_path;
    }
    
    return offsets;
  };
  
  get_usages=[this]() {
    std::vector<std::pair<Offset, std::string> > usages;
    if(!parsed) {
      Info::get().print("Buffer is parsing");
      return usages;
    }
    auto identifier=get_identifier();
    if(identifier) {
      wait_parsing();
      
      auto embolden_token=[](std::string &line, unsigned token_start_pos, unsigned token_end_pos) {
        //markup token as bold
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
        
        size_t start_pos=0;
        while(start_pos<line.size() && (line[start_pos]==' ' || line[start_pos]=='\t'))
          ++start_pos;
        if(start_pos>0)
          line.erase(0, start_pos);
      };
      
      std::vector<clangmm::TranslationUnit*> translation_units;
      translation_units.emplace_back(clang_tu.get());
      for(auto &view: views) {
        if(view!=this) {
          if(auto clang_view=dynamic_cast<Source::ClangView*>(view))
            translation_units.emplace_back(clang_view->clang_tu.get());
        }
      }
      
      auto build=Project::Build::create(this->file_path);
      auto usages_clang=Usages::Clang::get_usages(build->project_path, build->get_default_path(), build->get_debug_path(), {identifier.spelling}, {identifier.cursor}, translation_units);
      for(auto &usage: usages_clang) {
        for(size_t c=0;c<usage.offsets.size();++c) {
          std::string line=Glib::Markup::escape_text(usage.lines[c]);
          embolden_token(line, usage.offsets[c].first.index-1, usage.offsets[c].second.index-1);
          usages.emplace_back(Offset(usage.offsets[c].first.line-1, usage.offsets[c].first.index-1, usage.path), line);
        }
      }
    }
    
    if(usages.empty())
      Info::get().print("No symbol found at current cursor location");
    return usages;
  };
  
  get_method=[this] {
    if(!parsed) {
      Info::get().print("Buffer is parsing");
      return std::string();
    }
    auto iter=get_buffer()->get_insert()->get_iter();
    auto line=static_cast<unsigned>(iter.get_line());
    auto index=static_cast<unsigned>(iter.get_line_index());
    for(size_t c=0;c<clang_tokens->size();++c) {
      auto &token=(*clang_tokens)[c];
      if(token.is_identifier()) {
        auto &token_offsets=clang_tokens_offsets[c];
        if(line==token_offsets.first.line-1 && index>=token_offsets.first.index-1 && index <=token_offsets.second.index-1) {
          auto cursor=token.get_cursor();
          auto kind=cursor.get_kind();
          if(kind==clangmm::Cursor::Kind::FunctionDecl || kind==clangmm::Cursor::Kind::CXXMethod ||
             kind==clangmm::Cursor::Kind::Constructor || kind==clangmm::Cursor::Kind::Destructor ||
             kind==clangmm::Cursor::Kind::ConversionFunction) {
            auto referenced=cursor.get_referenced();
            if(referenced && referenced==cursor) {
              std::string result;
              std::string specifier;
              if(kind==clangmm::Cursor::Kind::FunctionDecl || kind==clangmm::Cursor::Kind::CXXMethod) {
                result=cursor.get_type().get_result().get_spelling();
                
                if(!result.empty() && result.back()!='*' && result.back()!='&')
                  result+=' ';
                
                if(clang_CXXMethod_isConst(cursor.cx_cursor))
                  specifier=" const";
              }
              
              auto name=cursor.get_spelling();
              auto parent=cursor.get_semantic_parent();
              std::vector<std::string> semantic_parents;
              while(parent && parent.get_kind()!=clangmm::Cursor::Kind::TranslationUnit) {
                auto spelling=parent.get_spelling()+"::";
                semantic_parents.emplace_back(spelling);
                name.insert(0, spelling);
                parent=parent.get_semantic_parent();
              }
              
              std::string arguments;
              for(auto &argument_cursor: cursor.get_arguments()) {
                auto argument_type=argument_cursor.get_type().get_spelling();
                for(auto it=semantic_parents.rbegin();it!=semantic_parents.rend();++it) {
                  size_t pos=argument_type.find(' '+*it);
                  if(pos!=std::string::npos)
                    argument_type.erase(pos+1, it->size());
                }
                auto argument=argument_cursor.get_spelling();
                if(!arguments.empty())
                  arguments+=", ";
                arguments+=argument_type;
                if(!arguments.empty() && arguments.back()!='*' && arguments.back()!='&')
                  arguments+=' ';
                arguments+=argument;
              }
              return result+name+'('+arguments+")"+specifier+" {}";
            }
          }
        }
      }
    }
    Info::get().print("No method found at current cursor location");
    return std::string();
  };
  
  get_methods=[this](){
    std::vector<std::pair<Offset, std::string> > methods;
    if(!parsed) {
      Info::get().print("Buffer is parsing");
      return methods;
    }
    clangmm::Offset last_offset{static_cast<unsigned>(-1), static_cast<unsigned>(-1)};
    for(auto &token: *clang_tokens) {
      if(token.is_identifier()) {
        auto cursor=token.get_cursor();
        auto kind=cursor.get_kind();
        if(kind==clangmm::Cursor::Kind::FunctionDecl || kind==clangmm::Cursor::Kind::CXXMethod ||
           kind==clangmm::Cursor::Kind::Constructor || kind==clangmm::Cursor::Kind::Destructor ||
           kind==clangmm::Cursor::Kind::FunctionTemplate || kind==clangmm::Cursor::Kind::ConversionFunction) {
          auto offset=cursor.get_source_location().get_offset();
          if(offset==last_offset)
            continue;
          last_offset=offset;
          
          std::string method;
          if(kind!=clangmm::Cursor::Kind::Constructor && kind!=clangmm::Cursor::Kind::Destructor) {
            method+=cursor.get_type().get_result().get_spelling();
            auto pos=method.find(" ");
            if(pos!=std::string::npos)
              method.erase(pos, 1);
            method+=" ";
          }
          method+=cursor.get_display_name();
          
          std::string prefix;
          auto parent=cursor.get_semantic_parent();
          while(parent && parent.get_kind()!=clangmm::Cursor::Kind::TranslationUnit) {
            prefix.insert(0, parent.get_display_name()+(prefix.empty()?"":"::"));
            parent=parent.get_semantic_parent();
          }
          
          method=Glib::Markup::escape_text(method);
          //Add bold method token
          size_t token_end_pos=method.find('(');
          if(token_end_pos==std::string::npos)
            continue;
          auto token_start_pos=token_end_pos;
          while(token_start_pos!=0 && method[token_start_pos]!=' ')
            --token_start_pos;
          method.insert(token_end_pos, "</b>");
          method.insert(token_start_pos, "<b>");
          
          if(!prefix.empty())
            prefix+=':';
          prefix+=std::to_string(offset.line)+": ";
          prefix=Glib::Markup::escape_text(prefix);
          
          methods.emplace_back(Offset(offset.line-1, offset.index-1), prefix+method);
        }
      }
    }
    if(methods.empty())
      Info::get().print("No methods found in current buffer");
    
    return methods;
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
    if(!parsed) {
      Info::get().print("Buffer is parsing");
      return data;
    }
    auto iter=get_buffer()->get_insert()->get_iter();
    auto line=static_cast<unsigned>(iter.get_line());
    auto index=static_cast<unsigned>(iter.get_line_index());
    for(size_t c=0;c<clang_tokens->size();++c) {
      auto &token=(*clang_tokens)[c];
      if(token.is_identifier()) {
        auto &token_offsets=clang_tokens_offsets[c];
        if(line==token_offsets.first.line-1 && index>=token_offsets.first.index-1 && index <=token_offsets.second.index-1) {
          auto referenced=token.get_cursor().get_referenced();
          if(referenced) {
            auto usr=referenced.get_usr();
            
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
    if(data.empty())
      Info::get().print("No symbol found at current cursor location");
    return data;
  };
  
  goto_next_diagnostic=[this]() {
    if(!parsed) {
      Info::get().print("Buffer is parsing");
      return;
    }
    auto insert_offset=get_buffer()->get_insert()->get_iter().get_offset();
    for(auto offset: diagnostic_offsets) {
      if(offset>insert_offset) {
        get_buffer()->place_cursor(get_buffer()->get_iter_at_offset(offset));
        scroll_to(get_buffer()->get_insert(), 0.0, 1.0, 0.5);
        return;
      }
    }
    if(diagnostic_offsets.size()==0)
      Info::get().print("No diagnostics found in current buffer");
    else {
      auto iter=get_buffer()->get_iter_at_offset(*diagnostic_offsets.begin());
      get_buffer()->place_cursor(iter);
      scroll_to(get_buffer()->get_insert(), 0.0, 1.0, 0.5);
    }
  };
  
  get_fix_its=[this]() {
    if(!parsed) {
      Info::get().print("Buffer is parsing");
      return std::vector<FixIt>();
    }
    if(fix_its.empty())
      Info::get().print("No fix-its found in current buffer");
    return fix_its;
  };
  
  get_documentation_template=[this]() {
    if(!parsed) {
      Info::get().print("Buffer is parsing");
      return std::tuple<Source::Offset, std::string, size_t>(Source::Offset(), "", 0);
    }
    auto identifier=get_identifier();
    if(identifier) {
      auto cursor=identifier.cursor.get_canonical();
      if(!clang_Range_isNull(clang_Cursor_getCommentRange(cursor.cx_cursor))) {
        Info::get().print("Symbol is already documented");
        return std::tuple<Source::Offset, std::string, size_t>(Source::Offset(), "", 0);
      }
      auto clang_offsets=cursor.get_source_range().get_offsets();
      auto source_offset=Offset(clang_offsets.first.line-1, 0, cursor.get_source_location().get_path());
      std::string tabs;
      for(size_t c=0;c<clang_offsets.first.index-1;++c)
        tabs+=' ';
      auto first_line=tabs+"/**\n";
      auto second_line=tabs+" * \n";
      auto iter_offset=first_line.size()+second_line.size()-1;
      
      std::string param_lines;
      for(int c=0;c<clang_Cursor_getNumArguments(cursor.cx_cursor);++c)
        param_lines+=tabs+" * @param "+clangmm::Cursor(clang_Cursor_getArgument(cursor.cx_cursor, c)).get_spelling()+'\n';
      
      std::string return_line;
      auto return_spelling=cursor.get_type().get_result().get_spelling();
      if(!return_spelling.empty() && return_spelling!="void")
        return_line+=tabs+" * @return\n";
      
      auto documentation=first_line+second_line;
      if(!param_lines.empty() || !return_line.empty())
        documentation+=tabs+" *\n";
      
      documentation+=param_lines+return_line+tabs+" */\n";
      
      return std::tuple<Source::Offset, std::string, size_t>(source_offset, documentation, iter_offset);
    }
    else {
      Info::get().print("No symbol found at current cursor location");
      return std::tuple<Source::Offset, std::string, size_t>(Source::Offset(), "", 0);
    }
  };
}

Source::ClangViewRefactor::Identifier Source::ClangViewRefactor::get_identifier() {
  if(!parsed)
    return Identifier();
  auto iter=get_buffer()->get_insert()->get_iter();
  auto line=static_cast<unsigned>(iter.get_line());
  auto index=static_cast<unsigned>(iter.get_line_index());
  for(size_t c=clang_tokens->size()-1;c!=static_cast<size_t>(-1);--c) {
    auto &token=(*clang_tokens)[c];
    if(token.is_identifier()) {
      auto &token_offsets=clang_tokens_offsets[c];
      if(line==token_offsets.first.line-1 && index>=token_offsets.first.index-1 && index <=token_offsets.second.index-1) {
        auto referenced=token.get_cursor().get_referenced();
        if(referenced)
          return Identifier(token.get_spelling(), referenced);
      }
    }
  }
  return Identifier();
}

void Source::ClangViewRefactor::wait_parsing() {
  std::unique_ptr<Dialog::Message> message;
  std::vector<Source::ClangView*> clang_views;
  for(auto &view: views) {
    if(auto clang_view=dynamic_cast<Source::ClangView*>(view)) {
      if(!clang_view->parsed) {
        clang_views.emplace_back(clang_view);
        if(!message)
          message=std::make_unique<Dialog::Message>("Please wait while all buffers finish parsing");
      }
    }
  }
  if(message) {
    for(;;) {
      while(Gtk::Main::events_pending())
        Gtk::Main::iteration(false);
      bool all_parsed=true;
      for(auto &clang_view: clang_views) {
        if(!clang_view->parsed) {
          all_parsed=false;
          break;
        }
      }
      if(all_parsed)
        break;
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    message->hide();
  }
}

void Source::ClangViewRefactor::tag_similar_identifiers(const Identifier &identifier) {
  if(parsed) {
    if(identifier && last_tagged_identifier!=identifier) {
      for(auto &mark: similar_identifiers_marks) {
        get_buffer()->remove_tag(similar_identifiers_tag, mark.first->get_iter(), mark.second->get_iter());
        get_buffer()->delete_mark(mark.first);
        get_buffer()->delete_mark(mark.second);
      }
      similar_identifiers_marks.clear();
      auto offsets=clang_tokens->get_similar_token_offsets(identifier.kind, identifier.spelling, identifier.cursor.get_all_usr_extended());
      for(auto &offset: offsets) {
        auto start_iter=get_buffer()->get_iter_at_line_index(offset.first.line-1, offset.first.index-1);
        auto end_iter=get_buffer()->get_iter_at_line_index(offset.second.line-1, offset.second.index-1);
        get_buffer()->apply_tag(similar_identifiers_tag, start_iter, end_iter);
        similar_identifiers_marks.emplace_back(get_buffer()->create_mark(start_iter), get_buffer()->create_mark(end_iter)); 
      }
      last_tagged_identifier=identifier;
    }
  }
  if(!identifier && last_tagged_identifier) {
    for(auto &mark: similar_identifiers_marks) {
      get_buffer()->remove_tag(similar_identifiers_tag, mark.first->get_iter(), mark.second->get_iter());
      get_buffer()->delete_mark(mark.first);
      get_buffer()->delete_mark(mark.second);
    }
    similar_identifiers_marks.clear();
    last_tagged_identifier=Identifier();
  }
}


Source::ClangView::ClangView(const boost::filesystem::path &file_path, Glib::RefPtr<Gsv::Language> language):
    ClangViewParse(file_path, language), ClangViewAutocomplete(file_path, language), ClangViewRefactor(file_path, language) {
  if(language) {
    get_source_buffer()->set_highlight_syntax(true);
    get_source_buffer()->set_language(language);
  }
  
  do_delete_object.connect([this]() {
    if(delete_thread.joinable())
      delete_thread.join();
    delete this;
  });
}

void Source::ClangView::full_reparse() {
  auto print_error=[this] {
    Terminal::get().async_print("Error: failed to reparse "+file_path.string()+". Please reopen the file manually.\n", true);
  };
  full_reparse_needed=false;
  if(full_reparse_running) {
    print_error();
    return;
  }
  else {
    auto expected=ParseState::PROCESSING;
    if(!parse_state.compare_exchange_strong(expected, ParseState::RESTARTING)) {
      expected=ParseState::RESTARTING;
      if(!parse_state.compare_exchange_strong(expected, ParseState::RESTARTING)) {
        print_error();
        return;
      }
    }
    autocomplete.state=Autocomplete::State::IDLE;
    soft_reparse_needed=false;
    full_reparse_running=true;
    if(full_reparse_thread.joinable())
      full_reparse_thread.join();
    full_reparse_thread=std::thread([this](){
      if(parse_thread.joinable())
        parse_thread.join();
      if(autocomplete.thread.joinable())
        autocomplete.thread.join();
      dispatcher.post([this] {
        parse_initialize();
        full_reparse_running=false;
      });
    });
  }
}

void Source::ClangView::async_delete() {
  delayed_show_arguments_connection.disconnect();
  delayed_tag_similar_identifiers_connection.disconnect();
  parsing_in_progress->cancel("canceled, freeing resources in the background");
  
  views.erase(this);
  std::set<boost::filesystem::path> project_paths_in_use;
  for(auto &view: views) {
    if(dynamic_cast<ClangView*>(view)) {
      auto build=Project::Build::create(view->file_path);
      if(!build->project_path.empty())
        project_paths_in_use.emplace(build->project_path);
    }
  }
  Usages::Clang::erase_unused_caches(project_paths_in_use);
  Usages::Clang::cache_in_progress();
  
  if(!get_buffer()->get_modified()) {
    if(full_reparse_needed)
      full_reparse();
    else if(soft_reparse_needed)
      soft_reparse();
  }
  
  auto before_parse_time=std::time(nullptr);
  delete_thread=std::thread([this, before_parse_time, project_paths_in_use=std::move(project_paths_in_use)] {
    while(!parsed)
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    delayed_reparse_connection.disconnect();
    parse_state=ParseState::STOP;
    dispatcher.disconnect();
    
    if(get_buffer()->get_modified()) {
      std::ifstream stream(file_path.string(), std::ios::binary);
      if(stream) {
        std::string buffer;
        buffer.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
        if(language && (language->get_id()=="chdr" || language->get_id()=="cpphdr"))
          clangmm::remove_include_guard(buffer);
        clang_tu->reparse(buffer);
        clang_tokens = clang_tu->get_tokens();
      }
      else
        clang_tokens=nullptr;
    }
    
    if(clang_tokens) {
      auto build=Project::Build::create(file_path);
      Usages::Clang::cache(build->project_path, build->get_default_path(), file_path, before_parse_time, project_paths_in_use, clang_tu.get(), clang_tokens.get());
    }
    
    if(full_reparse_thread.joinable())
      full_reparse_thread.join();
    if(parse_thread.joinable())
      parse_thread.join();
    if(autocomplete.thread.joinable())
      autocomplete.thread.join();
    do_delete_object();
  });
}
