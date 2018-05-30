#pragma once
#include "source_spellcheck.h"
#include "source_diff.h"
#include "tooltips.h"
#include <boost/property_tree/xml_parser.hpp>
#include <boost/filesystem.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <tuple>

namespace Source {
  /// Workaround for buggy Gsv::LanguageManager::get_default()
  class LanguageManager {
  public:
    static Glib::RefPtr<Gsv::LanguageManager> get_default();
  };
  /// Workaround for buggy Gsv::StyleSchemeManager::get_default()
  class StyleSchemeManager {
  public:
    static Glib::RefPtr<Gsv::StyleSchemeManager> get_default();
  };
  
  Glib::RefPtr<Gsv::Language> guess_language(const boost::filesystem::path &file_path);
  
  class Offset {
  public:
    Offset() = default;
    Offset(unsigned line, unsigned index, boost::filesystem::path file_path_=""): line(line), index(index), file_path(std::move(file_path_)) {}
    operator bool() { return !file_path.empty(); }
    bool operator==(const Offset &o) {return (line==o.line && index==o.index);}
    
    unsigned line;
    unsigned index;
    boost::filesystem::path file_path;
  };
  
  class FixIt {
  public:
    enum class Type {INSERT, REPLACE, ERASE};
    
    FixIt(std::string source_, std::pair<Offset, Offset> offsets_);
    
    std::string string(const Glib::RefPtr<Gtk::TextBuffer> &buffer);
    
    Type type;
    std::string source;
    std::pair<Offset, Offset> offsets;
  };

  class View : public SpellCheckView, public DiffView {
  public:
    static std::unordered_set<View*> non_deleted_views;
    static std::unordered_set<View*> views;
    
    View(const boost::filesystem::path &file_path, const Glib::RefPtr<Gsv::Language> &language, bool is_generic_view=false);
    ~View() override;
    
    bool save() override;
    
    void configure() override;
    
    void search_highlight(const std::string &text, bool case_sensitive, bool regex);
    std::function<void(int number)> update_search_occurrences;
    void search_forward();
    void search_backward();
    void replace_forward(const std::string &replacement);
    void replace_backward(const std::string &replacement);
    void replace_all(const std::string &replacement);
    
    void paste();
    
    std::function<void()> non_interactive_completion;
    std::function<void(bool)> format_style;
    std::function<Offset()> get_declaration_location;
    std::function<Offset()> get_type_declaration_location;
    std::function<std::vector<Offset>()> get_implementation_locations;
    std::function<std::vector<Offset>()> get_declaration_or_implementation_locations;
    std::function<std::vector<std::pair<Offset, std::string> >()> get_usages;
    std::function<std::string()> get_method;
    std::function<std::vector<std::pair<Offset, std::string> >()> get_methods;
    std::function<std::vector<std::string>()> get_token_data;
    std::function<std::string()> get_token_spelling;
    std::function<void(const std::string &text)> rename_similar_tokens;
    std::function<void()> goto_next_diagnostic;
    std::function<std::vector<FixIt>()> get_fix_its;
    std::function<void()> toggle_comments;
    std::function<std::tuple<Source::Offset, std::string, size_t>()> get_documentation_template;
    std::function<void(int)> toggle_breakpoint;
    
    void hide_tooltips() override;
    void hide_dialogs() override;
    
    void set_tab_char_and_size(char tab_char, unsigned tab_size);
    std::pair<char, unsigned> get_tab_char_and_size() {return {tab_char, tab_size};}
    
    bool soft_reparse_needed=false;
    bool full_reparse_needed=false;
    virtual void soft_reparse(bool delayed=false) {soft_reparse_needed=false;}
    virtual void full_reparse() {full_reparse_needed=false;}
  protected:
    std::atomic<bool> parsed;
    Tooltips diagnostic_tooltips;
    Tooltips type_tooltips;
    sigc::connection delayed_tooltips_connection;
    Glib::RefPtr<Gtk::TextTag> similar_symbol_tag;
    sigc::connection delayed_tag_similar_symbols_connection;
    virtual void show_diagnostic_tooltips(const Gdk::Rectangle &rectangle) { diagnostic_tooltips.show(rectangle); }
    void add_diagnostic_tooltip(const Gtk::TextIter &start, const Gtk::TextIter &end, std::string spelling, bool error);
    void clear_diagnostic_tooltips();
    virtual void show_type_tooltips(const Gdk::Rectangle &rectangle) {}
    gdouble on_motion_last_x=0.0;
    gdouble on_motion_last_y=0.0;
    
    /// Usually returns at start of line, but not always
    Gtk::TextIter find_start_of_sentence(Gtk::TextIter iter);
    bool find_open_non_curly_bracket_backward(Gtk::TextIter iter, Gtk::TextIter &found_iter);
    bool find_open_curly_bracket_backward(Gtk::TextIter iter, Gtk::TextIter &found_iter);
    bool find_close_curly_bracket_forward(Gtk::TextIter iter, Gtk::TextIter &found_iter);
    long symbol_count(Gtk::TextIter iter, unsigned int positive_char, unsigned int negative_char);
    bool is_templated_function(Gtk::TextIter iter, Gtk::TextIter &parenthesis_end_iter);
    
    std::string get_token(Gtk::TextIter iter);
    
    void cleanup_whitespace_characters_on_return(const Gtk::TextIter &iter);
    
    bool is_bracket_language=false;
    bool on_key_press_event(GdkEventKey* key) override;
    bool on_key_press_event_basic(GdkEventKey* key);
    bool on_key_press_event_bracket_language(GdkEventKey* key);
    bool on_key_press_event_smart_brackets(GdkEventKey* key);
    bool on_key_press_event_smart_inserts(GdkEventKey* key);
    bool on_button_press_event(GdkEventButton *event) override;
    bool on_motion_notify_event(GdkEventMotion *motion_event) override;
    
    std::pair<char, unsigned> find_tab_char_and_size();
    unsigned tab_size;
    char tab_char;
    std::string tab;
    
    bool interactive_completion=true;
    
    guint previous_non_modifier_keyval=0;
  private:
    void setup_tooltip_and_dialog_events();
    void setup_format_style(bool is_generic_view);
    
    void cleanup_whitespace_characters();
    Gsv::DrawSpacesFlags parse_show_whitespace_characters(const std::string &text);
    
    GtkSourceSearchContext *search_context;
    GtkSourceSearchSettings *search_settings;
    static void search_occurrences_updated(GtkWidget* widget, GParamSpec* property, gpointer data);
    
    sigc::connection renderer_activate_connection;
    
    bool multiple_cursors_signals_set=false;
    bool multiple_cursors_use=false;
    std::vector<std::pair<Glib::RefPtr<Gtk::TextBuffer::Mark>, int>> multiple_cursors_extra_cursors;
    Glib::RefPtr<Gtk::TextBuffer::Mark> multiple_cursors_last_insert;
    int multiple_cursors_erase_backward_length;
    int multiple_cursors_erase_forward_length;
    bool on_key_press_event_multiple_cursors(GdkEventKey* key);
  };
  
  class GenericView : public View {
  private:
    class CompletionBuffer : public Gtk::TextBuffer {
    public:
      static Glib::RefPtr<CompletionBuffer> create() {return Glib::RefPtr<CompletionBuffer>(new CompletionBuffer());}
    };
  public:
    GenericView(const boost::filesystem::path &file_path, const Glib::RefPtr<Gsv::Language> &language);
    
    void parse_language_file(Glib::RefPtr<CompletionBuffer> &completion_buffer, bool &has_context_class, const boost::property_tree::ptree &pt);
  };
}
