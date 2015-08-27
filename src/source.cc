#include "source.h"
#include "sourcefile.h"
#include <boost/property_tree/json_parser.hpp>
#include <boost/timer/timer.hpp>
#include "logging.h"
#include <algorithm>
#include "singletons.h"
#include <gtksourceview/gtksource.h>
#include <boost/lexical_cast.hpp>

#include <iostream> //TODO: remove
using namespace std; //TODO: remove

namespace sigc {
  SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
}

Glib::RefPtr<Gsv::Language> Source::guess_language(const boost::filesystem::path &file_path) {
  auto language_manager=Gsv::LanguageManager::get_default();
  bool result_uncertain = false;
  auto content_type = Gio::content_type_guess(file_path.string(), NULL, 0, result_uncertain);
  if(result_uncertain) {
    content_type.clear();
  }
  auto language=language_manager->guess_language(file_path.string(), content_type);
  if(!language) {
    auto filename=file_path.filename().string();
    if(filename=="CMakeLists.txt")
      language=language_manager->get_language("cmake");
    else if(filename=="Makefile")
      language=language_manager->get_language("makefile");
  }
  return language;
}

//////////////
//// View ////
//////////////
Source::View::View(const boost::filesystem::path &file_path): file_path(file_path) {
  set_smart_home_end(Gsv::SMART_HOME_END_BEFORE);
  get_source_buffer()->begin_not_undoable_action();
  if(!juci::filesystem::read(file_path, get_buffer()))
    Singleton::terminal()->print("Error: "+file_path.string()+" is not a valid UTF-8 file.\n");
  get_source_buffer()->end_not_undoable_action();
  get_buffer()->place_cursor(get_buffer()->get_iter_at_offset(0)); 
  search_settings = gtk_source_search_settings_new();
  gtk_source_search_settings_set_wrap_around(search_settings, true);
  search_context = gtk_source_search_context_new(get_source_buffer()->gobj(), search_settings);
  gtk_source_search_context_set_highlight(search_context, true);
  //TODO: why does this not work?: Might be best to use the styles from sourceview. These has to be read from file, search-matches got style "search-match"
  //TODO: in header if trying again: GtkSourceStyle* search_match_style;
  //TODO: We can drop this, only work on newer versions of gtksourceview.
  //search_match_style=(GtkSourceStyle*)g_object_new(GTK_SOURCE_TYPE_STYLE, "background-set", 1, "background", "#00FF00", NULL);
  //gtk_source_search_context_set_match_style(search_context, search_match_style);
  
  //TODO: either use lambda if possible or create a gtkmm wrapper around search_context (including search_settings):
  //TODO: (gtkmm's Gtk::Object has connect_property_changed, so subclassing this might be an idea)
  g_signal_connect(search_context, "notify::occurrences-count", G_CALLBACK(search_occurrences_updated), this);
  
  //TODO: Move this to notebook? Might take up too much memory doing this for every tab.
  auto style_scheme_manager=Gsv::StyleSchemeManager::get_default();
  style_scheme_manager->prepend_search_path(Singleton::style_dir());

  if(Singleton::Config::source()->style.size()>0) {
    auto scheme = style_scheme_manager->get_scheme(Singleton::Config::source()->style);

    if(scheme)
      get_source_buffer()->set_style_scheme(scheme);
    else
      Singleton::terminal()->print("Error: Could not find gtksourceview style: "+Singleton::Config::source()->style+'\n');
  }
  
  property_highlight_current_line() = Singleton::Config::source()->highlight_current_line;
  property_show_line_numbers() = Singleton::Config::source()->show_line_numbers;
  if(Singleton::Config::source()->font.size()>0)
    override_font(Pango::FontDescription(Singleton::Config::source()->font));
  
  tab_char=Singleton::Config::source()->default_tab_char;
  tab_size=Singleton::Config::source()->default_tab_size;
  if(Singleton::Config::source()->auto_tab_char_and_size) {
    auto tab_char_and_size=find_tab_char_and_size();
    if(tab_char_and_size.first!=0) {
      if(tab_char!=tab_char_and_size.first || tab_size!=tab_char_and_size.second) {
        std::string tab_str;
        if(tab_char_and_size.first==' ')
          tab_str="<space>";
        else
          tab_str="<tab>";
        Singleton::terminal()->print("Tab char and size for file "+file_path.string()+" set to: "+tab_str+", "+boost::lexical_cast<std::string>(tab_char_and_size.second)+".\n");
      }
      
      tab_char=tab_char_and_size.first;
      tab_size=tab_char_and_size.second;
    }
  }
  for(unsigned c=0;c<tab_size;c++)
    tab+=tab_char;
  
  tabs_regex=std::regex(std::string("^(")+tab_char+"*)(.*)$");
  
  get_buffer()->signal_changed().connect([this](){
    auto iter=get_buffer()->get_insert()->get_iter();
    if(iter.backward_char()) {
      auto context_iter=iter;
      if(context_iter.backward_char()) {
        if(get_source_buffer()->iter_has_context_class(context_iter, "comment")) {
          //TODO: get word, and spellcheck
          //cout << "comment: " << (char)*iter << endl;
        }
      }
    }
  });
}

void Source::View::search_occurrences_updated(GtkWidget* widget, GParamSpec* property, gpointer data) {
  auto view=(Source::View*)data;
  if(view->update_search_occurrences)
    view->update_search_occurrences(gtk_source_search_context_get_occurrences_count(view->search_context));
}

Source::View::~View() {
  g_clear_object(&search_context);
  g_clear_object(&search_settings);
}

void Source::View::search_highlight(const std::string &text, bool case_sensitive, bool regex) {
  gtk_source_search_settings_set_case_sensitive(search_settings, case_sensitive);
  gtk_source_search_settings_set_regex_enabled(search_settings, regex);
  gtk_source_search_settings_set_search_text(search_settings, text.c_str());
  search_occurrences_updated(NULL, NULL, this);
}

void Source::View::search_forward() {
  Gtk::TextIter insert, selection_bound;
  get_buffer()->get_selection_bounds(insert, selection_bound);
  auto& start=selection_bound;
  Gtk::TextIter match_start, match_end;
  if(gtk_source_search_context_forward(search_context, start.gobj(), match_start.gobj(), match_end.gobj())) {
    get_buffer()->select_range(match_start, match_end);
    scroll_to(get_buffer()->get_insert());
  }
}

