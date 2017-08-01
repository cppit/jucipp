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
#include "autocomplete.h"

namespace Source {
  class ClangViewParse : public View {
  protected:
    enum class ParseState {PROCESSING, RESTARTING, STOP};
    enum class ParseProcessState {IDLE, STARTING, PREPROCESSING, PROCESSING, POSTPROCESSING};
    
  public:
    ClangViewParse(const boost::filesystem::path &file_path, Glib::RefPtr<Gsv::Language> language);
    
    bool save() override;
    void configure() override;
    
    void soft_reparse(bool delayed=false) override;
  protected:
    Dispatcher dispatcher;
    void parse_initialize();
    std::unique_ptr<clangmm::TranslationUnit> clang_tu;
    std::unique_ptr<clangmm::Tokens> clang_tokens;
    std::vector<std::pair<clangmm::Offset, clangmm::Offset>> clang_tokens_offsets;
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
    std::vector<clangmm::Diagnostic> clang_diagnostics;
    
    static clangmm::Index clang_index;
  };
    
  class ClangViewAutocomplete : public virtual ClangViewParse {
  public:
    class Suggestion {
    public:
      explicit Suggestion(std::vector<clangmm::CompletionChunk> &&chunks) :
        chunks(chunks) { }
      std::vector<clangmm::CompletionChunk> chunks;
      std::string brief_comments;
    };
    
    ClangViewAutocomplete(const boost::filesystem::path &file_path, Glib::RefPtr<Gsv::Language> language);
  protected:
    Autocomplete<Suggestion> autocomplete;
  private:
    const std::unordered_map<std::string, std::string> &autocomplete_manipulators_map();
  };

  class ClangViewRefactor : public virtual ClangViewParse {
    class Identifier {
    public:
      Identifier(const std::string &spelling, const clangmm::Cursor &cursor)
          : kind(cursor.get_kind()), spelling(spelling), usr_extended(cursor.get_usr_extended()), cursor(cursor) {}
      Identifier() : kind(static_cast<clangmm::Cursor::Kind>(0)) {}
      
      operator bool() const { return static_cast<int>(kind)!=0; }
      bool operator==(const Identifier &rhs) const { return spelling==rhs.spelling && usr_extended==rhs.usr_extended; }
      bool operator!=(const Identifier &rhs) const { return !(*this==rhs); }
      bool operator<(const Identifier &rhs) const { return spelling<rhs.spelling || (spelling==rhs.spelling && usr_extended<rhs.usr_extended); }
      clangmm::Cursor::Kind kind;
      std::string spelling;
      std::string usr_extended;
      clangmm::Cursor cursor;
    };
  public:
    ClangViewRefactor(const boost::filesystem::path &file_path, Glib::RefPtr<Gsv::Language> language);
  protected:
    sigc::connection delayed_tag_similar_identifiers_connection;
  private:
    Identifier get_identifier();
    void wait_parsing();
    
    std::list<std::pair<Glib::RefPtr<Gtk::TextMark>, Glib::RefPtr<Gtk::TextMark> > > similar_identifiers_marks;
    void tag_similar_identifiers(const Identifier &identifier);
    Glib::RefPtr<Gtk::TextTag> similar_identifiers_tag;
    Identifier last_tagged_identifier;
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
