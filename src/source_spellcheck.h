#ifndef JUCI_SOURCE_SPELLCHECK_H_
#define JUCI_SOURCE_SPELLCHECK_H_
#include <gtksourceviewmm.h>
#include <aspell.h>
#include "selectiondialog.h"

namespace Source {
  class SpellCheckView : public Gsv::View {
  public:
    SpellCheckView();
    ~SpellCheckView();
    
    virtual void configure();
    
    virtual void hide_dialogs();
    
    void spellcheck();
    void remove_spellcheck_errors();
    void goto_next_spellcheck_error();
    
  protected:
    bool spellcheck_all=false;
    guint last_keyval=0;
  private:
    std::unique_ptr<SelectionDialog> spellcheck_suggestions_dialog;
    
    static AspellConfig* spellcheck_config;
    AspellCanHaveError *spellcheck_possible_err;
    AspellSpeller *spellcheck_checker;
    bool is_word_iter(const Gtk::TextIter& iter);
    std::pair<Gtk::TextIter, Gtk::TextIter> spellcheck_get_word(Gtk::TextIter iter);
    void spellcheck_word(const Gtk::TextIter& start, const Gtk::TextIter& end);
    std::vector<std::string> spellcheck_get_suggestions(const Gtk::TextIter& start, const Gtk::TextIter& end);
    sigc::connection delayed_spellcheck_suggestions_connection;
    sigc::connection delayed_spellcheck_error_clear;
    
    void spellcheck(const Gtk::TextIter& start, const Gtk::TextIter& end);
  };
}

#endif //JUCI_SOURCE_SPELLCHECK_H_