void Source::View::search_backward() {
  Gtk::TextIter insert, selection_bound;
  get_buffer()->get_selection_bounds(insert, selection_bound);
  auto &start=insert;
  Gtk::TextIter match_start, match_end;
  if(gtk_source_search_context_backward(search_context, start.gobj(), match_start.gobj(), match_end.gobj())) {
    get_buffer()->select_range(match_start, match_end);
    scroll_to(get_buffer()->get_insert());
  }
}

void Source::View::replace_forward(const std::string &replacement) {
  Gtk::TextIter insert, selection_bound;
  get_buffer()->get_selection_bounds(insert, selection_bound);
  auto &start=insert;
  Gtk::TextIter match_start, match_end;
  if(gtk_source_search_context_forward(search_context, start.gobj(), match_start.gobj(), match_end.gobj())) {
    auto offset=match_start.get_offset();
    gtk_source_search_context_replace(search_context, match_start.gobj(), match_end.gobj(), replacement.c_str(), replacement.size(), NULL);
    
    get_buffer()->select_range(get_buffer()->get_iter_at_offset(offset), get_buffer()->get_iter_at_offset(offset+replacement.size()));
    scroll_to(get_buffer()->get_insert());
  }
}

void Source::View::replace_backward(const std::string &replacement) {
  Gtk::TextIter insert, selection_bound;
  get_buffer()->get_selection_bounds(insert, selection_bound);
  auto &start=selection_bound;
  Gtk::TextIter match_start, match_end;
  if(gtk_source_search_context_backward(search_context, start.gobj(), match_start.gobj(), match_end.gobj())) {
  auto offset=match_start.get_offset();
    gtk_source_search_context_replace(search_context, match_start.gobj(), match_end.gobj(), replacement.c_str(), replacement.size(), NULL);

    get_buffer()->select_range(get_buffer()->get_iter_at_offset(offset), get_buffer()->get_iter_at_offset(offset+replacement.size()));
    scroll_to(get_buffer()->get_insert());
  }
}

void Source::View::replace_all(const std::string &replacement) {
  gtk_source_search_context_replace_all(search_context, replacement.c_str(), replacement.size(), NULL);
}

void Source::View::paste() {
  Gtk::Clipboard::get()->request_text([this](const Glib::ustring& text){
    auto line=get_line_before_insert();
    std::smatch sm;
    std::string prefix_tabs;
    if(!get_buffer()->get_has_selection() && std::regex_match(line, sm, tabs_regex) && sm[2].str().size()==0) {
      prefix_tabs=sm[1].str();

      Glib::ustring::size_type start_line=0;
      Glib::ustring::size_type end_line=0;
      bool paste_line=false;
      bool first_paste_line=true;
      size_t paste_line_tabs=-1;
      bool first_paste_line_has_tabs=false;
      for(Glib::ustring::size_type c=0;c<text.size();c++) {;
        if(text[c]=='\n') {
          end_line=c;
          paste_line=true;
        }
        else if(c==text.size()-1) {
          end_line=c+1;
          paste_line=true;
        }
        if(paste_line) {
          std::string line=text.substr(start_line, end_line-start_line);
          size_t tabs=0;
          for(auto chr: line) {
            if(chr==tab_char)
              tabs++;
            else
              break;
          }
          if(first_paste_line) {
            if(tabs!=0) {
              first_paste_line_has_tabs=true;
              paste_line_tabs=tabs;
            }
            first_paste_line=false;
          }
          else
            paste_line_tabs=std::min(paste_line_tabs, tabs);

          start_line=end_line+1;
          paste_line=false;
        }
      }
      if(paste_line_tabs==(size_t)-1)
        paste_line_tabs=0;
      start_line=0;
      end_line=0;
      paste_line=false;
      first_paste_line=true;
      get_source_buffer()->begin_user_action();
      for(Glib::ustring::size_type c=0;c<text.size();c++) {
        if(text[c]=='\n') {
          end_line=c;
          paste_line=true;
        }
        else if(c==text.size()-1) {
          end_line=c+1;
          paste_line=true;
        }
        if(paste_line) {
          if(first_paste_line) {
            if(first_paste_line_has_tabs)
              get_buffer()->insert_at_cursor(text.substr(start_line+paste_line_tabs, end_line-start_line-paste_line_tabs));
            else
              get_buffer()->insert_at_cursor(text.substr(start_line, end_line-start_line));
            first_paste_line=false;
          }
          else
            get_buffer()->insert_at_cursor('\n'+prefix_tabs+text.substr(start_line+paste_line_tabs, end_line-start_line-paste_line_tabs));
          start_line=end_line+1;
          paste_line=false;
        }
      }
      get_buffer()->place_cursor(get_buffer()->get_insert()->get_iter());
      scroll_to(get_buffer()->get_insert());
      get_source_buffer()->end_user_action();
    }
    else
      get_buffer()->paste_clipboard(Gtk::Clipboard::get());
  });
}

