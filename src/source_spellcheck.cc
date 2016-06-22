#include "source_spellcheck.h"
#include "config.h"
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

AspellConfig* Source::SpellCheckView::spellcheck_config=NULL;

Source::SpellCheckView::SpellCheckView() : Gsv::View() {
  if(spellcheck_config==NULL)
    spellcheck_config=new_aspell_config();
  spellcheck_checker=NULL;
  auto tag=get_buffer()->create_tag("spellcheck_error");
  tag->property_underline()=Pango::Underline::UNDERLINE_ERROR;
  
  signal_key_press_event().connect([this](GdkEventKey *event) {
    if(spellcheck_suggestions_dialog && spellcheck_suggestions_dialog->shown) {
      if(spellcheck_suggestions_dialog->on_key_press(event))
        return true;
    }
    
    return false;
  }, false);
  
  //The following signal is added in case SpellCheckView is not subclassed
  signal_key_press_event().connect([this](GdkEventKey *event) {
    last_keyval=event->keyval;
    return false;
  });
  
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
        if(last_keyval==GDK_KEY_BackSpace && !is_word_iter(iter) && iter.forward_char()) {} //backspace fix
        if((spellcheck_all && !get_source_buffer()->iter_has_context_class(context_iter, "no-spell-check")) || get_source_buffer()->iter_has_context_class(context_iter, "comment") || get_source_buffer()->iter_has_context_class(context_iter, "string")) {
          if(!is_word_iter(iter) || last_keyval==GDK_KEY_Return) { //Might have used space or - to split two words
            auto first=iter;
            auto second=iter;
            if(last_keyval==GDK_KEY_Return) {
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
      if(spellcheck_suggestions_dialog)
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
          auto word=spellcheck_get_word(get_buffer()->get_insert()->get_iter());
          auto suggestions=spellcheck_get_suggestions(word.first, word.second);
          if(suggestions.size()==0)
            return false;
          for(auto &suggestion: suggestions)
            spellcheck_suggestions_dialog->add_row(suggestion);
          spellcheck_suggestions_dialog->on_select=[this, word](const std::string& selected, bool hide_window) {
            get_buffer()->begin_user_action();
            get_buffer()->erase(word.first, word.second);
            get_buffer()->insert(get_buffer()->get_insert()->get_iter(), selected);
            get_buffer()->end_user_action();
          };
          hide_tooltips();
          spellcheck_suggestions_dialog->show();
        }
        return false;
      }, 500);
    }
  });
  
  get_buffer()->signal_mark_set().connect([this](const Gtk::TextBuffer::iterator& iterator, const Glib::RefPtr<Gtk::TextBuffer::Mark>& mark) {
    if(mark->get_name()=="insert") {
      if(spellcheck_suggestions_dialog)
        spellcheck_suggestions_dialog->hide();
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
}

Source::SpellCheckView::~SpellCheckView() {
  delayed_spellcheck_suggestions_connection.disconnect();
  delayed_spellcheck_error_clear.disconnect();
  
  if(spellcheck_checker!=NULL)
    delete_aspell_speller(spellcheck_checker);//asd
}

void Source::SpellCheckView::configure() {
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

void Source::SpellCheckView::hide_dialogs() {
  delayed_spellcheck_suggestions_connection.disconnect();
  if(spellcheck_suggestions_dialog)
    spellcheck_suggestions_dialog->hide();
}

void Source::SpellCheckView::spellcheck(const Gtk::TextIter& start, const Gtk::TextIter& end) {
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

void Source::SpellCheckView::remove_spellcheck_errors() {
  get_buffer()->remove_tag_by_name("spellcheck_error", get_buffer()->begin(), get_buffer()->end());
}

void Source::SpellCheckView::goto_next_spellcheck_error() {
  auto iter=get_buffer()->get_insert()->get_iter();
  auto insert_iter=iter;
  bool wrapped=false;
  iter.forward_char();
  while(!wrapped || iter<insert_iter) {
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

bool Source::SpellCheckView::is_word_iter(const Gtk::TextIter& iter) {
  return ((*iter>='A' && *iter<='Z') || (*iter>='a' && *iter<='z') || *iter=='\'' || *iter>=128);
}

std::pair<Gtk::TextIter, Gtk::TextIter> Source::SpellCheckView::spellcheck_get_word(Gtk::TextIter iter) {
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
    if(*before_end=='\'' || *start=='\'')
      return;
  }
  else if((end.get_offset()-start.get_offset())==3) {
    auto before_end=end;
    before_end.backward_char();
    if(*before_end=='\'' && *start=='\'')
      return;
  }
  
  auto word=get_buffer()->get_text(start, end);
  if(word.size()>0) {
    auto correct = aspell_speller_check(spellcheck_checker, word.data(), word.bytes());
    if(correct==0)
      get_buffer()->apply_tag_by_name("spellcheck_error", start, end);
    else
      get_buffer()->remove_tag_by_name("spellcheck_error", start, end);
  }
}

std::vector<std::string> Source::SpellCheckView::spellcheck_get_suggestions(const Gtk::TextIter& start, const Gtk::TextIter& end) {
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
