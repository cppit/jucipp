#include "source_language_protocol.h"
#include "info.h"
#include "selection_dialog.h"
#include "terminal.h"
#include "project.h"
#include <regex>
#include <condition_variable>

const bool output_messages_and_errors=false;

LanguageProtocol::Client::Client(std::string root_uri_, std::string language_id_) : root_uri(std::move(root_uri_)), language_id(std::move(language_id_)) {
  process = std::make_unique<TinyProcessLib::Process>(language_id+"-language-server", "",
                                                      [this](const char *bytes, size_t n) {
    server_message_stream.write(bytes, n);
    parse_server_message();
  }, [](const char *bytes, size_t n) {
    if(output_messages_and_errors)
      Terminal::get().async_print("Error (language server): "+std::string(bytes, n)+'\n', true);
  }, true);
}

std::shared_ptr<LanguageProtocol::Client> LanguageProtocol::Client::get(const boost::filesystem::path &file_path, const std::string &language_id) {
  std::string root_uri;
  auto build=Project::Build::create(file_path);
  if(!build->project_path.empty())
    root_uri=build->project_path.string();
  else
    root_uri=file_path.parent_path().string();
  
  auto cache_id=root_uri+'|'+language_id;
  
  static std::unordered_map<std::string, std::weak_ptr<Client>> cache;
  static std::mutex mutex;
  std::lock_guard<std::mutex> lock(mutex);
  auto it=cache.find(cache_id);
  if(it==cache.end())
    it=cache.emplace(cache_id, std::weak_ptr<Client>()).first;
  auto instance=it->second.lock();
  if(!instance)
    it->second=instance=std::shared_ptr<Client>(new Client(root_uri, language_id));
  return instance;
}

LanguageProtocol::Client::~Client() {
  std::condition_variable cv;
  std::mutex cv_mutex;
  bool cv_notified=false;
  write_request("shutdown", "", [this, &cv, &cv_mutex, &cv_notified](const boost::property_tree::ptree &result, bool error) {
    if(!error)
      this->write_notification("exit", "");
    std::unique_lock<std::mutex> lock(cv_mutex);
    cv_notified=true;
    cv.notify_one();
  });
  
  {
    std::unique_lock<std::mutex> lock(cv_mutex);
    if(!cv_notified)
      cv.wait(lock);
  }
  
  std::unique_lock<std::mutex> lock(timeout_threads_mutex);
  for(auto &thread: timeout_threads)
    thread.join();
  
  int exit_status=-1;
  for(size_t c=0;c<20;++c) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    if(process->try_get_exit_status(exit_status))
      break;
  }
  if(output_messages_and_errors)
    std::cout << "Language server exit status: " << exit_status << std::endl;
  if(exit_status==-1)
    process->kill();
}

LanguageProtocol::Capabilities LanguageProtocol::Client::initialize(Source::LanguageProtocolView *view) {
  {
    std::unique_lock<std::mutex> lock(views_mutex);
    views.emplace(view);
  }
  
  if(initialized)
    return capabilities;
  
  std::condition_variable cv;
  std::mutex cv_mutex;
  bool cv_notified=false;
  write_request("initialize", "\"processId\":"+std::to_string(process->get_id())+",\"rootUri\":\"file://"+root_uri+"\",\"capabilities\":{\"workspace\":{\"didChangeConfiguration\":{\"dynamicRegistration\":true},\"didChangeWatchedFiles\":{\"dynamicRegistration\":true},\"symbol\":{\"dynamicRegistration\":true},\"executeCommand\":{\"dynamicRegistration\":true}},\"textDocument\":{\"synchronization\":{\"dynamicRegistration\":true,\"willSave\":true,\"willSaveWaitUntil\":true,\"didSave\":true},\"completion\":{\"dynamicRegistration\":true,\"completionItem\":{\"snippetSupport\":true}},\"hover\":{\"dynamicRegistration\":true},\"signatureHelp\":{\"dynamicRegistration\":true},\"definition\":{\"dynamicRegistration\":true},\"references\":{\"dynamicRegistration\":true},\"documentHighlight\":{\"dynamicRegistration\":true},\"documentSymbol\":{\"dynamicRegistration\":true},\"codeAction\":{\"dynamicRegistration\":true},\"codeLens\":{\"dynamicRegistration\":true},\"formatting\":{\"dynamicRegistration\":true},\"rangeFormatting\":{\"dynamicRegistration\":true},\"onTypeFormatting\":{\"dynamicRegistration\":true},\"rename\":{\"dynamicRegistration\":true},\"documentLink\":{\"dynamicRegistration\":true}}},\"initializationOptions\":{\"omitInitBuild\":true},\"trace\":\"off\"", [this, &cv, &cv_mutex, &cv_notified](const boost::property_tree::ptree &result, bool error) {
    if(!error) {
      auto capabilities_pt=result.find("capabilities");
      if(capabilities_pt!=result.not_found())
        capabilities.text_document_sync=static_cast<LanguageProtocol::Capabilities::TextDocumentSync>(capabilities_pt->second.get<unsigned>("textDocumentSync", 0));
      write_notification("initialized", "");
    }
    std::unique_lock<std::mutex> lock(cv_mutex);
    cv_notified=true;
    cv.notify_one();
  });
  {
    std::unique_lock<std::mutex> lock(cv_mutex);
    if(!cv_notified)
      cv.wait(lock);
  }
  initialized=true;
  return capabilities;
}

