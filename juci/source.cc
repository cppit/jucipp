#include "source.h"
#include "sourcefile.h"
#include <boost/property_tree/json_parser.hpp>
#include <fstream>
#include <boost/timer/timer.hpp>
#include "logging.h"
#include <algorithm>
#include <regex>
#include "singletons.h"

#include <iostream> //TODO: remove
using namespace std; //TODO: remove

namespace sigc {
  SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
}

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
Source::View::View(const std::string& file_path, const std::string& project_path):
file_path(file_path), project_path(project_path) {
  set_smart_home_end(Gsv::SMART_HOME_END_BEFORE);
  set_show_line_numbers(Singleton::Config::source()->show_line_numbers);
  set_highlight_current_line(Singleton::Config::source()->highlight_current_line);
  sourcefile s(file_path);
  get_source_buffer()->get_undo_manager()->begin_not_undoable_action();
  get_source_buffer()->set_text(s.get_content());
  get_source_buffer()->get_undo_manager()->end_not_undoable_action();
  search_start = search_end = this->get_buffer()->end();
  
  override_font(Pango::FontDescription(Singleton::Config::source()->font));
  
  override_background_color(Gdk::RGBA(Singleton::Config::source()->background));
  override_background_color(Gdk::RGBA(Singleton::Config::source()->background_selected), Gtk::StateFlags::STATE_FLAG_SELECTED);
  for (auto &item : Singleton::Config::source()->tags) {
    get_source_buffer()->create_tag(item.first)->property_foreground() = item.second;
  }
  
  get_buffer()->place_cursor(get_buffer()->get_iter_at_offset(0));
  signal_size_allocate().connect([this](Gtk::Allocation& allocation){
    if(!after_user_input)
      scroll_to(get_buffer()->get_insert(), 0.0, 1.0, 0.5);
  });
  
  signal_event().connect([this](GdkEvent* event){
    if(event->type==GDK_KEY_PRESS || event->type==GDK_BUTTON_PRESS || event->type==GDK_SCROLL)
      after_user_input=true;
    return false;
  });
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

//Basic indentation
bool Source::View::on_key_press_event(GdkEventKey* key) {
  auto config=Singleton::Config::source();
  const std::regex spaces_regex(std::string("^(")+config->tab_char+"*).*$");
  //Indent as in next or previous line
  if(key->keyval==GDK_KEY_Return && key->state==0 && !get_buffer()->get_has_selection()) {
    int line_nr=get_source_buffer()->get_insert()->get_iter().get_line();
    string line(get_line_before_insert());
    std::smatch sm;
    if(std::regex_match(line, sm, spaces_regex)) {
      if((line_nr+1)<get_source_buffer()->get_line_count()) {
        string next_line=get_line(line_nr+1);
        std::smatch sm2;
        if(std::regex_match(next_line, sm2, spaces_regex)) {
          if(sm2[1].str().size()>sm[1].str().size()) {
            get_source_buffer()->insert_at_cursor("\n"+sm2[1].str());
            scroll_to(get_source_buffer()->get_insert());
            return true;
          }
        }
      }
      get_source_buffer()->insert_at_cursor("\n"+sm[1].str());
      scroll_to(get_source_buffer()->get_insert());
      return true;
    }
  }
  //Indent right when clicking tab, no matter where in the line the cursor is. Also works on selected text.
  else if(key->keyval==GDK_KEY_Tab && key->state==0) {
    Gtk::TextIter selection_start, selection_end;
    get_source_buffer()->get_selection_bounds(selection_start, selection_end);
    int line_start=selection_start.get_line();
    int line_end=selection_end.get_line();
    for(int line=line_start;line<=line_end;line++) {
      Gtk::TextIter line_it = get_source_buffer()->get_iter_at_line(line);
      get_source_buffer()->insert(line_it, config->tab);
    }
    return true;
  }
  //Indent left when clicking shift-tab, no matter where in the line the cursor is. Also works on selected text.
  else if((key->keyval==GDK_KEY_ISO_Left_Tab || key->keyval==GDK_KEY_Tab) && key->state==GDK_SHIFT_MASK) {
    Gtk::TextIter selection_start, selection_end;
    get_source_buffer()->get_selection_bounds(selection_start, selection_end);
    int line_start=selection_start.get_line();
    int line_end=selection_end.get_line();
    
    for(int line_nr=line_start;line_nr<=line_end;line_nr++) {
      string line=get_line(line_nr);
      if(!(line.size()>=config->tab_size && line.substr(0, config->tab_size)==config->tab))
        return true;
    }
    
    for(int line_nr=line_start;line_nr<=line_end;line_nr++) {
      Gtk::TextIter line_it = get_source_buffer()->get_iter_at_line(line_nr);
      Gtk::TextIter line_plus_it=line_it;
      
      for(unsigned c=0;c<config->tab_size;c++)
        line_plus_it++;
      get_source_buffer()->erase(line_it, line_plus_it);
    }
    return true;
  }
  //"Smart" backspace key
  else if(key->keyval==GDK_KEY_BackSpace && !get_buffer()->get_has_selection()) {
    Gtk::TextIter insert_it=get_source_buffer()->get_insert()->get_iter();
    int line_nr=insert_it.get_line();
    if(line_nr>0) {
      string line=get_line(line_nr);
      string previous_line=get_line(line_nr-1);
      smatch sm;
      if(std::regex_match(previous_line, sm, spaces_regex)) {
        if(line==sm[1] || line==(std::string(sm[1])+config->tab) || (line+config->tab==sm[1])) {
          Gtk::TextIter line_it = get_source_buffer()->get_iter_at_line(line_nr);
          get_source_buffer()->erase(line_it, insert_it);
        }
      }
    }
  }
  return Gsv::View::on_key_press_event(key);
}

/////////////////////////
//// ClangViewParse ///
/////////////////////////
clang::Index Source::ClangViewParse::clang_index(0, 0);

Source::ClangViewParse::ClangViewParse(const std::string& file_path, const std::string& project_path):
Source::View(file_path, project_path),
parse_thread_go(true), parse_thread_mapped(false), parse_thread_stop(false) {  
  int start_offset = get_source_buffer()->begin().get_offset();
  int end_offset = get_source_buffer()->end().get_offset();
  auto buffer_map=get_buffer_map();
  //Remove includes for first parse for initial syntax highlighting
  auto& str=buffer_map[file_path];
  std::size_t pos=0;
  while((pos=str.find("#include", pos))!=std::string::npos) {
    auto start_pos=pos;
    pos=str.find('\n', pos+8);
    if(pos==std::string::npos)
      break;
    if(start_pos==0 || str[start_pos-1]=='\n') {
      str.replace(start_pos, pos-start_pos, pos-start_pos, ' ');
    }
    pos++;
  }
  init_syntax_highlighting(buffer_map,
                           start_offset,
                           end_offset);
  update_syntax();
  
  //GTK-calls must happen in main thread, so the parse_thread
  //sends signals to the main thread that it is to call the following functions:
  parse_start.connect([this]{
    if(parse_thread_buffer_map_mutex.try_lock()) {
      parse_thread_buffer_map=get_buffer_map();
      parse_thread_mapped=true;
      parse_thread_buffer_map_mutex.unlock();
    }
    parse_thread_go=true;
  });
  
  parsing_in_progress=Singleton::terminal()->print_in_progress("Parsing "+file_path);
  parse_done.connect([this](){
    if(parse_thread_mapped) {
      if(parsing_mutex.try_lock()) {
        INFO("Updating syntax");
        update_syntax();
        update_diagnostics();
        update_types();
        clang_readable=true;
        parsing_mutex.unlock();
        INFO("Syntax updated");
      }
      parsing_in_progress->done("done");
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
      else if (parse_thread_mapped && parsing_mutex.try_lock() && parse_thread_buffer_map_mutex.try_lock()) {
        reparse(parse_thread_buffer_map);
        parse_thread_go=false;
        parsing_mutex.unlock();
        parse_thread_buffer_map_mutex.unlock();
        parse_done();
      }
    }
  });
  
  get_buffer()->signal_changed().connect([this]() {
    start_reparse();
    type_tooltips.hide();
    diagnostic_tooltips.hide();
  });
  
  get_buffer()->signal_mark_set().connect(sigc::mem_fun(*this, &Source::ClangViewParse::on_mark_set), false);
}

Source::ClangViewParse::~ClangViewParse() {
  //TODO: Is it possible to stop the clang-process in progress?
  parsing_in_progress->cancel("canceled");
  parse_thread_stop=true;
  if(parse_thread.joinable())
    parse_thread.join();
  parsing_mutex.lock(); //Be sure not to destroy while still parsing with libclang
  parsing_mutex.unlock();
}

void Source::ClangViewParse::
init_syntax_highlighting(const std::map<std::string, std::string>
                         &buffers,
                         int start_offset,
                         int end_offset) {
  std::vector<string> arguments = get_compilation_commands();
  clang_tu = std::unique_ptr<clang::TranslationUnit>(new clang::TranslationUnit(clang_index,
                                                                           file_path,
                                                                           arguments,
                                                                           buffers));
  clang_tokens=clang_tu->get_tokens(0, buffers.find(file_path)->second.size()-1);
}

std::map<std::string, std::string> Source::ClangViewParse::get_buffer_map() const {
  std::map<std::string, std::string> buffer_map;
  buffer_map[file_path]=get_source_buffer()->get_text().raw();
  return buffer_map;
}

void Source::ClangViewParse::start_reparse() {
  parse_thread_mapped=false;
  clang_readable=false;
  delayed_reparse_connection.disconnect();
  delayed_reparse_connection=Glib::signal_timeout().connect([this]() {
    parse_thread_go=true;
    return false;
  }, 1000);
}

int Source::ClangViewParse::reparse(const std::map<std::string, std::string> &buffer) {
  int status = clang_tu->ReparseTranslationUnit(buffer);
  clang_tokens=clang_tu->get_tokens(0, parse_thread_buffer_map.find(file_path)->second.size()-1);
  return status;
}

std::vector<std::string> Source::ClangViewParse::get_compilation_commands() {
  clang::CompilationDatabase db(project_path);
  clang::CompileCommands commands(file_path, db);
  std::vector<clang::CompileCommand> cmds = commands.get_commands();
  std::vector<std::string> arguments;
  for (auto &i : cmds) {
    std::vector<std::string> lol = i.get_command_as_args();
    for (size_t a = 1; a < lol.size()-4; a++) {
      arguments.emplace_back(lol[a]);
    }
  }
  if(boost::filesystem::path(file_path).extension()==".h") //TODO: temporary fix for .h-files (parse as c++)
    arguments.emplace_back("-xc++");
  return arguments;
}

void Source::ClangViewParse::update_syntax() {
  std::vector<Source::Range> ranges;
  for (auto &token : *clang_tokens) {
    if(token.get_kind()==0) // PunctuationToken
      ranges.emplace_back(token.offsets.first, token.offsets.second, (int) token.get_cursor().get_kind());
    else if(token.get_kind()==1) // KeywordToken
      ranges.emplace_back(token.offsets.first, token.offsets.second, 702);
    else if(token.get_kind()==2) // IdentifierToken
      ranges.emplace_back(token.offsets.first, token.offsets.second, (int) token.get_cursor().get_kind());
    else if(token.get_kind()==3) // LiteralToken
      ranges.emplace_back(token.offsets.first, token.offsets.second, 109);
    else if(token.get_kind()==4) // CommentToken
      ranges.emplace_back(token.offsets.first, token.offsets.second, 705);
  }
  if (ranges.empty() || ranges.size() == 0) {
    return;
  }
  auto buffer = get_source_buffer();
  buffer->remove_all_tags(buffer->begin(), buffer->end());
  for (auto &range : ranges) {
    std::string type = std::to_string(range.kind);
    try {
      Singleton::Config::source()->types.at(type);
    } catch (std::exception) {
      continue;
    }
    
    Gtk::TextIter begin_iter = buffer->get_iter_at_offset(range.start_offset);
    Gtk::TextIter end_iter  = buffer->get_iter_at_offset(range.end_offset);
    buffer->apply_tag_by_name(Singleton::Config::source()->types.at(type),
                              begin_iter, end_iter);
  }
}

void Source::ClangViewParse::update_diagnostics() {
  diagnostic_tooltips.clear();
  auto diagnostics=clang_tu->get_diagnostics();
  for(auto &diagnostic: diagnostics) {
    if(diagnostic.path==file_path) {
      auto start=get_buffer()->get_iter_at_offset(diagnostic.offsets.first);
      auto end=get_buffer()->get_iter_at_offset(diagnostic.offsets.second);
      std::string diagnostic_tag_name;
      if(diagnostic.severity<=CXDiagnostic_Warning)
        diagnostic_tag_name="diagnostic_warning";
      else
        diagnostic_tag_name="diagnostic_error";
      
      auto spelling=diagnostic.spelling;
      auto severity_spelling=diagnostic.severity_spelling;
      auto get_tooltip_buffer=[this, spelling, severity_spelling, diagnostic_tag_name]() {
        auto tooltip_buffer=Gtk::TextBuffer::create(get_buffer()->get_tag_table());
        tooltip_buffer->insert_with_tag(tooltip_buffer->get_insert()->get_iter(), severity_spelling, diagnostic_tag_name);
        tooltip_buffer->insert_at_cursor(":\n"+spelling);
        //TODO: Insert newlines to clang_tu->diagnostics[c].spelling (use 80 chars, then newline?)
        return tooltip_buffer;
      };
      diagnostic_tooltips.emplace_back(get_tooltip_buffer, *this, get_buffer()->create_mark(start), get_buffer()->create_mark(end));
      
      auto tag=get_buffer()->create_tag();
      tag->property_underline()=Pango::Underline::UNDERLINE_ERROR;
      auto tag_class=G_OBJECT_GET_CLASS(tag->gobj()); //For older GTK+ 3 versions:
      auto param_spec=g_object_class_find_property(tag_class, "underline-rgba");
      if(param_spec!=NULL) {
        auto diagnostic_tag=get_buffer()->get_tag_table()->lookup(diagnostic_tag_name);
        if(diagnostic_tag!=0)
          tag->set_property("underline-rgba", diagnostic_tag->property_foreground_rgba().get_value());
      }
      get_buffer()->apply_tag(tag, start, end);
    }
  }
}

void Source::ClangViewParse::update_types() {
  type_tooltips.clear();
  for(auto &token: *clang_tokens) {
    if(token.has_type()) {
      auto start=get_buffer()->get_iter_at_offset(token.offsets.first);
      auto end=get_buffer()->get_iter_at_offset(token.offsets.second);
      auto get_tooltip_buffer=[this, &token]() {
        auto tooltip_buffer=Gtk::TextBuffer::create(get_buffer()->get_tag_table());
        tooltip_buffer->insert_at_cursor("Type: "+token.get_type());
        auto brief_comment=token.get_brief_comments();
        if(brief_comment!="")
          tooltip_buffer->insert_at_cursor("\n\n"+brief_comment+".");
        return tooltip_buffer;
      };
      
      type_tooltips.emplace_back(get_tooltip_buffer, *this, get_buffer()->create_mark(start), get_buffer()->create_mark(end));
    }
  }
}

bool Source::ClangViewParse::on_motion_notify_event(GdkEventMotion* event) {
  delayed_tooltips_connection.disconnect();
  if(clang_readable) {
    Gdk::Rectangle rectangle(event->x, event->y, 1, 1);
    Tooltips::init();
    type_tooltips.show(rectangle);
    diagnostic_tooltips.show(rectangle);
  }
  else {
    type_tooltips.hide();
    diagnostic_tooltips.hide();
  }
    
  return Source::View::on_motion_notify_event(event);
}

void Source::ClangViewParse::on_mark_set(const Gtk::TextBuffer::iterator& iterator, const Glib::RefPtr<Gtk::TextBuffer::Mark>& mark) {
  if(get_buffer()->get_has_selection() && mark->get_name()=="selection_bound")
    delayed_tooltips_connection.disconnect();
  
  if(mark->get_name()=="insert") {
    delayed_tooltips_connection.disconnect();
    delayed_tooltips_connection=Glib::signal_timeout().connect([this]() {
      if(clang_readable) {
        Gdk::Rectangle rectangle;
        get_iter_location(get_buffer()->get_insert()->get_iter(), rectangle);
        int location_window_x, location_window_y;
        buffer_to_window_coords(Gtk::TextWindowType::TEXT_WINDOW_TEXT, rectangle.get_x(), rectangle.get_y(), location_window_x, location_window_y);
        rectangle.set_x(location_window_x-2);
        rectangle.set_y(location_window_y);
        rectangle.set_width(4);
        Tooltips::init();
        type_tooltips.show(rectangle);
        diagnostic_tooltips.show(rectangle);
      }
      return false;
    }, 500);
    type_tooltips.hide();
    diagnostic_tooltips.hide();
  }
}

bool Source::ClangViewParse::on_focus_out_event(GdkEventFocus* event) {
    delayed_tooltips_connection.disconnect();
    type_tooltips.hide();
    diagnostic_tooltips.hide();
    return Source::View::on_focus_out_event(event);
}

bool Source::ClangViewParse::on_scroll_event(GdkEventScroll* event) {
  delayed_tooltips_connection.disconnect();
  type_tooltips.hide();
  diagnostic_tooltips.hide();
  return Source::View::on_scroll_event(event);
}

//Clang indentation
//TODO: replace indentation methods with a better implementation or maybe use libclang
bool Source::ClangViewParse::on_key_press_event(GdkEventKey* key) {
  if(get_buffer()->get_has_selection()) {
    return Source::View::on_key_press_event(key);
  }
  auto config=Singleton::Config::source();
  const std::regex bracket_regex(std::string("^(")+config->tab_char+"*).*\\{ *$");
  const std::regex no_bracket_statement_regex(std::string("^(")+config->tab_char+"*)(if|for|else if|catch|while) *\\(.*[^;}] *$");
  const std::regex no_bracket_no_para_statement_regex(std::string("^(")+config->tab_char+"*)(else|try|do) *$");
  const std::regex spaces_regex(std::string("^(")+config->tab_char+"*).*$");
  
  //Indent depending on if/else/etc and brackets
  if(key->keyval==GDK_KEY_Return && key->state==0) {
    string line(get_line_before_insert());
    std::smatch sm;
    if(std::regex_match(line, sm, bracket_regex)) {
      int line_nr=get_source_buffer()->get_insert()->get_iter().get_line();
      if((line_nr+1)<get_source_buffer()->get_line_count()) {
        string next_line=get_line(line_nr+1);
        std::smatch sm2;
        if(std::regex_match(next_line, sm2, spaces_regex)) {
          if(sm2[1].str()==sm[1].str()+config->tab) {
            get_source_buffer()->insert_at_cursor("\n"+sm[1].str()+config->tab);
            scroll_to(get_source_buffer()->get_insert());
            return true;
          }
        }
      }
      //TODO: insert without moving mark backwards.
      get_source_buffer()->insert_at_cursor("\n"+sm[1].str()+config->tab+"\n"+sm[1].str()+"}");
      auto insert_it = get_source_buffer()->get_insert()->get_iter();
      for(size_t c=0;c<config->tab_size+sm[1].str().size();c++)
        insert_it--;
      scroll_to(get_source_buffer()->get_insert());
      get_source_buffer()->place_cursor(insert_it);
      return true;
    }
    else if(std::regex_match(line, sm, no_bracket_statement_regex)) {
      get_source_buffer()->insert_at_cursor("\n"+sm[1].str()+config->tab);
      scroll_to(get_source_buffer()->get_insert());
      return true;
    }
    else if(std::regex_match(line, sm, no_bracket_no_para_statement_regex)) {
      get_source_buffer()->insert_at_cursor("\n"+sm[1].str()+config->tab);
      scroll_to(get_source_buffer()->get_insert());
      return true;
    }
    else if(std::regex_match(line, sm, spaces_regex)) {
      std::smatch sm2;
      size_t line_nr=get_source_buffer()->get_insert()->get_iter().get_line();
      if(line_nr>0 && sm[1].str().size()>=config->tab_size) {
        string previous_line=get_line(line_nr-1);
        if(!std::regex_match(previous_line, sm2, bracket_regex)) {
          if(std::regex_match(previous_line, sm2, no_bracket_statement_regex)) {
            get_source_buffer()->insert_at_cursor("\n"+sm2[1].str());
            scroll_to(get_source_buffer()->get_insert());
            return true;
          }
          else if(std::regex_match(previous_line, sm2, no_bracket_no_para_statement_regex)) {
            get_source_buffer()->insert_at_cursor("\n"+sm2[1].str());
            scroll_to(get_source_buffer()->get_insert());
            return true;
          }
        }
      }
    }
  }
  //Indent left when writing } on a new line
  else if(key->keyval==GDK_KEY_braceright) {
    string line=get_line_before_insert();
    if(line.size()>=config->tab_size) {
      for(auto c: line) {
        if(c!=config->tab_char)
          return Source::View::on_key_press_event(key);
      }
      Gtk::TextIter insert_it = get_source_buffer()->get_insert()->get_iter();
      Gtk::TextIter line_it = get_source_buffer()->get_iter_at_line(insert_it.get_line());
      Gtk::TextIter line_plus_it=line_it;
      for(unsigned c=0;c<config->tab_size;c++)
        line_plus_it++;
      
      get_source_buffer()->erase(line_it, line_plus_it);
    }
    return Source::View::on_key_press_event(key);
  }
  
  return Source::View::on_key_press_event(key);
}

//////////////////////////////
//// ClangViewAutocomplete ///
//////////////////////////////
Source::ClangViewAutocomplete::ClangViewAutocomplete(const std::string& file_path, const std::string& project_path):
Source::ClangViewParse(file_path, project_path), completion_dialog(*this), autocomplete_cancel_starting(false) {
  completion_dialog.on_hide=[this](){
    start_reparse();
  };
  
  get_buffer()->signal_changed().connect([this](){
    if(completion_dialog.shown)
      delayed_reparse_connection.disconnect();
    start_autocomplete();
  });
  get_buffer()->signal_mark_set().connect([this](const Gtk::TextBuffer::iterator& iterator, const Glib::RefPtr<Gtk::TextBuffer::Mark>& mark){
    if(mark->get_name()=="insert") {
      autocomplete_cancel_starting=true;
      if(completion_dialog.shown) {
        completion_dialog.hide();
      }
    }
  });
  signal_scroll_event().connect([this](GdkEventScroll* event){
    if(completion_dialog.shown)
      completion_dialog.hide();
    return false;
  }, false);
  signal_key_release_event().connect([this](GdkEventKey* key){
    if(completion_dialog.shown) {
      if(completion_dialog.on_key_release(key))
        return true;
    }
    
    return false;
  }, false);
}

bool Source::ClangViewAutocomplete::on_key_press_event(GdkEventKey *key) {
  last_keyval=key->keyval;
  if(completion_dialog.shown) {
    if(completion_dialog.on_key_press(key))
      return true;
  }
  return ClangViewParse::on_key_press_event(key);
}

bool Source::ClangViewAutocomplete::on_focus_out_event(GdkEventFocus* event) {
  if(completion_dialog.shown) {
    completion_dialog.hide();
  }
    
  return Source::ClangViewParse::on_focus_out_event(event);
}

void Source::ClangViewAutocomplete::start_autocomplete() {
  if(!((last_keyval>='0' && last_keyval<='9') || 
       (last_keyval>='a' && last_keyval<='z') || (last_keyval>='A' && last_keyval<='Z') ||
       last_keyval=='_' || last_keyval=='>' || last_keyval=='.' || last_keyval==':')) {
    autocomplete_cancel_starting=true;
    return;
  }
  std::string line=" "+get_line_before_insert();
  if((std::count(line.begin(), line.end(), '\"')%2)!=1 && line.find("//")==std::string::npos) {
    const std::regex in_specified_namespace("^(.*[a-zA-Z0-9_\\)])(->|\\.|::)([a-zA-Z0-9_]*)$");
    const std::regex within_namespace("^(.*)([^a-zA-Z0-9_]+)([a-zA-Z0-9_]{3,})$");
    std::smatch sm;
    if(std::regex_match(line, sm, in_specified_namespace)) {
      prefix_mutex.lock();
      prefix=sm[3].str();
      prefix_mutex.unlock();
      if((prefix.size()==0 || prefix[0]<'0' || prefix[0]>'9') && !autocomplete_starting && !completion_dialog.shown) {
        autocomplete();
      }
      else if(last_keyval=='.' && autocomplete_starting)
        autocomplete_cancel_starting=true;
    }
    else if(std::regex_match(line, sm, within_namespace)) {
      prefix_mutex.lock();
      prefix=sm[3].str();
      prefix_mutex.unlock();
      if((prefix.size()==0 || prefix[0]<'0' || prefix[0]>'9') && !autocomplete_starting && !completion_dialog.shown) {
        autocomplete();
      }
    }
    else
      autocomplete_cancel_starting=true;
    if(autocomplete_starting || completion_dialog.shown)
      delayed_reparse_connection.disconnect();
  }
}

void Source::ClangViewAutocomplete::autocomplete() {
  if(!autocomplete_starting) {
    autocomplete_starting=true;
    autocomplete_cancel_starting=false;
    INFO("Source::ClangViewAutocomplete::autocomplete getting autocompletions");
    std::shared_ptr<std::vector<Source::AutoCompleteData> > ac_data=std::make_shared<std::vector<Source::AutoCompleteData> >();
    autocomplete_done_connection.disconnect();
    autocomplete_done_connection=autocomplete_done.connect([this, ac_data](){
      autocomplete_starting=false;
      if(!autocomplete_cancel_starting) {
        if(completion_dialog.start_mark)
          get_buffer()->delete_mark(completion_dialog.start_mark);
        auto start_iter=get_buffer()->get_insert()->get_iter();
        for(size_t c=0;c<prefix.size();c++)
          start_iter--;
        completion_dialog.start_mark=get_buffer()->create_mark(start_iter);
        
        std::map<std::string, std::pair<std::string, std::string> > rows;
        for (auto &data : *ac_data) {
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
          auto ss_str=ss.str();
          if (ss_str.length() > 0) { // if length is 0 the result is empty
            auto pair=std::pair<std::string, std::string>(ss_str, data.brief_comments);
            rows[ss.str() + " --> " + return_value] = pair;
          }
        }
        if (rows.empty()) {
          rows["No suggestions found..."] = std::pair<std::string, std::string>();
        }
        completion_dialog.rows=std::move(rows);
        completion_dialog.show();
      }
      else
        start_autocomplete();
    });
    
    std::shared_ptr<std::map<std::string, std::string> > buffer_map=std::make_shared<std::map<std::string, std::string> >();
    auto& buffer=(*buffer_map)[this->file_path];
    buffer=get_buffer()->get_text(get_buffer()->begin(), get_buffer()->get_insert()->get_iter());
    auto iter = get_source_buffer()->get_insert()->get_iter();
    auto line_nr=iter.get_line()+1;
    auto column_nr=iter.get_line_offset()+1;
    while((buffer.back()>='a' && buffer.back()<='z') || (buffer.back()>='A' && buffer.back()<='Z') || (buffer.back()>='0' && buffer.back()<='9') || buffer.back()=='_') {
      buffer.pop_back();
      column_nr--;
    }
    buffer+="\n";
    std::thread autocomplete_thread([this, ac_data, line_nr, column_nr, buffer_map](){
      parsing_mutex.lock();
      *ac_data=move(get_autocomplete_suggestions(line_nr, column_nr, *buffer_map));
      autocomplete_done();
      parsing_mutex.unlock();
    });
    
    autocomplete_thread.detach();
  }
}

std::vector<Source::AutoCompleteData> Source::ClangViewAutocomplete::
get_autocomplete_suggestions(int line_number, int column, std::map<std::string, std::string>& buffer_map) {
  INFO("Getting auto complete suggestions");
  std::vector<Source::AutoCompleteData> suggestions;
  auto results=clang_tu->get_code_completions(buffer_map, line_number, column);
  if(!autocomplete_cancel_starting) {
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
  DEBUG("Number of suggestions");
  DEBUG_VAR(suggestions.size());
  return suggestions;
}

////////////////////////////
//// ClangViewRefactor /////
////////////////////////////

Source::ClangViewRefactor::ClangViewRefactor(const std::string& file_path, const std::string& project_path):
Source::ClangViewAutocomplete(file_path, project_path), selection_dialog(*this) {
  similar_tokens_tag=get_buffer()->create_tag();
  similar_tokens_tag->property_weight()=Pango::WEIGHT_BOLD;
  
  get_buffer()->signal_changed().connect([this]() {
    if(last_similar_tokens_tagged!="") {
      get_buffer()->remove_tag(similar_tokens_tag, get_buffer()->begin(), get_buffer()->end());
      last_similar_tokens_tagged="";
    }
  });
  
  get_buffer()->signal_mark_set().connect([this](const Gtk::TextBuffer::iterator& iterator, const Glib::RefPtr<Gtk::TextBuffer::Mark>& mark){
    if(mark->get_name()=="insert") {
      bool found=false;
      if(clang_readable) {
        for(auto &token: *clang_tokens) {
          if(token.has_type()) {
            auto insert_offset=(unsigned)get_buffer()->get_insert()->get_iter().get_offset();
            if(insert_offset>=token.offsets.first && insert_offset<=token.offsets.second) {
              found=true;
              auto referenced=token.get_cursor().get_referenced();
              if(referenced) {
                auto usr_and_spelling=referenced.get_usr()+token.get_spelling();
                if(last_similar_tokens_tagged!=usr_and_spelling) {
                  get_buffer()->remove_tag(similar_tokens_tag, get_buffer()->begin(), get_buffer()->end());
                  auto offsets=clang_tokens->get_similar_token_offsets(token);
                  for(auto &offset: offsets) {
                    get_buffer()->apply_tag(similar_tokens_tag, get_buffer()->get_iter_at_offset(offset.first), get_buffer()->get_iter_at_offset(offset.second));
                  }
                  last_similar_tokens_tagged=usr_and_spelling;
                  break;
                }
              }
            }
          }
        }
      }
      if(!found && last_similar_tokens_tagged!="") {
        get_buffer()->remove_tag(similar_tokens_tag, get_buffer()->begin(), get_buffer()->end());
        last_similar_tokens_tagged="";
      }
    }
  });
  
  get_declaration_location=[this](){
    std::pair<std::string, unsigned> location;
    if(clang_readable) {
      for(auto &token: *clang_tokens) {
        if(token.has_type()) {
          auto insert_offset=(unsigned)get_buffer()->get_insert()->get_iter().get_offset();
          if(insert_offset>=token.offsets.first && insert_offset<=token.offsets.second) {
            auto referenced=token.get_cursor().get_referenced();
            if(referenced) {
              location.first=referenced.get_source_location().get_path();
              location.second=referenced.get_source_location().get_offset();
              break;
            }
          }
        }
      }
    }
    return location;
  };
  
  goto_method=[this](){
    if(selection_dialog.start_mark)
      get_buffer()->delete_mark(selection_dialog.start_mark);
    selection_dialog.start_mark=get_buffer()->create_mark(get_buffer()->get_insert()->get_iter());
    
    std::map<std::string, std::pair<std::string, std::string> > rows;
    rows["Not implemented yet"]=std::pair<std::string, std::string>("1", "");
    rows["but you can try the selection search"]=std::pair<std::string, std::string>("2", "");
    rows["search for instance for 'try'"]=std::pair<std::string, std::string>("3", "");
    selection_dialog.rows=std::move(rows);
    selection_dialog.on_select=[this](std::string selected) {
      cout << selected << endl;
    };
    selection_dialog.show();
  };
}

////////////////
//// Source ////
////////////////

Source::Source(const std::string& file_path, std::string project_path) {
  if(project_path=="") {
    project_path=boost::filesystem::path(file_path).parent_path().string();
  }
  if (Singleton::Config::source()->legal_extension(file_path.substr(file_path.find_last_of(".") + 1)))
    view=std::unique_ptr<View>(new ClangView(file_path, project_path));
  else
    view=std::unique_ptr<View>(new GenericView(file_path, project_path));
  INFO("Source Controller with childs constructed");
}
