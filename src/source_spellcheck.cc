#include "source_spellcheck.h"
#include "config.h"
#include "info.h"
#include "selection_dialog.h"
#include <iostream>

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

AspellConfig* Source::SpellCheckView::spellcheck_config=nullptr;

Source::SpellCheckView::SpellCheckView() : Gsv::View() {
  if(spellcheck_config==nullptr)
    spellcheck_config=new_aspell_config();
  spellcheck_checker=nullptr;
  spellcheck_error_tag=get_buffer()->create_tag("spellcheck_error");
  spellcheck_error_tag->property_underline()=Pango::Underline::UNDERLINE_ERROR;
  
  signal_key_press_event().connect([this](GdkEventKey *event) {
    if(SelectionDialog::get() && SelectionDialog::get()->is_visible()) {
      if(SelectionDialog::get()->on_key_press(event))
        return true;
    }
    
    return false;
  }, false);
  
  //The following signal is added in case SpellCheckView is not subclassed
  signal_key_press_event().connect([this](GdkEventKey *event) {
    last_keyval=event->keyval;
    return false;
  });
  
  get_buffer()->signal_changed().connect([this]() {
    if(spellcheck_checker==nullptr)
      return;
    
    delayed_spellcheck_suggestions_connection.disconnect();
    
    auto iter=get_buffer()->get_insert()->get_iter();
    if(!is_word_iter(iter) && !iter.starts_line())
      iter.backward_char();
    if(!is_code_iter(iter)) {
      if(last_keyval==GDK_KEY_Return || last_keyval==GDK_KEY_KP_Enter) {
        auto previous_line_iter=iter;
        while(previous_line_iter.backward_char() && !previous_line_iter.ends_line()) {}
        if(previous_line_iter.backward_char()) {
          get_buffer()->remove_tag(spellcheck_error_tag, previous_line_iter, iter);
          auto word=get_word(previous_line_iter);
          spellcheck_word(word.first, word.second);
          word=get_word(iter);
          spellcheck_word(word.first, word.second);
        }
      }
      else {
        auto previous_iter=iter;
        //When for instance using space to split two words
        if(!iter.starts_line() && !iter.ends_line() && is_word_iter(iter) &&
           previous_iter.backward_char() && !previous_iter.starts_line() && !is_word_iter(previous_iter)) {
          auto first=previous_iter;
          auto second=iter;
          if(first.backward_char()) {
            get_buffer()->remove_tag(spellcheck_error_tag, first, second);
            auto word=get_word(first);
            spellcheck_word(word.first, word.second);
            word=get_word(second);
            spellcheck_word(word.first, word.second);
          }
        }
        else {
          auto word=get_word(iter);
          spellcheck_word(word.first, word.second);
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
            get_buffer()->remove_tag(spellcheck_error_tag, begin_no_spellcheck_iter, iter);
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
          get_buffer()->remove_tag(spellcheck_error_tag, begin_no_spellcheck_iter, iter);
      }
      return false;
    }, 1000);
  });
  
  // In case of for instance text paste or undo/redo
  get_buffer()->signal_insert().connect([this](const Gtk::TextIter &start_iter, const Glib::ustring &inserted_string, int) {
    if(spellcheck_checker==nullptr)
      return;
    if(inserted_string.size()<=1)
      return;
    
    auto iter=start_iter;
    if(!is_word_iter(iter) && !iter.starts_line())
      iter.backward_char();
    if(is_word_iter(iter)) {
      auto word=get_word(iter);
      get_buffer()->remove_tag(spellcheck_error_tag, word.first, word.second);
    }
  }, false);
  
  get_buffer()->signal_mark_set().connect([this](const Gtk::TextBuffer::iterator& iter, const Glib::RefPtr<Gtk::TextBuffer::Mark>& mark) {
    if(spellcheck_checker==nullptr)
      return;
    
    if(mark->get_name()=="insert") {
      if(SelectionDialog::get())
        SelectionDialog::get()->hide();
      delayed_spellcheck_suggestions_connection.disconnect();
      delayed_spellcheck_suggestions_connection=Glib::signal_timeout().connect([this]() {
        if(get_buffer()->get_insert()->get_iter().has_tag(spellcheck_error_tag)) {
          SelectionDialog::create(this, get_buffer()->create_mark(get_buffer()->get_insert()->get_iter()), false);
          auto word=get_word(get_buffer()->get_insert()->get_iter());
          auto suggestions=get_spellcheck_suggestions(word.first, word.second);
          if(suggestions.size()==0)
            return false;
          for(auto &suggestion: suggestions)
            SelectionDialog::get()->add_row(suggestion);
          SelectionDialog::get()->on_select=[this, word](const std::string& selected, bool hide_window) {
            get_buffer()->begin_user_action();
            get_buffer()->erase(word.first, word.second);
            get_buffer()->insert(get_buffer()->get_insert()->get_iter(), selected);
            get_buffer()->end_user_action();
          };
          hide_tooltips();
          SelectionDialog::get()->show();
        }
        return false;
      }, 500);
    }
  });
  
  get_buffer()->signal_mark_set().connect([this](const Gtk::TextBuffer::iterator& iterator, const Glib::RefPtr<Gtk::TextBuffer::Mark>& mark) {
    if(mark->get_name()=="insert") {
      if(SelectionDialog::get())
        SelectionDialog::get()->hide();
    }
  });
  
  signal_focus_out_event().connect([this](GdkEventFocus* event) {
    delayed_spellcheck_suggestions_connection.disconnect();
    return false;
  });
  
  signal_leave_notify_event().connect([this](GdkEventCrossing*) {
    delayed_spellcheck_suggestions_connection.disconnect();
    return false;
  });
  
  signal_tag_added_connection=get_buffer()->get_tag_table()->signal_tag_added().connect([this](const Glib::RefPtr<Gtk::TextTag> &tag) {
    if(tag->property_name()=="gtksourceview:context-classes:comment")
      comment_tag=tag;
    else if(tag->property_name()=="gtksourceview:context-classes:string")
      string_tag=tag;
    else if(tag->property_name()=="gtksourceview:context-classes:no-spell-check")
      no_spell_check_tag=tag;
  });
  signal_tag_removed_connection=get_buffer()->get_tag_table()->signal_tag_removed().connect([this](const Glib::RefPtr<Gtk::TextTag> &tag) {
    if(tag->property_name()=="gtksourceview:context-classes:comment")
      comment_tag.reset();
    else if(tag->property_name()=="gtksourceview:context-classes:string")
      string_tag.reset();
    else if(tag->property_name()=="gtksourceview:context-classes:no-spell-check")
      no_spell_check_tag.reset();
  });
}