void Source::View::set_status(const std::string &status) {
  this->status=status;
  if(on_update_status)
    on_update_status(this, status);
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
  get_source_buffer()->begin_user_action();
  //Indent as in next or previous line
  if(key->keyval==GDK_KEY_Return && key->state==0 && !get_buffer()->get_has_selection()) {
    auto insert_it=get_buffer()->get_insert()->get_iter();
    int line_nr=insert_it.get_line();
    auto line=get_line_before_insert();
    std::smatch sm;
    if(std::regex_match(line, sm, tabs_regex)) {
      if((line_nr+1)<get_buffer()->get_line_count()) {
        string next_line=get_line(line_nr+1);
        auto line_end_iter=get_buffer()->get_iter_at_line(line_nr+1);
        line_end_iter--;
        std::smatch sm2;
        if(insert_it==line_end_iter && std::regex_match(next_line, sm2, tabs_regex)) {
          if(sm2[1].str().size()>sm[1].str().size()) {
            get_source_buffer()->insert_at_cursor("\n"+sm2[1].str());
            scroll_to(get_source_buffer()->get_insert());
            get_source_buffer()->end_user_action();
            return true;
          }
        }
      }
      get_source_buffer()->insert_at_cursor("\n"+sm[1].str());
      scroll_to(get_source_buffer()->get_insert());
      get_source_buffer()->end_user_action();
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
      get_source_buffer()->insert(line_it, tab);
    }
    get_source_buffer()->end_user_action();
    return true;
  }
  //Indent left when clicking shift-tab, no matter where in the line the cursor is. Also works on selected text.
  else if((key->keyval==GDK_KEY_ISO_Left_Tab || key->keyval==GDK_KEY_Tab) && key->state==GDK_SHIFT_MASK) {
    Gtk::TextIter selection_start, selection_end;
    get_source_buffer()->get_selection_bounds(selection_start, selection_end);
    int line_start=selection_start.get_line();
    int line_end=selection_end.get_line();
    
    unsigned indent_left_steps=tab_size;
    for(int line_nr=line_start;line_nr<=line_end;line_nr++) {
      string line=get_line(line_nr);
      std::smatch sm;
      if(std::regex_match(line, sm, tabs_regex) && sm[1].str().size()>0) {
        indent_left_steps=std::min(indent_left_steps, (unsigned)sm[1].str().size());
      }
      else {
        get_source_buffer()->end_user_action();
        return true;
      }
    }
    
    for(int line_nr=line_start;line_nr<=line_end;line_nr++) {
      Gtk::TextIter line_it = get_source_buffer()->get_iter_at_line(line_nr);
      Gtk::TextIter line_plus_it=line_it;
      
      for(unsigned c=0;c<indent_left_steps;c++)
        line_plus_it++;
      get_source_buffer()->erase(line_it, line_plus_it);
    }
    get_source_buffer()->end_user_action();
    return true;
  }
  //"Smart" backspace key
  else if(key->keyval==GDK_KEY_BackSpace && !get_buffer()->get_has_selection()) {
    auto insert_it=get_buffer()->get_insert()->get_iter();
    int line_nr=insert_it.get_line();
    auto line=get_line_before_insert();
    std::smatch sm;
    if(std::regex_match(line, sm, tabs_regex) && sm[1].str().size()==line.size()) {
      if((line_nr-1)>=0) {
        string previous_line=get_line(line_nr-1);
        std::smatch sm2;
        if(std::regex_match(previous_line, sm2, tabs_regex)) {
          if(line.size()==sm2[1].str().size() || line.size()==sm2[1].str().size()+tab_size || line.size()==sm2[1].str().size()-tab_size) {
            auto previous_line_end_it=insert_it;
            for(unsigned c=0;c<line.size();c++)
              previous_line_end_it--;
            previous_line_end_it--;
            get_source_buffer()->erase(previous_line_end_it, insert_it);
            get_source_buffer()->end_user_action();
            return true;
          }
        }
      }
      if(line.size()>=tab_size) {
        auto insert_minus_tab_it=insert_it;
        for(unsigned c=0;c<tab_size;c++)
          insert_minus_tab_it--;
        get_source_buffer()->erase(insert_minus_tab_it, insert_it);
        get_source_buffer()->end_user_action();
        return true;
      }
    }
  }

  bool stop=Gsv::View::on_key_press_event(key);
  get_source_buffer()->end_user_action();
  return stop;
}

bool Source::View::on_button_press_event(GdkEventButton *event) {
  if(event->type==GDK_2BUTTON_PRESS) {
    Gtk::TextIter start, end;
    if(get_buffer()->get_selection_bounds(start, end)) {
      auto iter=start;
      while((*iter>=48 && *iter<=57) || (*iter>=65 && *iter<=90) || (*iter>=97 && *iter<=122) || *iter==95) {
        start=iter;
        if(!iter.backward_char())
          break;
      }
      while((*end>=48 && *end<=57) || (*end>=65 && *end<=90) || (*end>=97 && *end<=122) || *end==95) {
        if(!end.forward_char())
          break;
      }
      get_buffer()->select_range(start, end);
      return true;
    }
  }
  
  return Gsv::View::on_button_press_event(event);
}

std::pair<char, unsigned> Source::View::find_tab_char_and_size() {
  const std::regex indent_regex("^([ \t]+).*$");
  auto size=get_buffer()->get_line_count();
  std::unordered_map<char, size_t> tab_chars;
  std::unordered_map<unsigned, size_t> tab_sizes;
  unsigned last_tab_size=0;
  for(int c=0;c<size;c++) {
    auto line=get_line(c);
    std::smatch sm;
    if(std::regex_match(line, sm, indent_regex)) {
      auto str=sm[1].str();
      
      long tab_diff=abs(static_cast<long>(str.size()-last_tab_size));
      if(tab_diff>0) {
        unsigned tab_diff_unsigned=static_cast<unsigned>(tab_diff);
        auto it_size=tab_sizes.find(tab_diff_unsigned);
        if(it_size!=tab_sizes.end())
          it_size->second++;
        else
          tab_sizes[tab_diff_unsigned]=1;
      }
      
      last_tab_size=str.size();
      
      if(str.size()>0) {
        auto it_char=tab_chars.find(str[0]);
        if(it_char!=tab_chars.end())
          it_char->second++;
        else
          tab_chars[str[0]]=1;
      }
    }
  }
  char found_tab_char=0;
  size_t occurences=0;
  for(auto &tab_char: tab_chars) {
    if(tab_char.second>occurences) {
      found_tab_char=tab_char.first;
      occurences=tab_char.second;
    }
  }
  unsigned found_tab_size=0;
  occurences=0;
  for(auto &tab_size: tab_sizes) {
    if(tab_size.second>occurences) {
      found_tab_size=tab_size.first;
      occurences=tab_size.second;
    }
  }
  return {found_tab_char, found_tab_size};
}

/////////////////////
//// GenericView ////
/////////////////////
Source::GenericView::GenericView(const boost::filesystem::path &file_path, Glib::RefPtr<Gsv::Language> language) : View(file_path) {
  if(language) {
    get_source_buffer()->set_language(language);
    Singleton::terminal()->print("Language for file "+file_path.string()+" set to "+language->get_name()+".\n");
  }
  auto completion=get_completion();
  auto completion_words=Gsv::CompletionWords::create("", Glib::RefPtr<Gdk::Pixbuf>());
  completion_words->register_provider(get_buffer());
  completion->add_provider(completion_words);
  completion->property_show_headers()=false;
  completion->property_show_icons()=false;
  completion->property_accelerators()=0;
}

////////////////////////
//// ClangViewParse ////
////////////////////////
clang::Index Source::ClangViewParse::clang_index(0, 0);