void LanguageProtocol::Client::close(Source::LanguageProtocolView *view) {
  std::unique_lock<std::mutex> lock(views_mutex);
  auto it=views.find(view);
  if(it!=views.end())
    views.erase(it);
}

void LanguageProtocol::Client::parse_server_message() {
  if(!header_read) {
    std::string line;
    while(!header_read && std::getline(server_message_stream, line)) {
      if(!line.empty()) {
        if(line.back()=='\r')
          line.pop_back();
        if(line.compare(0, 16, "Content-Length: ")==0) {
          try {
            server_message_size=static_cast<size_t>(std::stoul(line.substr(16)));
          }
          catch(...) {}
        }
      }
      if(line.empty()) {
        server_message_content_pos=server_message_stream.tellg();
        server_message_size+=server_message_content_pos;
        header_read=true;
      }
    }
  }
  
  if(header_read) {
    server_message_stream.seekg(0, std::ios::end);
    size_t read_size=server_message_stream.tellg();
    std::stringstream tmp;
    if(read_size>=server_message_size) {
      if(read_size>server_message_size) {
        server_message_stream.seekg(server_message_size, std::ios::beg);
        server_message_stream.seekp(server_message_size, std::ios::beg);
        for(size_t c=server_message_size;c<read_size;++c) {
          tmp.put(server_message_stream.get());
          server_message_stream.put(' ');
        }
      }
      
      server_message_stream.seekg(server_message_content_pos, std::ios::beg);
      boost::property_tree::ptree pt;
      boost::property_tree::read_json(server_message_stream, pt);
      
      if(output_messages_and_errors) {
        std::cout << "language server: ";
        boost::property_tree::write_json(std::cout, pt);
      }
      
      auto message_id=pt.get<size_t>("id", 0);
      auto result_it=pt.find("result");
      {
        std::unique_lock<std::mutex> lock(read_write_mutex);
        if(result_it!=pt.not_found()) {
          if(message_id) {
            auto id_it=handlers.find(message_id);
            if(id_it!=handlers.end()) {
              auto function=std::move(id_it->second);
              lock.unlock();
              function(result_it->second, false);
              lock.lock();
              handlers.erase(id_it->first);
            }
          }
        }
        else {
          auto method_it=pt.find("method");
          if(method_it!=pt.not_found()) {
            auto params_it=pt.find("params");
            if(params_it!=pt.not_found()) {
              lock.unlock();
              handle_server_request(method_it->second.get_value<std::string>(""), params_it->second);
              lock.lock();
            }
          }
        }
      }
      
      server_message_stream=std::stringstream();
      header_read=false;
      server_message_size=static_cast<size_t>(-1);
      
      tmp.seekg(0, std::ios::end);
      if(tmp.tellg()>0) {
        tmp.seekg(0, std::ios::beg);
        server_message_stream << tmp.rdbuf();
        parse_server_message();
      }
    }
  }
}

