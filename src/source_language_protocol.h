#pragma once
#include "autocomplete.h"
#include "process.hpp"
#include "source.h"
#include <boost/property_tree/json_parser.hpp>
#include <list>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace Source {
  class LanguageProtocolView;
}

namespace LanguageProtocol {
  class Diagnostic {
  public:
    std::string spelling;
    std::pair<Source::Offset, Source::Offset> offsets;
    unsigned severity;
    std::string uri;
  };

  class Capabilities {
  public:
    enum class TextDocumentSync { NONE = 0,
                                  FULL,
                                  INCREMENTAL };
    TextDocumentSync text_document_sync;
    bool hover;
    bool completion;
    bool definition;
    bool references;
    bool document_highlight;
    bool workspace_symbol;
    bool document_formatting;
    bool document_range_formatting;
    bool rename;
  };

  class Client {
    Client(std::string root_uri, std::string language_id);
    std::string root_uri;
    std::string language_id;

    Capabilities capabilities;

    std::unordered_set<Source::LanguageProtocolView *> views;
    std::mutex views_mutex;
    
    std::mutex initialize_mutex;

    std::unique_ptr<TinyProcessLib::Process> process;
    std::mutex read_write_mutex;

    std::stringstream server_message_stream;
    size_t server_message_size = static_cast<size_t>(-1);
    size_t server_message_content_pos;
    bool header_read = false;

    size_t message_id = 1;

    std::unordered_map<size_t, std::pair<Source::LanguageProtocolView*, std::function<void(const boost::property_tree::ptree &, bool error)>>> handlers;
    std::vector<std::thread> timeout_threads;
    std::mutex timeout_threads_mutex;

  public:
    static std::shared_ptr<Client> get(const boost::filesystem::path &file_path, const std::string &language_id);

    ~Client();

    bool initialized = false;
    Capabilities initialize(Source::LanguageProtocolView *view);
    void close(Source::LanguageProtocolView *view);
    
    void parse_server_message();
    void write_request(Source::LanguageProtocolView *view, const std::string &method, const std::string &params, std::function<void(const boost::property_tree::ptree &, bool)> &&function = nullptr);
    void write_notification(const std::string &method, const std::string &params);
    void handle_server_request(const std::string &method, const boost::property_tree::ptree &params);
  };
} // namespace LanguageProtocol

namespace Source {
  class LanguageProtocolView : public View {
  public:
    LanguageProtocolView(const boost::filesystem::path &file_path, const Glib::RefPtr<Gsv::Language> &language, std::string language_id_);
    ~LanguageProtocolView() override;
    std::string uri;
    
    bool save() override;

    void update_diagnostics(std::vector<LanguageProtocol::Diagnostic> &&diagnostics);
    
    Gtk::TextIter get_iter_at_line_pos(int line, int pos) override;

  protected:
    void show_type_tooltips(const Gdk::Rectangle &rectangle) override;

  private:
    std::string language_id;
    LanguageProtocol::Capabilities capabilities;
    
    std::shared_ptr<LanguageProtocol::Client> client;

    size_t document_version = 1;

    std::thread initialize_thread;
    Dispatcher dispatcher;
    
    void setup_navigation_and_refactoring();

    void escape_text(std::string &text);
    void unescape_text(std::string &text);
    
    void tag_similar_symbols();
    
    Offset get_declaration(const Gtk::TextIter &iter);

    Autocomplete autocomplete;
    void setup_autocomplete();
    std::vector<std::string> autocomplete_comment;
    std::vector<std::string> autocomplete_insert;
    std::list<std::pair<Glib::RefPtr<Gtk::TextBuffer::Mark>, Glib::RefPtr<Gtk::TextBuffer::Mark>>> autocomplete_marks;
    bool autocomplete_keep_marks = false;
    
    boost::filesystem::path flow_coverage_executable;
    std::vector<std::pair<Glib::RefPtr<Gtk::TextMark>, Glib::RefPtr<Gtk::TextMark>>> flow_coverage_marks;
    const std::string flow_coverage_message="Not covered by Flow";
    size_t num_warnings=0, num_errors=0, num_fix_its=0, num_flow_coverage_warnings=0;
    void add_flow_coverage_tooltips(bool called_in_thread);
  };
} // namespace Source
