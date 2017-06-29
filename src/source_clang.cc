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

clangmm::Index Source::ClangViewParse::clang_index(0, 0);

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
  auto &buffer_raw=const_cast<std::string&>(buffer.raw());
  remove_include_guard(buffer_raw);
  clang_tu = std::make_unique<clangmm::TranslationUnit>(clang_index, file_path.string(), get_compilation_commands(), buffer_raw);
  clang_tokens=clang_tu->get_tokens(0, buffer.bytes()-1);
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
        remove_include_guard(parse_thread_buffer_raw);
        auto status=clang_tu->ReparseTranslationUnit(parse_thread_buffer_raw);
        parsing_in_progress->done("done");
        if(status==0) {
          auto expected=ParseProcessState::PROCESSING;
          if(parse_process_state.compare_exchange_strong(expected, ParseProcessState::POSTPROCESSING)) {
            clang_tokens=clang_tu->get_tokens(0, parse_thread_buffer.bytes()-1);
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
    if(parse_process_state.compare_exchange_strong(expected, ParseProcessState::STARTING)) {
      status_state="parsing...";
      if(update_status_state)
        update_status_state(this);
    }
    return false;
  }, 1000);
}

std::vector<std::string> Source::ClangViewParse::get_compilation_commands() {
  auto build=Project::Build::create(file_path);
  if(build->project_path.empty())
    Info::get().print(file_path.filename().string()+": could not find a supported build system");
  auto default_build_path=build->get_default_path();
  build->update_default();
  clangmm::CompilationDatabase db(default_build_path.string());
  clangmm::CompileCommands commands(file_path.string(), db);
  std::vector<clangmm::CompileCommand> cmds = commands.get_commands();
  std::vector<std::string> arguments;
  for (auto &i : cmds) {
    std::vector<std::string> lol = i.get_command_as_args();
    for (size_t a = 1; a < lol.size()-4; a++) {
      arguments.emplace_back(lol[a]);
    }
  }
  auto clang_version_string=clangmm::to_string(clang_getClangVersion());
  const static std::regex clang_version_regex("^[A-Za-z ]+([0-9.]+).*$");
  std::smatch sm;
  if(std::regex_match(clang_version_string, sm, clang_version_regex)) {
    auto clang_version=sm[1].str();
    arguments.emplace_back("-I/usr/lib/clang/"+clang_version+"/include");
#if defined(__APPLE__) && CINDEX_VERSION_MAJOR==0 && CINDEX_VERSION_MINOR<32
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
  
  if(file_path.extension()==".h" ||  //TODO: temporary fix for .h-files (parse as c++)
     (language && (language->get_id()=="cpp" || language->get_id()=="cpphdr")))
    arguments.emplace_back("-xc++");
  
  if(language && (language->get_id()=="chdr" || language->get_id()=="cpphdr"))
    arguments.emplace_back("-Wno-pragma-once-outside-header");
  
  if(!default_build_path.empty()) {
    arguments.emplace_back("-working-directory");
    arguments.emplace_back(default_build_path.string());
  }

  return arguments;
}

void Source::ClangViewParse::remove_include_guard(std::string &buffer) {
  if(!(language && (language->get_id()=="chdr" || language->get_id()=="cpphdr")))
    return;
  
  static std::regex ifndef_regex1("^[ \t]*#ifndef[ \t]+([A-Za-z0-9_]+).*$");
  static std::regex ifndef_regex2("^[ \t]*#if[ \t]+![ \t]*defined[ \t]*\\([ \t]*([A-Za-z0-9_]+).*$");
  static std::regex define_regex("^[ \t]*#define[ \t]+([A-Za-z0-9_]+).*$");
  static std::regex endif_regex("^[ \t]*#endif.*$");
  std::vector<std::pair<size_t, size_t>> ranges;
  bool found_ifndef=false, found_define=false;
  bool line_comment=false, multiline_comment=false;
  size_t start_of_line=0;
  std::string line;
  std::string preprocessor_identifier;
  for(size_t c=0;c<buffer.size();++c) {
    if(!line_comment && !multiline_comment && buffer[c]=='/' && c+1<buffer.size() && (buffer[c+1]=='/' || buffer[c+1]=='*')) {
      if(buffer[c+1]=='/')
        line_comment=true;
      else
        multiline_comment=true;
      ++c;
    }
    else if(multiline_comment && buffer[c]=='*' && c+1<buffer.size() && buffer[c+1]=='/') {
      multiline_comment=false;
      ++c;
    }
    else if(buffer[c]=='\n') {
      bool empty_line=true;
      for(auto &chr: line) {
        if(chr!=' ' && chr!='\t') {
          empty_line=false;
          break;
        }
      }
      
      std::smatch sm;
      if(empty_line) {}
      else if(!found_ifndef && (std::regex_match(line, sm, ifndef_regex1) || std::regex_match(line, sm, ifndef_regex2))) {
        found_ifndef=true;
        ranges.emplace_back(start_of_line, c);
        preprocessor_identifier=sm[1].str();
      }
      else if(found_ifndef && std::regex_match(line, sm, define_regex)) {
        found_define=true;
        ranges.emplace_back(start_of_line, c);
        if(preprocessor_identifier!=sm[1].str())
          return;
        break;
      }
      else
        return;
      
      line_comment=false;
      line.clear();
      if(c+1<buffer.size())
        start_of_line=c+1;
      else
        return;
    }
    else if(!line_comment && !multiline_comment && buffer[c]!='\r')
      line+=buffer[c];
  }
  if(found_ifndef && found_define) {
    size_t last_char_pos=std::string::npos;
    for(size_t c=buffer.size()-1;c!=std::string::npos;--c) {
      if(last_char_pos==std::string::npos) {
        if(buffer[c]!=' ' && buffer[c]!='\t' && buffer[c]!='\r' && buffer[c]!='\n')
          last_char_pos=c;
      }
      else {
        if(buffer[c]=='\n' && c+1<buffer.size()) {
          auto line=buffer.substr(c+1, last_char_pos-c);
          std::smatch sm;
          if(std::regex_match(line, sm, endif_regex)) {
            ranges.emplace_back(c+1, last_char_pos+1);
            for(auto &range: ranges) {
              for(size_t c=range.first;c<range.second;++c) {
                if(buffer[c]!='\r')
                  buffer[c]=' ';
              }
            }
            return;
          }
          return;
        }
      }
    }
  }
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
  
  for (auto &token : *clang_tokens) {
    //if(token.get_kind()==clangmm::Token::Kind::Token_Punctuation)
      //ranges.emplace_back(token.offsets, static_cast<int>(token.get_cursor().get_kind()));
    auto token_kind=token.get_kind();
    if(token_kind==clangmm::Token::Kind::Keyword)
      apply_tag(token.offsets, 702);
    else if(token_kind==clangmm::Token::Kind::Identifier) {
      auto cursor_kind=token.get_cursor().get_kind();
      if(cursor_kind==clangmm::Cursor::Kind::DeclRefExpr || cursor_kind==clangmm::Cursor::Kind::MemberRefExpr)
        cursor_kind=token.get_cursor().get_referenced().get_kind();
      if(cursor_kind!=clangmm::Cursor::Kind::PreprocessingDirective)
        apply_tag(token.offsets, static_cast<int>(cursor_kind));
    }
    else if(token_kind==clangmm::Token::Kind::Literal)
      apply_tag(token.offsets, static_cast<int>(clangmm::Cursor::Kind::StringLiteral));
    else if(token_kind==clangmm::Token::Kind::Comment)
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
      if(token.is_identifier()) {
        if(line==token.offsets.first.line-1 && index>=token.offsets.first.index-1 && index <=token.offsets.second.index-1) {
          auto cursor=token.get_cursor();
          auto referenced=cursor.get_referenced();
          if(referenced) {
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
    Source::ClangViewParse(file_path, language), autocomplete_state(AutocompleteState::IDLE) {
  non_interactive_completion=[this] {
    if(CompletionDialog::get() && CompletionDialog::get()->is_visible())
      return;
    autocomplete_check();
  };
  
  get_buffer()->signal_changed().connect([this](){
    if(CompletionDialog::get() && CompletionDialog::get()->is_visible())
      delayed_reparse_connection.disconnect();
    else {
      if(!has_focus())
        return;
      if((last_keyval>='0' && last_keyval<='9') ||
         (last_keyval>='a' && last_keyval<='z') || (last_keyval>='A' && last_keyval<='Z') ||
         last_keyval=='_') {
        if(interactive_completion || autocomplete_state!=AutocompleteState::IDLE)
          autocomplete_check();
      }
      else {
        if(autocomplete_state==AutocompleteState::STARTING || autocomplete_state==AutocompleteState::RESTARTING)
          autocomplete_state=AutocompleteState::CANCELED;
        auto iter=get_buffer()->get_insert()->get_iter();
        iter.backward_chars(2);
        if(last_keyval=='.' || (last_keyval==':' && *iter==':') || (last_keyval=='>' && *iter=='-')) {
          if(interactive_completion)
            autocomplete_check();
        }
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
    if(CompletionDialog::get() && CompletionDialog::get()->is_visible()) {
      if(CompletionDialog::get()->on_key_release(key))
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
  CompletionDialog::create(this, get_buffer()->create_mark(start_iter));
  completion_dialog_rows.clear();
  CompletionDialog::get()->on_hide=[this](){
    get_buffer()->end_user_action();
    autocomplete_tooltips.hide();
    autocomplete_tooltips.clear();
    parsed=false;
    soft_reparse();
  };
  CompletionDialog::get()->on_select=[this](const std::string& selected, bool hide_window) {
    auto row = completion_dialog_rows.at(selected).first;
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
    static auto manipulators_map=autocomplete_manipulators_map();
    auto it=manipulators_map.find(row);
    if(it!=manipulators_map.end())
      row=it->second;
    get_buffer()->insert(CompletionDialog::get()->start_mark->get_iter(), row);
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
        auto start_offset=CompletionDialog::get()->start_mark->get_iter().get_offset()+start_pos+1;
        auto end_offset=CompletionDialog::get()->start_mark->get_iter().get_offset()+end_pos;
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
  
  CompletionDialog::get()->on_changed=[this](const std::string &selected) {
    if(selected.empty()) {
      autocomplete_tooltips.hide();
      return;
    }
    auto tooltip=std::make_shared<std::string>(completion_dialog_rows.at(selected).second);
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
      
      auto iter=CompletionDialog::get()->start_mark->get_iter();
      autocomplete_tooltips.emplace_back(create_tooltip_buffer, this, get_buffer()->create_mark(iter), get_buffer()->create_mark(iter));
  
      autocomplete_tooltips.show(true);
    }
  };
}

void Source::ClangViewAutocomplete::autocomplete_check() {
  auto iter=get_buffer()->get_insert()->get_iter();
  if(iter.backward_char() && iter.backward_char() && !is_code_iter(iter))
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
  
  status_state="autocomplete...";
  if(update_status_state)
    update_status_state(this);
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
      
      auto &buffer_raw=const_cast<std::string&>(buffer->raw());
      remove_include_guard(buffer_raw);
      auto autocomplete_data=std::make_shared<std::vector<AutoCompleteData> >(autocomplete_get_suggestions(buffer_raw, line_nr, column_nr));
      
      if(parse_state==ParseState::PROCESSING) {
        dispatcher.post([this, autocomplete_data] {
          if(autocomplete_state==AutocompleteState::CANCELED) {
            status_state="";
            if(update_status_state)
              update_status_state(this);
            soft_reparse();
            autocomplete_state=AutocompleteState::IDLE;
          }
          else if(autocomplete_state==AutocompleteState::RESTARTING) {
            status_state="";
            if(update_status_state)
              update_status_state(this);
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
                if(chunk.kind==clangmm::CompletionChunk_ResultType)
                  return_value=chunk.chunk;
                else if(chunk.kind!=clangmm::CompletionChunk_Informative)
                  row+=chunk.chunk;
              }
              data.chunks.clear();
              if (!row.empty()) {
                auto row_insert_on_selection=row;
                if(!return_value.empty())
                  row+=" --> " + return_value;
                completion_dialog_rows[row] = std::pair<std::string, std::string>(std::move(row_insert_on_selection), std::move(data.brief_comments));
                CompletionDialog::get()->add_row(row);
              }
            }
            autocomplete_data->clear();
            status_state="";
            if(update_status_state)
              update_status_state(this);
            autocomplete_state=AutocompleteState::IDLE;
            if (!completion_dialog_rows.empty()) {
              get_buffer()->begin_user_action();
              hide_tooltips();
              CompletionDialog::get()->show();
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
          if(chunk.kind!=clangmm::CompletionChunk_ResultType && chunk.kind!=clangmm::CompletionChunk_Informative) {
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
      if(identifier.kind==clangmm::Cursor::Kind::Constructor || identifier.kind==clangmm::Cursor::Kind::Destructor) {
        auto parent_cursor=identifier.cursor.get_semantic_parent();
        identifier=Identifier(parent_cursor.get_kind(), identifier.spelling, parent_cursor.get_usr(), parent_cursor);
      }
      
      std::vector<Source::View*> renamed_views;
      for(auto &view: views) {
        if(auto clang_view=dynamic_cast<Source::ClangView*>(view)) {
          
          //If rename class, also rename constructors and destructor
          std::set<Identifier> identifiers;
          identifiers.emplace(identifier);
          auto identifier_cursor_kind=identifier.cursor.get_kind();
          if(identifier_cursor_kind==clangmm::Cursor::Kind::ClassDecl || identifier_cursor_kind==clangmm::Cursor::Kind::ClassTemplate) {
            for(auto &token: *clang_view->clang_tokens) {
              auto cursor=token.get_cursor();
              auto cursor_kind=cursor.get_kind();
              auto parent_cursor=cursor.get_semantic_parent();
              if((cursor_kind==clangmm::Cursor::Kind::Constructor || cursor_kind==clangmm::Cursor::Kind::Destructor) &&
                 token.is_identifier() && parent_cursor.get_usr()==identifier.cursor.get_usr()) {
                identifiers.emplace(cursor.get_kind(), token.get_spelling(), cursor.get_usr());
              }
            }
          }
          
          std::vector<std::pair<clangmm::Offset, clangmm::Offset> > offsets;
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
        
        if(!client_data.found_include.empty()) {
          // Workaround for bug in ArchLinux's clang_getFileName()
          // TODO: remove the workaround when this is fixed
          auto include_path=filesystem::get_normal_path(client_data.found_include);
          boost::system::error_code ec;
          if(!boost::filesystem::exists(include_path, ec))
            include_path="/usr/include"/include_path;
          
          return Offset(0, 0, include_path);
        }
        
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
        
        if(!client_data.found_include.empty()) {
          // Workaround for bug in ArchLinux's clang_getFileName()
          // TODO: remove the workaround when this is fixed
          auto include_path=filesystem::get_normal_path(client_data.found_include);
          boost::system::error_code ec;
          if(!boost::filesystem::exists(include_path, ec))
            include_path="/usr/include"/include_path;
          
          return Offset(0, 0, include_path);
        }
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
    return offset;
  };
  
  auto implementation_locations=[this](const std::vector<Source::View*> &views) {
    std::vector<Offset> offsets;
    auto identifier=get_identifier();
    if(identifier) {
      wait_parsing(views);
      for(auto &view: views) {
        if(auto clang_view=dynamic_cast<Source::ClangView*>(view)) {
          for(auto &token: *clang_view->clang_tokens) {
            auto cursor=token.get_cursor();
            auto cursor_kind=cursor.get_kind();
            if((cursor_kind==clangmm::Cursor::Kind::FunctionDecl || cursor_kind==clangmm::Cursor::Kind::CXXMethod ||
                cursor_kind==clangmm::Cursor::Kind::Constructor || cursor_kind==clangmm::Cursor::Kind::Destructor ||
                cursor_kind==clangmm::Cursor::Kind::ConversionFunction) &&
               token.is_identifier()) {
              auto referenced=cursor.get_referenced();
              if(referenced && identifier.kind==referenced.get_kind() &&
                 identifier.spelling==token.get_spelling() && identifier.usr==referenced.get_usr()) {
                if(clang_isCursorDefinition(referenced.cx_cursor)) {
                  Offset offset;
                  offset.file_path=cursor.get_source_location().get_path();
                  auto clang_offset=cursor.get_source_location().get_offset();
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
        auto definition_location=definition.get_source_location();
        Offset offset;
        offset.file_path=definition_location.get_path();
        auto definition_offset=definition_location.get_offset();
        offset.line=definition_offset.line-1;
        offset.index=definition_offset.index-1;
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
  
  get_implementation_locations=[this, implementation_locations](const std::vector<Source::View*> &views){
    if(!parsed) {
      Info::get().print("Buffer is parsing");
      return std::vector<Offset>();
    }
    auto offsets=implementation_locations(views);
    if(offsets.empty())
      Info::get().print("No implementation found");
    return offsets;
  };
  
  get_declaration_or_implementation_locations=[this, declaration_location, implementation_locations](const std::vector<Source::View*> &views) {
    if(!parsed) {
      Info::get().print("Buffer is parsing");
      return std::vector<Offset>();
    }
    
    std::vector<Offset> offsets;
    
    bool is_implementation=false;
    auto iter=get_buffer()->get_insert()->get_iter();
    auto line=static_cast<unsigned>(iter.get_line());
    auto index=static_cast<unsigned>(iter.get_line_index());
    for(auto &token: *clang_tokens) {
      if(token.is_identifier()) {
        if(line==token.offsets.first.line-1 && index>=token.offsets.first.index-1 && index<=token.offsets.second.index-1) {
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
      auto implementation_offsets=implementation_locations(views);
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
    
    return offsets;
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
    for(auto &token: *clang_tokens) {
      if(token.is_identifier()) {
        if(line==token.offsets.first.line-1 && index>=token.offsets.first.index-1 && index <=token.offsets.second.index-1) {
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
    clangmm::Offset last_offset(-1, -1);
    for(auto &token: *clang_tokens) {
      if(token.is_identifier()) {
        auto cursor=token.get_cursor();
        auto kind=cursor.get_kind();
        if(kind==clangmm::Cursor::Kind::FunctionDecl || kind==clangmm::Cursor::Kind::CXXMethod ||
           kind==clangmm::Cursor::Kind::Constructor || kind==clangmm::Cursor::Kind::Destructor ||
           kind==clangmm::Cursor::Kind::ConversionFunction) {
          auto offset=cursor.get_source_location().get_offset();
          if(offset==last_offset)
            continue;
          last_offset=offset;
          
          std::string method;
          if(kind==clangmm::Cursor::Kind::FunctionDecl || kind==clangmm::Cursor::Kind::CXXMethod) {
            method+=cursor.get_type().get_result().get_spelling();
            auto pos=method.find(" ");
            if(pos!=std::string::npos)
              method.erase(pos, 1);
            method+=" ";
          }
          
          std::string parent_str;
          auto parent=cursor.get_semantic_parent();
          while(parent && parent.get_kind()!=clangmm::Cursor::Kind::TranslationUnit) {
            parent_str.insert(0, parent.get_display_name()+"::");
            parent=parent.get_semantic_parent();
          }
          
          method+=parent_str+cursor.get_display_name();
          
          std::string row=std::to_string(offset.line)+": "+Glib::Markup::escape_text(method);
          //Add bold method token
          size_t token_end_pos=row.find('(');
          if(token_end_pos==0 || token_end_pos==std::string::npos)
            continue;
          auto pos=token_end_pos;
          do {
            pos--;
          } while(row[pos]!=':' && row[pos]!=' ' && pos!=std::string::npos);
          row.insert(token_end_pos, "</b>");
          row.insert(pos+1, "<b>");
          methods.emplace_back(Offset(offset.line-1, offset.index-1), row);
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
    for(auto &token: *clang_tokens) {
      if(token.is_identifier()) {
        if(line==token.offsets.first.line-1 && index>=token.offsets.first.index-1 && index <=token.offsets.second.index-1) {
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
      if(line==token.offsets.first.line-1 && index>=token.offsets.first.index-1 && index <=token.offsets.second.index-1) {
        auto referenced=token.get_cursor().get_referenced();
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
  }
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