void LanguageProtocol::Client::write_request(const std::string &method, const std::string &params, std::function<void(const boost::property_tree::ptree &, bool error)> &&function) {
  std::unique_lock<std::mutex> lock(read_write_mutex);
  if(function)
    handlers.emplace(message_id, std::move(function));
  {
    std::unique_lock<std::mutex> lock(timeout_threads_mutex);
    timeout_threads.emplace_back([this] {
      for(size_t c=0;c<20;++c) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::unique_lock<std::mutex> lock(read_write_mutex);
        auto id_it=handlers.find(message_id);
        if(id_it==handlers.end())
          return;
      }
      std::unique_lock<std::mutex> lock(read_write_mutex);
      auto id_it=handlers.find(message_id);
      if(id_it!=handlers.end()) {
        auto function=std::move(id_it->second);
        lock.unlock();
        function(boost::property_tree::ptree(), false);
        lock.lock();
        handlers.erase(id_it->first);
      }
    });
  }
  std::string content("{\"jsonrpc\":\"2.0\",\"id\":"+std::to_string(message_id++)+",\"method\":\""+method+"\",\"params\":{"+params+"}}");
  auto message="Content-Length: "+std::to_string(content.size())+"\r\n\r\n"+content;
  if(output_messages_and_errors)
    std::cout << "Language client: " << content << std::endl;
  process->write(message);
}

void LanguageProtocol::Client::write_notification(const std::string &method, const std::string &params) {
  std::unique_lock<std::mutex> lock(read_write_mutex);
  std::string content("{\"jsonrpc\":\"2.0\",\"method\":\""+method+"\",\"params\":{"+params+"}}");
  auto message="Content-Length: "+std::to_string(content.size())+"\r\n\r\n"+content;
  if(output_messages_and_errors)
    std::cout << "Language client: " << content << std::endl;
  process->write(message);
}

void LanguageProtocol::Client::handle_server_request(const std::string &method, const boost::property_tree::ptree &params) {
  if(method=="textDocument/publishDiagnostics") {
    std::vector<Diagnostic> diagnostics;
    auto uri=params.get<std::string>("uri", "");
    if(!uri.empty()) {
      auto diagnostics_pt=params.get_child("diagnostics", boost::property_tree::ptree());
      for(auto it=diagnostics_pt.begin();it!=diagnostics_pt.end();++it) {
        auto range_it=it->second.find("range");
        if(range_it!=it->second.not_found()) {
          auto start_it=range_it->second.find("start");
          auto end_it=range_it->second.find("end");
          if(start_it!=range_it->second.not_found() && start_it!=range_it->second.not_found()) {
            diagnostics.emplace_back(Diagnostic{it->second.get<std::string>("message", ""),
                                                std::make_pair<Source::Offset, Source::Offset>(Source::Offset(start_it->second.get<unsigned>("line", 0), start_it->second.get<unsigned>("character", 0)),
                                                                                               Source::Offset(end_it->second.get<unsigned>("line", 0), end_it->second.get<unsigned>("character", 0))),
                                                it->second.get<unsigned>("severity", 0), uri});
          }
        }
      }
      std::unique_lock<std::mutex> lock(views_mutex);
      for(auto view: views) {
        if(view->uri==uri) {
          view->update_diagnostics(std::move(diagnostics));
          break;
        }
      }
    }
  }
}

