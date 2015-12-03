#ifndef JUCI_SOURCE_H_
#define JUCI_SOURCE_H_
#include <aspell.h>
#include <boost/property_tree/xml_parser.hpp>
#include <gtksourceviewmm.h>
#include <string>
#include <unordered_map>
#include <vector>

#include "selectiondialog.h"
#include "tooltips.h"

namespace Source {
  Glib::RefPtr<Gsv::Language> guess_language(const boost::filesystem::path &file_path);
  
  class Token {
  public:
    Token(): type(-1) {}
    Token(Glib::RefPtr<Gsv::Language> language, int type, const std::string &spelling, const std::string &usr): 
      language(language), type(type), spelling(spelling), usr(usr) {}
    operator bool() const {return (type>=0 && spelling.size()>0 && usr.size()>0);}
    bool operator==(const Token &o) const {return (type==o.type &&
                                                   spelling==o.spelling &&
                                                   usr==o.usr);}
    bool operator!=(const Token &o) const {return !(*this==o);}
    
    Glib::RefPtr<Gsv::Language> language;
    int type;
    std::string spelling;
    std::string usr;
  };
  
  class Offset {
  public:
    Offset() {}
    Offset(unsigned line, unsigned index, const boost::filesystem::path &file_path=""): line(line), index(index), file_path(file_path) {}
    bool operator==(const Offset &o) {return (line==o.line && index==o.index);}
    
    unsigned line;
    unsigned index;
    boost::filesystem::path file_path;
  };
  
  class FixIt {
  public:
    enum class Type {INSERT, REPLACE, ERASE};
    
    FixIt(const std::string &source, const std::pair<Offset, Offset> &offsets);
    
    std::string string(Glib::RefPtr<Gtk::TextBuffer> buffer);
    
    Type type;
    std::string source;
    std::pair<Offset, Offset> offsets;
  };

  class View : public Gsv::View {
  public:
    View(const boost::filesystem::path &file_path, const boost::filesystem::path &project_path, Glib::RefPtr<Gsv::Language> language);
    ~View();
    
    virtual void configure();
    
    void search_highlight(const std::string &text, bool case_sensitive, bool regex);
    std::function<void(int number)> update_search_occurrences;
    void search_forward();
    void search_backward();
    void replace_forward(const std::string &replacement);
    void replace_backward(const std::string &replacement);
    void replace_all(const std::string &replacement);
    
    void paste();
        
    boost::filesystem::path file_path;
    boost::filesystem::path project_path;
    Glib::RefPtr<Gsv::Language> language;
    
    std::function<void()> auto_indent;
    std::function<Offset()> get_declaration_location;
    std::function<std::vector<std::pair<Offset, std::string> >(const Token &token)> get_usages;
    std::function<void()> goto_method;
    std::function<Token()> get_token;
    std::function<std::vector<std::string>()> get_token_data;
    std::function<size_t(const Token &token, const std::string &text)> rename_similar_tokens;
    std::function<void()> goto_next_diagnostic;
    std::function<void()> apply_fix_its;
    
    std::unique_ptr<SelectionDialog> selection_dialog;
    sigc::connection delayed_tooltips_connection;
    
    std::function<void(View* view, const std::string &status)> on_update_status;
    std::function<void(View* view, const std::string &info)> on_update_info;
    void set_status(const std::string &status);
    void set_info(const std::string &info);
    std::string status;
    std::string info;
    
    void spellcheck();
    void remove_spellcheck_errors();
    void goto_next_spellcheck_error();
    
    void set_tab_char_and_size(char tab_char, unsigned tab_size);
    std::pair<char, unsigned> get_tab_char_and_size() {return {tab_char, tab_size};}
    
    bool soft_reparse_needed=false;
    bool full_reparse_needed=false;
    virtual void soft_reparse() {soft_reparse_needed=false;}
    virtual bool full_reparse() {full_reparse_needed=false; return true;}
  protected:
    bool parsed=false;
    Tooltips diagnostic_tooltips;
    Tooltips type_tooltips;
    virtual void show_diagnostic_tooltips(const Gdk::Rectangle &rectangle) {}
    virtual void show_type_tooltips(const Gdk::Rectangle &rectangle) {}
    gdouble on_motion_last_x;
    gdouble on_motion_last_y;
    void set_tooltip_events();
        
    std::string get_line(const Gtk::TextIter &iter);
    std::string get_line(Glib::RefPtr<Gtk::TextBuffer::Mark> mark);
    std::string get_line(int line_nr);
    std::string get_line();
    std::string get_line_before(const Gtk::TextIter &iter);
    std::string get_line_before(Glib::RefPtr<Gtk::TextBuffer::Mark> mark);
    std::string get_line_before();
    Gtk::TextIter get_tabs_end_iter(const Gtk::TextIter &iter);
    Gtk::TextIter get_tabs_end_iter(Glib::RefPtr<Gtk::TextBuffer::Mark> mark);
    Gtk::TextIter get_tabs_end_iter(int line_nr);
    Gtk::TextIter get_tabs_end_iter();
    
    bool find_start_of_closed_expression(Gtk::TextIter iter, Gtk::TextIter &found_iter);
    bool find_open_expression_symbol(Gtk::TextIter iter, const Gtk::TextIter &until_iter, Gtk::TextIter &found_iter);
    bool find_right_bracket_forward(Gtk::TextIter iter, Gtk::TextIter &found_iter);
    bool find_left_bracket_backward(Gtk::TextIter iter, Gtk::TextIter &found_iter);
    
    bool on_key_press_event(GdkEventKey* key) override;
    bool on_button_press_event(GdkEventButton *event) override;
    
    std::pair<char, unsigned> find_tab_char_and_size();
    unsigned tab_size;
    char tab_char;
    std::string tab;
    
    bool spellcheck_all=false;
    std::unique_ptr<SelectionDialog> spellcheck_suggestions_dialog;
    bool spellcheck_suggestions_dialog_shown=false;
    bool last_keyval_is_backspace=false;
    bool last_keyval_is_return=false;
  private:
    GtkSourceSearchContext *search_context;
    GtkSourceSearchSettings *search_settings;
    static void search_occurrences_updated(GtkWidget* widget, GParamSpec* property, gpointer data);
        
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
  
  class GenericView : public View {
  private:
    class CompletionBuffer : public Gtk::TextBuffer {
    public:
      static Glib::RefPtr<CompletionBuffer> create() {return Glib::RefPtr<CompletionBuffer>(new CompletionBuffer());}
    };
  public:
    GenericView(const boost::filesystem::path &file_path, const boost::filesystem::path &project_path, Glib::RefPtr<Gsv::Language> language);
    
    void parse_language_file(Glib::RefPtr<CompletionBuffer> &completion_buffer, bool &has_context_class, const boost::property_tree::ptree &pt);
  };
}
#endif  // JUCI_SOURCE_H_
