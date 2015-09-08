#ifndef JUCI_SOURCE_H_
#define JUCI_SOURCE_H_
#include <iostream>
#include <unordered_map>
#include <vector>
#include "gtkmm.h"
#include "clangmm.h"
#include <thread>
#include <mutex>
#include <string>
#include <atomic>
#include "gtksourceviewmm.h"
#include "terminal.h"
#include "tooltips.h"
#include "selectiondialog.h"
#include <set>
#include <regex>
#include <aspell.h>

namespace Source {
  Glib::RefPtr<Gsv::Language> guess_language(const boost::filesystem::path &file_path);
  
  class Config {
  public:
    std::string style;
    std::string font;
    std::string spellcheck_language;
    bool auto_tab_char_and_size;
    char default_tab_char;
    unsigned default_tab_size;
    bool wrap_lines;
    bool highlight_current_line;
    bool show_line_numbers;
    std::unordered_map<std::string, std::string> clang_types;
  };

  class Range {
  public:
    Range(std::pair<clang::Offset, clang::Offset> offsets, int kind):
      offsets(offsets), kind(kind) {}
    std::pair<clang::Offset, clang::Offset> offsets;
    int kind;
  };

  class AutoCompleteData {
  public:
    explicit AutoCompleteData(const std::vector<clang::CompletionChunk> &chunks) :
      chunks(chunks) { }
    std::vector<clang::CompletionChunk> chunks;
    std::string brief_comments;
  };

  class View : public Gsv::View {
  public:
    View(const boost::filesystem::path &file_path);
    ~View();
    
    void search_highlight(const std::string &text, bool case_sensitive, bool regex);
    std::function<void(int number)> update_search_occurrences;
    void search_forward();
    void search_backward();
    void replace_forward(const std::string &replacement);
    void replace_backward(const std::string &replacement);
    void replace_all(const std::string &replacement);
    
    void paste();
        
    boost::filesystem::path file_path;
    
    std::function<std::pair<std::string, clang::Offset>()> get_declaration_location;
    std::function<void()> goto_method;
    std::function<std::string()> get_token;
    std::function<std::string()> get_token_name;
    std::function<void(const std::string &token)> tag_similar_tokens;
    std::function<size_t(const std::string &token, const std::string &text)> rename_similar_tokens;
    
    std::function<void(View* view, const std::string &status)> on_update_status;
    std::string status;
  protected:
    bool source_readable;
    Tooltips diagnostic_tooltips;
    Tooltips type_tooltips;
    gdouble on_motion_last_x;
    gdouble on_motion_last_y;
    sigc::connection delayed_tooltips_connection;
    void set_tooltip_events();
    
    void set_status(const std::string &status);
    
    std::string get_line(const Gtk::TextIter &iter);
    std::string get_line(Glib::RefPtr<Gtk::TextBuffer::Mark> mark);
    std::string get_line(int line_nr);
    std::string get_line();
    std::string get_line_before(const Gtk::TextIter &iter);
    std::string get_line_before(Glib::RefPtr<Gtk::TextBuffer::Mark> mark);
    std::string get_line_before();
    
    bool find_start_of_closed_expression(Gtk::TextIter iter, Gtk::TextIter &found_iter);
    bool find_open_expression_symbol(Gtk::TextIter iter, const Gtk::TextIter &until_iter, Gtk::TextIter &found_iter);
    bool find_right_bracket_forward(Gtk::TextIter iter, Gtk::TextIter &found_iter);
    
    bool on_key_press_event(GdkEventKey* key);
    bool on_button_press_event(GdkEventButton *event);
    
    std::pair<char, unsigned> find_tab_char_and_size();
    unsigned tab_size;
    char tab_char;
    std::string tab;
    std::regex tabs_regex;
    
    bool spellcheck_all=false;
    std::unique_ptr<SelectionDialog> spellcheck_suggestions_dialog;
    bool spellcheck_suggestions_dialog_shown=false;
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
    bool last_keyval_is_backspace=false;
  };
  
  class GenericView : public View {
  public:
    GenericView(const boost::filesystem::path &file_path, Glib::RefPtr<Gsv::Language> language);
  };
  
  class ClangViewParse : public View {
  public:
    ClangViewParse(const boost::filesystem::path &file_path, const boost::filesystem::path& project_path);
    ~ClangViewParse();
    boost::filesystem::path project_path;
    void start_reparse();
    bool reparse_needed=false;
  protected:
    void init_parse();
    bool on_key_press_event(GdkEventKey* key);
    std::unique_ptr<clang::TranslationUnit> clang_tu;
    std::mutex parsing_mutex;
    std::unique_ptr<clang::Tokens> clang_tokens;
    sigc::connection delayed_reparse_connection;
    
    std::shared_ptr<Terminal::InProgress> parsing_in_progress;
    std::thread parse_thread;
    std::atomic<bool> parse_thread_stop;
    std::atomic<bool> parse_error;
    
    std::regex bracket_regex;
    std::regex no_bracket_statement_regex;
    std::regex no_bracket_no_para_statement_regex;
  private:
    std::map<std::string, std::string> get_buffer_map() const;
    void update_syntax();
    std::set<std::string> last_syntax_tags;
    void update_diagnostics();
    void update_types();
    
    static clang::Index clang_index;
    std::vector<std::string> get_compilation_commands();
    
    Glib::Dispatcher parse_done;
    Glib::Dispatcher parse_start;
    Glib::Dispatcher parse_fail;
    std::map<std::string, std::string> parse_thread_buffer_map;
    std::mutex parse_thread_buffer_map_mutex;
    std::atomic<bool> parse_thread_go;
    std::atomic<bool> parse_thread_mapped;
  };
    
  class ClangViewAutocomplete : public ClangViewParse {
  public:
    ClangViewAutocomplete(const boost::filesystem::path &file_path, const boost::filesystem::path& project_path);
    void async_delete();
    bool restart_parse();
  protected:
    bool on_key_press_event(GdkEventKey* key);
    std::thread autocomplete_thread;
  private:
    void start_autocomplete();
    void autocomplete();
    std::unique_ptr<CompletionDialog> completion_dialog;
    bool completion_dialog_shown=false;
    std::vector<Source::AutoCompleteData> get_autocomplete_suggestions(int line_number, int column, std::map<std::string, std::string>& buffer_map);
    Glib::Dispatcher autocomplete_done;
    sigc::connection autocomplete_done_connection;
    Glib::Dispatcher autocomplete_fail;
    bool autocomplete_starting=false;
    std::atomic<bool> autocomplete_cancel_starting;
    guint last_keyval=0;
    std::string prefix;
    std::mutex prefix_mutex;
    
    Glib::Dispatcher do_delete_object;
    Glib::Dispatcher do_restart_parse;
    std::thread delete_thread;
    std::thread restart_parse_thread;
    bool restart_parse_running=false;
  };

  class ClangViewRefactor : public ClangViewAutocomplete {
  public:
    ClangViewRefactor(const boost::filesystem::path &file_path, const boost::filesystem::path& project_path);
    ~ClangViewRefactor();
  private:
    Glib::RefPtr<Gtk::TextTag> similar_tokens_tag;
    std::string last_similar_tokens_tagged;
    sigc::connection delayed_tag_similar_tokens_connection;
    std::unique_ptr<SelectionDialog> selection_dialog;
    bool renaming=false;
  };
  
  class ClangView : public ClangViewRefactor {
  public:
    ClangView(const boost::filesystem::path &file_path, const boost::filesystem::path& project_path, Glib::RefPtr<Gsv::Language> language);
  };
};  // class Source
#endif  // JUCI_SOURCE_H_