Source::LanguageProtocolView::LanguageProtocolView(const boost::filesystem::path &file_path, Glib::RefPtr<Gsv::Language> language, std::string language_id_)
    : Source::View(file_path, language), uri("file://"+file_path.string()), language_id(std::move(language_id_)), client(LanguageProtocol::Client::get(file_path, language_id)), autocomplete(this, interactive_completion, last_keyval, false) {
  configure();
  get_source_buffer()->set_language(language);
  get_source_buffer()->set_highlight_syntax(true);
  parsed=true;
  
  capabilities=client->initialize(this);
  if(language_id=="rust")
    client->write_notification("workspace/didChangeConfiguration", "\"settings\":{\"rust\":{\"sysroot\":null,\"target\":null,\"rustflags\":null,\"clear_env_rust_log\":true,\"build_lib\":null,\"build_bin\":null,\"cfg_test\":false,\"unstable_features\":false,\"wait_to_build\":500,\"show_warnings\":true,\"goto_def_racer_fallback\":false,\"use_crate_blacklist\":true,\"build_on_save\":false,\"workspace_mode\":false,\"analyze_package\":null,\"features\":[],\"all_features\":false,\"no_default_features\":false}}");
  std::string text=get_buffer()->get_text();
  escape_text(text);
  client->write_notification("textDocument/didOpen", "\"textDocument\":{\"uri\":\""+uri+"\",\"languageId\":\""+language_id+"\",\"version\":"+std::to_string(document_version++)+",\"text\":\""+text+"\"}");
  
  format_style=[this](bool) {
    client->write_request("textDocument/formatting", "\"textDocument\":{\"uri\":\""+uri+"\"},\"options\":{\"tabSize\":2,\"insertSpaces\":true}");
  };
  
  // Completion test
  get_declaration_location=[this]() {
    auto iter=get_buffer()->get_insert()->get_iter();
    std::condition_variable cv;
    std::mutex cv_mutex;
    bool cv_notified=false;
    auto offset=std::make_shared<Offset>();
    client->write_request("textDocument/definition", "\"textDocument\":{\"uri\":\""+uri+"\"}, \"position\": {\"line\": "+std::to_string(iter.get_line())+", \"character\": "+std::to_string(iter.get_line_offset())+"}", [&cv, &cv_mutex, &cv_notified, offset](const boost::property_tree::ptree &result, bool error) {
      if(!error) {
        for(auto it=result.begin();it!=result.end();++it) {
          auto uri=it->second.get<std::string>("uri", "");
          if(uri.compare(0, 7, "file://")==0)
            uri.erase(0, 7);
          auto range=it->second.find("range");
          if(range!=it->second.not_found()) {
            auto start=range->second.find("start");
            if(start!=range->second.not_found())
              *offset=Offset(start->second.get<unsigned>("line", 0), start->second.get<unsigned>("character", 0), uri);
          }
          break; // TODO: can a language server return several definitions?
        }
      }
      std::unique_lock<std::mutex> lock(cv_mutex);
      cv_notified=true;
      cv.notify_one();
    });
    {
      std::unique_lock<std::mutex> lock(cv_mutex);
      if(!cv_notified)
        cv.wait(lock);
    }
    if(!*offset)
      Info::get().print("No declaration found");
    return *offset;
  };

  get_buffer()->signal_insert().connect([this](const Gtk::TextBuffer::iterator &start, const Glib::ustring &text_, int bytes) {
    std::string content_changes;
    if(capabilities.text_document_sync==LanguageProtocol::Capabilities::TextDocumentSync::NONE)
      return;
    if(capabilities.text_document_sync==LanguageProtocol::Capabilities::TextDocumentSync::INCREMENTAL) {
      std::string text=text_;
      escape_text(text);
      content_changes="{\"range\":{\"start\":{\"line\": "+std::to_string(start.get_line())+",\"character\":"+std::to_string(start.get_line_offset())+"},\"end\":{\"line\":"+std::to_string(start.get_line())+",\"character\":"+std::to_string(start.get_line_offset())+"}},\"text\":\""+text+"\"}";
    }
    else {
      std::string text=get_buffer()->get_text();
      escape_text(text);
      content_changes="{\"text\":\""+text+"\"}";
    }
    client->write_notification("textDocument/didChange", "\"textDocument\":{\"uri\":\""+uri+"\",\"version\":"+std::to_string(document_version++)+"},\"contentChanges\":["+content_changes+"]");
  }, false);

  get_buffer()->signal_erase().connect([this](const Gtk::TextBuffer::iterator &start, const Gtk::TextBuffer::iterator &end) {
    std::string content_changes;
    if(capabilities.text_document_sync==LanguageProtocol::Capabilities::TextDocumentSync::NONE)
      return;
    if(capabilities.text_document_sync==LanguageProtocol::Capabilities::TextDocumentSync::INCREMENTAL)
      content_changes="{\"range\":{\"start\":{\"line\": "+std::to_string(start.get_line())+",\"character\":"+std::to_string(start.get_line_offset())+"},\"end\":{\"line\":"+std::to_string(end.get_line())+",\"character\":"+std::to_string(end.get_line_offset())+"}},\"text\":\"\"}";
    else {
      std::string text=get_buffer()->get_text();
      escape_text(text);
      content_changes="{\"text\":\""+text+"\"}";
    }
    client->write_notification("textDocument/didChange", "\"textDocument\":{\"uri\":\""+uri+"\",\"version\":"+std::to_string(document_version++)+"},\"contentChanges\":["+content_changes+"]");
  }, false);
  
  goto_next_diagnostic=[this]() {
    auto insert_offset=get_buffer()->get_insert()->get_iter().get_offset();
    for(auto offset: diagnostic_offsets) {
      if(offset>insert_offset) {
        get_buffer()->place_cursor(get_buffer()->get_iter_at_offset(offset));
        scroll_to(get_buffer()->get_insert(), 0.0, 1.0, 0.5);
        return;
      }
    }
    if(diagnostic_offsets.size()==0)
      Info::get().print("No diagnostics found in current buffer");
    else {
      auto iter=get_buffer()->get_iter_at_offset(*diagnostic_offsets.begin());
      get_buffer()->place_cursor(iter);
      scroll_to(get_buffer()->get_insert(), 0.0, 1.0, 0.5);
    }
  };
  
  setup_autocomplete();
}