Source::ClangViewParse::ClangViewParse(const boost::filesystem::path &file_path, const boost::filesystem::path& project_path):
Source::View(file_path), project_path(project_path) {
  auto scheme = get_source_buffer()->get_style_scheme();
  auto tag_table=get_buffer()->get_tag_table();
  for (auto &item : Singleton::Config::source()->clang_types) {
    if(!tag_table->lookup(item.second)) {
      auto style = scheme->get_style(item.second);
      auto tag = get_source_buffer()->create_tag(item.second);
      if (style) {
        DEBUG("Style " + item.second + " found in style " + scheme->get_name());
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
        DEBUG("Style " + item.second + " not found in " + scheme->get_name());
    }
  }
  INFO("Tagtable filled");  
  
  //Create tags for diagnostic warnings and errors:
  auto style=scheme->get_style("def:warning");
  auto diagnostic_tag=get_source_buffer()->create_tag("def:warning");
  auto diagnostic_tag_underline=get_source_buffer()->create_tag("def:warning_underline");
  if(style && (style->property_foreground_set() || style->property_background_set())) {
    Glib::ustring warning_property;
    if(style->property_foreground_set()) {
      warning_property=style->property_foreground().get_value();
      diagnostic_tag->property_foreground() = warning_property;
    }
    else if(style->property_background_set())
      warning_property=style->property_background().get_value();

    diagnostic_tag_underline->property_underline()=Pango::Underline::UNDERLINE_ERROR;
    auto tag_class=G_OBJECT_GET_CLASS(diagnostic_tag_underline->gobj()); //For older GTK+ 3 versions:
    auto param_spec=g_object_class_find_property(tag_class, "underline-rgba");
    if(param_spec!=NULL) {
      diagnostic_tag_underline->set_property("underline-rgba", Gdk::RGBA(warning_property));
    }
  }
  style=scheme->get_style("def:error");
  diagnostic_tag=get_source_buffer()->create_tag("def:error");
  diagnostic_tag_underline=get_source_buffer()->create_tag("def:error_underline");
  if(style && (style->property_foreground_set() || style->property_background_set())) {
    Glib::ustring error_property;
    if(style->property_foreground_set()) {
      error_property=style->property_foreground().get_value();
      diagnostic_tag->property_foreground() = error_property;
    }
    else if(style->property_background_set())
      error_property=style->property_background().get_value();
    
    diagnostic_tag_underline->property_underline()=Pango::Underline::UNDERLINE_ERROR;
    auto tag_class=G_OBJECT_GET_CLASS(diagnostic_tag_underline->gobj()); //For older GTK+ 3 versions:
    auto param_spec=g_object_class_find_property(tag_class, "underline-rgba");
    if(param_spec!=NULL) {
      diagnostic_tag_underline->set_property("underline-rgba", Gdk::RGBA(error_property));
    }
  }
  //TODO: clear tag_class and param_spec?
  
  //Add tooltip foreground and background
  style = scheme->get_style("def:note");
  auto note_tag=get_source_buffer()->create_tag("def:note_background");
  if(style->property_background_set()) {
    note_tag->property_background()=style->property_background();
  }
  note_tag=get_source_buffer()->create_tag("def:note");
  if(style->property_foreground_set()) {
    note_tag->property_foreground()=style->property_foreground();
  }
  
  parsing_in_progress=Singleton::terminal()->print_in_progress("Parsing "+file_path.string());
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
  parse_done.connect([this](){
    if(parse_thread_mapped) {
      if(parsing_mutex.try_lock()) {
        INFO("Updating syntax");
        update_syntax();
        update_diagnostics();
        update_types();
        clang_readable=true;
        set_status("");
        parsing_mutex.unlock();
        INFO("Syntax updated");
      }
      parsing_in_progress->done("done");
    }
    else {
      parse_thread_go=true;
    }
  });
  init_parse();
  
  get_buffer()->signal_changed().connect([this]() {
    start_reparse();
    type_tooltips.hide();
    diagnostic_tooltips.hide();
  });
  
  get_buffer()->signal_mark_set().connect(sigc::mem_fun(*this, &Source::ClangViewParse::on_mark_set), false);
  
  bracket_regex=std::regex(std::string("^(")+tab_char+"*).*\\{ *$");
  no_bracket_statement_regex=std::regex(std::string("^(")+tab_char+"*)(if|for|else if|catch|while) *\\(.*[^;}] *$");
  no_bracket_no_para_statement_regex=std::regex(std::string("^(")+tab_char+"*)(else|try|do) *$");
}

void Source::ClangViewParse::init_parse() {
  type_tooltips.hide();
  diagnostic_tooltips.hide();
  get_buffer()->remove_all_tags(get_buffer()->begin(), get_buffer()->end());
  clang_readable=false;
  parse_thread_go=true;
  parse_thread_mapped=false;
  parse_thread_stop=false;
  
  
  auto buffer_map=get_buffer_map();
  //Remove includes for first parse for initial syntax highlighting
  auto& str=buffer_map[file_path.string()];
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
  init_syntax_highlighting(buffer_map);
  update_syntax();
    
  set_status("parsing...");
  if(parse_thread.joinable())
    parse_thread.join();
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
}

void Source::ClangViewParse::init_syntax_highlighting(const std::map<std::string, std::string> &buffers) {
  std::vector<string> arguments = get_compilation_commands();
  clang_tu = std::unique_ptr<clang::TranslationUnit>(new clang::TranslationUnit(clang_index,
                                                                           file_path.string(),
                                                                           arguments,
                                                                           buffers));
  clang_tokens=clang_tu->get_tokens(0, buffers.find(file_path.string())->second.size()-1);
}

std::map<std::string, std::string> Source::ClangViewParse::get_buffer_map() const {
  std::map<std::string, std::string> buffer_map;
  buffer_map[file_path.string()]=get_source_buffer()->get_text();
  return buffer_map;
}

void Source::ClangViewParse::start_reparse() {
  parse_thread_mapped=false;
  clang_readable=false;
  delayed_reparse_connection.disconnect();
  delayed_reparse_connection=Glib::signal_timeout().connect([this]() {
    clang_readable=false;
    parse_thread_go=true;
    set_status("parsing...");
    return false;
  }, 1000);
}

int Source::ClangViewParse::reparse(const std::map<std::string, std::string> &buffer) {
  int status = clang_tu->ReparseTranslationUnit(buffer);
  clang_tokens=clang_tu->get_tokens(0, parse_thread_buffer_map.find(file_path.string())->second.size()-1);
  return status;
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
  if(file_path.extension()==".h") //TODO: temporary fix for .h-files (parse as c++)
    arguments.emplace_back("-xc++");
#ifdef _WIN32 //Temporary fix to MSYS2's libclang
  arguments.emplace_back("-IC:/msys32/mingw32/lib/gcc/i686-w64-mingw32/5.2.0/include");
  arguments.emplace_back("-IC:/msys32/mingw32//include");
  arguments.emplace_back("-IC:/msys32/mingw32/lib/gcc/i686-w64-mingw32/5.2.0/include-fixed");
  arguments.emplace_back("-IC:/msys32/mingw32/i686-w64-mingw32/include");
  arguments.emplace_back("-IC:/msys32/mingw32/include/c++/5.2.0");
  arguments.emplace_back("-IC:/msys32/mingw32/include/c++/5.2.0/i686-w64-mingw32");
  arguments.emplace_back("-IC:/msys32/mingw32/include/c++/5.2.0/backward");
  
  arguments.emplace_back("-IC:/msys32/mingw64/lib/gcc/i686-w64-mingw32/5.2.0/include");
  arguments.emplace_back("-IC:/msys32/mingw64//include");
  arguments.emplace_back("-IC:/msys32/mingw64/lib/gcc/i686-w64-mingw32/5.2.0/include-fixed");
  arguments.emplace_back("-IC:/msys32/mingw64/i686-w64-mingw32/include");
  arguments.emplace_back("-IC:/msys32/mingw64/include/c++/5.2.0");
  arguments.emplace_back("-IC:/msys32/mingw64/include/c++/5.2.0/i686-w64-mingw32");
  arguments.emplace_back("-IC:/msys32/mingw64/include/c++/5.2.0/backward");
  
  arguments.emplace_back("-IC:/msys64/mingw32/lib/gcc/i686-w64-mingw32/5.2.0/include");
  arguments.emplace_back("-IC:/msys64/mingw32//include");
  arguments.emplace_back("-IC:/msys64/mingw32/lib/gcc/i686-w64-mingw32/5.2.0/include-fixed");
  arguments.emplace_back("-IC:/msys64/mingw32/i686-w64-mingw32/include");
  arguments.emplace_back("-IC:/msys64/mingw32/include/c++/5.2.0");
  arguments.emplace_back("-IC:/msys64/mingw32/include/c++/5.2.0/i686-w64-mingw32");
  arguments.emplace_back("-IC:/msys64/mingw32/include/c++/5.2.0/backward");
  
  arguments.emplace_back("-IC:/msys64/mingw64/lib/gcc/i686-w64-mingw32/5.2.0/include");
  arguments.emplace_back("-IC:/msys64/mingw64//include");
  arguments.emplace_back("-IC:/msys64/mingw64/lib/gcc/i686-w64-mingw32/5.2.0/include-fixed");
  arguments.emplace_back("-IC:/msys64/mingw64/i686-w64-mingw32/include");
  arguments.emplace_back("-IC:/msys64/mingw64/include/c++/5.2.0");
  arguments.emplace_back("-IC:/msys64/mingw64/include/c++/5.2.0/i686-w64-mingw32");
  arguments.emplace_back("-IC:/msys64/mingw64/include/c++/5.2.0/backward");
#endif
  return arguments;
}

void Source::ClangViewParse::update_syntax() {
  std::vector<Source::Range> ranges;
  for (auto &token : *clang_tokens) {
    //if(token.get_kind()==0) // PunctuationToken
      //ranges.emplace_back(token.offsets, (int) token.get_cursor().get_kind());
    if(token.get_kind()==1) // KeywordToken
      ranges.emplace_back(token.offsets, 702);
    else if(token.get_kind()==2) {// IdentifierToken 
      auto kind=(int)token.get_cursor().get_kind();
      if(kind==101 || kind==102)
        kind=(int)token.get_cursor().get_referenced().get_kind();
      ranges.emplace_back(token.offsets, kind); //TODO: Need to get type of referenced token if any
    }
    else if(token.get_kind()==3) // LiteralToken
      ranges.emplace_back(token.offsets, 109);
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
    auto type = boost::lexical_cast<std::string>(range.kind);
    try {
      last_syntax_tags.emplace(Singleton::Config::source()->clang_types.at(type));
    } catch (std::exception) {
      //cout << range.kind << ": " << range.offsets.first.line << ", " << range.offsets.first.index << endl; //TODO: remove
      continue;
    }
    
    Gtk::TextIter begin_iter = buffer->get_iter_at_line_index(range.offsets.first.line-1, range.offsets.first.index-1);
    Gtk::TextIter end_iter  = buffer->get_iter_at_line_index(range.offsets.second.line-1, range.offsets.second.index-1);
    buffer->apply_tag_by_name(Singleton::Config::source()->clang_types.at(type), begin_iter, end_iter);
  }
}

void Source::ClangViewParse::update_diagnostics() {
  diagnostic_tooltips.clear();
  get_buffer()->remove_tag_by_name("def:warning_underline", get_buffer()->begin(), get_buffer()->end());
  get_buffer()->remove_tag_by_name("def:error_underline", get_buffer()->begin(), get_buffer()->end());
  auto diagnostics=clang_tu->get_diagnostics();
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
      if(end_line_index>=end_line.size()) {
        if(end_line.size()==0)
          end_line_index=0;
        else
          end_line_index=end_line.size()-1;
      }
      auto start=get_buffer()->get_iter_at_line_index(diagnostic.offsets.first.line-1, start_line_index);
      auto end=get_buffer()->get_iter_at_line_index(diagnostic.offsets.second.line-1, end_line_index);
      std::string diagnostic_tag_name;
      if(diagnostic.severity<=CXDiagnostic_Warning)
        diagnostic_tag_name="def:warning";
      else
        diagnostic_tag_name="def:error";
      
      auto spelling=diagnostic.spelling;
      auto severity_spelling=diagnostic.severity_spelling;
      auto create_tooltip_buffer=[this, spelling, severity_spelling, diagnostic_tag_name]() {
        auto tooltip_buffer=Gtk::TextBuffer::create(get_buffer()->get_tag_table());
        tooltip_buffer->insert_with_tag(tooltip_buffer->get_insert()->get_iter(), severity_spelling, diagnostic_tag_name);
        tooltip_buffer->insert_with_tag(tooltip_buffer->get_insert()->get_iter(), ":\n"+spelling, "def:note");
        //TODO: Insert newlines to clang_tu->diagnostics[c].spelling (use 80 chars, then newline?)
        return tooltip_buffer;
      };
      diagnostic_tooltips.emplace_back(create_tooltip_buffer, *this, get_buffer()->create_mark(start), get_buffer()->create_mark(end));
    
      get_buffer()->apply_tag_by_name(diagnostic_tag_name+"_underline", start, end);
    }
  }
}

