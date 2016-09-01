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
    ClangViewParse(const boost::filesystem::path &file_path, Glib::RefPtr<Gsv::Language> language);
    
    bool save(const std::vector<Source::View*> &views) override;
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
    std::vector<clang::Diagnostic> diagnostics;
    
    static clang::Index clang_index;
    std::vector<std::string> get_compilation_commands();
  };
    
  class ClangViewAutocomplete : public virtual ClangViewParse {
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
  protected:
    std::atomic<AutocompleteState> autocomplete_state;
    std::thread autocomplete_thread;
  private:
    void autocomplete_dialog_setup();
    void autocomplete_check();
    void autocomplete();
    std::unordered_map<std::string, std::pair<std::string, std::string> > autocomplete_dialog_rows;
    std::vector<AutoCompleteData> autocomplete_get_suggestions(const std::string &buffer, int line_number, int column);
    Tooltips autocomplete_tooltips;
    std::string prefix;
    std::mutex prefix_mutex;
    static std::unordered_map<std::string, std::string> autocomplete_manipulators_map();
  };

  class ClangViewRefactor : public virtual ClangViewParse {
    class Identifier {
    public:
      Identifier(clang::Cursor::Kind kind, const std::string &spelling, const std::string &usr, const clang::Cursor &cursor=clang::Cursor()) :
        kind(kind), spelling(spelling), usr(usr), cursor(cursor) {}
      
      Identifier() : kind(static_cast<clang::Cursor::Kind>(0)) {}
      operator bool() const { return static_cast<int>(kind)!=0; }
      bool operator==(const Identifier &rhs) const { return (kind==rhs.kind && spelling==rhs.spelling && usr==rhs.usr); }
      bool operator!=(const Identifier &rhs) const { return !(*this==rhs); }
      bool operator<(const Identifier &rhs) const { return usr<rhs.usr; }
      clang::Cursor::Kind kind;
      std::string spelling;
      std::string usr;
      clang::Cursor cursor;
    };
  public:
    ClangViewRefactor(const boost::filesystem::path &file_path, Glib::RefPtr<Gsv::Language> language);
  protected:
    sigc::connection delayed_tag_similar_identifiers_connection;
  private:
    Identifier get_identifier();
    void wait_parsing(const std::vector<Source::View*> &views);
    
    std::list<std::pair<Glib::RefPtr<Gtk::TextMark>, Glib::RefPtr<Gtk::TextMark> > > similar_identifiers_marks;
    void tag_similar_identifiers(const Identifier &identifier);
    Glib::RefPtr<Gtk::TextTag> similar_identifiers_tag;
    Identifier last_tagged_identifier;
    bool renaming=false;
  };
  
  class ClangView : public ClangViewAutocomplete, public ClangViewRefactor {
  public:
    ClangView(const boost::filesystem::path &file_path, Glib::RefPtr<Gsv::Language> language);
    
    void full_reparse() override;
    void async_delete();
    
  private:
    Glib::Dispatcher do_delete_object;
    std::thread delete_thread;
    std::thread full_reparse_thread;
    bool full_reparse_running=false;
  };
}

#endif  // JUCI_SOURCE_CLANG_H_