Source::LanguageProtocolView::~LanguageProtocolView() {
  autocomplete.state=Autocomplete::State::IDLE;
  if(autocomplete.thread.joinable())
    autocomplete.thread.join();
  
  client->write_notification("textDocument/didClose", "\"textDocument\":{\"uri\":\""+uri+"\"}");
  client->close(this);
  
  client=nullptr;
}

void Source::LanguageProtocolView::escape_text(std::string &text) {
  for(size_t c=0;c<text.size();++c) {
    if(text[c]=='\n') {
      text.replace(c, 1, "\\n");
      ++c;
    }
    else if(text[c]=='\r') {
      text.replace(c, 1, "\\r");
      ++c;
    }
    else if(text[c]=='\"') {
      text.replace(c, 1, "\\\"");
      ++c;
    }
  }
}

void Source::LanguageProtocolView::update_diagnostics(std::vector<LanguageProtocol::Diagnostic> &&diagnostics) {
  dispatcher.post([this, diagnostics=std::move(diagnostics)] {
    diagnostic_offsets.clear();
    diagnostic_tooltips.clear();
    get_buffer()->remove_tag_by_name("def:warning_underline", get_buffer()->begin(), get_buffer()->end());
    get_buffer()->remove_tag_by_name("def:error_underline", get_buffer()->begin(), get_buffer()->end());
    size_t num_warnings=0;
    size_t num_errors=0;
    size_t num_fix_its=0;
    for(auto &diagnostic: diagnostics) {
      if(diagnostic.uri==uri) {      
        int line=diagnostic.offsets.first.line;
        if(line<0 || line>=get_buffer()->get_line_count())
          line=get_buffer()->get_line_count()-1;
        auto start=get_iter_at_line_end(line);
        int index=diagnostic.offsets.first.index;
        if(index>=0 && index<start.get_line_offset())
          start=get_buffer()->get_iter_at_line_offset(line, index);
        if(start.ends_line()) {
          while(!start.is_start() && start.ends_line())
            start.backward_char();
        }
        diagnostic_offsets.emplace(start.get_offset());
        
        line=diagnostic.offsets.second.line;
        if(line<0 || line>=get_buffer()->get_line_count())
          line=get_buffer()->get_line_count();
        auto end=get_iter_at_line_end(line);
        index=diagnostic.offsets.second.index;
        if(index>=0 && index<end.get_line_offset())
          end=get_buffer()->get_iter_at_line_offset(line, index);
        
        if(start==end) {
          if(!end.is_end())
            end.forward_char();
          else
            start.forward_char();
        }
        
        std::string diagnostic_tag_name;
        std::string severity_spelling;
        if(diagnostic.severity>=2) {
          severity_spelling="Warning";
          diagnostic_tag_name="def:warning";
          num_warnings++;
        }
        else {
          severity_spelling="Error";
          diagnostic_tag_name="def:error";
          num_errors++;
        }
        
        auto spelling=diagnostic.spelling;
        
        auto create_tooltip_buffer=[this, spelling, severity_spelling, diagnostic_tag_name]() {
          auto tooltip_buffer=Gtk::TextBuffer::create(get_buffer()->get_tag_table());
          tooltip_buffer->insert_with_tag(tooltip_buffer->get_insert()->get_iter(), severity_spelling, diagnostic_tag_name);
          tooltip_buffer->insert_with_tag(tooltip_buffer->get_insert()->get_iter(), ":\n"+spelling, "def:note");
          return tooltip_buffer;
        };
        diagnostic_tooltips.emplace_back(create_tooltip_buffer, this, get_buffer()->create_mark(start), get_buffer()->create_mark(end));
      
        get_buffer()->apply_tag_by_name(diagnostic_tag_name+"_underline", start, end);
        auto iter=get_buffer()->get_insert()->get_iter();
        if(iter.ends_line()) {
          auto next_iter=iter;
          if(next_iter.forward_char())
            get_buffer()->remove_tag_by_name(diagnostic_tag_name+"_underline", iter, next_iter);
        }
      }
    }
    status_diagnostics=std::make_tuple(num_warnings, num_errors, num_fix_its);
    if(update_status_diagnostics)
      update_status_diagnostics(this);
  });
}

