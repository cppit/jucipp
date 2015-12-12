#include "source.h"
#include "config.h"
#include <boost/property_tree/json_parser.hpp>
#include "logging.h"
#include <algorithm>
#include <gtksourceview/gtksource.h>
#include <iostream>
#include "filesystem.h"
#include "terminal.h"

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

Source::FixIt::FixIt(const std::string &source, const std::pair<Offset, Offset> &offsets) : source(source), offsets(offsets) {
  if(source.size()==0)
    type=Type::ERASE;
  else {
    if(this->offsets.first==this->offsets.second)
      type=Type::INSERT;
    else
      type=Type::REPLACE;
  }
}

std::string Source::FixIt::string(Glib::RefPtr<Gtk::TextBuffer> buffer) {
  auto iter=buffer->get_iter_at_line_index(offsets.first.line-1, offsets.first.index-1);
  unsigned first_line_offset=iter.get_line_offset()+1;
  iter=buffer->get_iter_at_line_index(offsets.second.line-1, offsets.second.index-1);
  unsigned second_line_offset=iter.get_line_offset()+1;
  
  std::string text;
  if(type==Type::INSERT) {
    text+="Insert "+source+" at ";
    text+=std::to_string(offsets.first.line)+":"+std::to_string(first_line_offset);
  }
  else if(type==Type::REPLACE) {
    text+="Replace ";
    text+=std::to_string(offsets.first.line)+":"+std::to_string(first_line_offset)+" - ";
    text+=std::to_string(offsets.second.line)+":"+std::to_string(second_line_offset);
    text+=" with "+source;
  }
  else {
    text+="Erase ";
    text+=std::to_string(offsets.first.line)+":"+std::to_string(first_line_offset)+" - ";
    text+=std::to_string(offsets.second.line)+":"+std::to_string(second_line_offset);
  }
  
  return text;
}

//////////////
//// View ////
//////////////
AspellConfig* Source::View::spellcheck_config=NULL;

Source::View::View(const boost::filesystem::path &file_path, const boost::filesystem::path &project_path, Glib::RefPtr<Gsv::Language> language): file_path(file_path), project_path(project_path), language(language) {
  get_source_buffer()->begin_not_undoable_action();
  if(language) {
    if(filesystem::read_non_utf8(file_path, get_buffer())==-1)
      Terminal::get().print("Warning: "+file_path.string()+" is not a valid UTF-8 file. Saving might corrupt the file.\n");
  }
  else {
    if(filesystem::read(file_path, get_buffer())==-1)
      Terminal::get().print("Error: "+file_path.string()+" is not a valid UTF-8 file.\n", true);
  }
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
  
  get_buffer()->create_tag("def:warning");
  get_buffer()->create_tag("def:warning_underline");
  get_buffer()->create_tag("def:error");
  get_buffer()->create_tag("def:error_underline");
  get_buffer()->create_tag("def:note_background");
  get_buffer()->create_tag("def:note");
  if(spellcheck_config==NULL)
    spellcheck_config=new_aspell_config();
  spellcheck_checker=NULL;
  auto tag=get_buffer()->create_tag("spellcheck_error");
  tag->property_underline()=Pango::Underline::UNDERLINE_ERROR;
    
  get_buffer()->signal_changed().connect([this](){
    if(spellcheck_checker==NULL)
      return;
    
    delayed_spellcheck_suggestions_connection.disconnect();
    
    auto iter=get_buffer()->get_insert()->get_iter();
    bool ends_line=iter.ends_line();
    if(iter.backward_char()) {
      auto context_iter=iter;
      bool backward_success=true;
      if(ends_line || *iter=='/' || *iter=='*') //iter_has_context_class is sadly bugged
        backward_success=context_iter.backward_char();
      if(backward_success) {
        if(last_keyval_is_backspace && !is_word_iter(iter) && iter.forward_char()) {} //backspace fix
        if((spellcheck_all && !get_source_buffer()->iter_has_context_class(context_iter, "no-spell-check")) || get_source_buffer()->iter_has_context_class(context_iter, "comment") || get_source_buffer()->iter_has_context_class(context_iter, "string")) {
          if(!is_word_iter(iter) || last_keyval_is_return) { //Might have used space or - to split two words
            auto first=iter;
            auto second=iter;
            if(last_keyval_is_return) {
              while(first && !first.ends_line() && first.backward_char()) {}
              if(first.backward_char() && second.forward_char()) {
                get_buffer()->remove_tag_by_name("spellcheck_error", first, second);
                auto word=spellcheck_get_word(first);
                spellcheck_word(word.first, word.second);
                word=spellcheck_get_word(second);
                spellcheck_word(word.first, word.second);
              }
            }
            else if(first.backward_char() && second.forward_char() && !second.starts_line()) {
              get_buffer()->remove_tag_by_name("spellcheck_error", first, second);
              auto word=spellcheck_get_word(first);
              spellcheck_word(word.first, word.second);
              word=spellcheck_get_word(second);
              spellcheck_word(word.first, word.second);
            }
          }
          else {
            auto word=spellcheck_get_word(iter);
            spellcheck_word(word.first, word.second);
          }
        }
      }
    }
    delayed_spellcheck_error_clear.disconnect();
    delayed_spellcheck_error_clear=Glib::signal_timeout().connect([this]() {
      auto iter=get_buffer()->begin();
      Gtk::TextIter begin_no_spellcheck_iter;
      if(spellcheck_all) {
        bool spell_check=!get_source_buffer()->iter_has_context_class(iter, "no-spell-check");
        if(!spell_check)
          begin_no_spellcheck_iter=iter;
        while(iter!=get_buffer()->end()) {
          if(!get_source_buffer()->iter_forward_to_context_class_toggle(iter, "no-spell-check"))
            iter=get_buffer()->end();
          
          spell_check=!spell_check;
          if(!spell_check)
            begin_no_spellcheck_iter=iter;
          else
            get_buffer()->remove_tag_by_name("spellcheck_error", begin_no_spellcheck_iter, iter);
        }
        return false;
      }
      
      bool spell_check=get_source_buffer()->iter_has_context_class(iter, "string") || get_source_buffer()->iter_has_context_class(iter, "comment");
      if(!spell_check)
        begin_no_spellcheck_iter=iter;
      while(iter!=get_buffer()->end()) {
        auto iter1=iter;
        auto iter2=iter;
        if(!get_source_buffer()->iter_forward_to_context_class_toggle(iter1, "string"))
          iter1=get_buffer()->end();
        if(!get_source_buffer()->iter_forward_to_context_class_toggle(iter2, "comment"))
          iter2=get_buffer()->end();
        
        if(iter2<iter1)
          iter=iter2;
        else
          iter=iter1;
        spell_check=!spell_check;
        if(!spell_check)
          begin_no_spellcheck_iter=iter;
        else
          get_buffer()->remove_tag_by_name("spellcheck_error", begin_no_spellcheck_iter, iter);
      }
      return false;
    }, 1000);
  });
  
  get_buffer()->signal_mark_set().connect([this](const Gtk::TextBuffer::iterator& iter, const Glib::RefPtr<Gtk::TextBuffer::Mark>& mark) {
    if(spellcheck_checker==NULL)
      return;
    
    if(mark->get_name()=="insert") {
      if(spellcheck_suggestions_dialog_shown)
        spellcheck_suggestions_dialog->hide();
      delayed_spellcheck_suggestions_connection.disconnect();
      delayed_spellcheck_suggestions_connection=Glib::signal_timeout().connect([this]() {
        auto tags=get_buffer()->get_insert()->get_iter().get_tags();
        bool need_suggestions=false;
        for(auto &tag: tags) {
          if(tag->property_name()=="spellcheck_error") {
            need_suggestions=true;
            break;
          }
        }
        if(need_suggestions) {
          spellcheck_suggestions_dialog=std::unique_ptr<SelectionDialog>(new SelectionDialog(*this, get_buffer()->create_mark(get_buffer()->get_insert()->get_iter()), false));
          spellcheck_suggestions_dialog->on_hide=[this](){
            spellcheck_suggestions_dialog_shown=false;
          };
          auto word=spellcheck_get_word(get_buffer()->get_insert()->get_iter());
          auto suggestions=spellcheck_get_suggestions(word.first, word.second);
          if(suggestions.size()==0)
            return false;
          for(auto &suggestion: suggestions)
            spellcheck_suggestions_dialog->add_row(suggestion);
          spellcheck_suggestions_dialog->on_select=[this, word](const std::string& selected, bool hide_window) {
            get_source_buffer()->begin_user_action();
            get_buffer()->erase(word.first, word.second);
            get_buffer()->insert(get_buffer()->get_insert()->get_iter(), selected);
            get_source_buffer()->end_user_action();
            delayed_tooltips_connection.disconnect();
          };
          spellcheck_suggestions_dialog->show();
          spellcheck_suggestions_dialog_shown=true;
        }
        return false;
      }, 500);
    }
  });
  
  get_buffer()->signal_changed().connect([this](){
    set_info(info);
  });
  
  set_tooltip_events();
  
  tab_char=Config::get().source.default_tab_char;
  tab_size=Config::get().source.default_tab_size;
  if(Config::get().source.auto_tab_char_and_size) {
    auto tab_char_and_size=find_tab_char_and_size();
    if(tab_char_and_size.first!=0) {
      if(tab_char!=tab_char_and_size.first || tab_size!=tab_char_and_size.second) {
        std::string tab_str;
        if(tab_char_and_size.first==' ')
          tab_str="<space>";
        else
          tab_str="<tab>";
        Terminal::get().print("Tab char and size for file "+file_path.string()+" set to: "+tab_str+", "+std::to_string(tab_char_and_size.second)+".\n");
      }
      
      tab_char=tab_char_and_size.first;
      tab_size=tab_char_and_size.second;
    }
  }
  set_tab_char_and_size(tab_char, tab_size);
}