void Source::ClangViewParse::update_types() {
  type_tooltips.clear();
  for(auto &token: *clang_tokens) {
    if(token.get_kind()==clang::Token_Identifier && token.has_type()) {
      auto start=get_buffer()->get_iter_at_line_index(token.offsets.first.line-1, token.offsets.first.index-1);
      auto end=get_buffer()->get_iter_at_line_index(token.offsets.second.line-1, token.offsets.second.index-1);
      auto create_tooltip_buffer=[this, &token]() {
        auto tooltip_buffer=Gtk::TextBuffer::create(get_buffer()->get_tag_table());
        tooltip_buffer->insert_with_tag(tooltip_buffer->get_insert()->get_iter(), "Type: "+token.get_type(), "def:note");
        auto brief_comment=token.get_brief_comments();
        if(brief_comment!="")
          tooltip_buffer->insert_with_tag(tooltip_buffer->get_insert()->get_iter(), "\n\n"+brief_comment, "def:note");
        return tooltip_buffer;
      };
      
      type_tooltips.emplace_back(create_tooltip_buffer, *this, get_buffer()->create_mark(start), get_buffer()->create_mark(end));
    }
  }
}

bool Source::ClangViewParse::on_motion_notify_event(GdkEventMotion* event) {
  if(on_motion_last_x!=event->x || on_motion_last_y!=event->y) {
    delayed_tooltips_connection.disconnect();
    if(event->state==0) {
      gdouble x=event->x;
      gdouble y=event->y;
      delayed_tooltips_connection=Glib::signal_timeout().connect([this, x, y]() {
        if(clang_readable) {
          Gdk::Rectangle rectangle(x, y, 1, 1);
          Tooltips::init();
          type_tooltips.show(rectangle);
          diagnostic_tooltips.show(rectangle);
        }
        return false;
      }, 100);
    }
    type_tooltips.hide();
    diagnostic_tooltips.hide();
  }
  on_motion_last_x=event->x;
  on_motion_last_y=event->y;
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
  get_source_buffer()->begin_user_action();
  
  //Indent depending on if/else/etc and brackets
  if(key->keyval==GDK_KEY_Return && key->state==0) {
    string line(get_line_before_insert());
    std::smatch sm;
    if(std::regex_match(line, sm, bracket_regex)) {
      int line_nr=get_source_buffer()->get_insert()->get_iter().get_line();
      if((line_nr+1)<get_source_buffer()->get_line_count()) {
        string next_line=get_line(line_nr+1);
        std::smatch sm2;
        if(std::regex_match(next_line, sm2, tabs_regex)) {
          if(sm2[1].str()==sm[1].str()+tab) {
            get_source_buffer()->insert_at_cursor("\n"+sm[1].str()+tab);
            scroll_to(get_source_buffer()->get_insert());
            get_source_buffer()->end_user_action();
            return true;
          }
        }
        auto next_char=*get_buffer()->get_insert()->get_iter();
        auto next_line_with_end_bracket=sm[1].str()+"}";
        if(next_char!='}' && next_line.substr(0, next_line_with_end_bracket.size())!=next_line_with_end_bracket) {
          get_source_buffer()->insert_at_cursor("\n"+sm[1].str()+tab+"\n"+sm[1].str()+"}");
          auto insert_it = get_source_buffer()->get_insert()->get_iter();
          for(size_t c=0;c<sm[1].str().size()+2;c++)
            insert_it--;
          scroll_to(get_source_buffer()->get_insert());
          get_source_buffer()->place_cursor(insert_it);
          get_source_buffer()->end_user_action();
          return true;
        }
        else if(next_char=='}') {
          get_source_buffer()->insert_at_cursor("\n"+sm[1].str()+tab+"\n"+sm[1].str());
          auto insert_it = get_source_buffer()->get_insert()->get_iter();
          for(size_t c=0;c<sm[1].str().size()+1;c++)
            insert_it--;
          scroll_to(get_source_buffer()->get_insert());
          get_source_buffer()->place_cursor(insert_it);
          get_source_buffer()->end_user_action();
          return true;
        }
        else {
          get_source_buffer()->insert_at_cursor("\n"+sm[1].str()+tab);
          scroll_to(get_source_buffer()->get_insert());
          get_source_buffer()->end_user_action();
          return true;
        }
      }
    }
    else if(std::regex_match(line, sm, no_bracket_statement_regex)) {
      get_source_buffer()->insert_at_cursor("\n"+sm[1].str()+tab);
      scroll_to(get_source_buffer()->get_insert());
      get_source_buffer()->end_user_action();
      return true;
    }
    else if(std::regex_match(line, sm, no_bracket_no_para_statement_regex)) {
      get_source_buffer()->insert_at_cursor("\n"+sm[1].str()+tab);
      scroll_to(get_source_buffer()->get_insert());
      get_source_buffer()->end_user_action();
      return true;
    }
    else if(std::regex_match(line, sm, tabs_regex)) {
      std::smatch sm2;
      size_t line_nr=get_source_buffer()->get_insert()->get_iter().get_line();
      if(line_nr>0 && sm[1].str().size()>=tab_size) {
        string previous_line=get_line(line_nr-1);
        if(!std::regex_match(previous_line, sm2, bracket_regex)) {
          if(std::regex_match(previous_line, sm2, no_bracket_statement_regex)) {
            get_source_buffer()->insert_at_cursor("\n"+sm2[1].str());
            scroll_to(get_source_buffer()->get_insert());
            get_source_buffer()->end_user_action();
            return true;
          }
          else if(std::regex_match(previous_line, sm2, no_bracket_no_para_statement_regex)) {
            get_source_buffer()->insert_at_cursor("\n"+sm2[1].str());
            scroll_to(get_source_buffer()->get_insert());
            get_source_buffer()->end_user_action();
            return true;
          }
        }
      }
    }
  }
  //Indent left when writing } on a new line
  else if(key->keyval==GDK_KEY_braceright) {
    string line=get_line_before_insert();
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
      for(unsigned c=0;c<tab_size;c++)
        line_plus_it++;
      
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
Source::ClangViewAutocomplete::ClangViewAutocomplete(const boost::filesystem::path &file_path, const boost::filesystem::path& project_path):
Source::ClangViewParse(file_path, project_path), autocomplete_cancel_starting(false) {
  get_buffer()->signal_changed().connect([this](){
    if(completion_dialog_shown)
      delayed_reparse_connection.disconnect();
    start_autocomplete();
  });
  get_buffer()->signal_mark_set().connect([this](const Gtk::TextBuffer::iterator& iterator, const Glib::RefPtr<Gtk::TextBuffer::Mark>& mark){
    if(mark->get_name()=="insert") {
      autocomplete_cancel_starting=true;
      if(completion_dialog_shown) {
        completion_dialog->hide();
      }
    }
  });
  signal_scroll_event().connect([this](GdkEventScroll* event){
    if(completion_dialog_shown)
      completion_dialog->hide();
    return false;
  }, false);
  signal_key_release_event().connect([this](GdkEventKey* key){
    if(completion_dialog_shown) {
      if(completion_dialog->on_key_release(key))
        return true;
    }
    
    return false;
  }, false);

}

bool Source::ClangViewAutocomplete::on_key_press_event(GdkEventKey *key) {
  last_keyval=key->keyval;
  if(completion_dialog_shown) {
    if(completion_dialog->on_key_press(key))
      return true;
  }
  return ClangViewParse::on_key_press_event(key);
}

bool Source::ClangViewAutocomplete::on_focus_out_event(GdkEventFocus* event) {
  autocomplete_cancel_starting=true;
  if(completion_dialog_shown) {
    completion_dialog->hide();
  }
    
  return Source::ClangViewParse::on_focus_out_event(event);
}

void Source::ClangViewAutocomplete::start_autocomplete() {
  if(!has_focus())
    return;
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
      if((prefix.size()==0 || prefix[0]<'0' || prefix[0]>'9') && !autocomplete_starting && !completion_dialog_shown) {
        autocomplete();
      }
      else if(last_keyval=='.' && autocomplete_starting)
        autocomplete_cancel_starting=true;
    }
    else if(std::regex_match(line, sm, within_namespace)) {
      prefix_mutex.lock();
      prefix=sm[3].str();
      prefix_mutex.unlock();
      if((prefix.size()==0 || prefix[0]<'0' || prefix[0]>'9') && !autocomplete_starting && !completion_dialog_shown) {
        autocomplete();
      }
    }
    else
      autocomplete_cancel_starting=true;
    if(autocomplete_starting || completion_dialog_shown)
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
        auto start_iter=get_buffer()->get_insert()->get_iter();
        for(size_t c=0;c<prefix.size();c++)
          start_iter--;
        completion_dialog=std::unique_ptr<CompletionDialog>(new CompletionDialog(*this, get_buffer()->create_mark(start_iter)));
        auto rows=std::make_shared<std::unordered_map<std::string, std::string> >();
        completion_dialog->on_hide=[this](){
          start_reparse();
          get_source_buffer()->end_user_action();
          completion_dialog_shown=false;
        };
        completion_dialog->on_select=[this, rows](const std::string& selected, bool hide_window) {
          auto row = rows->at(selected);
          get_buffer()->erase(completion_dialog->start_mark->get_iter(), get_buffer()->get_insert()->get_iter());
          get_buffer()->insert(completion_dialog->start_mark->get_iter(), row);
          if(hide_window) {
            char find_char=row.back();
            if(find_char==')' || find_char=='>') {
              if(find_char==')')
                find_char='(';
              else
                find_char='<';
              size_t pos=row.find(find_char);
              if(pos!=std::string::npos) {
                auto start_offset=completion_dialog->start_mark->get_iter().get_offset()+pos+1;
                auto end_offset=completion_dialog->start_mark->get_iter().get_offset()+row.size()-1;
                if(start_offset!=end_offset)
                  get_buffer()->select_range(get_buffer()->get_iter_at_offset(start_offset), get_buffer()->get_iter_at_offset(end_offset));
              }
            }
          }
        };
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
            (*rows)[ss.str() + " --> " + return_value] = ss_str;
            completion_dialog->add_row(ss.str() + " --> " + return_value, data.brief_comments);
          }
        }
        set_status("");
        if (!rows->empty()) {
          completion_dialog_shown=true;
          get_source_buffer()->begin_user_action();
          completion_dialog->show();
        }
      }
      else {
        set_status("");
        start_autocomplete();
      }
    });
    
    std::shared_ptr<std::map<std::string, std::string> > buffer_map=std::make_shared<std::map<std::string, std::string> >();
    auto& buffer=(*buffer_map)[this->file_path.string()];
    auto iter=get_buffer()->get_insert()->get_iter();
    buffer=get_buffer()->get_text(get_buffer()->begin(), iter);
    auto line_nr=iter.get_line()+1;
    auto column_nr=iter.get_line_offset()+1;
    unsigned c=1;
    if(c<=buffer.size()) {
      char *chr=&buffer[buffer.size()-c];
      while((*chr>='a' && *chr<='z') || (*chr>='A' && *chr<='Z') || (*chr>='0' && *chr<='9') || *chr=='_') {
        *chr=' ';
        column_nr--;
        c++;
        if(c>buffer.size())
          break;
        chr--;
      }
    }
    buffer+=get_buffer()->get_text(iter, get_buffer()->end());
    set_status("autocomplete...");
    if(autocomplete_thread.joinable())
      autocomplete_thread.join();
    autocomplete_thread=std::thread([this, ac_data, line_nr, column_nr, buffer_map](){
      parsing_mutex.lock();
      *ac_data=move(get_autocomplete_suggestions(line_nr, column_nr, *buffer_map));
      autocomplete_done();
      parsing_mutex.unlock();
    });
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