void Source::LanguageProtocolView::show_diagnostic_tooltips(const Gdk::Rectangle &rectangle) {
  diagnostic_tooltips.show(rectangle);
}

void Source::LanguageProtocolView::show_type_tooltips(const Gdk::Rectangle &rectangle) {
  Gtk::TextIter iter;
  int location_x, location_y;
  window_to_buffer_coords(Gtk::TextWindowType::TEXT_WINDOW_TEXT, rectangle.get_x(), rectangle.get_y(), location_x, location_y);
  location_x += (rectangle.get_width() - 1) / 2;
  get_iter_at_location(iter, location_x, location_y);
  Gdk::Rectangle iter_rectangle;
  get_iter_location(iter, iter_rectangle);
  if(iter.ends_line() && location_x > iter_rectangle.get_x())
    return;
  
  auto offset=iter.get_offset();
  client->write_request("textDocument/hover", "\"textDocument\": {\"uri\":\"file://"+file_path.string()+"\"}, \"position\": {\"line\": "+std::to_string(iter.get_line())+", \"character\": "+std::to_string(iter.get_line_offset())+"}", [this, offset](const boost::property_tree::ptree &result, bool error) {
    if(!error) {
      auto contents=result.get_child("contents", boost::property_tree::ptree());
      auto it=contents.begin();
      if(it!=contents.end()) {
        auto value=it->second.get<std::string>("value", "");
        if(!value.empty()) {
          dispatcher.post([this, offset, value=std::move(value)] {
            if(offset>=get_buffer()->get_char_count())
              return;
            type_tooltips.clear();
            auto create_tooltip_buffer=[this, value=std::move(value)]() {
              auto tooltip_buffer=Gtk::TextBuffer::create(get_buffer()->get_tag_table());
              tooltip_buffer->insert_with_tag(tooltip_buffer->get_insert()->get_iter(), "Type: "+value, "def:note");
              
              return tooltip_buffer;
            };
            
            auto iter=get_buffer()->get_iter_at_offset(offset);
            auto start=iter;
            auto end=iter;
            while(((*start>='A' && *start<='Z') || (*start>='a' && *start<='z') || (*start>='0' && *start<='9') || *start=='_') && start.backward_char()) {}
            start.forward_char();
            while(((*end>='A' && *end<='Z') || (*end>='a' && *end<='z') || (*end>='0' && *end<='9') || *end=='_') && end.forward_char()) {}
            type_tooltips.emplace_back(create_tooltip_buffer, this, get_buffer()->create_mark(start), get_buffer()->create_mark(end));
            type_tooltips.show();
          });
        }
      }
    }
  });
}