void Source::View::set_tab_char_and_size(char tab_char, unsigned tab_size) {
  this->tab_char=tab_char;
  this->tab_size=tab_size;
      
  tab.clear();
  for(unsigned c=0;c<tab_size;c++)
    tab+=tab_char;
}

void Source::View::configure() {
  //TODO: Move this to notebook? Might take up too much memory doing this for every tab.
  auto style_scheme_manager=Gsv::StyleSchemeManager::get_default();
  style_scheme_manager->prepend_search_path((Config::get().juci_home_path()/"styles").string());
  
  if(Config::get().source.style.size()>0) {
    auto scheme = style_scheme_manager->get_scheme(Config::get().source.style);
  
    if(scheme)
      get_source_buffer()->set_style_scheme(scheme);
    else
      Terminal::get().print("Error: Could not find gtksourceview style: "+Config::get().source.style+'\n', true);
  }
  
  if(Config::get().source.wrap_lines)
    set_wrap_mode(Gtk::WrapMode::WRAP_CHAR);
  else
    set_wrap_mode(Gtk::WrapMode::WRAP_NONE);
  property_highlight_current_line() = Config::get().source.highlight_current_line;
  property_show_line_numbers() = Config::get().source.show_line_numbers;
  if(Config::get().source.font.size()>0)
    override_font(Pango::FontDescription(Config::get().source.font));
#if GTKSOURCEVIEWMM_MAJOR_VERSION > 2 & GTKSOURCEVIEWMM_MINOR_VERSION > 15
  gtk_source_view_set_background_pattern(this->gobj(), GTK_SOURCE_BACKGROUND_PATTERN_TYPE_GRID);
#endif
  
  //Create tags for diagnostic warnings and errors:
  auto scheme = get_source_buffer()->get_style_scheme();
  auto tag_table=get_buffer()->get_tag_table();
  auto style=scheme->get_style("def:warning");
  auto diagnostic_tag=get_buffer()->get_tag_table()->lookup("def:warning");
  auto diagnostic_tag_underline=get_buffer()->get_tag_table()->lookup("def:warning_underline");
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
  diagnostic_tag=get_buffer()->get_tag_table()->lookup("def:error");
  diagnostic_tag_underline=get_buffer()->get_tag_table()->lookup("def:error_underline");
  if(style && (style->property_foreground_set() || style->property_background_set())) {
    Glib::ustring error_property;
    if(style->property_foreground_set()) {
      error_property=style->property_foreground().get_value();
      diagnostic_tag->property_foreground() = error_property;
    }
    else if(style->property_background_set())
      error_property=style->property_background().get_value();
    
    diagnostic_tag_underline->property_underline()=Pango::Underline::UNDERLINE_ERROR;
#if GTK_VERSION_GE(3, 16)
    diagnostic_tag_underline->set_property("underline-rgba", Gdk::RGBA(error_property));
#endif
  }
  //TODO: clear tag_class and param_spec?

  //Add tooltip foreground and background
  style = scheme->get_style("def:note");
  auto note_tag=get_buffer()->get_tag_table()->lookup("def:note_background");
  if(style->property_background_set()) {
    note_tag->property_background()=style->property_background();
  }
  note_tag=get_buffer()->get_tag_table()->lookup("def:note");
  if(style->property_foreground_set()) {
    note_tag->property_foreground()=style->property_foreground();
  }
    
  if(Config::get().source.spellcheck_language.size()>0) {
    aspell_config_replace(spellcheck_config, "lang", Config::get().source.spellcheck_language.c_str());
    aspell_config_replace(spellcheck_config, "encoding", "utf-8");
  }
  spellcheck_possible_err=new_aspell_speller(spellcheck_config);
  if(spellcheck_checker!=NULL)
    delete_aspell_speller(spellcheck_checker);
  spellcheck_checker=NULL;
  if (aspell_error_number(spellcheck_possible_err) != 0)
    std::cerr << "Spell check error: " << aspell_error_message(spellcheck_possible_err) << std::endl;
  else
    spellcheck_checker = to_aspell_speller(spellcheck_possible_err);
  get_buffer()->remove_tag_by_name("spellcheck_error", get_buffer()->begin(), get_buffer()->end());
}