Source::SpellCheckView::~SpellCheckView() {
  delayed_spellcheck_suggestions_connection.disconnect();
  delayed_spellcheck_error_clear.disconnect();
  
  if(spellcheck_checker!=nullptr)
    delete_aspell_speller(spellcheck_checker);
  
  signal_tag_added_connection.disconnect();
  signal_tag_removed_connection.disconnect();
}

void Source::SpellCheckView::configure() {
  if(Config::get().source.spellcheck_language.size()>0) {
    aspell_config_replace(spellcheck_config, "lang", Config::get().source.spellcheck_language.c_str());
    aspell_config_replace(spellcheck_config, "encoding", "utf-8");
  }
  spellcheck_possible_err=new_aspell_speller(spellcheck_config);
  if(spellcheck_checker!=nullptr)
    delete_aspell_speller(spellcheck_checker);
  spellcheck_checker=nullptr;
  if (aspell_error_number(spellcheck_possible_err) != 0)
    std::cerr << "Spell check error: " << aspell_error_message(spellcheck_possible_err) << std::endl;
  else
    spellcheck_checker = to_aspell_speller(spellcheck_possible_err);
  get_buffer()->remove_tag(spellcheck_error_tag, get_buffer()->begin(), get_buffer()->end());
}

void Source::SpellCheckView::hide_dialogs() {
  delayed_spellcheck_suggestions_connection.disconnect();
  if(SelectionDialog::get())
    SelectionDialog::get()->hide();
}

void Source::SpellCheckView::spellcheck(const Gtk::TextIter& start, const Gtk::TextIter& end) {
  if(spellcheck_checker==nullptr)
    return;
  auto iter=start;
  while(iter && iter<end) {
    if(is_word_iter(iter)) {
      auto word=get_word(iter);
      spellcheck_word(word.first, word.second);
      iter=word.second; 
    }
    iter.forward_char();
  }
}