void Source::LanguageProtocolView::setup_autocomplete() {
  non_interactive_completion=[this] {
    if(CompletionDialog::get() && CompletionDialog::get()->is_visible())
      return;
    autocomplete.run();
  };
  
  autocomplete.reparse=[this] {
    autocomplete_comment.clear();
    autocomplete_insert.clear();
  };
  
  autocomplete.is_continue_key=[](guint keyval) {
    if((keyval>='0' && keyval<='9') || (keyval>='a' && keyval<='z') || (keyval>='A' && keyval<='Z') || keyval=='_')
      return true;
    
    return false;
  };
  
  autocomplete.is_restart_key=[this](guint keyval) {
    auto iter=get_buffer()->get_insert()->get_iter();
    iter.backward_chars(2);
    if(keyval=='.' || (keyval==':' && *iter==':'))
      return true;
    return false;
  };
  
  autocomplete.run_check=[this]() {
    auto iter=get_buffer()->get_insert()->get_iter();
    iter.backward_char();
    if(!is_code_iter(iter))
      return false;
    
    std::string line=" "+get_line_before();
    const static std::regex dot_or_arrow("^.*[a-zA-Z0-9_\\)\\]\\>](\\.)([a-zA-Z0-9_]*)$");
    const static std::regex colon_colon("^.*::([a-zA-Z0-9_]*)$");
    const static std::regex part_of_symbol("^.*[^a-zA-Z0-9_]+([a-zA-Z0-9_]{3,})$");
    std::smatch sm;
    if(std::regex_match(line, sm, dot_or_arrow)) {
      {
        std::unique_lock<std::mutex> lock(autocomplete.prefix_mutex);
        autocomplete.prefix=sm[2].str();
      }
      if(autocomplete.prefix.size()==0 || autocomplete.prefix[0]<'0' || autocomplete.prefix[0]>'9')
        return true;
    }
    else if(std::regex_match(line, sm, colon_colon)) {
      {
        std::unique_lock<std::mutex> lock(autocomplete.prefix_mutex);
        autocomplete.prefix=sm[1].str();
      }
      if(autocomplete.prefix.size()==0 || autocomplete.prefix[0]<'0' || autocomplete.prefix[0]>'9')
        return true;
    }
    else if(std::regex_match(line, sm, part_of_symbol)) {
      {
        std::unique_lock<std::mutex> lock(autocomplete.prefix_mutex);
        autocomplete.prefix=sm[1].str();
      }
      if(autocomplete.prefix.size()==0 || autocomplete.prefix[0]<'0' || autocomplete.prefix[0]>'9')
        return true;
    }
    else if(!interactive_completion) {
      auto end_iter=get_buffer()->get_insert()->get_iter();
      auto iter=end_iter;
      while(iter.backward_char() && autocomplete.is_continue_key(*iter)) {}
      if(iter!=end_iter)
        iter.forward_char();
      std::unique_lock<std::mutex> lock(autocomplete.prefix_mutex);
      autocomplete.prefix=get_buffer()->get_text(iter, end_iter);
      return true;
    }
    
    return false;
  };
  
  autocomplete.before_add_rows=[this] {
    status_state="autocomplete...";
    if(update_status_state)
      update_status_state(this);
  };
  
  autocomplete.after_add_rows=[this] {
    status_state="";
    if(update_status_state)
      update_status_state(this);
  };
  
  autocomplete.on_add_rows_error=[this] {
    autocomplete_comment.clear();
    autocomplete_insert.clear();
  };
  
  autocomplete.add_rows=[this](std::string &buffer, int line_number, int column) {
    if(autocomplete.state==Autocomplete::State::STARTING) {
      autocomplete_comment.clear();
      autocomplete_insert.clear();
      std::condition_variable cv;
      std::mutex cv_mutex;
      bool cv_notified=false;
      client->write_request("textDocument/completion", "\"textDocument\":{\"uri\":\""+uri+"\"}, \"position\": {\"line\": "+std::to_string(line_number-1)+", \"character\": "+std::to_string(column-1)+"}", [this, &cv, &cv_mutex, &cv_notified](const boost::property_tree::ptree &result, bool error) {
        if(!error) {
          auto begin=result.begin(); // rust language server is bugged
          auto end=result.end();
          auto items_it=result.find("items"); // correct
          if(items_it!=result.not_found()) {
            begin=items_it->second.begin();
            end=items_it->second.end();
          }
          for(auto it=begin;it!=end;++it) {
            auto label=it->second.get<std::string>("label", "");
            auto detail=it->second.get<std::string>("detail", "");
            auto insert=it->second.get<std::string>("insertText", label);
            if(!label.empty()) {
              autocomplete.rows.emplace_back(std::move(label));
              autocomplete_comment.emplace_back(std::move(detail));
              autocomplete_insert.emplace_back(std::move(insert));
            }
          }
        }
        std::unique_lock<std::mutex> lock(cv_mutex);
        cv_notified=true;
        cv.notify_one();
      });
      std::unique_lock<std::mutex> lock(cv_mutex);
      if(!cv_notified)
        cv.wait(lock);
    }
  };
  
  signal_key_press_event().connect([this](GdkEventKey *event) {
    if((event->keyval==GDK_KEY_Tab || event->keyval==GDK_KEY_ISO_Left_Tab) && (event->state&GDK_SHIFT_MASK)==0) {
      if(!autocomplete_marks.empty()) {
        auto it=autocomplete_marks.begin();
        auto start=it->first->get_iter();
        auto end=it->second->get_iter();
        if(start==end)
          return false;
        autocomplete_keep_marks=true;
        get_buffer()->select_range(it->first->get_iter(), it->second->get_iter());
        autocomplete_keep_marks=false;
        get_buffer()->delete_mark(it->first);
        get_buffer()->delete_mark(it->second);
        autocomplete_marks.erase(it);
        return true;
      }
    }
    return false;
  }, false);
  
  get_buffer()->signal_mark_set().connect([this](const Gtk::TextBuffer::iterator &iterator, const Glib::RefPtr<Gtk::TextBuffer::Mark> &mark) {
    if(mark->get_name() == "insert") {
      if(!autocomplete_keep_marks) {
        for(auto &pair: autocomplete_marks) {
          get_buffer()->delete_mark(pair.first);
          get_buffer()->delete_mark(pair.second);
        }
        autocomplete_marks.clear();
      }
    }
  });
  
  autocomplete.on_show = [this] {
    for(auto &pair: autocomplete_marks) {
      get_buffer()->delete_mark(pair.first);
      get_buffer()->delete_mark(pair.second);
    }
    autocomplete_marks.clear();
    hide_tooltips();
  };
  
  autocomplete.on_hide = [this] {
    autocomplete_comment.clear();
    autocomplete_insert.clear();
  };
  
  autocomplete.on_select = [this](unsigned int index, const std::string &text, bool hide_window) {
    get_buffer()->erase(CompletionDialog::get()->start_mark->get_iter(), get_buffer()->get_insert()->get_iter());
    if(hide_window) {
      Glib::ustring insert=autocomplete_insert[index];
      size_t pos1=0;
      std::vector<std::pair<size_t, size_t>> mark_offsets;
      while((pos1=insert.find("${"), pos1)!=Glib::ustring::npos) {
        size_t pos2=insert.find(":", pos1+2);
        if(pos2!=Glib::ustring::npos) {
          size_t pos3=insert.find("}", pos2+1);
          if(pos3!=Glib::ustring::npos) {
            size_t length=pos3-pos2-1;
            insert.erase(pos3, 1);
            insert.erase(pos1, pos2-pos1+1);
            mark_offsets.emplace_back(pos1, pos1+length);
            pos1+=length;
          }
          else
            break;
        }
        else
          break;
      }
      get_buffer()->insert(CompletionDialog::get()->start_mark->get_iter(), insert);
      for(auto &offset: mark_offsets) {
        auto start=CompletionDialog::get()->start_mark->get_iter();
        auto end=start;
        start.forward_chars(offset.first);
        end.forward_chars(offset.second);
        autocomplete_marks.emplace_back(get_buffer()->create_mark(start), get_buffer()->create_mark(end));
      }
      if(!autocomplete_marks.empty()) {
        auto it=autocomplete_marks.begin();
        autocomplete_keep_marks=true;
        get_buffer()->select_range(it->first->get_iter(), it->second->get_iter());
        autocomplete_keep_marks=false;
        get_buffer()->delete_mark(it->first);
        get_buffer()->delete_mark(it->second);
        autocomplete_marks.erase(it);
      }
    }
    else
      get_buffer()->insert(CompletionDialog::get()->start_mark->get_iter(), text);
  };
  
  autocomplete.get_tooltip = [this](unsigned int index) {
    return autocomplete_comment[index];
  };
}
