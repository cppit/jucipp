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

Source::ClangViewParse::ClangViewParse(const boost::filesystem::path &file_path, Glib::RefPtr<Gsv::Language> language):
    Source::View(file_path, language) {
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
    soft_reparse();
  });
}

bool Source::ClangViewParse::save(const std::vector<Source::View*> &views) {
  if(!Source::View::save(views))
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
  clang_tu = std::make_unique<clang::TranslationUnit>(clang_index, file_path.string(), get_compilation_commands(), buffer.raw());
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
        auto status=clang_tu->ReparseTranslationUnit(parse_thread_buffer.raw());
        parsing_in_progress->done("done");
        if(status==0) {
          auto expected=ParseProcessState::PROCESSING;
          if(parse_process_state.compare_exchange_strong(expected, ParseProcessState::POSTPROCESSING)) {
            clang_tokens=clang_tu->get_tokens(0, parse_thread_buffer.bytes()-1);
            diagnostics=clang_tu->get_diagnostics();
            parse_lock.unlock();
            dispatcher.post([this] {
              std::unique_lock<std::mutex> parse_lock(parse_mutex, std::defer_lock);
              if(parse_lock.try_lock()) {
                auto expected=ParseProcessState::POSTPROCESSING;
                if(parse_process_state.compare_exchange_strong(expected, ParseProcessState::IDLE)) {
                  update_syntax();
                  update_diagnostics();
                  parsed=true;
                  set_status("");
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
            set_status("");
            set_info("");
            parsing_in_progress->cancel("failed");
          });
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
  auto build=Project::Build::create(file_path);
  if(build->project_path.empty())
    Info::get().print(file_path.filename().string()+": could not find a supported build system");
  auto default_build_path=build->get_default_path();
  build->update_default();
  clang::CompilationDatabase db(default_build_path.string());
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
  const static std::regex clang_version_regex("^[A-Za-z ]+([0-9.]+).*$");
  std::smatch sm;
  if(std::regex_match(clang_version_string, sm, clang_version_regex)) {
    auto clang_version=sm[1].str();
    arguments.emplace_back("-I/usr/lib/clang/"+clang_version+"/include");
#ifdef __APPLE__
    arguments.emplace_back("-I/usr/local/Cellar/llvm/"+clang_version+"/lib/clang/"+clang_version+"/include");
    arguments.emplace_back("-I/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/../include/c++/v1");
    arguments.emplace_back("-I/Library/Developer/CommandLineTools/usr/bin/../include/c++/v1"); //Added for OS X 10.11
#endif
#ifdef _WIN32
    if(!Config::get().terminal.msys2_mingw_path.empty())
      arguments.emplace_back("-I"+(Config::get().terminal.msys2_mingw_path/"lib/clang"/clang_version/"include").string());
#endif
  }
  arguments.emplace_back("-fretain-comments-from-system-headers");
  if(file_path.extension()==".h") //TODO: temporary fix for .h-files (parse as c++)
    arguments.emplace_back("-xc++");

  if(!default_build_path.empty()) {
    arguments.emplace_back("-working-directory");
    arguments.emplace_back(default_build_path.string());
  }

  return arguments;
}

void Source::ClangViewParse::update_syntax() {
  auto buffer=get_buffer();
  const auto apply_tag=[this, buffer](const std::pair<clang::Offset, clang::Offset> &offsets, int type) {
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
  
  for (auto &token : *clang_tokens) {
    //if(token.get_kind()==clang::Token::Kind::Token_Punctuation)
      //ranges.emplace_back(token.offsets, static_cast<int>(token.get_cursor().get_kind()));
    auto token_kind=token.get_kind();
    if(token_kind==clang::Token::Kind::Keyword)
      apply_tag(token.offsets, 702);
    else if(token_kind==clang::Token::Kind::Identifier) {
      auto cursor_kind=token.get_cursor().get_kind();
      if(cursor_kind==clang::Cursor::Kind::DeclRefExpr || cursor_kind==clang::Cursor::Kind::MemberRefExpr)
        cursor_kind=token.get_cursor().get_referenced().get_kind();
      if(cursor_kind!=clang::Cursor::Kind::PreprocessingDirective)
        apply_tag(token.offsets, static_cast<int>(cursor_kind));
    }
    else if(token_kind==clang::Token::Kind::Literal)
      apply_tag(token.offsets, static_cast<int>(clang::Cursor::Kind::StringLiteral));
    else if(token_kind==clang::Token::Kind::Comment)
      apply_tag(token.offsets, 705);
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
  for(auto &diagnostic: diagnostics) {
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
    if(iter.ends_line() && location_x>iter_rectangle.get_x())
      return;
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
        if(token.get_kind()==clang::Token::Kind::Identifier && cursor.has_type_description()) {
          if(token.get_cursor().get_kind()==clang::Cursor::Kind::CallExpr) //These cursors are buggy
            continue;
          auto start=get_buffer()->get_iter_at_line_index(token.offsets.first.line-1, token.offsets.first.index-1);
          auto end=get_buffer()->get_iter_at_line_index(token.offsets.second.line-1, token.offsets.second.index-1);
          auto create_tooltip_buffer=[this, &token]() {
            auto tooltip_buffer=Gtk::TextBuffer::create(get_buffer()->get_tag_table());
            tooltip_buffer->insert_with_tag(tooltip_buffer->get_insert()->get_iter(), "Type: "+token.get_cursor().get_type_description(), "def:note");
            auto brief_comment=token.get_cursor().get_brief_comments();
            if(brief_comment!="")
              tooltip_buffer->insert_with_tag(tooltip_buffer->get_insert()->get_iter(), "\n\n"+brief_comment, "def:note");

#ifdef JUCI_ENABLE_DEBUG
            if(Debug::LLDB::get().is_stopped()) {
              auto location=token.get_cursor().get_referenced().get_source_location();
              Glib::ustring value_type="Value";
              
              auto start=get_buffer()->get_iter_at_line_index(token.offsets.first.line-1, token.offsets.first.index-1);
              auto end=get_buffer()->get_iter_at_line_index(token.offsets.second.line-1, token.offsets.second.index-1);
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
          
          type_tooltips.emplace_back(create_tooltip_buffer, *this, get_buffer()->create_mark(start), get_buffer()->create_mark(end));
        }
      }
      
      type_tooltips.show();
    }
  }
}


Source::ClangViewAutocomplete::ClangViewAutocomplete(const boost::filesystem::path &file_path, Glib::RefPtr<Gsv::Language> language):
    Source::ClangViewParse(file_path, language), autocomplete_state(AutocompleteState::IDLE) {
  get_buffer()->signal_changed().connect([this](){
    if(autocomplete_dialog && autocomplete_dialog->shown)
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
        if(autocomplete_state==AutocompleteState::STARTING || autocomplete_state==AutocompleteState::RESTARTING)
          autocomplete_state=AutocompleteState::CANCELED;
        auto iter=get_buffer()->get_insert()->get_iter();
        iter.backward_chars(2);
        if(last_keyval=='.' || (last_keyval==':' && *iter==':') || (last_keyval=='>' && *iter=='-'))
          autocomplete_check();
      }
    }
  });
  get_buffer()->signal_mark_set().connect([this](const Gtk::TextBuffer::iterator& iterator, const Glib::RefPtr<Gtk::TextBuffer::Mark>& mark){
    if(mark->get_name()=="insert") {
      if(autocomplete_state==AutocompleteState::STARTING || autocomplete_state==AutocompleteState::RESTARTING)
        autocomplete_state=AutocompleteState::CANCELED;
    }
  });
  
  signal_key_release_event().connect([this](GdkEventKey* key){
    if(autocomplete_dialog && autocomplete_dialog->shown) {
      if(autocomplete_dialog->on_key_release(key))
        return true;
    }
    return false;
  }, false);

  signal_focus_out_event().connect([this](GdkEventFocus* event) {
    if(autocomplete_state==AutocompleteState::STARTING || autocomplete_state==AutocompleteState::RESTARTING)
      autocomplete_state=AutocompleteState::CANCELED;
    return false;
  });
}

void Source::ClangViewAutocomplete::autocomplete_dialog_setup() {
  auto start_iter=get_buffer()->get_insert()->get_iter();
  if(prefix.size()>0 && !start_iter.backward_chars(prefix.size()))
    return;
  autocomplete_dialog=std::make_unique<CompletionDialog>(*this, get_buffer()->create_mark(start_iter));
  autocomplete_dialog_rows.clear();
  autocomplete_dialog->on_hide=[this](){
    get_buffer()->end_user_action();
    autocomplete_tooltips.hide();
    autocomplete_tooltips.clear();
    parsed=false;
    soft_reparse();
  };
  autocomplete_dialog->on_select=[this](const std::string& selected, bool hide_window) {
    auto row = autocomplete_dialog_rows.at(selected).first;
    //erase existing variable or function before insert iter
    get_buffer()->erase(autocomplete_dialog->start_mark->get_iter(), get_buffer()->get_insert()->get_iter());
    //do not insert template argument or function parameters if they already exist
    auto iter=get_buffer()->get_insert()->get_iter();
    if(*iter=='<' || *iter=='(') {
      auto bracket_pos=row.find(*iter);
      if(bracket_pos!=std::string::npos) {
        row=row.substr(0, bracket_pos);
      }
    }
    //Fixes for the most commonly used stream manipulators
    static auto manipulators_map=autocomplete_manipulators_map();
    auto it=manipulators_map.find(row);
    if(it!=manipulators_map.end())
      row=it->second;
    get_buffer()->insert(autocomplete_dialog->start_mark->get_iter(), row);
    //if selection is finalized, select text inside template arguments or function parameters
    if(hide_window) {
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
        if(iter.backward_char() && *iter==':')
          autocomplete_check();
      }
    }
  };
  
  autocomplete_dialog->on_changed=[this](const std::string &selected) {
    if(selected.empty()) {
      autocomplete_tooltips.hide();
      return;
    }
    auto tooltip=std::make_shared<std::string>(autocomplete_dialog_rows.at(selected).second);
    if(tooltip->empty()) {
      autocomplete_tooltips.hide();
    }
    else {
      autocomplete_tooltips.clear();
      auto create_tooltip_buffer=[this, tooltip]() {
        auto tooltip_buffer=Gtk::TextBuffer::create(get_buffer()->get_tag_table());
        
        tooltip_buffer->insert_with_tag(tooltip_buffer->get_insert()->get_iter(), *tooltip, "def:note");
        
        return tooltip_buffer;
      };
      
      auto iter=autocomplete_dialog->start_mark->get_iter();
      autocomplete_tooltips.emplace_back(create_tooltip_buffer, *this, get_buffer()->create_mark(iter), get_buffer()->create_mark(iter));
  
      autocomplete_tooltips.show(true);
    }
  };
}

void Source::ClangViewAutocomplete::autocomplete_check() {
  auto iter=get_buffer()->get_insert()->get_iter();
  if(iter.backward_char() && iter.backward_char() && (get_source_buffer()->iter_has_context_class(iter, "string") ||
                                                      get_source_buffer()->iter_has_context_class(iter, "comment")))
    return;
  std::string line=" "+get_line_before();
  const static std::regex in_specified_namespace("^(.*[a-zA-Z0-9_\\)\\]\\>])(->|\\.|::)([a-zA-Z0-9_]*)$");
  const static std::regex within_namespace("^(.*)([^a-zA-Z0-9_]+)([a-zA-Z0-9_]{3,})$");
  std::smatch sm;
  if(std::regex_match(line, sm, in_specified_namespace)) {
    {
      std::unique_lock<std::mutex> lock(prefix_mutex);
      prefix=sm[3].str();
    }
    if(prefix.size()==0 || prefix[0]<'0' || prefix[0]>'9')
      autocomplete();
  }
  else if(std::regex_match(line, sm, within_namespace)) {
    {
      std::unique_lock<std::mutex> lock(prefix_mutex);
      prefix=sm[3].str();
    }
    if(prefix.size()==0 || prefix[0]<'0' || prefix[0]>'9')
      autocomplete();
  }
  if(autocomplete_state!=AutocompleteState::IDLE)
    delayed_reparse_connection.disconnect();
}

void Source::ClangViewAutocomplete::autocomplete() {
  if(parse_state!=ParseState::PROCESSING)
    return;
  
  if(autocomplete_state==AutocompleteState::CANCELED)
    autocomplete_state=AutocompleteState::RESTARTING;
  
  if(autocomplete_state!=AutocompleteState::IDLE)
    return;

  autocomplete_state=AutocompleteState::STARTING;
  
  set_status("autocomplete...");
  if(autocomplete_thread.joinable())
    autocomplete_thread.join();
  auto buffer=std::make_shared<Glib::ustring>(get_buffer()->get_text());
  auto iter=get_buffer()->get_insert()->get_iter();
  auto line_nr=iter.get_line()+1;
  auto column_nr=iter.get_line_index()+1;
  auto pos=iter.get_offset()-1;
  while(pos>=0 && (((*buffer)[pos]>='a' && (*buffer)[pos]<='z') || ((*buffer)[pos]>='A' && (*buffer)[pos]<='Z') ||
                   ((*buffer)[pos]>='0' && (*buffer)[pos]<='9') || (*buffer)[pos]=='_')) {
    buffer->replace(pos, 1, " ");
    column_nr--;
    pos--;
  }
  autocomplete_thread=std::thread([this, line_nr, column_nr, buffer](){
    std::unique_lock<std::mutex> lock(parse_mutex);
    if(parse_state==ParseState::PROCESSING) {
      parse_process_state=ParseProcessState::IDLE;
      auto autocomplete_data=std::make_shared<std::vector<AutoCompleteData> >(autocomplete_get_suggestions(buffer->raw(), line_nr, column_nr));
      
      if(parse_state==ParseState::PROCESSING) {
        dispatcher.post([this, autocomplete_data] {
          if(autocomplete_state==AutocompleteState::CANCELED) {
            set_status("");
            soft_reparse();
            autocomplete_state=AutocompleteState::IDLE;
          }
          else if(autocomplete_state==AutocompleteState::RESTARTING) {
            set_status("");
            soft_reparse();
            autocomplete_state=AutocompleteState::IDLE;
            autocomplete_check();
          }
          else {
            autocomplete_dialog_setup();
            
            for (auto &data : *autocomplete_data) {
              std::string row;
              std::string return_value;
              for (auto &chunk : data.chunks) {
                if(chunk.kind==clang::CompletionChunk_ResultType)
                  return_value=chunk.chunk;
                else if(chunk.kind!=clang::CompletionChunk_Informative)
                  row+=chunk.chunk;
              }
              data.chunks.clear();
              if (!row.empty()) {
                auto row_insert_on_selection=row;
                if(!return_value.empty())
                  row+=" --> " + return_value;
                autocomplete_dialog_rows[row] = std::pair<std::string, std::string>(std::move(row_insert_on_selection), std::move(data.brief_comments));
                autocomplete_dialog->add_row(row);
              }
            }
            autocomplete_data->clear();
            set_status("");
            autocomplete_state=AutocompleteState::IDLE;
            if (!autocomplete_dialog_rows.empty()) {
              get_buffer()->begin_user_action();
              hide_tooltips();
              autocomplete_dialog->show();
            }
            else
              soft_reparse();
          }
        });
      }
      else {
        dispatcher.post([this] {
          Terminal::get().print("Error: autocomplete failed, reparsing "+this->file_path.string()+"\n", true);
          autocomplete_state=AutocompleteState::CANCELED;
          full_reparse();
        });
      }
    }
  });
}

std::vector<Source::ClangViewAutocomplete::AutoCompleteData> Source::ClangViewAutocomplete::autocomplete_get_suggestions(const std::string &buffer, int line_number, int column) {
  std::vector<AutoCompleteData> suggestions;
  auto results=clang_tu->get_code_completions(buffer, line_number, column);
  if(results.cx_results==nullptr) {
    auto expected=ParseState::PROCESSING;
    parse_state.compare_exchange_strong(expected, ParseState::RESTARTING);
    return suggestions;
  }
  
  if(autocomplete_state==AutocompleteState::STARTING) {
    std::unique_lock<std::mutex> lock(prefix_mutex);
    auto prefix_copy=prefix;
    lock.unlock();
      
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

std::unordered_map<std::string, std::string> Source::ClangViewAutocomplete::autocomplete_manipulators_map() {
  std::unordered_map<std::string, std::string> map;
  //TODO: feel free to add more
  map["endl(basic_ostream<_CharT, _Traits> &__os)"]="endl";
  map["flush(basic_ostream<_CharT, _Traits> &__os)"]="flush";
  map["hex(std::ios_base &__str)"]="hex"; //clang++ headers
  map["hex(std::ios_base &__base)"]="hex"; //g++ headers
  map["dec(std::ios_base &__str)"]="dec";
  map["dec(std::ios_base &__base)"]="dec";
  return map;
}


Source::ClangViewRefactor::ClangViewRefactor(const boost::filesystem::path &file_path, Glib::RefPtr<Gsv::Language> language) :
    Source::ClangViewParse(file_path, language) {
  similar_identifiers_tag=get_buffer()->create_tag();
  similar_identifiers_tag->property_weight()=1000; //TODO: Replace 1000 with Pango::WEIGHT_ULTRAHEAVY when debian stable gets updated in 2017
  
  get_buffer()->signal_changed().connect([this]() {
    if(!renaming && last_tagged_identifier) {
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
    return get_identifier().spelling;
  };
  
  rename_similar_tokens=[this](const std::vector<Source::View*> &views, const std::string &text) {
    std::vector<std::pair<boost::filesystem::path, size_t> > renamed;
    if(!parsed) {
      Info::get().print("Buffer is parsing");
      return renamed;
    }
    auto identifier=get_identifier();
    if(identifier) {
      wait_parsing(views);
      
      //If rename constructor or destructor, set token to class
      if(identifier.kind==clang::Cursor::Kind::Constructor || identifier.kind==clang::Cursor::Kind::Destructor) {
        auto parent_cursor=identifier.cursor.get_semantic_parent();
        identifier=Identifier(parent_cursor.get_kind(), identifier.spelling, parent_cursor.get_usr(), parent_cursor);
      }
      
      std::vector<Source::View*> renamed_views;
      for(auto &view: views) {
        if(auto clang_view=dynamic_cast<Source::ClangView*>(view)) {
          
          //If rename class, also rename constructors and destructor
          std::set<Identifier> identifiers;
          identifiers.emplace(identifier);
          if(identifier.cursor.get_kind()==clang::Cursor::Kind::ClassDecl) {
            for(auto &token: *clang_view->clang_tokens) {
              auto cursor=token.get_cursor();
              auto cursor_kind=cursor.get_kind();
              auto parent_cursor=cursor.get_semantic_parent();
              if(token.get_kind()==clang::Token::Kind::Identifier &&
                 (cursor_kind==clang::Cursor::Kind::Constructor || cursor_kind==clang::Cursor::Kind::Destructor) &&
                 parent_cursor.get_usr()==identifier.cursor.get_usr() && cursor.has_type_description()) {
                identifiers.emplace(cursor.get_kind(), token.get_spelling(), cursor.get_usr());
              }
            }
          }
          
          std::vector<std::pair<clang::Offset, clang::Offset> > offsets;
          for(auto &identifier: identifiers) {
            auto token_offsets=clang_view->clang_tokens->get_similar_token_offsets(identifier.kind, identifier.spelling, identifier.usr);
            for(auto &token_offset: token_offsets)
              offsets.emplace_back(token_offset);
          }
          std::vector<std::pair<Glib::RefPtr<Gtk::TextMark>, Glib::RefPtr<Gtk::TextMark> > > marks;
          for(auto &offset: offsets) {
            marks.emplace_back(clang_view->get_buffer()->create_mark(clang_view->get_buffer()->get_iter_at_line_index(offset.first.line-1, offset.first.index-1)),
                               clang_view->get_buffer()->create_mark(clang_view->get_buffer()->get_iter_at_line_index(offset.second.line-1, offset.second.index-1)));
          }
          if(!marks.empty()) {
            clang_view->renaming=true;
            clang_view->get_buffer()->begin_user_action();
            for(auto &mark: marks) {
              clang_view->get_buffer()->erase(mark.first->get_iter(), mark.second->get_iter());
              clang_view->get_buffer()->insert(mark.first->get_iter(), text);
              clang_view->get_buffer()->delete_mark(mark.first);
              clang_view->get_buffer()->delete_mark(mark.second);
            }
            clang_view->get_buffer()->end_user_action();
            clang_view->renaming=false;
            clang_view->save(views);
            renamed_views.emplace_back(clang_view);
            renamed.emplace_back(clang_view->file_path, marks.size());
          }
        }
      }
      for(auto &view: renamed_views)
        view->soft_reparse_needed=false;
    }
    return renamed;
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
  
  get_declaration_location=[this](){
    if(!parsed) {
      Info::get().print("Buffer is parsing");
      return Offset();
    }
    auto identifier=get_identifier();
    if(identifier) {
      auto source_location=identifier.cursor.get_canonical().get_source_location();
      auto offset=source_location.get_offset();
      return Offset(offset.line-1, offset.index-1, source_location.get_path());
    }
    return Offset();
  };
  
  get_implementation_locations=[this](const std::vector<Source::View*> &views){
    std::vector<Offset> locations;
    if(!parsed) {
      Info::get().print("Buffer is parsing");
      return locations;
    }
    auto identifier=get_identifier();
    if(identifier) {
      wait_parsing(views);
      for(auto &view: views) {
        if(auto clang_view=dynamic_cast<Source::ClangView*>(view)) {
          for(auto &token: *clang_view->clang_tokens) {
            auto cursor=token.get_cursor();
            auto kind=cursor.get_kind();
            if((kind==clang::Cursor::Kind::FunctionDecl || kind==clang::Cursor::Kind::CXXMethod ||
                kind==clang::Cursor::Kind::Constructor || kind==clang::Cursor::Kind::Destructor) &&
               token.get_kind()==clang::Token::Kind::Identifier && cursor.has_type_description()) {
              auto referenced=cursor.get_referenced();
              if(referenced && identifier.kind==referenced.get_kind() &&
                 identifier.spelling==token.get_spelling() && identifier.usr==referenced.get_usr()) {
                if(clang_isCursorDefinition(referenced.cx_cursor)) {
                  Offset location;
                  location.file_path=cursor.get_source_location().get_path();
                  auto clang_offset=cursor.get_source_location().get_offset();
                  location.line=clang_offset.line-1;
                  location.index=clang_offset.index-1;
                  locations.emplace_back(location);
                }
              }
            }
          }
        }
      }
      if(!locations.empty())
        return locations;
      
      //If no implementation was found, try using clang_getCursorDefinition
      auto definition=identifier.cursor.get_definition();
      if(definition) {
        auto definition_location=definition.get_source_location();
        Offset location;
        location.file_path=definition_location.get_path();
        auto offset=definition_location.get_offset();
        location.line=offset.line-1;
        location.index=offset.index-1;
        locations.emplace_back(location);
        return locations;
      }
      
      //If no implementation was found, try using Ctags
      auto name=identifier.cursor.get_spelling();
      auto parent=identifier.cursor.get_semantic_parent();
      while(parent && parent.get_kind()!=clang::Cursor::Kind::TranslationUnit) {
        auto spelling=parent.get_spelling()+"::";
        name.insert(0, spelling);
        parent=parent.get_semantic_parent();
      }
      auto ctags_locations=Ctags::get_locations(this->file_path, name, identifier.cursor.get_type_description());
      if(!ctags_locations.empty()) {
        for(auto &ctags_location: ctags_locations) {
          Offset location;
          location.file_path=ctags_location.file_path;
          location.line=ctags_location.line;
          location.index=ctags_location.index;
          locations.emplace_back(location);
        }
        return locations;
      }
      
      Info::get().print("Could not find implementation");
    }
    return locations;
  };
  
  get_usages=[this](const std::vector<Source::View*> &views) {
    std::vector<std::pair<Offset, std::string> > usages;
    if(!parsed) {
      Info::get().print("Buffer is parsing");
      return usages;
    }
    auto identifier=get_identifier();
    if(identifier) {
      wait_parsing(views);
      std::vector<Source::View*> views_reordered;
      views_reordered.emplace_back(this);
      for(auto &view: views) {
        if(view!=this)
          views_reordered.emplace_back(view);
      }
      for(auto &view: views_reordered) {
        if(auto clang_view=dynamic_cast<Source::ClangView*>(view)) {
          auto offsets=clang_view->clang_tokens->get_similar_token_offsets(identifier.kind, identifier.spelling, identifier.usr);
          for(auto &offset: offsets) {
            size_t whitespaces_removed=0;
            auto start_iter=clang_view->get_buffer()->get_iter_at_line(offset.first.line-1);
            while(!start_iter.ends_line() && (*start_iter==' ' || *start_iter=='\t')) {
              start_iter.forward_char();
              whitespaces_removed++;
            }
            auto end_iter=clang_view->get_iter_at_line_end(offset.first.line-1);
            std::string line=Glib::Markup::escape_text(clang_view->get_buffer()->get_text(start_iter, end_iter));
            
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
            usages.emplace_back(Offset(offset.first.line-1, offset.first.index-1, clang_view->file_path), line);
          }
        }
      }
    }
    
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
    for(auto &token: *clang_tokens) {
      auto cursor=token.get_cursor();
      if(token.get_kind()==clang::Token::Kind::Identifier && cursor.has_type_description()) {
        if(line==token.offsets.first.line-1 && index>=token.offsets.first.index-1 && index <=token.offsets.second.index-1) {
          auto kind=cursor.get_kind();
          if(kind==clang::Cursor::Kind::CXXMethod || kind==clang::Cursor::Kind::Constructor || kind==clang::Cursor::Kind::Destructor) {
            auto referenced=cursor.get_referenced();
            if(referenced && referenced==cursor) {
              std::string result;
              std::string specifier;
              if(kind==clang::Cursor::Kind::CXXMethod) {
                result=cursor.get_type().get_result().get_spelling();
                
                if(!result.empty() && result.back()!='*' && result.back()!='&')
                  result+=' ';
                
                if(clang_CXXMethod_isConst(cursor.cx_cursor))
                  specifier=" const";
              }
              
              auto name=cursor.get_spelling();
              auto parent=cursor.get_semantic_parent();
              std::vector<std::string> semantic_parents;
              while(parent && parent.get_kind()!=clang::Cursor::Kind::TranslationUnit) {
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
    return std::string();
  };
  
  get_methods=[this](){
    std::vector<std::pair<Offset, std::string> > methods;
    if(!parsed) {
      Info::get().print("Buffer is parsing");
      return methods;
    }
    auto cxx_methods=clang_tokens->get_cxx_methods();
    for(auto &method: cxx_methods) {
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
      methods.emplace_back(Offset(method.second.line-1, method.second.index-1), row);
    }
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
    for(auto &token: *clang_tokens) {
      auto cursor=token.get_cursor();
      if(token.get_kind()==clang::Token::Kind::Identifier && cursor.has_type_description()) {
        if(line==token.offsets.first.line-1 && index>=token.offsets.first.index-1 && index <=token.offsets.second.index-1) {
          auto referenced=cursor.get_referenced();
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
    if(diagnostic_offsets.size()>0) {
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
    return fix_its;
  };
}

Source::ClangViewRefactor::Identifier Source::ClangViewRefactor::get_identifier() {
  if(!parsed)
    return Identifier();
  auto iter=get_buffer()->get_insert()->get_iter();
  auto line=static_cast<unsigned>(iter.get_line());
  auto index=static_cast<unsigned>(iter.get_line_index());
  for(auto &token: *clang_tokens) {
    auto cursor=token.get_cursor();
    if(token.get_kind()==clang::Token::Kind::Identifier && cursor.has_type_description()) {
      if(line==token.offsets.first.line-1 && index>=token.offsets.first.index-1 && index <=token.offsets.second.index-1) {
        if(token.get_cursor().get_kind()==clang::Cursor::Kind::CallExpr) //These cursors are buggy
          continue;
        auto referenced=cursor.get_referenced();
        if(referenced)
          return Identifier(referenced.get_kind(), token.get_spelling(), referenced.get_usr(), referenced);
      }
    }
  }
  return Identifier();
}

void Source::ClangViewRefactor::wait_parsing(const std::vector<Source::View*> &views) {
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
      while(g_main_context_pending(nullptr))
        g_main_context_iteration(nullptr, false);
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
      auto offsets=clang_tokens->get_similar_token_offsets(identifier.kind, identifier.spelling, identifier.usr);
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

bool Source::ClangView::full_reparse() {
  full_reparse_needed=false;
  if(!full_reparse_running) {
    auto expected=ParseState::PROCESSING;
    if(!parse_state.compare_exchange_strong(expected, ParseState::RESTARTING)) {
      expected=ParseState::RESTARTING;
      if(!parse_state.compare_exchange_strong(expected, ParseState::RESTARTING))
        return false;
    }
    autocomplete_state=AutocompleteState::IDLE;
    soft_reparse_needed=false;
    full_reparse_running=true;
    if(full_reparse_thread.joinable())
      full_reparse_thread.join();
    full_reparse_thread=std::thread([this](){
      if(parse_thread.joinable())
        parse_thread.join();
      if(autocomplete_thread.joinable())
        autocomplete_thread.join();
      dispatcher.post([this] {
        parse_initialize();
        full_reparse_running=false;
      });
    });
    return true;
  }
  return false;
}

void Source::ClangView::async_delete() {
  dispatcher.disconnect();
  delayed_reparse_connection.disconnect();
  delayed_tag_similar_identifiers_connection.disconnect();
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
