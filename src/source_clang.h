#pragma once
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
    ClangViewParse(const boost::filesystem::path &file_path, const Glib::RefPtr<Gsv::Language> &language);
    
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
    
    void show_type_tooltips(const Gdk::Rectangle &rectangle) override;
    
    std::vector<FixIt> fix_its;
    
    std::thread parse_thread;
    std::mutex parse_mutex;
    std::atomic<ParseState> parse_state;
    std::atomic<ParseProcessState> parse_process_state;
    
    CXCompletionString selected_completion_string=nullptr;
  private:
    Glib::ustring parse_thread_buffer;
    
    static const std::unordered_map<int, std::string> &clang_types();
    void update_syntax();
    std::map<int, Glib::RefPtr<Gtk::TextTag>> syntax_tags;

    void update_diagnostics();
    std::vector<clangmm::Diagnostic> clang_diagnostics;
    
    static clangmm::Index clang_index;
  };
    
  class ClangViewAutocomplete : public virtual ClangViewParse {
  public:
    ClangViewAutocomplete(const boost::filesystem::path &file_path, const Glib::RefPtr<Gsv::Language> &language);
  protected:
    Autocomplete autocomplete;
    std::unique_ptr<clangmm::CodeCompleteResults> code_complete_results;
    std::vector<CXCompletionString> completion_strings;
    sigc::connection delayed_show_arguments_connection;
  private:
    bool is_possible_parameter();
    bool show_arguments;
    const std::unordered_map<std::string, std::string> &autocomplete_manipulators_map();
  };

  class ClangViewRefactor : public virtual ClangViewParse {
    class Identifier {
    public:
      Identifier(std::string spelling_, const clangmm::Cursor &cursor)
          : kind(cursor.get_kind()), spelling(std::move(spelling_)), usr_extended(cursor.get_usr_extended()), cursor(cursor) {}
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
    ClangViewRefactor(const boost::filesystem::path &file_path, const Glib::RefPtr<Gsv::Language> &language);
  private:
    Identifier get_identifier();
    void wait_parsing();
    
    void tag_similar_identifiers(const Identifier &identifier);
    Identifier last_tagged_identifier;
  };
  
  class ClangView : public ClangViewAutocomplete, public ClangViewRefactor {
  public:
    ClangView(const boost::filesystem::path &file_path, const Glib::RefPtr<Gsv::Language> &language);
    
    void full_reparse() override;
    void async_delete();
    
  private:
    Glib::Dispatcher do_delete_object;
    std::thread delete_thread;
    std::thread full_reparse_thread;
    bool full_reparse_running=false;
  };
}