Source::ClangViewRefactor::ClangViewRefactor(const boost::filesystem::path &file_path, const boost::filesystem::path& project_path):
Source::ClangViewAutocomplete(file_path, project_path) {
  similar_tokens_tag=get_buffer()->create_tag();
  similar_tokens_tag->property_weight()=Pango::WEIGHT_BOLD;
  
  get_buffer()->signal_changed().connect([this]() {
    if(!renaming && last_similar_tokens_tagged!="") {
      get_buffer()->remove_tag(similar_tokens_tag, get_buffer()->begin(), get_buffer()->end());
      last_similar_tokens_tagged="";
    }
  });
  
  get_token=[this]() -> std::string {
    if(clang_readable) {
      auto iter=get_buffer()->get_insert()->get_iter();
      auto line=(unsigned)iter.get_line();
      auto index=(unsigned)iter.get_line_index();
      for(auto &token: *clang_tokens) {
        if(token.get_kind()==clang::Token_Identifier && token.has_type()) {
          if(line==token.offsets.first.line-1 && index>=token.offsets.first.index-1 && index <=token.offsets.second.index-1) {
            auto referenced=token.get_cursor().get_referenced();
            if(referenced)
              return referenced.get_usr();
          }
        }
      }
    }
    return "";
  };
  
  get_token_name=[this]() -> std::string {
    if(clang_readable) {
      auto iter=get_buffer()->get_insert()->get_iter();
      auto line=(unsigned)iter.get_line();
      auto index=(unsigned)iter.get_line_index();
      for(auto &token: *clang_tokens) {
        if(token.get_kind()==clang::Token_Identifier && token.has_type()) {
          if(line==token.offsets.first.line-1 && index>=token.offsets.first.index-1 && index <=token.offsets.second.index-1) {
            return token.get_spelling();
          }
        }
      }
    }
    return "";
  };
  
  tag_similar_tokens=[this](const std::string &usr){
    if(clang_readable) {
      if(usr.size()>0 && last_similar_tokens_tagged!=usr) {
        get_buffer()->remove_tag(similar_tokens_tag, get_buffer()->begin(), get_buffer()->end());
        auto offsets=clang_tokens->get_similar_token_offsets(usr);
        for(auto &offset: offsets) {
          get_buffer()->apply_tag(similar_tokens_tag, get_buffer()->get_iter_at_line_index(offset.first.line-1, offset.first.index-1), get_buffer()->get_iter_at_line_index(offset.second.line-1, offset.second.index-1));
        }
        last_similar_tokens_tagged=usr;
      }
    }
    if(usr.size()==0 && last_similar_tokens_tagged!="") {
      get_buffer()->remove_tag(similar_tokens_tag, get_buffer()->begin(), get_buffer()->end());
      last_similar_tokens_tagged="";
    }
  };
  
  rename_similar_tokens=[this](const std::string &usr, const std::string &text) {
    size_t number=0;
    if(clang_readable) {
      auto offsets=clang_tokens->get_similar_token_offsets(usr);
      std::vector<std::pair<Glib::RefPtr<Gtk::TextMark>, Glib::RefPtr<Gtk::TextMark> > > marks;
      for(auto &offset: offsets) {
        marks.emplace_back(get_buffer()->create_mark(get_buffer()->get_iter_at_line_index(offset.first.line-1, offset.first.index-1)), get_buffer()->create_mark(get_buffer()->get_iter_at_line_index(offset.second.line-1, offset.second.index-1)));
        number++;
      }
      get_source_buffer()->begin_user_action();
      for(auto &mark: marks) {
        renaming=true;
        get_buffer()->erase(mark.first->get_iter(), mark.second->get_iter());
        get_buffer()->insert_with_tag(mark.first->get_iter(), text, similar_tokens_tag);
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
        auto usr=get_token();
        tag_similar_tokens(usr);
        return false;
      }, 100);
    }
  });
  
  get_declaration_location=[this](){
    std::pair<std::string, clang::Offset> location;
    if(clang_readable) {
      auto iter=get_buffer()->get_insert()->get_iter();
      auto line=(unsigned)iter.get_line();
      auto index=(unsigned)iter.get_line_index();
      for(auto &token: *clang_tokens) {
        if(token.get_kind()==clang::Token_Identifier && token.has_type()) {
          if(line==token.offsets.first.line-1 && index>=token.offsets.first.index-1 && index <=token.offsets.second.index-1) {
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
    if(clang_readable) {
      selection_dialog=std::unique_ptr<SelectionDialog>(new SelectionDialog(*this, get_buffer()->create_mark(get_buffer()->get_insert()->get_iter())));
      auto rows=std::make_shared<std::unordered_map<std::string, clang::Offset> >();
      auto methods=clang_tokens->get_cxx_methods();
      if(methods.size()==0)
        return;
      for(auto &method: methods) {
        (*rows)[method.first]=method.second;
        selection_dialog->add_row(method.first);
      }
      //TODO see if rows gets destroyed when selection_dialog gets destroyed.
      selection_dialog->on_select=[this, rows](const std::string& selected, bool hide_window) {
        auto offset=rows->at(selected);
        get_buffer()->place_cursor(get_buffer()->get_iter_at_line_index(offset.line-1, offset.index-1));
        scroll_to(get_buffer()->get_insert(), 0.0, 1.0, 0.5);
        delayed_tooltips_connection.disconnect();
      };
      selection_dialog->show();
    }
  };
}

Source::ClangView::ClangView(const boost::filesystem::path &file_path, const boost::filesystem::path& project_path, Glib::RefPtr<Gsv::Language> language): ClangViewRefactor(file_path, project_path) {
  if(language) {
    get_source_buffer()->set_highlight_syntax(false);
    get_source_buffer()->set_language(language);
  }
  
  do_delete_object.connect([this](){
    if(delete_thread.joinable())
      delete_thread.join();
    delete this;
  });
  do_restart_parse.connect([this](){
    init_parse();
    restart_parse_running=false;
  });
}

void Source::ClangView::async_delete() {
  parsing_in_progress->cancel("canceled, freeing resources in the background");
  parse_thread_stop=true;
  delete_thread=std::thread([this](){
    //TODO: Is it possible to stop the clang-process in progress?
    if(restart_parse_thread.joinable())
      restart_parse_thread.join();
    if(parse_thread.joinable())
      parse_thread.join();
    if(autocomplete_thread.joinable())
      autocomplete_thread.join();
    do_delete_object();
  });
}

bool Source::ClangView::restart_parse() {
  if(!restart_parse_running) {
    restart_parse_running=true;
    parse_thread_stop=true;
    if(restart_parse_thread.joinable())
      restart_parse_thread.join();
    restart_parse_thread=std::thread([this](){
      if(parse_thread.joinable())
        parse_thread.join();
      if(autocomplete_thread.joinable())
        autocomplete_thread.join();
      do_restart_parse();
    });
    return true;
  }
  return false;
}