void Source::SpellCheckView::spellcheck() {
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
    bool spell_check=!is_code_iter(iter);
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

void Source::SpellCheckView::remove_spellcheck_errors() {
  get_buffer()->remove_tag(spellcheck_error_tag, get_buffer()->begin(), get_buffer()->end());
}

void Source::SpellCheckView::goto_next_spellcheck_error() {
  auto iter=get_buffer()->get_insert()->get_iter();
  auto insert_iter=iter;
  bool wrapped=false;
  iter.forward_char();
  while(!wrapped || iter<insert_iter) {
    auto toggled_tags=iter.get_toggled_tags();
    for(auto &toggled_tag: toggled_tags) {
      if(toggled_tag==spellcheck_error_tag) {
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
  Info::get().print("No spelling errors found in current buffer");
}

bool Source::SpellCheckView::is_code_iter(const Gtk::TextIter &iter) {
  if(*iter=='\'') {
    auto previous_iter=iter;
    if(!iter.starts_line() && previous_iter.backward_char() && *previous_iter=='\'')
      return false;
  }
  if(spellcheck_all) {
    if(no_spell_check_tag && (iter.has_tag(no_spell_check_tag) || iter.begins_tag(no_spell_check_tag) || iter.ends_tag(no_spell_check_tag)))
      return true;
    return false;
  }
  if(comment_tag) {
    if(iter.has_tag(comment_tag) && !iter.begins_tag(comment_tag))
      return false;
    //Exception at the end of /**/
    else if(iter.ends_tag(comment_tag)) {
      auto previous_iter=iter;
      if(previous_iter.backward_char() && *previous_iter=='/') {
        auto previous_previous_iter=previous_iter;
        if(previous_previous_iter.backward_char() && *previous_previous_iter=='*') {
          auto it=previous_iter;
          while(!it.begins_tag(comment_tag) && it.backward_to_tag_toggle(comment_tag)) {}
          auto next_iter=it;
          if(it.begins_tag(comment_tag) && next_iter.forward_char() && *it=='/' && *next_iter=='*' && previous_iter!=it)
            return true;
        }
      }
      return false;
    }
  }
  if(string_tag) {
    if(iter.has_tag(string_tag)) {
      if(!iter.begins_tag(string_tag))
        return false;
    }
    // If iter is at the end of string_tag, with exception of after " and '
    else if(iter.ends_tag(string_tag)) {
      auto previous_iter=iter;
      if(!iter.starts_line() && previous_iter.backward_char()) {
        if((*previous_iter=='"' || *previous_iter=='\'')) {
          long backslash_count=0;
          auto it=previous_iter;
          while(it.backward_char() && *it=='\\')
            ++backslash_count;
          if(backslash_count%2==0) {
            auto it=previous_iter;
            while(!it.begins_tag(string_tag) && it.backward_to_tag_toggle(string_tag)) {}
            if(it.begins_tag(string_tag) && *previous_iter==*it && previous_iter!=it)
              return true;
          }
        }
        return false;
      }
    }
  }
  return true;
}

bool Source::SpellCheckView::is_word_iter(const Gtk::TextIter& iter) {
  return ((*iter>='A' && *iter<='Z') || (*iter>='a' && *iter<='z') || *iter=='\'' || *iter>=128);
}

std::pair<Gtk::TextIter, Gtk::TextIter> Source::SpellCheckView::get_word(Gtk::TextIter iter) {
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

void Source::SpellCheckView::spellcheck_word(const Gtk::TextIter& start, const Gtk::TextIter& end) {
  if((end.get_offset()-start.get_offset())==2) {
    auto before_end=end;
    before_end.backward_char();
    if(*before_end=='\'' || *start=='\'') {
      get_buffer()->remove_tag(spellcheck_error_tag, start, end);
      return;
    }
  }
  else if((end.get_offset()-start.get_offset())==3) {
    auto before_end=end;
    before_end.backward_char();
    if(*before_end=='\'' && *start=='\'') {
      get_buffer()->remove_tag(spellcheck_error_tag, start, end);
      return;
    }
  }
  
  auto word=get_buffer()->get_text(start, end);
  if(word.size()>0) {
    auto correct = aspell_speller_check(spellcheck_checker, word.data(), word.bytes());
    if(correct==0)
      get_buffer()->apply_tag(spellcheck_error_tag, start, end);
    else
      get_buffer()->remove_tag(spellcheck_error_tag, start, end);
  }
}

std::vector<std::string> Source::SpellCheckView::get_spellcheck_suggestions(const Gtk::TextIter& start, const Gtk::TextIter& end) {
  auto word_with_error=get_buffer()->get_text(start, end);
  
  const AspellWordList *suggestions = aspell_speller_suggest(spellcheck_checker, word_with_error.data(), word_with_error.bytes());
  AspellStringEnumeration *elements = aspell_word_list_elements(suggestions);
  
  std::vector<std::string> words;
  const char *word;
  while ((word = aspell_string_enumeration_next(elements))!= nullptr) {
    words.emplace_back(word);
  }
  delete_aspell_string_enumeration(elements);
  
  return words;
}
