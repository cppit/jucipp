#include "source_language_protocol.h"
#include "info.h"
#include "selection_dialog.h"
#include "terminal.h"
#include "project.h"
#include "filesystem.h"
#include "debug_lldb.h"
#include <regex>
#include <future>

const bool output_messages_and_errors=false;

LanguageProtocol::Client::Client(std::string root_uri_, std::string language_id_) : root_uri(std::move(root_uri_)), language_id(std::move(language_id_)) {
  process = std::make_unique<TinyProcessLib::Process>(language_id+"-language-server", "",
                                                      [this](const char *bytes, size_t n) {
    server_message_stream.write(bytes, n);
    parse_server_message();
  }, [](const char *bytes, size_t n) {
    std::cerr.write(bytes, n);
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
  std::promise<void> result_processed;
  write_request("shutdown", "", [this, &result_processed](const boost::property_tree::ptree &result, bool error) {
    if(!error)
      this->write_notification("exit", "");
    result_processed.set_value();
  });
  result_processed.get_future().get();
  
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
  
  std::promise<void> result_processed;
  write_request("initialize", "\"processId\":"+std::to_string(process->get_id())+",\"rootUri\":\"file://"+root_uri+"\",\"capabilities\":{\"workspace\":{\"didChangeConfiguration\":{\"dynamicRegistration\":true},\"didChangeWatchedFiles\":{\"dynamicRegistration\":true},\"symbol\":{\"dynamicRegistration\":true},\"executeCommand\":{\"dynamicRegistration\":true}},\"textDocument\":{\"synchronization\":{\"dynamicRegistration\":true,\"willSave\":true,\"willSaveWaitUntil\":true,\"didSave\":true},\"completion\":{\"dynamicRegistration\":true,\"completionItem\":{\"snippetSupport\":true}},\"hover\":{\"dynamicRegistration\":true},\"signatureHelp\":{\"dynamicRegistration\":true},\"definition\":{\"dynamicRegistration\":true},\"references\":{\"dynamicRegistration\":true},\"documentHighlight\":{\"dynamicRegistration\":true},\"documentSymbol\":{\"dynamicRegistration\":true},\"codeAction\":{\"dynamicRegistration\":true},\"codeLens\":{\"dynamicRegistration\":true},\"formatting\":{\"dynamicRegistration\":true},\"rangeFormatting\":{\"dynamicRegistration\":true},\"onTypeFormatting\":{\"dynamicRegistration\":true},\"rename\":{\"dynamicRegistration\":true},\"documentLink\":{\"dynamicRegistration\":true}}},\"initializationOptions\":{\"omitInitBuild\":true},\"trace\":\"off\"", [this, &result_processed](const boost::property_tree::ptree &result, bool error) {
    if(!error) {
      auto capabilities_pt=result.find("capabilities");
      if(capabilities_pt!=result.not_found())
        capabilities.text_document_sync=static_cast<LanguageProtocol::Capabilities::TextDocumentSync>(capabilities_pt->second.get<unsigned>("textDocumentSync", 0));
      write_notification("initialized", "");
    }
    result_processed.set_value();
  });
  result_processed.get_future().get();
  
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
      auto error_it=pt.find("error");
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
        else if(error_it!=pt.not_found()) {
          if(!output_messages_and_errors)
            boost::property_tree::write_json(std::cerr, pt);
          if(message_id) {
            auto id_it=handlers.find(message_id);
            if(id_it!=handlers.end()) {
              auto function=std::move(id_it->second);
              lock.unlock();
              function(result_it->second, true);
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
    auto message_id=this->message_id;
    std::unique_lock<std::mutex> lock(timeout_threads_mutex);
    timeout_threads.emplace_back([this, message_id] {
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
  if(!process->write(message)) {
    auto id_it=handlers.find(message_id);
    if(id_it!=handlers.end()) {
      auto function=std::move(id_it->second);
      lock.unlock();
      function(boost::property_tree::ptree(), false);
      lock.lock();
      handlers.erase(id_it->first);
    }
  }
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
  
  similar_symbol_tag=get_buffer()->create_tag();
  similar_symbol_tag->property_weight()=Pango::WEIGHT_ULTRAHEAVY;
  
  capabilities=client->initialize(this);
  if(language_id=="rust")
    client->write_notification("workspace/didChangeConfiguration", "\"settings\":{\"rust\":{\"sysroot\":null,\"target\":null,\"rustflags\":null,\"clear_env_rust_log\":true,\"build_lib\":null,\"build_bin\":null,\"cfg_test\":false,\"unstable_features\":false,\"wait_to_build\":500,\"show_warnings\":true,\"goto_def_racer_fallback\":false,\"use_crate_blacklist\":true,\"build_on_save\":false,\"workspace_mode\":false,\"analyze_package\":null,\"features\":[],\"all_features\":false,\"no_default_features\":false}}");
  std::string text=get_buffer()->get_text();
  escape_text(text);
  client->write_notification("textDocument/didOpen", "\"textDocument\":{\"uri\":\""+uri+"\",\"languageId\":\""+language_id+"\",\"version\":"+std::to_string(document_version++)+",\"text\":\""+text+"\"}");
  
  get_buffer()->signal_changed().connect([this] {
    get_buffer()->remove_tag(similar_symbol_tag, get_buffer()->begin(), get_buffer()->end());
  });
  
  get_buffer()->signal_mark_set().connect([this](const Gtk::TextBuffer::iterator &iterator, const Glib::RefPtr<Gtk::TextBuffer::Mark> &mark) {
    if(mark->get_name() == "insert") {
      delayed_tag_similar_symbols_connection.disconnect();
      delayed_tag_similar_symbols_connection=Glib::signal_timeout().connect([this]() {
        tag_similar_symbols();
        return false;
      }, 200);
    }
  });

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
  
  setup_navigation_and_refactoring();
  setup_autocomplete();
}

Source::LanguageProtocolView::~LanguageProtocolView() {
  delayed_tag_similar_symbols_connection.disconnect();
  
  autocomplete.state=Autocomplete::State::IDLE;
  if(autocomplete.thread.joinable())
    autocomplete.thread.join();
  
  client->write_notification("textDocument/didClose", "\"textDocument\":{\"uri\":\""+uri+"\"}");
  client->close(this);
  
  client=nullptr;
}

void Source::LanguageProtocolView::setup_navigation_and_refactoring() {
  format_style=[this](bool continue_without_style_file) {
    if(!continue_without_style_file)
      return;
    
    class Replace {
    public:
      Offset start, end;
      std::string text;
    };
    std::vector<Replace> replaces;
    std::promise<void> result_processed;
    client->write_request("textDocument/formatting", "\"textDocument\":{\"uri\":\""+uri+"\"},\"options\":{\"tabSize\":2,\"insertSpaces\":true}", [&replaces, &result_processed](const boost::property_tree::ptree &result, bool error) {
      if(!error) {
        for(auto it=result.begin();it!=result.end();++it) {
          auto range_it=it->second.find("range");
          auto text_it=it->second.find("newText");
          if(range_it!=it->second.not_found() && text_it!=it->second.not_found()) {
            auto start_it=range_it->second.find("start");
            auto end_it=range_it->second.find("end");
            if(start_it!=range_it->second.not_found() && end_it!=range_it->second.not_found()) {
              try {
                replaces.emplace_back(Replace{Offset(start_it->second.get<unsigned>("line"), start_it->second.get<unsigned>("character")),
                                              Offset(end_it->second.get<unsigned>("line"), end_it->second.get<unsigned>("character")),
                                              text_it->second.get_value<std::string>()});
              }
              catch(...) {}
            }
          }
        }
      }
      result_processed.set_value();
    });
    result_processed.get_future().get();
    
    get_buffer()->begin_user_action();
    for(auto it=replaces.rbegin();it!=replaces.rend();++it) {
      auto start=get_iter_at_line_pos(it->start.line, it->start.index);
      auto end=get_iter_at_line_pos(it->end.line, it->end.index);
      get_buffer()->erase(start, end);
      start=get_iter_at_line_pos(it->start.line, it->start.index);
      unescape_text(it->text);
      get_buffer()->insert(start, it->text);
    }
    get_buffer()->end_user_action();
  };
  
  get_declaration_location=[this]() {
    auto offset=get_declaration(get_buffer()->get_insert()->get_iter());
    if(!offset)
      Info::get().print("No declaration found");
    return offset;
  };
  
  get_usages=[this] {
    auto iter=get_buffer()->get_insert()->get_iter();
    std::vector<std::pair<Offset, std::string>> usages;
    std::vector<Offset> end_offsets;
    std::promise<void> result_processed;
    client->write_request("textDocument/references", "\"textDocument\":{\"uri\":\""+uri+"\"}, \"position\": {\"line\": "+std::to_string(iter.get_line())+", \"character\": "+std::to_string(iter.get_line_offset())+"}, \"context\": {\"includeDeclaration\": true}", [&usages, &end_offsets, &result_processed](const boost::property_tree::ptree &result, bool error) {
      if(!error) {
        try {
          for(auto it=result.begin();it!=result.end();++it) {
            auto path=it->second.get<std::string>("uri", "");
            if(path.size()>=7) {
              path.erase(0, 7);
              auto range_it=it->second.find("range");
              if(range_it!=it->second.not_found()) {
                auto start_it=range_it->second.find("start");
                auto end_it=range_it->second.find("end");
                if(start_it!=range_it->second.not_found() && end_it!=range_it->second.not_found()) {
                  usages.emplace_back(std::make_pair(Offset(start_it->second.get<unsigned>("line"), start_it->second.get<unsigned>("character"), path), ""));
                  end_offsets.emplace_back(end_it->second.get<unsigned>("line"), end_it->second.get<unsigned>("character"));
                }
              }
            }
          }
        }
        catch(...) {
          usages.clear();
          end_offsets.clear();
        }
      }
      result_processed.set_value();
    });
    result_processed.get_future().get();
    
    auto embolden_token=[](std::string &line_, unsigned token_start_pos, unsigned token_end_pos) {
      Glib::ustring line=line_;
      if(token_start_pos>=line.size() || token_end_pos>=line.size())
        return;
  
      //markup token as bold
      size_t pos=0;
      while((pos=line.find('&', pos))!=Glib::ustring::npos) {
        size_t pos2=line.find(';', pos+2);
        if(token_start_pos>pos) {
          token_start_pos+=pos2-pos;
          token_end_pos+=pos2-pos;
        }
        else if(token_end_pos>pos)
          token_end_pos+=pos2-pos;
        else
          break;
        pos=pos2+1;
      }
      line.insert(token_end_pos, "</b>");
      line.insert(token_start_pos, "<b>");
      
      size_t start_pos=0;
      while(start_pos<line.size() && (line[start_pos]==' ' || line[start_pos]=='\t'))
        ++start_pos;
      if(start_pos>0)
        line.erase(0, start_pos);
      
      line_=line.raw();
    };
    
    std::map<boost::filesystem::path, std::vector<std::string>> file_lines;
    size_t c=static_cast<size_t>(-1);
    for(auto &usage: usages) {
      ++c;
      auto view_it=views.end();
      for(auto it=views.begin();it!=views.end();++it) {
        if(usage.first.file_path==(*it)->file_path) {
          view_it=it;
          break;
        }
      }
      if(view_it!=views.end()) {
        if(usage.first.line<static_cast<unsigned>((*view_it)->get_buffer()->get_line_count())) {
          auto start=(*view_it)->get_buffer()->get_iter_at_line(usage.first.line);
          auto end=start;
          end.forward_to_line_end();
          usage.second=(*view_it)->get_buffer()->get_text(start, end);
          embolden_token(usage.second, usage.first.index, end_offsets[c].index);
        }
      }
      else {
        auto it=file_lines.find(usage.first.file_path);
        if(it==file_lines.end()) {
          std::ifstream ifs(usage.first.file_path.string());
          if(ifs) {
            std::vector<std::string> lines;
            std::string line;
            while(std::getline(ifs, line)) {
              if(!line.empty() && line.back()=='\r')
                line.pop_back();
              lines.emplace_back(line);
            }
            auto pair=file_lines.emplace(usage.first.file_path, lines);
            it=pair.first;
          }
          else {
            auto pair=file_lines.emplace(usage.first.file_path, std::vector<std::string>());
            it=pair.first;
          }
        }
        
        if(usage.first.line<it->second.size()) {
          usage.second=it->second[usage.first.line];
          embolden_token(usage.second, usage.first.index, end_offsets[c].index);
        }
      }
    }
    
    if(usages.empty())
      Info::get().print("No symbol found at current cursor location");
    
    return usages;
  };
  
  get_token_spelling=[this]() -> std::string {
    auto start=get_buffer()->get_insert()->get_iter();
    auto end=start;
    auto previous=start;
    while(previous.backward_char() && ((*previous>='A' && *previous<='Z') || (*previous>='a' && *previous<='z') || (*previous>='0' && *previous<='9') || *previous=='_') && start.backward_char()) {}
    while(((*end>='A' && *end<='Z') || (*end>='a' && *end<='z') || (*end>='0' && *end<='9') || *end=='_') && end.forward_char()) {}
    
    if(start==end) {
      Info::get().print("No valid symbol found at current cursor location");
      return "";
    }
    
    return get_buffer()->get_text(start, end);
  };
  
  rename_similar_tokens=[this](const std::string &text) {
    class Usages {
    public:
      boost::filesystem::path path;
      std::vector<std::pair<Offset, Offset>> offsets;
    };
    
    auto previous_text=get_token_spelling();
    if(previous_text.empty())
      return;
    
    auto iter=get_buffer()->get_insert()->get_iter();
    std::vector<Usages> usages;
    std::promise<void> result_processed;
    client->write_request("textDocument/rename", "\"textDocument\":{\"uri\":\""+uri+"\"}, \"position\": {\"line\": "+std::to_string(iter.get_line())+", \"character\": "+std::to_string(iter.get_line_offset())+"}, \"newName\": \""+text+"\"", [&usages, &result_processed](const boost::property_tree::ptree &result, bool error) {
      if(!error) {
        try {
          auto changes_it=result.find("changes");
          if(changes_it!=result.not_found()) {
            for(auto file_it=changes_it->second.begin();file_it!=changes_it->second.end();++file_it) {
              auto path=file_it->first;
              if(path.size()>=7) {
                path.erase(0, 7);
                usages.emplace_back(Usages{path, std::vector<std::pair<Offset, Offset>>()});
                for(auto edit_it=file_it->second.begin();edit_it!=file_it->second.end();++edit_it) {
                  auto range_it=edit_it->second.find("range");
                  if(range_it!=edit_it->second.not_found()) {
                    auto start_it=range_it->second.find("start");
                    auto end_it=range_it->second.find("end");
                    if(start_it!=range_it->second.not_found() && end_it!=range_it->second.not_found())
                      usages.back().offsets.emplace_back(std::make_pair(Offset(start_it->second.get<unsigned>("line"), start_it->second.get<unsigned>("character")),
                                                                        Offset(end_it->second.get<unsigned>("line"), end_it->second.get<unsigned>("character"))));
                  }
                }
              }
            }
          }
        }
        catch(...) {
          usages.clear();
        }
      }
      result_processed.set_value();
    });
    result_processed.get_future().get();
    
    std::vector<Usages*> usages_renamed;
    for(auto &usage: usages) {
      auto view_it=views.end();
      for(auto it=views.begin();it!=views.end();++it) {
        if((*it)->file_path==usage.path) {
          view_it=it;
          break;
        }
      }
      if(view_it!=views.end()) {
        (*view_it)->get_buffer()->begin_user_action();
        for(auto offset_it=usage.offsets.rbegin();offset_it!=usage.offsets.rend();++offset_it) {
          auto start_iter=(*view_it)->get_iter_at_line_pos(offset_it->first.line, offset_it->first.index);
          auto end_iter=(*view_it)->get_iter_at_line_pos(offset_it->second.line, offset_it->second.index);
          (*view_it)->get_buffer()->erase(start_iter, end_iter);
          start_iter=(*view_it)->get_iter_at_line_pos(offset_it->first.line, offset_it->first.index);
          (*view_it)->get_buffer()->insert(start_iter, text);
        }
        (*view_it)->get_buffer()->end_user_action();
        (*view_it)->save();
        usages_renamed.emplace_back(&usage);
      }
      else {
        Glib::ustring buffer;
        {
          std::ifstream stream(usage.path.string(), std::ifstream::binary);
          if(stream)
            buffer.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
        }
        std::ofstream stream(usage.path.string(), std::ifstream::binary);
        if(!buffer.empty() && stream) {
          std::vector<size_t> lines_start_pos={0};
          for(size_t c=0;c<buffer.size();++c) {
            if(buffer[c]=='\n')
              lines_start_pos.emplace_back(c+1);
          }
          for(auto offset_it=usage.offsets.rbegin();offset_it!=usage.offsets.rend();++offset_it) {
            auto start_line=offset_it->first.line;
            auto end_line=offset_it->second.line;
            if(start_line<lines_start_pos.size() && end_line<lines_start_pos.size()) {
              auto start=lines_start_pos[start_line]+offset_it->first.index;
              auto end=lines_start_pos[end_line]+offset_it->second.index;
              if(start<buffer.size() && end<=buffer.size())
                buffer.replace(start, end-start, text);
            }
          }
          stream.write(buffer.data(), buffer.bytes());
          usages_renamed.emplace_back(&usage);
        }
        else
          Terminal::get().print("Error: could not write to file "+usage.path.string()+'\n', true);
      }
    }
    
    if(!usages_renamed.empty()) {
      Terminal::get().print("Renamed ");
      Terminal::get().print(previous_text, true);
      Terminal::get().print(" to ");
      Terminal::get().print(text, true);
      Terminal::get().print("\n");
    }
  };
  
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

void Source::LanguageProtocolView::unescape_text(std::string &text) {
  for(size_t c=0;c<text.size();++c) {
    if(text[c]=='\\' && c+1<text.size()) {
      if(text[c+1]=='n')
        text.replace(c, 2, "\n");
      else if(text[c+1]=='r')
        text.replace(c, 2, "\r");
      else
        text.erase(c, 1);
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
        auto start=get_iter_at_line_pos(diagnostic.offsets.first.line, diagnostic.offsets.first.index);
        auto end=get_iter_at_line_pos(diagnostic.offsets.second.line, diagnostic.offsets.second.index);
        
        if(start==end) {
          if(!end.is_end())
            end.forward_char();
          else
            start.forward_char();
        }
        
        diagnostic_offsets.emplace(start.get_offset());
        
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

Gtk::TextIter Source::LanguageProtocolView::get_iter_at_line_pos(int line, int pos) {
  line=std::min(line, get_buffer()->get_line_count()-1);
  if(line<0)
    line=0;
  auto iter=get_iter_at_line_end(line);
  pos=std::min(pos, iter.get_line_offset());
  if(pos<0)
    pos=0;
  return get_buffer()->get_iter_at_line_offset(line, pos);
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
            auto create_tooltip_buffer=[this, offset, value=std::move(value)]() {
              auto tooltip_buffer=Gtk::TextBuffer::create(get_buffer()->get_tag_table());
              tooltip_buffer->insert_with_tag(tooltip_buffer->get_insert()->get_iter(), "Type: "+value, "def:note");
              
#ifdef JUCI_ENABLE_DEBUG
              if(language_id=="rust") {
                if(Debug::LLDB::get().is_stopped()) {
                  Glib::ustring value_type="Value";
                  
                  auto start=get_buffer()->get_iter_at_offset(offset);
                  auto end=start;
                  auto previous=start;
                  while(previous.backward_char() && ((*previous>='A' && *previous<='Z') || (*previous>='a' && *previous<='z') || (*previous>='0' && *previous<='9') || *previous=='_') && start.backward_char()) {}
                  while(((*end>='A' && *end<='Z') || (*end>='a' && *end<='z') || (*end>='0' && *end<='9') || *end=='_') && end.forward_char()) {}
                  
                  auto offset=get_declaration(start);
                  Glib::ustring debug_value=Debug::LLDB::get().get_value(get_buffer()->get_text(start, end), offset.file_path, offset.line+1, offset.index+1);
                  if(debug_value.empty()) {
                    value_type="Return value";
                    debug_value=Debug::LLDB::get().get_return_value(file_path, start.get_line()+1, start.get_line_index()+1);
                  }
                  if(!debug_value.empty()) {
                    size_t pos=debug_value.find(" = ");
                    if(pos!=Glib::ustring::npos) {
                      Glib::ustring::iterator iter;
                      while(!debug_value.validate(iter)) {
                        auto next_char_iter=iter;
                        next_char_iter++;
                        debug_value.replace(iter, next_char_iter, "?");
                      }
                      tooltip_buffer->insert_with_tag(tooltip_buffer->get_insert()->get_iter(), "\n\n"+value_type+": "+debug_value.substr(pos+3, debug_value.size()-(pos+3)-1), "def:note");
                    }
                  }
                }
              }
#endif
              
              return tooltip_buffer;
            };
            
            auto start=get_buffer()->get_iter_at_offset(offset);
            auto end=start;
            auto previous=start;
            while(previous.backward_char() && ((*previous>='A' && *previous<='Z') || (*previous>='a' && *previous<='z') || (*previous>='0' && *previous<='9') || *previous=='_') && start.backward_char()) {}
            while(((*end>='A' && *end<='Z') || (*end>='a' && *end<='z') || (*end>='0' && *end<='9') || *end=='_') && end.forward_char()) {}
            type_tooltips.emplace_back(create_tooltip_buffer, this, get_buffer()->create_mark(start), get_buffer()->create_mark(end));
            type_tooltips.show();
          });
        }
      }
    }
  });
}

void Source::LanguageProtocolView::tag_similar_symbols() {
  auto iter=get_buffer()->get_insert()->get_iter();
  std::vector<std::pair<Offset, Offset>> offsets;
  std::promise<void> result_processed;
  client->write_request("textDocument/references", "\"textDocument\":{\"uri\":\""+uri+"\"}, \"position\": {\"line\": "+std::to_string(iter.get_line())+", \"character\": "+std::to_string(iter.get_line_offset())+"}, \"context\": {\"includeDeclaration\": true}", [this, &result_processed, &offsets](const boost::property_tree::ptree &result, bool error) {
    if(!error) {
      for(auto it=result.begin();it!=result.end();++it) {
        if(it->second.get<std::string>("uri", "")==uri) {
          auto range_it=it->second.find("range");
          if(range_it!=it->second.not_found()) {
            auto start_it=range_it->second.find("start");
            auto end_it=range_it->second.find("end");
            if(start_it!=range_it->second.not_found() && end_it!=range_it->second.not_found()) {
              try {
                offsets.emplace_back(std::make_pair(Offset(start_it->second.get<unsigned>("line"), start_it->second.get<unsigned>("character")),
                                                    Offset(end_it->second.get<unsigned>("line"), end_it->second.get<unsigned>("character"))));
              }
              catch(...) {}
            }
          }
        }
      }
    }
    result_processed.set_value();
  });
  result_processed.get_future().get();
  
  get_buffer()->remove_tag(similar_symbol_tag, get_buffer()->begin(), get_buffer()->end());
  for(auto &pair: offsets) {
    auto start=get_iter_at_line_pos(pair.first.line, pair.first.index);
    auto end=get_iter_at_line_pos(pair.second.line, pair.second.index);
    get_buffer()->apply_tag(similar_symbol_tag, start, end);
  }
}

Source::Offset Source::LanguageProtocolView::get_declaration(const Gtk::TextIter &iter) {
  auto offset=std::make_shared<Offset>();
  std::promise<void> result_processed;
  client->write_request("textDocument/definition", "\"textDocument\":{\"uri\":\""+uri+"\"}, \"position\": {\"line\": "+std::to_string(iter.get_line())+", \"character\": "+std::to_string(iter.get_line_offset())+"}", [offset, &result_processed](const boost::property_tree::ptree &result, bool error) {
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
    result_processed.set_value();
  });
  result_processed.get_future().get();
  return *offset;
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
      std::promise<void> result_processed;
      client->write_request("textDocument/completion", "\"textDocument\":{\"uri\":\""+uri+"\"}, \"position\": {\"line\": "+std::to_string(line_number-1)+", \"character\": "+std::to_string(column-1)+"}", [this, &result_processed](const boost::property_tree::ptree &result, bool error) {
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
        result_processed.set_value();
      });
      result_processed.get_future().get();
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