void Source::View::set_tooltip_events() {
  signal_motion_notify_event().connect([this](GdkEventMotion* event) {
    if(on_motion_last_x!=event->x || on_motion_last_y!=event->y) {
      delayed_tooltips_connection.disconnect();
      if((event->state&GDK_BUTTON1_MASK)==0) {
        gdouble x=event->x;
        gdouble y=event->y;
        delayed_tooltips_connection=Glib::signal_timeout().connect([this, x, y]() {
          Tooltips::init();
          Gdk::Rectangle rectangle(x, y, 1, 1);
          if(parsed) {
            show_type_tooltips(rectangle);
            show_diagnostic_tooltips(rectangle);
          }
          return false;
        }, 100);
      }
      type_tooltips.hide();
      diagnostic_tooltips.hide();
    }
    on_motion_last_x=event->x;
    on_motion_last_y=event->y;
    return false;
  });
  
  get_buffer()->signal_mark_set().connect([this](const Gtk::TextBuffer::iterator& iterator, const Glib::RefPtr<Gtk::TextBuffer::Mark>& mark) {
    if(get_buffer()->get_has_selection() && mark->get_name()=="selection_bound")
      delayed_tooltips_connection.disconnect();
    
    if(mark->get_name()=="insert") {
      delayed_tooltips_connection.disconnect();
      delayed_tooltips_connection=Glib::signal_timeout().connect([this]() {
        Tooltips::init();
        Gdk::Rectangle rectangle;
        get_iter_location(get_buffer()->get_insert()->get_iter(), rectangle);
        int location_window_x, location_window_y;
        buffer_to_window_coords(Gtk::TextWindowType::TEXT_WINDOW_TEXT, rectangle.get_x(), rectangle.get_y(), location_window_x, location_window_y);
        rectangle.set_x(location_window_x-2);
        rectangle.set_y(location_window_y);
        rectangle.set_width(5);
        if(parsed) {
          show_type_tooltips(rectangle);
          show_diagnostic_tooltips(rectangle);
        }
        return false;
      }, 500);
      type_tooltips.hide();
      diagnostic_tooltips.hide();
      set_info(info);
    }
  });
  
  signal_scroll_event().connect([this](GdkEventScroll* event) {
    delayed_tooltips_connection.disconnect();
    type_tooltips.hide();
    diagnostic_tooltips.hide();
    return false;
  });
  
  signal_focus_out_event().connect([this](GdkEventFocus* event) {
    delayed_tooltips_connection.disconnect();
    type_tooltips.hide();
    diagnostic_tooltips.hide();
    return false;
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
  
  delayed_tooltips_connection.disconnect();
  delayed_spellcheck_suggestions_connection.disconnect();
  delayed_spellcheck_error_clear.disconnect();
  
  if(spellcheck_checker!=NULL)
    delete_aspell_speller(spellcheck_checker);
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
  std::string text=Gtk::Clipboard::get()->wait_for_text();
  
  //Replace carriage returns (which leads to crash) with newlines
  for(size_t c=0;c<text.size();c++) {
    if(text[c]=='\r') {
      if((c+1)<text.size() && text[c+1]=='\n')
        text.replace(c, 2, "\n");
      else
        text.replace(c, 1, "\n");
    }
  }

  auto line=get_line_before();
  std::string prefix_tabs;
  auto tabs_end_iter=get_tabs_end_iter();
  if(!get_buffer()->get_has_selection() && tabs_end_iter.ends_line()) {
    prefix_tabs=get_line_before(tabs_end_iter);

    size_t start_line=0;
    size_t end_line=0;
    bool paste_line=false;
    bool first_paste_line=true;
    size_t paste_line_tabs=-1;
    bool first_paste_line_has_tabs=false;
    for(size_t c=0;c<text.size();c++) {
      if(text[c]=='\n') {
        end_line=c;
        paste_line=true;
      }
      else if(c==text.size()-1) {
        end_line=c+1;
        paste_line=true;
      }
      if(paste_line) {
        bool empty_line=true;
        std::string line=text.substr(start_line, end_line-start_line);
        size_t tabs=0;
        for(auto chr: line) {
          if(chr==tab_char)
            tabs++;
          else {
            empty_line=false;
            break;
          }
        }
        if(first_paste_line) {
          if(tabs!=0) {
            first_paste_line_has_tabs=true;
            paste_line_tabs=tabs;
          }
          first_paste_line=false;
        }
        else if(!empty_line)
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
    for(size_t c=0;c<text.size();c++) {
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
        size_t line_tabs=0;
        for(auto chr: line) {
          if(chr==tab_char)
            line_tabs++;
          else
            break;
        }
        auto tabs=paste_line_tabs;
        if(!(first_paste_line && !first_paste_line_has_tabs) && line_tabs<paste_line_tabs) {
          tabs=line_tabs;
        }
        
        if(first_paste_line) {
          if(first_paste_line_has_tabs)
            get_buffer()->insert_at_cursor(text.substr(start_line+tabs, end_line-start_line-tabs));
          else
            get_buffer()->insert_at_cursor(text.substr(start_line, end_line-start_line));
          first_paste_line=false;
        }
        else
          get_buffer()->insert_at_cursor('\n'+prefix_tabs+text.substr(start_line+tabs, end_line-start_line-tabs));
        start_line=end_line+1;
        paste_line=false;
      }
    }
    get_buffer()->place_cursor(get_buffer()->get_insert()->get_iter());
    scroll_to(get_buffer()->get_insert());
    get_source_buffer()->end_user_action();
  }
  else {
    Gtk::Clipboard::get()->set_text(text);
    get_buffer()->paste_clipboard(Gtk::Clipboard::get());
  }
}

void Source::View::set_status(const std::string &status) {
  this->status=status;
  if(on_update_status)
    on_update_status(this, status);
}

void Source::View::set_info(const std::string &info) {
  this->info=info;
  auto iter=get_buffer()->get_insert()->get_iter();
  auto positions=std::to_string(iter.get_line()+1)+":"+std::to_string(iter.get_line_offset()+1);
  if(on_update_info)
    on_update_info(this, positions+" "+info);
}

void Source::View::spellcheck(const Gtk::TextIter& start, const Gtk::TextIter& end) {
  if(spellcheck_checker==NULL)
    return;
  auto iter=start;
  while(iter && iter<end) {
    if(is_word_iter(iter)) {
      auto word=spellcheck_get_word(iter);
      spellcheck_word(word.first, word.second);
      iter=word.second; 
    }
    iter.forward_char();
  }
}

void Source::View::spellcheck() {
  auto iter=get_buffer()->begin();
  Gtk::TextIter begin_spellcheck_iter;
  if(spellcheck_all) {
    bool spell_check=!get_source_buffer()->iter_has_context_class(iter, "no-spell-check");
    if(spell_check)
      begin_spellcheck_iter=iter;
    while(iter!=get_buffer()->end()) {
      if(!get_source_buffer()->iter_forward_to_context_class_toggle(iter, "no-spell-check"))
        iter=get_buffer()->end();
      
      spell_check=!spell_check;
      if(spell_check)
        begin_spellcheck_iter=iter;
      else
        spellcheck(begin_spellcheck_iter, iter);
    }
  }
  else {
    bool spell_check=get_source_buffer()->iter_has_context_class(iter, "string") || get_source_buffer()->iter_has_context_class(iter, "comment");
    if(spell_check)
      begin_spellcheck_iter=iter;
    while(iter!=get_buffer()->end()) {
      auto iter1=iter;
      auto iter2=iter;
      if(!get_source_buffer()->iter_forward_to_context_class_toggle(iter1, "string"))
        iter1=get_buffer()->end();
      if(!get_source_buffer()->iter_forward_to_context_class_toggle(iter2, "comment"))
        iter2=get_buffer()->end();
      
      if(iter2<iter1)
        iter=iter2;
      else
        iter=iter1;
      spell_check=!spell_check;
      if(spell_check)
        begin_spellcheck_iter=iter;
      else
        spellcheck(begin_spellcheck_iter, iter);
    }
  }
}

void Source::View::remove_spellcheck_errors() {
  get_buffer()->remove_tag_by_name("spellcheck_error", get_buffer()->begin(), get_buffer()->end());
}

void Source::View::goto_next_spellcheck_error() {
  auto iter=get_buffer()->get_insert()->get_iter();
  auto insert_iter=iter;
  bool wrapped=false;
  iter.forward_char();
  while(iter && (!wrapped || iter<insert_iter)) {
    auto toggled_tags=iter.get_toggled_tags();
    for(auto &toggled_tag: toggled_tags) {
      if(toggled_tag->property_name()=="spellcheck_error") {
        get_buffer()->place_cursor(iter);
        scroll_to(get_buffer()->get_insert(), 0.0, 1.0, 0.5);
        return;
      }
    }
    iter.forward_char();
    if(!wrapped && iter==get_buffer()->end()) {
      iter=get_buffer()->begin();
      wrapped=true;
    }
  }
}

std::string Source::View::get_line(const Gtk::TextIter &iter) {
  auto line_start_it = get_buffer()->get_iter_at_line(iter.get_line());
  auto line_end_it = iter;
  while(!line_end_it.ends_line() && line_end_it.forward_char()) {}
  std::string line(get_source_buffer()->get_text(line_start_it, line_end_it));
  return line;
}
std::string Source::View::get_line(Glib::RefPtr<Gtk::TextBuffer::Mark> mark) {
  return get_line(mark->get_iter());
}
std::string Source::View::get_line(int line_nr) {
  return get_line(get_buffer()->get_iter_at_line(line_nr));
}
std::string Source::View::get_line() {
  return get_line(get_buffer()->get_insert());
}

std::string Source::View::get_line_before(const Gtk::TextIter &iter) {
  auto line_it = get_source_buffer()->get_iter_at_line(iter.get_line());
  std::string line(get_source_buffer()->get_text(line_it, iter));
  return line;
}
std::string Source::View::get_line_before(Glib::RefPtr<Gtk::TextBuffer::Mark> mark) {
  return get_line_before(mark->get_iter());
}
std::string Source::View::get_line_before() {
  return get_line_before(get_buffer()->get_insert());
}

Gtk::TextIter Source::View::get_tabs_end_iter(const Gtk::TextIter &iter) {
  return get_tabs_end_iter(iter.get_line());
}
Gtk::TextIter Source::View::get_tabs_end_iter(Glib::RefPtr<Gtk::TextBuffer::Mark> mark) {
  return get_tabs_end_iter(mark->get_iter());
}
Gtk::TextIter Source::View::get_tabs_end_iter(int line_nr) {
  auto sentence_iter = get_source_buffer()->get_iter_at_line(line_nr);
  while((*sentence_iter==' ' || *sentence_iter=='\t') && !sentence_iter.ends_line() && sentence_iter.forward_char()) {}
  return sentence_iter;
}
Gtk::TextIter Source::View::get_tabs_end_iter() {
  return get_tabs_end_iter(get_buffer()->get_insert());
}

bool Source::View::find_start_of_closed_expression(Gtk::TextIter iter, Gtk::TextIter &found_iter) {
  int count1=0;
  int count2=0;
  
  bool ignore=false;
  
  do {
    if(!get_source_buffer()->iter_has_context_class(iter, "comment") && !get_source_buffer()->iter_has_context_class(iter, "string")) {
      if(*iter=='\'') {
        auto before_iter=iter;
        before_iter.backward_char();
        auto before_before_iter=before_iter;
        before_before_iter.backward_char();
        if(*before_iter!='\\' || *before_before_iter=='\\')
          ignore=!ignore;
      }
      else if(!ignore) {
        if(*iter==')')
          count1++;
        else if(*iter==']')
          count2++;
        else if(*iter=='(')
          count1--;
        else if(*iter=='[')
          count2--;
      }
    }
    if(iter.starts_line() && count1<=0 && count2<=0) {
      auto insert_iter=get_buffer()->get_insert()->get_iter();
      while(iter!=insert_iter && *iter==static_cast<unsigned char>(tab_char) && iter.forward_char()) {}
      found_iter=iter;
      return true;
    }
  } while(iter.backward_char());
  return false;
}

bool Source::View::find_open_expression_symbol(Gtk::TextIter iter, const Gtk::TextIter &until_iter, Gtk::TextIter &found_iter) {
  int count1=0;
  int count2=0;

  bool ignore=false;
  
  while(iter!=until_iter && iter.backward_char()) {
    if(!get_source_buffer()->iter_has_context_class(iter, "comment") && !get_source_buffer()->iter_has_context_class(iter, "string")) {
      if(*iter=='\'') {
        auto before_iter=iter;
        before_iter.backward_char();
        auto before_before_iter=before_iter;
        before_before_iter.backward_char();
        if(*before_iter!='\\' || *before_before_iter=='\\')
          ignore=!ignore;
      }
      else if(!ignore) {
        if(*iter==')')
          count1++;
        else if(*iter==']')
          count2++;
        else if(*iter=='(')
          count1--;
        else if(*iter=='[')
          count2--;
      }
      if(count1<0 || count2<0) {
        found_iter=iter;
        return true;
      }
    }
  }
  
  return false;
}  

bool Source::View::find_right_bracket_forward(Gtk::TextIter iter, Gtk::TextIter &found_iter) {
  int count=0;
  
  bool ignore=false;
  
  while(iter.forward_char()) {
    if(!get_source_buffer()->iter_has_context_class(iter, "comment") && !get_source_buffer()->iter_has_context_class(iter, "string")) {
      if(*iter=='\'') {
        auto before_iter=iter;
        before_iter.backward_char();
        auto before_before_iter=before_iter;
        before_before_iter.backward_char();
        if(*before_iter!='\\' || *before_before_iter=='\\')
          ignore=!ignore;
      }
      else if(!ignore) {
        if(*iter=='}') {
          if(count==0) {
            found_iter=iter;
            return true;
          }
          count--;
        }
        else if(*iter=='{')
          count++;
      }
    }
  }
  return false;
}

bool Source::View::find_left_bracket_backward(Gtk::TextIter iter, Gtk::TextIter &found_iter) {
  int count=0;
  
  bool ignore=false;
  
  while(iter.backward_char()) {
    if(!get_source_buffer()->iter_has_context_class(iter, "comment") && !get_source_buffer()->iter_has_context_class(iter, "string")) {
      if(*iter=='\'') {
        auto before_iter=iter;
        before_iter.backward_char();
        auto before_before_iter=before_iter;
        before_before_iter.backward_char();
        if(*before_iter!='\\' || *before_before_iter=='\\')
          ignore=!ignore;
      }
      else if(!ignore) {
        if(*iter=='{') {
          if(count==0) {
            found_iter=iter;
            return true;
          }
          count--;
        }
        else if(*iter=='}')
          count++;
      }
    }
  }
  return false;
}

//Basic indentation
bool Source::View::on_key_press_event(GdkEventKey* key) {
  if(spellcheck_suggestions_dialog_shown) {
    if(spellcheck_suggestions_dialog->on_key_press(key))
      return true;
  }

  if(key->keyval==GDK_KEY_BackSpace)
    last_keyval_is_backspace=true;
  else
    last_keyval_is_backspace=false;
  if(key->keyval==GDK_KEY_Return)
    last_keyval_is_return=true;
  else
    last_keyval_is_return=false;
  
  get_source_buffer()->begin_user_action();
  auto iter=get_buffer()->get_insert()->get_iter();
  //Indent as in next or previous line
  if(key->keyval==GDK_KEY_Return && !get_buffer()->get_has_selection() && !iter.starts_line()) {
    //First remove spaces or tabs around cursor
    auto start_blank_iter=iter;
    auto end_blank_iter=iter;
    while((*end_blank_iter==' ' || *end_blank_iter=='\t') &&
          !end_blank_iter.ends_line() && end_blank_iter.forward_char()) {}
    start_blank_iter.backward_char();
    while((*start_blank_iter==' ' || *start_blank_iter=='\t') &&
          !start_blank_iter.starts_line() && start_blank_iter.backward_char()) {}
    
    if(start_blank_iter.starts_line() && (*start_blank_iter==' ' || *start_blank_iter=='\t'))
      get_buffer()->erase(iter, end_blank_iter);
    else {
      start_blank_iter.forward_char();
      get_buffer()->erase(start_blank_iter, end_blank_iter);
    }
    
    iter=get_buffer()->get_insert()->get_iter();
    int line_nr=iter.get_line();
    auto tabs_end_iter=get_tabs_end_iter();
    auto line_tabs=get_line_before(tabs_end_iter);
    if((line_nr+1)<get_buffer()->get_line_count()) {
      auto next_line_tabs_end_iter=get_tabs_end_iter(line_nr+1);
      auto next_line_tabs=get_line_before(next_line_tabs_end_iter);
      if(iter.ends_line() && next_line_tabs.size()>line_tabs.size()) {
        get_source_buffer()->insert_at_cursor("\n"+next_line_tabs);
        scroll_to(get_source_buffer()->get_insert());
        get_source_buffer()->end_user_action();
        return true;
      }
    }
    get_source_buffer()->insert_at_cursor("\n"+line_tabs);
    scroll_to(get_source_buffer()->get_insert());
    get_source_buffer()->end_user_action();
    return true;
  }
  //Indent right when clicking tab, no matter where in the line the cursor is. Also works on selected text.
  else if(key->keyval==GDK_KEY_Tab) {
    //Special case if insert is at beginning of empty line:
    if(iter.starts_line() && iter.ends_line() && !get_buffer()->get_has_selection()) {
      auto prev_line_iter=iter;
      while(prev_line_iter.starts_line() && prev_line_iter.backward_char()) {}
      auto prev_line_tabs_end_iter=get_tabs_end_iter(prev_line_iter);
      auto previous_line_tabs=get_line_before(prev_line_tabs_end_iter);

      auto next_line_iter=iter;
      while(next_line_iter.starts_line() && next_line_iter.forward_char()) {}
      auto next_line_tabs_end_iter=get_tabs_end_iter(next_line_iter);
      auto next_line_tabs=get_line_before(next_line_tabs_end_iter);
      
      std::string tabs;
      if(previous_line_tabs.size()<next_line_tabs.size())
        tabs=previous_line_tabs;
      else
        tabs=next_line_tabs;
      if(tabs.size()>=tab_size) {
        get_buffer()->insert_at_cursor(tabs);
        get_source_buffer()->end_user_action();
        return true;
      }
    }
    
    Gtk::TextIter selection_start, selection_end;
    get_source_buffer()->get_selection_bounds(selection_start, selection_end);
    int line_start=selection_start.get_line();
    int line_end=selection_end.get_line();
    for(int line=line_start;line<=line_end;line++) {
      Gtk::TextIter line_it = get_source_buffer()->get_iter_at_line(line);
      if(!get_buffer()->get_has_selection() || line_it!=selection_end)
        get_source_buffer()->insert(line_it, tab);
    }
    get_source_buffer()->end_user_action();
    return true;
  }
  //Indent left when clicking shift-tab, no matter where in the line the cursor is. Also works on selected text.
  else if((key->keyval==GDK_KEY_ISO_Left_Tab || key->keyval==GDK_KEY_Tab) && (key->state&GDK_SHIFT_MASK)>0) {
    Gtk::TextIter selection_start, selection_end;
    get_source_buffer()->get_selection_bounds(selection_start, selection_end);
    int line_start=selection_start.get_line();
    int line_end=selection_end.get_line();
    
    unsigned indent_left_steps=tab_size;
    std::vector<bool> ignore_line;
    for(int line_nr=line_start;line_nr<=line_end;line_nr++) {
      auto line_it = get_source_buffer()->get_iter_at_line(line_nr);
      if(!get_buffer()->get_has_selection() || line_it!=selection_end) {        
        auto tabs_end_iter=get_tabs_end_iter(line_nr);
        if(tabs_end_iter.starts_line() && tabs_end_iter.ends_line())
          ignore_line.push_back(true);
        else {
          auto line_tabs=get_line_before(tabs_end_iter);
          
          if(line_tabs.size()>0) {
            indent_left_steps=std::min(indent_left_steps, static_cast<unsigned>(line_tabs.size()));
            ignore_line.push_back(false);
          }
          else {
            get_source_buffer()->end_user_action();
            return true;
          }
        }
      }
    }
    
    for(int line_nr=line_start;line_nr<=line_end;line_nr++) {
      Gtk::TextIter line_it = get_source_buffer()->get_iter_at_line(line_nr);
      Gtk::TextIter line_plus_it=line_it;
      if(!get_buffer()->get_has_selection() || line_it!=selection_end) {
        line_plus_it.forward_chars(indent_left_steps);
        if(!ignore_line.at(line_nr-line_start))
          get_source_buffer()->erase(line_it, line_plus_it);
      }
    }
    get_source_buffer()->end_user_action();
    return true;
  }
  //"Smart" backspace key
  else if(key->keyval==GDK_KEY_BackSpace && !get_buffer()->get_has_selection()) {
    auto line=get_line_before();
    bool do_smart_backspace=true;
    for(auto &chr: line) {
      if(chr!=' ' && chr!='\t') {
        do_smart_backspace=false;
        break;
      }
    }
    if(do_smart_backspace) {
      auto line_start_iter=iter;
      if(line_start_iter.backward_chars(line.size()))
        get_buffer()->erase(iter, line_start_iter);
    }
  }
  //"Smart" delete key
  else if(key->keyval==GDK_KEY_Delete && !get_buffer()->get_has_selection()) {
    auto insert_iter=iter;
    bool do_smart_delete=true;
    do {
      if(*iter!=' ' && *iter!='\t' && !iter.ends_line()) {
        do_smart_delete=false;
        break;
      }
      if(iter.ends_line()) {
        if(!iter.forward_char())
          do_smart_delete=false;
        break;
      }
    } while(iter.forward_char());
    if(do_smart_delete) {
      if(!insert_iter.starts_line())
        while((*iter==' ' || *iter=='\t') && iter.forward_char()) {}
      if(iter.backward_char())
        get_buffer()->erase(insert_iter, iter);
    }
  }
  //Next two are smart home/end keys that works with wrapped lines
  //Note that smart end goes FIRST to end of line to avoid hiding empty chars after expressions
  else if(key->keyval==GDK_KEY_End && (key->state&GDK_CONTROL_MASK)==0) {
    auto end_line_iter=iter;
    while(!end_line_iter.ends_line() && end_line_iter.forward_char()) {}
    auto end_sentence_iter=end_line_iter;
    while(!end_sentence_iter.starts_line() && 
          (*end_sentence_iter==' ' || *end_sentence_iter=='\t' || end_sentence_iter.ends_line()) &&
          end_sentence_iter.backward_char()) {}
    if(!end_sentence_iter.ends_line() && !end_sentence_iter.starts_line())
      end_sentence_iter.forward_char();
    if(iter==end_line_iter) {
      if((key->state&GDK_SHIFT_MASK)>0)
        get_buffer()->move_mark_by_name("insert", end_sentence_iter);
      else
        get_buffer()->place_cursor(end_sentence_iter);
    }
    else {
      if((key->state&GDK_SHIFT_MASK)>0)
        get_buffer()->move_mark_by_name("insert", end_line_iter);
      else
        get_buffer()->place_cursor(end_line_iter);
    }
    scroll_to(get_buffer()->get_insert());
    get_source_buffer()->end_user_action();
    return true;
  }
  else if(key->keyval==GDK_KEY_Home && (key->state&GDK_CONTROL_MASK)==0) {
    auto start_line_iter=get_buffer()->get_iter_at_line(iter.get_line());
    auto start_sentence_iter=start_line_iter;
    while(!start_sentence_iter.ends_line() && 
          (*start_sentence_iter==' ' || *start_sentence_iter=='\t') &&
          start_sentence_iter.forward_char()) {}
    
    if(iter>start_sentence_iter || iter==start_line_iter) {
      if((key->state&GDK_SHIFT_MASK)>0)
        get_buffer()->move_mark_by_name("insert", start_sentence_iter);
      else
        get_buffer()->place_cursor(start_sentence_iter);
    }
    else {
      if((key->state&GDK_SHIFT_MASK)>0)
        get_buffer()->move_mark_by_name("insert", start_line_iter);
      else
        get_buffer()->place_cursor(start_line_iter);
    }
    scroll_to(get_buffer()->get_insert());
    get_source_buffer()->end_user_action();
    return true;
  }

  bool stop=Gsv::View::on_key_press_event(key);
  get_source_buffer()->end_user_action();
  return stop;
}

bool Source::View::on_button_press_event(GdkEventButton *event) {
  if(event->type==GDK_2BUTTON_PRESS) {
    Gtk::TextIter start, end;
    get_buffer()->get_selection_bounds(start, end);
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
  
  return Gsv::View::on_button_press_event(event);
}

std::pair<char, unsigned> Source::View::find_tab_char_and_size() {
  std::unordered_map<char, size_t> tab_chars;
  std::unordered_map<unsigned, size_t> tab_sizes;
  auto iter=get_buffer()->begin();
  long tab_count=-1;
  long last_tab_count=0;
  bool single_quoted=false;
  bool double_quoted=false;
  //For bracket languages, TODO: add more language ids
  if(language && (language->get_id()=="chdr" || language->get_id()=="cpphdr" || language->get_id()=="c" ||
                     language->get_id()=="cpp" || language->get_id()=="objc" || language->get_id()=="java" ||
                     language->get_id()=="javascript")) {
    bool line_comment=false;
    bool comment=false;
    bool bracket_last_line=false;
    char last_char=0;
    long last_tab_diff=-1;
    while(iter) {
      if(iter.starts_line()) {
        line_comment=false;
        single_quoted=false;
        double_quoted=false;
        tab_count=0;
        if(last_char=='{')
          bracket_last_line=true;
        else
          bracket_last_line=false;
      }
      if(bracket_last_line && tab_count!=-1) {
        if(*iter==' ') {
          tab_chars[' ']++;
          tab_count++;
        }
        else if(*iter=='\t') {
          tab_chars['\t']++;
          tab_count++;
        }
        else {
          auto line_iter=iter;
          char last_line_char=0;
          while(line_iter && !line_iter.ends_line()) {
            if(*line_iter!=' ' && *line_iter!='\t')
              last_line_char=*line_iter;
            if(*line_iter=='(')
              break;
            line_iter.forward_char();
          }
          if(last_line_char==':' || *iter=='#') {
            tab_count=0;
            if((iter.get_line()+1) < get_buffer()->get_line_count()) {
              iter=get_buffer()->get_iter_at_line(iter.get_line()+1);
              continue;
            }
          }
          else if(!iter.ends_line()) {
            if(tab_count!=last_tab_count)
              tab_sizes[abs(tab_count-last_tab_count)]++;
            last_tab_diff=abs(tab_count-last_tab_count);
            last_tab_count=tab_count;
            last_char=0;
          }
        }
      }

      auto prev_iter=iter;
      prev_iter.backward_char();
      auto prev_prev_iter=prev_iter;
      prev_prev_iter.backward_char();
      if(!double_quoted && *iter=='\'' && !(*prev_iter=='\\' && *prev_prev_iter!='\\'))
        single_quoted=!single_quoted;
      else if(!single_quoted && *iter=='\"' && !(*prev_iter=='\\' && *prev_prev_iter!='\\'))
        double_quoted=!double_quoted;
      else if(!single_quoted && !double_quoted) {
        auto next_iter=iter;
        next_iter.forward_char();
        if(*iter=='/' && *next_iter=='/')
          line_comment=true;
        else if(*iter=='/' && *next_iter=='*')
          comment=true;
        else if(*iter=='*' && *next_iter=='/') {
          iter.forward_char();
          iter.forward_char();
          comment=false;
        }
      }
      if(!single_quoted && !double_quoted && !comment && !line_comment && *iter!=' ' && *iter!='\t' && !iter.ends_line())
        last_char=*iter;
      if(!single_quoted && !double_quoted && !comment && !line_comment && *iter=='}' && tab_count!=-1 && last_tab_diff!=-1)
        last_tab_count-=last_tab_diff;
      if(*iter!=' ' && *iter!='\t')
        tab_count=-1;

      iter.forward_char();
    }
  }
  else {
    long para_count=0;
    while(iter) {
      if(iter.starts_line())
        tab_count=0;
      if(tab_count!=-1 && para_count==0 && single_quoted==false && double_quoted==false) {
        if(*iter==' ') {
          tab_chars[' ']++;
          tab_count++;
        }
        else if(*iter=='\t') {
          tab_chars['\t']++;
          tab_count++;
        }
        else if(!iter.ends_line()) {
          if(tab_count!=last_tab_count)
            tab_sizes[abs(tab_count-last_tab_count)]++;
          last_tab_count=tab_count;
        }
      }
      auto prev_iter=iter;
      prev_iter.backward_char();
      auto prev_prev_iter=prev_iter;
      prev_prev_iter.backward_char();
      if(!double_quoted && *iter=='\'' && !(*prev_iter=='\\' && *prev_prev_iter!='\\'))
        single_quoted=!single_quoted;
      else if(!single_quoted && *iter=='\"' && !(*prev_iter=='\\' && *prev_prev_iter!='\\'))
        double_quoted=!double_quoted;
      else if(!single_quoted && !double_quoted) {
        if(*iter=='(')
          para_count++;
        else if(*iter==')')
          para_count--;
      }
      if(*iter!=' ' && *iter!='\t')
        tab_count=-1;

      iter.forward_char();
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

bool Source::View::is_word_iter(const Gtk::TextIter& iter) {
  return ((*iter>=65 && *iter<=90) || (*iter>=97 && *iter<=122) || *iter==39 || *iter>=128);
}

std::pair<Gtk::TextIter, Gtk::TextIter> Source::View::spellcheck_get_word(Gtk::TextIter iter) {
  auto start=iter;
  auto end=iter;
  
  while(is_word_iter(iter)) {
    start=iter;
    if(!iter.backward_char())
      break;
  }
  while(is_word_iter(end)) {
    if(!end.forward_char())
      break;
  }
  
  return {start, end};
}

void Source::View::spellcheck_word(const Gtk::TextIter& start, const Gtk::TextIter& end) {
  auto spellcheck_start=start;
  auto spellcheck_end=end;
  if((spellcheck_end.get_offset()-spellcheck_start.get_offset())>=2) {
    auto last_char=spellcheck_end;
    last_char.backward_char();
    if(*spellcheck_start=='\'' && *last_char=='\'') {
      spellcheck_start.forward_char();
      spellcheck_end.backward_char();
    }
  }
  
  auto word=get_buffer()->get_text(spellcheck_start, spellcheck_end);
  if(word.size()>0) {
    auto correct = aspell_speller_check(spellcheck_checker, word.data(), word.bytes());
    if(correct==0)
      get_buffer()->apply_tag_by_name("spellcheck_error", start, end);
    else
      get_buffer()->remove_tag_by_name("spellcheck_error", start, end);
  }
}

std::vector<std::string> Source::View::spellcheck_get_suggestions(const Gtk::TextIter& start, const Gtk::TextIter& end) {
  auto word_with_error=get_buffer()->get_text(start, end);
  
  const AspellWordList *suggestions = aspell_speller_suggest(spellcheck_checker, word_with_error.data(), word_with_error.bytes());
  AspellStringEnumeration *elements = aspell_word_list_elements(suggestions);
  
  std::vector<std::string> words;
  const char *word;
  while ((word = aspell_string_enumeration_next(elements))!= NULL) {
    words.emplace_back(word);
  }
  delete_aspell_string_enumeration(elements);
  
  return words;
}


/////////////////////
//// GenericView ////
/////////////////////
Source::GenericView::GenericView(const boost::filesystem::path &file_path, const boost::filesystem::path &project_path, Glib::RefPtr<Gsv::Language> language) : View(file_path, project_path, language) {
  configure();
  spellcheck_all=true;
  
  if(language) {
    get_source_buffer()->set_language(language);
    Terminal::get().print("Language for file "+file_path.string()+" set to "+language->get_name()+".\n");
  }
  
  auto completion=get_completion();
  completion->property_show_headers()=false;
  completion->property_show_icons()=false;
  completion->property_accelerators()=0;
  
  auto completion_words=Gsv::CompletionWords::create("", Glib::RefPtr<Gdk::Pixbuf>());
  completion_words->register_provider(get_buffer());
  completion->add_provider(completion_words);
  
  if(language) {
    auto language_manager=Gsv::LanguageManager::get_default();
    auto search_paths=language_manager->get_search_path();
    bool found_language_file=false;
    boost::filesystem::path language_file;
    for(auto &search_path: search_paths) {
      boost::filesystem::path p(static_cast<std::string>(search_path)+'/'+static_cast<std::string>(language->get_id())+".lang");
      if(boost::filesystem::exists(p) && boost::filesystem::is_regular_file(p)) {
        language_file=p;
        found_language_file=true;
        break;
      }
    }
    if(found_language_file) {
      auto completion_buffer_keywords=CompletionBuffer::create();
      boost::property_tree::ptree pt;
      try {
        boost::property_tree::xml_parser::read_xml(language_file.string(), pt);
      }
      catch(const std::exception &e) {
        Terminal::get().print("Error: error parsing language file "+language_file.string()+": "+e.what()+'\n', true);
      }
      bool has_context_class=false;
      parse_language_file(completion_buffer_keywords, has_context_class, pt);
      if(!has_context_class)
        spellcheck_all=false;
      completion_words->register_provider(completion_buffer_keywords);
    }
  }
}

void Source::GenericView::parse_language_file(Glib::RefPtr<CompletionBuffer> &completion_buffer, bool &has_context_class, const boost::property_tree::ptree &pt) {
  bool case_insensitive=false;
  for(auto &node: pt) {
    if(node.first=="<xmlcomment>") {
      auto data=static_cast<std::string>(node.second.data());
      std::transform(data.begin(), data.end(), data.begin(), ::tolower);
      if(data.find("case insensitive")!=std::string::npos)
        case_insensitive=true;
    }
    else if(node.first=="keyword") {
      auto data=static_cast<std::string>(node.second.data());
      completion_buffer->insert_at_cursor(data+'\n');
      if(case_insensitive) {
        std::transform(data.begin(), data.end(), data.begin(), ::tolower);
        completion_buffer->insert_at_cursor(data+'\n');
      }
    }
    else if(!has_context_class && node.first=="context") {
      auto class_attribute=node.second.get<std::string>("<xmlattr>.class", "");
      auto class_disabled_attribute=node.second.get<std::string>("<xmlattr>.class-disabled", "");
      if(class_attribute.size()>0 || class_disabled_attribute.size()>0)
        has_context_class=true;
    }
    try {
      parse_language_file(completion_buffer, has_context_class, node.second);
    }
    catch(const std::exception &e) {        
    }
  }
}
