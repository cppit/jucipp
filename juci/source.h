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

namespace Source {
  class Config {
  public:
    bool legal_extension(std::string e) const ;
    unsigned tab_size;
    bool show_line_numbers, highlight_current_line;
    std::string tab, background, background_selected, background_tooltips, font;
    char tab_char=' ';
    std::vector<std::string> extensions;
    std::unordered_map<std::string, std::string> tags, types;
  };  // class Config

  class Range {
  public:
    Range(unsigned start_offset, unsigned end_offset, int kind):
      start_offset(start_offset), end_offset(end_offset), kind(kind) {}
    unsigned start_offset;
    unsigned end_offset;
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
    View(const std::string& file_path, const std::string& project_path);
    std::string get_line(size_t line_number);
    std::string get_line_before_insert();
    std::string file_path;
    std::string project_path;
    Gtk::TextIter search_start, search_end;
  protected:
    bool on_key_press_event(GdkEventKey* key);
  };  // class View
  
  class GenericView : public View {
  public:
    GenericView(const std::string& file_path, const std::string& project_path):
    View(file_path, project_path) {}
  protected:
    bool on_key_press_event(GdkEventKey* key) {
      return Source::View::on_key_press_event(key);
    }
  };
  
  class ClangView : public View {
  public:
    ClangView(const std::string& file_path, const std::string& project_path, Terminal::Controller& terminal);
    ~ClangView();
  protected:
    std::unique_ptr<clang::TranslationUnit> clang_tu;
    std::map<std::string, std::string> get_buffer_map() const;
    std::mutex parsing_mutex;
    sigc::connection delayed_reparse_connection;
    bool on_key_press_event(GdkEventKey* key);
    bool on_focus_out_event(GdkEventFocus* event);
  private:
    // inits the syntax highligthing on file open
    void init_syntax_highlighting(const std::map<std::string, std::string>
                                &buffers,
                                int start_offset,
                                int end_offset);
    int reparse(const std::map<std::string, std::string> &buffers);
    void update_syntax();
    void update_diagnostics();
    void update_types();
    Tooltips diagnostic_tooltips;
    Tooltips type_tooltips;
    bool on_motion_notify_event(GdkEventMotion* event);
    void on_mark_set(const Gtk::TextBuffer::iterator& iterator, const Glib::RefPtr<Gtk::TextBuffer::Mark>& mark);
    sigc::connection delayed_tooltips_connection;
    Glib::RefPtr<Gtk::TextTag> similar_tokens_tag;
    std::string last_similar_token_marked;
    
    bool on_scroll_event(GdkEventScroll* event);
    static clang::Index clang_index;
    std::unique_ptr<clang::Tokens> clang_tokens;
    bool clang_readable=false;
    std::vector<std::string> get_compilation_commands();
    Terminal::Controller& terminal;
        
    std::shared_ptr<Terminal::InProgress> parsing_in_progress;
    Glib::Dispatcher parse_done;
    Glib::Dispatcher parse_start;
    std::thread parse_thread;
    std::map<std::string, std::string> parse_thread_buffer_map;
    std::mutex parse_thread_buffer_map_mutex;
    std::atomic<bool> parse_thread_go;
    std::atomic<bool> parse_thread_mapped;
    std::atomic<bool> parse_thread_stop;
  };
  
  class ClangViewAutocomplete : public ClangView {
  public:
    ClangViewAutocomplete(const std::string& file_path, const std::string& project_path, Terminal::Controller& terminal);
  protected:
    bool on_key_press_event(GdkEventKey* key);
    bool on_focus_out_event(GdkEventFocus* event);
  private:
    void autocomplete();
    SelectionDialog selection_dialog;
    std::vector<Source::AutoCompleteData> get_autocomplete_suggestions(int line_number, int column, std::map<std::string, std::string>& buffer_map);
    Glib::Dispatcher autocomplete_done;
    sigc::connection autocomplete_done_connection;
    bool autocomplete_starting=false;
    std::atomic<bool> autocomplete_cancel_starting;
    guint last_keyval=0;
    std::string prefix;
    std::mutex prefix_mutex;
  };

  class Controller {
  public:
    Controller(const std::string& file_path, std::string project_path, Terminal::Controller& terminal);
    Glib::RefPtr<Gsv::Buffer> buffer();
    
    bool is_saved = true;
    
    std::unique_ptr<Source::View> view;
  };  // class Controller
}  // namespace Source
#endif  // JUCI_SOURCE_H_
