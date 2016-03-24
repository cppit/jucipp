#ifndef JUCI_SOURCE_CLANG_H_
#define JUCI_SOURCE_CLANG_H_

#include <thread>
#include <atomic>
#include <mutex>
#include <set>
#include "clangmm.h"
#include "source.h"
#include "terminal.h"
#include "dispatcher.h"

namespace Source {
  class ClangViewParse : public View {
  protected:
    enum class ParseState {PROCESSING, RESTARTING, STOP};
    enum class ParseProcessState {IDLE, STARTING, PREPROCESSING, PROCESSING, POSTPROCESSING};
  public:
    class TokenRange {
    public:
      TokenRange(std::pair<clang::Offset, clang::Offset> offsets, int kind):
        offsets(offsets), kind(kind) {}
      std::pair<clang::Offset, clang::Offset> offsets;
      int kind;
    };
    
    ClangViewParse(const boost::filesystem::path &file_path, Glib::RefPtr<Gsv::Language> language);
    
    void configure() override;
    
    void soft_reparse() override;
  protected:
    Dispatcher dispatcher;
    void parse_initialize();
    std::unique_ptr<clang::TranslationUnit> clang_tu;
    std::unique_ptr<clang::Tokens> clang_tokens;
    sigc::connection delayed_reparse_connection;
    
    std::shared_ptr<Terminal::InProgress> parsing_in_progress;
    
    void show_diagnostic_tooltips(const Gdk::Rectangle &rectangle) override;
    void show_type_tooltips(const Gdk::Rectangle &rectangle) override;
    
    std::set<int> diagnostic_offsets;
    std::vector<FixIt> fix_its;
    
    std::thread parse_thread;
    std::mutex parse_mutex;
    std::atomic<ParseState> parse_state;
    std::atomic<ParseProcessState> parse_process_state;
  private:
    Glib::ustring parse_thread_buffer;
    
    void update_syntax();
    std::set<std::string> last_syntax_tags;
    void update_diagnostics();
    
    static clang::Index clang_index;
    std::vector<std::string> get_compilation_commands();
  };
    
  class ClangViewAutocomplete : public ClangViewParse {
  protected:
    enum class AutocompleteState {IDLE, STARTING, RESTARTING, CANCELED};
  public:
    class AutoCompleteData {
    public:
      explicit AutoCompleteData(const std::vector<clang::CompletionChunk> &chunks) :
        chunks(chunks) { }
      std::vector<clang::CompletionChunk> chunks;
      std::string brief_comments;
    };
    
    ClangViewAutocomplete(const boost::filesystem::path &file_path, Glib::RefPtr<Gsv::Language> language);
    
    virtual void async_delete();
    bool full_reparse() override;
  protected:
    std::thread autocomplete_thread;
  private:
    std::atomic<AutocompleteState> autocomplete_state;
    void autocomplete_dialog_setup();
    void autocomplete_check();
    void autocomplete();
    std::unordered_map<std::string, std::pair<std::string, std::string> > autocomplete_dialog_rows;
    std::vector<AutoCompleteData> autocomplete_get_suggestions(const std::string &buffer, int line_number, int column);
    Tooltips autocomplete_tooltips;
    std::string prefix;
    std::mutex prefix_mutex;
    
    Glib::Dispatcher do_delete_object;
    std::thread delete_thread;
    std::thread full_reparse_thread;
    bool full_reparse_running=false;
  };

  class ClangViewRefactor : public ClangViewAutocomplete {
  public:
    ClangViewRefactor(const boost::filesystem::path &file_path, Glib::RefPtr<Gsv::Language> language);
  protected:
    sigc::connection delayed_tag_similar_tokens_connection;
  private:
    std::list<std::pair<Glib::RefPtr<Gtk::TextMark>, Glib::RefPtr<Gtk::TextMark> > > similar_token_marks;
    void tag_similar_tokens(const Token &token);
    Glib::RefPtr<Gtk::TextTag> similar_tokens_tag;
    Token last_tagged_token;
    bool renaming=false;
  };
  
  class ClangView : public ClangViewRefactor {
  public:
    ClangView(const boost::filesystem::path &file_path, Glib::RefPtr<Gsv::Language> language);
    void async_delete() override;
  };
}

#endif  // JUCI_SOURCE_CLANG_H_
