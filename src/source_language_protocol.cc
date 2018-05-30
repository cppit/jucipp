#include "source_language_protocol.h"
#include "info.h"
#include "selection_dialog.h"
#include "terminal.h"
#include "project.h"
#include "filesystem.h"
#ifdef JUCI_ENABLE_DEBUG
#include "debug_lldb.h"
#endif
#include "menu.h"
#include <regex>
#include <future>
#include <limits>

const bool output_messages_and_errors=false;

LanguageProtocol::Client::Client(std::string root_uri_, std::string language_id_) : root_uri(std::move(root_uri_)), language_id(std::move(language_id_)) {
  process = std::make_unique<TinyProcessLib::Process>(language_id+"-language-server", root_uri,
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
    it->second=instance=std::shared_ptr<Client>(new Client(root_uri, language_id), [](Client *client_ptr) {
      std::thread delete_thread([client_ptr] {
        delete client_ptr;
      });
      delete_thread.detach();
    });
  return instance;
}

LanguageProtocol::Client::~Client() {
  std::promise<void> result_processed;
  write_request(nullptr, "shutdown", "", [this, &result_processed](const boost::property_tree::ptree &result, bool error) {
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
  if(view) {
    std::unique_lock<std::mutex> lock(views_mutex);
    views.emplace(view);
  }
  
  std::lock_guard<std::mutex> lock(initialize_mutex);
  
  if(initialized)
    return capabilities;
  
  std::promise<void> result_processed;
  write_request(nullptr, "initialize", "\"processId\":"+std::to_string(process->get_id())+R"(,"rootUri":"file://)"+root_uri+R"(","capabilities":{"workspace":{"didChangeConfiguration":{"dynamicRegistration":true},"didChangeWatchedFiles":{"dynamicRegistration":true},"symbol":{"dynamicRegistration":true},"executeCommand":{"dynamicRegistration":true}},"textDocument":{"synchronization":{"dynamicRegistration":true,"willSave":true,"willSaveWaitUntil":true,"didSave":true},"completion":{"dynamicRegistration":true,"completionItem":{"snippetSupport":true}},"hover":{"dynamicRegistration":true},"signatureHelp":{"dynamicRegistration":true},"definition":{"dynamicRegistration":true},"references":{"dynamicRegistration":true},"documentHighlight":{"dynamicRegistration":true},"documentSymbol":{"dynamicRegistration":true},"codeAction":{"dynamicRegistration":true},"codeLens":{"dynamicRegistration":true},"formatting":{"dynamicRegistration":true},"rangeFormatting":{"dynamicRegistration":true},"onTypeFormatting":{"dynamicRegistration":true},"rename":{"dynamicRegistration":true},"documentLink":{"dynamicRegistration":true}}},"initializationOptions":{"omitInitBuild":true},"trace":"off")", [this, &result_processed](const boost::property_tree::ptree &result, bool error) {
    if(!error) {
      auto capabilities_pt=result.find("capabilities");
      if(capabilities_pt!=result.not_found()) {
        capabilities.text_document_sync=static_cast<LanguageProtocol::Capabilities::TextDocumentSync>(capabilities_pt->second.get<unsigned>("textDocumentSync", 0));
        capabilities.hover=capabilities_pt->second.get<bool>("hoverProvider", false);
        capabilities.completion=capabilities_pt->second.find("completionProvider")!=capabilities_pt->second.not_found()?true:false;
        capabilities.definition=capabilities_pt->second.get<bool>("definitionProvider", false);
        capabilities.references=capabilities_pt->second.get<bool>("referencesProvider", false);
        capabilities.document_highlight=capabilities_pt->second.get<bool>("documentHighlightProvider", false);
        capabilities.workspace_symbol=capabilities_pt->second.get<bool>("workspaceSymbolProvider", false);
        capabilities.document_formatting=capabilities_pt->second.get<bool>("documentFormattingProvider", false);
        capabilities.document_range_formatting=capabilities_pt->second.get<bool>("documentRangeFormattingProvider", false);
        capabilities.rename=capabilities_pt->second.get<bool>("renameProvider", false);
      }
      
      write_notification("initialized", "");
      if(language_id=="rust")
        write_notification("workspace/didChangeConfiguration", R"("settings":{"rust":{"sysroot":null,"target":null,"rustflags":null,"clear_env_rust_log":true,"build_lib":null,"build_bin":null,"cfg_test":false,"unstable_features":false,"wait_to_build":500,"show_warnings":true,"goto_def_racer_fallback":false,"use_crate_blacklist":true,"build_on_save":false,"workspace_mode":true,"analyze_package":null,"features":[],"all_features":false,"no_default_features":false}})");
    }
    result_processed.set_value();
  });
  result_processed.get_future().get();
  
  initialized=true;
  return capabilities;
}

void LanguageProtocol::Client::close(Source::LanguageProtocolView *view) {
  {
    std::unique_lock<std::mutex> lock(views_mutex);
    auto it=views.find(view);
    if(it!=views.end())
      views.erase(it);
  }
  std::unique_lock<std::mutex> lock(read_write_mutex);
  for(auto it=handlers.begin();it!=handlers.end();) {
    if(it->second.first==view)
      it=handlers.erase(it);
    else
      it++;
  }
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
              auto function=std::move(id_it->second.second);
              handlers.erase(id_it->first);
              lock.unlock();
              function(result_it->second, false);
              lock.lock();
            }
          }
        }
        else if(error_it!=pt.not_found()) {
          if(!output_messages_and_errors)
            boost::property_tree::write_json(std::cerr, pt);
          if(message_id) {
            auto id_it=handlers.find(message_id);
            if(id_it!=handlers.end()) {
              auto function=std::move(id_it->second.second);
              handlers.erase(id_it->first);
              lock.unlock();
              function(result_it->second, true);
              lock.lock();
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

void LanguageProtocol::Client::write_request(Source::LanguageProtocolView *view, const std::string &method, const std::string &params, std::function<void(const boost::property_tree::ptree &, bool error)> &&function) {
  std::unique_lock<std::mutex> lock(read_write_mutex);
  if(function) {
    handlers.emplace(message_id, std::make_pair(view, std::move(function)));
    
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
        auto function=std::move(id_it->second.second);
        handlers.erase(id_it->first);
        lock.unlock();
        function(boost::property_tree::ptree(), false);
        lock.lock();
      }
    });
  }
  std::string content(R"({"jsonrpc":"2.0","id":)"+std::to_string(message_id++)+R"(,"method":")"+method+R"(","params":{)"+params+"}}");
  auto message="Content-Length: "+std::to_string(content.size())+"\r\n\r\n"+content;
  if(output_messages_and_errors)
    std::cout << "Language client: " << content << std::endl;
  if(!process->write(message)) {
    Terminal::get().async_print("Error writing to language protocol server. Please close and reopen all project source files.\n", true);
    auto id_it=handlers.find(message_id-1);
    if(id_it!=handlers.end()) {
      auto function=std::move(id_it->second.second);
      handlers.erase(id_it->first);
      lock.unlock();
      function(boost::property_tree::ptree(), false);
      lock.lock();
    }
  }
}

void LanguageProtocol::Client::write_notification(const std::string &method, const std::string &params) {
  std::unique_lock<std::mutex> lock(read_write_mutex);
  std::string content(R"({"jsonrpc":"2.0","method":")"+method+R"(","params":{)"+params+"}}");
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

Source::LanguageProtocolView::LanguageProtocolView(const boost::filesystem::path &file_path, const Glib::RefPtr<Gsv::Language> &language, std::string language_id_)
    : Source::BaseView(file_path, language), Source::View(file_path, language), uri("file://"+file_path.string()), language_id(std::move(language_id_)), client(LanguageProtocol::Client::get(file_path, language_id)), autocomplete(this, interactive_completion, last_keyval, false) {
  configure();
  get_source_buffer()->set_language(language);
  get_source_buffer()->set_highlight_syntax(true);
  
  status_state="initializing...";
  if(update_status_state)
    update_status_state(this);
  
  if(language_id=="javascript") {
    boost::filesystem::path project_path;
    auto build=Project::Build::create(file_path);
    if(auto npm_build=dynamic_cast<Project::NpmBuild*>(build.get())) {
      boost::system::error_code ec;
      if(!npm_build->project_path.empty() && boost::filesystem::exists(npm_build->project_path/".flowconfig", ec)) {
        auto executable=npm_build->project_path/"node_modules"/".bin"/"flow"; // It is recommended to use Flow binary installed in project, despite the security risk of doing so...
        if(boost::filesystem::exists(executable, ec))
          flow_coverage_executable=executable;
        else
          flow_coverage_executable=filesystem::find_executable("flow");
      }
    }
  }
  
  initialize_thread=std::thread([this] {
    auto capabilities=client->initialize(this);
    
    if(!flow_coverage_executable.empty())
      add_flow_coverage_tooltips(true);
    
    dispatcher.post([this, capabilities] {
      this->capabilities=capabilities;
      
      std::string text=get_buffer()->get_text();
      escape_text(text);
      client->write_notification("textDocument/didOpen", R"("textDocument":{"uri":")"+uri+R"(","languageId":")"+language_id+R"(","version":)"+std::to_string(document_version++)+R"(,"text":")"+text+"\"}");
      
      setup_autocomplete();
      setup_navigation_and_refactoring();
      Menu::get().toggle_menu_items();
      
      if(status_state=="initializing...") {
        status_state="";
        if(update_status_state)
          update_status_state(this);
      }
    });
  });
  
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
      content_changes=R"({"range":{"start":{"line": )"+std::to_string(start.get_line())+",\"character\":"+std::to_string(start.get_line_offset())+R"(},"end":{"line":)"+std::to_string(start.get_line())+",\"character\":"+std::to_string(start.get_line_offset())+R"(}},"text":")"+text+"\"}";
    }
    else {
      std::string text=get_buffer()->get_text();
      escape_text(text);
      content_changes=R"({"text":")"+text+"\"}";
    }
    client->write_notification("textDocument/didChange", R"("textDocument":{"uri":")"+uri+R"(","version":)"+std::to_string(document_version++)+"},\"contentChanges\":["+content_changes+"]");
  }, false);

  get_buffer()->signal_erase().connect([this](const Gtk::TextBuffer::iterator &start, const Gtk::TextBuffer::iterator &end) {
    std::string content_changes;
    if(capabilities.text_document_sync==LanguageProtocol::Capabilities::TextDocumentSync::NONE)
      return;
    if(capabilities.text_document_sync==LanguageProtocol::Capabilities::TextDocumentSync::INCREMENTAL)
      content_changes=R"({"range":{"start":{"line": )"+std::to_string(start.get_line())+",\"character\":"+std::to_string(start.get_line_offset())+R"(},"end":{"line":)"+std::to_string(end.get_line())+",\"character\":"+std::to_string(end.get_line_offset())+R"(}},"text":""})";
    else {
      std::string text=get_buffer()->get_text();
      escape_text(text);
      content_changes=R"({"text":")"+text+"\"}";
    }
    client->write_notification("textDocument/didChange", R"("textDocument":{"uri":")"+uri+R"(","version":)"+std::to_string(document_version++)+"},\"contentChanges\":["+content_changes+"]");
  }, false);
}

Source::LanguageProtocolView::~LanguageProtocolView() {
  if(initialize_thread.joinable())
    initialize_thread.join();
  
  autocomplete.state=Autocomplete::State::IDLE;
  if(autocomplete.thread.joinable())
    autocomplete.thread.join();
  
  client->write_notification("textDocument/didClose", R"("textDocument":{"uri":")"+uri+"\"}");
  client->close(this);
  
  client=nullptr;
}

bool Source::LanguageProtocolView::save() {
  if(!Source::View::save())
    return false;
  
  if(!flow_coverage_executable.empty())
    add_flow_coverage_tooltips(false);
  
  return true;
}

void Source::LanguageProtocolView::setup_navigation_and_refactoring() {
  if(capabilities.document_formatting) {
    format_style=[this](bool continue_without_style_file) {
      if(!continue_without_style_file) {
        bool has_style_file=false;
        auto style_file_search_path=this->file_path.parent_path();
        auto style_file='.'+language_id+"-format";
        
        while(true) {
          if(boost::filesystem::exists(style_file_search_path/style_file)) {
            has_style_file=true;
            break;
          }
          if(style_file_search_path==style_file_search_path.root_directory())
            break;
          style_file_search_path=style_file_search_path.parent_path();
        }
        
        if(!has_style_file && !continue_without_style_file)
          return;
      }
      
      class Replace {
      public:
        Offset start, end;
        std::string text;
      };
      std::vector<Replace> replaces;
      std::promise<void> result_processed;
      
      std::string method;
      std::string params;
      std::string options("\"tabSize\":"+std::to_string(tab_size)+",\"insertSpaces\":"+(tab_char==' '?"true":"false"));
      if(get_buffer()->get_has_selection() && capabilities.document_range_formatting) {
        method="textDocument/rangeFormatting";
        Gtk::TextIter start, end;
        get_buffer()->get_selection_bounds(start, end);
        params=R"("textDocument":{"uri":")"+uri+R"("},"range":{"start":{"line":)"+std::to_string(start.get_line())+",\"character\":"+std::to_string(start.get_line_offset())+R"(},"end":{"line":)"+std::to_string(end.get_line())+",\"character\":"+std::to_string(end.get_line_offset())+"}},\"options\":{"+options+"}";
      }
      else {
        method="textDocument/formatting";
        params=R"("textDocument":{"uri":")"+uri+R"("},"options":{)"+options+"}";
      }
      
      client->write_request(this, method, params, [&replaces, &result_processed](const boost::property_tree::ptree &result, bool error) {
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
      
      auto end_iter=get_buffer()->end();
      if(replaces.size()==1 &&
         replaces[0].start.line==0 && replaces[0].start.index==0 &&
         (replaces[0].end.line>static_cast<unsigned>(end_iter.get_line()) ||
          (replaces[0].end.line==static_cast<unsigned>(end_iter.get_line()) && replaces[0].end.index>=static_cast<unsigned>(end_iter.get_line_offset())))) {
        unescape_text(replaces[0].text);
        replace_text(replaces[0].text);
      }
      else {
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
      }
    };
  }
  
  if(capabilities.definition) {
    get_declaration_location=[this]() {
      auto offset=get_declaration(get_buffer()->get_insert()->get_iter());
      if(!offset)
        Info::get().print("No declaration found");
      return offset;
    };
    get_declaration_or_implementation_locations=[this]() {
      std::vector<Offset> offsets;
      auto offset=get_declaration_location();
      if(offset)
        offsets.emplace_back(std::move(offset));
      return offsets;
    };
  }
  
  if(capabilities.references) {
    get_usages=[this] {
      auto iter=get_buffer()->get_insert()->get_iter();
      std::vector<std::pair<Offset, std::string>> usages;
      std::vector<Offset> end_offsets;
      std::promise<void> result_processed;
      client->write_request(this, "textDocument/references", R"("textDocument":{"uri":")"+uri+R"("}, "position": {"line": )"+std::to_string(iter.get_line())+", \"character\": "+std::to_string(iter.get_line_offset())+R"(}, "context": {"includeDeclaration": true})", [&usages, &end_offsets, &result_processed](const boost::property_tree::ptree &result, bool error) {
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
      auto c=static_cast<size_t>(-1);
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
            usage.second=Glib::Markup::escape_text((*view_it)->get_buffer()->get_text(start, end));
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
  }
  
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
  
  if(capabilities.rename) {
    rename_similar_tokens=[this](const std::string &text) {
      class Usages {
      public:
        boost::filesystem::path path;
        std::unique_ptr<std::string> new_text;
        std::vector<std::pair<Offset, Offset>> offsets;
      };
      
      auto previous_text=get_token_spelling();
      if(previous_text.empty())
        return;
      
      auto iter=get_buffer()->get_insert()->get_iter();
      std::vector<Usages> usages;
      std::promise<void> result_processed;
      client->write_request(this, "textDocument/rename", R"("textDocument":{"uri":")"+uri+R"("}, "position": {"line": )"+std::to_string(iter.get_line())+", \"character\": "+std::to_string(iter.get_line_offset())+R"(}, "newName": ")"+text+"\"", [this, &usages, &result_processed](const boost::property_tree::ptree &result, bool error) {
        if(!error) {
          boost::filesystem::path project_path;
          auto build=Project::Build::create(file_path);
          if(!build->project_path.empty())
            project_path=build->project_path;
          else
            project_path=file_path.parent_path();
          try {
            auto changes_it=result.find("changes");
            if(changes_it!=result.not_found()) {
              for(auto file_it=changes_it->second.begin();file_it!=changes_it->second.end();++file_it) {
                auto path=file_it->first;
                if(path.size()>=7) {
                  path.erase(0, 7);
                  if(filesystem::file_in_path(path, project_path)) {
                    usages.emplace_back(Usages{path, nullptr, std::vector<std::pair<Offset, Offset>>()});
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
            else {
              auto changes_pt=result.get_child("documentChanges", boost::property_tree::ptree());
              for(auto change_it=changes_pt.begin();change_it!=changes_pt.end();++change_it) {
                auto document_it=change_it->second.find("textDocument");
                if(document_it!=change_it->second.not_found()) {
                  auto path=document_it->second.get<std::string>("uri", "");
                  if(path.size()>=7) {
                    path.erase(0, 7);
                    if(filesystem::file_in_path(path, project_path)) {
                      usages.emplace_back(Usages{path, std::make_unique<std::string>(), std::vector<std::pair<Offset, Offset>>()});
                      auto edits_pt=change_it->second.get_child("edits", boost::property_tree::ptree());
                      for(auto edit_it=edits_pt.begin();edit_it!=edits_pt.end();++edit_it) {
                        auto new_text_it=edit_it->second.find("newText");
                        if(new_text_it!=edit_it->second.not_found()) {
                          auto range_it=edit_it->second.find("range");
                          if(range_it!=edit_it->second.not_found()) {
                            auto start_it=range_it->second.find("start");
                            auto end_it=range_it->second.find("end");
                            if(start_it!=range_it->second.not_found() && end_it!=range_it->second.not_found()) {
                              auto end_line=end_it->second.get<size_t>("line");
                              if(end_line>std::numeric_limits<int>::max())
                                end_line=std::numeric_limits<int>::max();
                              *usages.back().new_text=new_text_it->second.get_value<std::string>();
                              usages.back().offsets.emplace_back(std::make_pair(Offset(start_it->second.get<unsigned>("line"), start_it->second.get<unsigned>("character")),
                                                                                Offset(static_cast<unsigned>(end_line), end_it->second.get<unsigned>("character"))));
                            }
                          }
                        }
                      }
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
          auto iter=get_buffer()->get_insert()->get_iter();
          auto line=iter.get_line();
          auto offset=iter.get_line_offset();
          for(auto offset_it=usage.offsets.rbegin();offset_it!=usage.offsets.rend();++offset_it) {
            auto start_iter=(*view_it)->get_iter_at_line_pos(offset_it->first.line, offset_it->first.index);
            auto end_iter=(*view_it)->get_iter_at_line_pos(offset_it->second.line, offset_it->second.index);
            (*view_it)->get_buffer()->erase(start_iter, end_iter);
            start_iter=(*view_it)->get_iter_at_line_pos(offset_it->first.line, offset_it->first.index);
            if(usage.new_text)
              (*view_it)->get_buffer()->insert(start_iter, *usage.new_text);
            else
              (*view_it)->get_buffer()->insert(start_iter, text);
          }
          if(usage.new_text && get_buffer()->get_insert()->get_iter().is_end()) {
            place_cursor_at_line_offset(line, offset);
            hide_tooltips();
            scroll_to_cursor_delayed(this, true, false);
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
              if(start_line<lines_start_pos.size()) {
                auto start=lines_start_pos[start_line]+offset_it->first.index;
                unsigned end;
                if(end_line>=lines_start_pos.size())
                  end=buffer.size();
                else
                  end=lines_start_pos[end_line]+offset_it->second.index;
                if(start<buffer.size() && end<=buffer.size()) {
                  if(usage.new_text)
                    buffer.replace(start, end-start, *usage.new_text);
                  else
                    buffer.replace(start, end-start, text);
                }
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
  }
  
  goto_next_diagnostic=[this]() {
    place_cursor_at_next_diagnostic();
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
    clear_diagnostic_tooltips();
    num_warnings=0;
    num_errors=0;
    num_fix_its=0;
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
        
        bool error=false;
        std::string severity_tag_name;
        if(diagnostic.severity>=2) {
          severity_tag_name="def:warning";
          num_warnings++;
        }
        else {
          severity_tag_name="def:error";
          num_errors++;
          error=true;
        }
        
        add_diagnostic_tooltip(start, end, diagnostic.spelling, error);
      }
    }
    
    for(auto &mark: flow_coverage_marks)
      add_diagnostic_tooltip(mark.first->get_iter(), mark.second->get_iter(), flow_coverage_message, false);
    
    status_diagnostics=std::make_tuple(num_warnings+num_flow_coverage_warnings, num_errors, num_fix_its);
    if(update_status_diagnostics)
      update_status_diagnostics(this);
  });
}

Gtk::TextIter Source::LanguageProtocolView::get_iter_at_line_pos(int line, int pos) {
  return get_iter_at_line_offset(line, pos);
}

void Source::LanguageProtocolView::show_type_tooltips(const Gdk::Rectangle &rectangle) {
  if(!capabilities.hover)
    return;
  
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
  
  static int request_count=0;
  request_count++;
  auto current_request=request_count;
  client->write_request(this, "textDocument/hover", R"("textDocument": {"uri":"file://)"+file_path.string()+R"("}, "position": {"line": )"+std::to_string(iter.get_line())+", \"character\": "+std::to_string(iter.get_line_offset())+"}", [this, offset, current_request](const boost::property_tree::ptree &result, bool error) {
    if(!error) {
      // hover result structure vary significantly from the different language servers
      auto content=std::make_shared<std::string>();
      auto contents_pt=result.get_child("contents", boost::property_tree::ptree());
      for(auto it=contents_pt.begin();it!=contents_pt.end();++it) {
        auto value=it->second.get<std::string>("value", "");
        if(!value.empty())
          content->insert(0, value+(content->empty()?"":"\n\n"));
        else {
          value=it->second.get_value<std::string>("");
          if(!value.empty())
            *content+=(content->empty()?"":"\n\n")+value;
        }
      }
      if(content->empty()) {
        auto contents_it=result.find("contents");
        if(contents_it!=result.not_found()) {
          *content=contents_it->second.get<std::string>("value", "");
          if(content->empty())
            *content=contents_it->second.get_value<std::string>("");
        }
      }
      if(!content->empty()) {
        while(!content->empty() && content->back()=='\n') { content->pop_back(); } // Remove unnecessary newlines
        dispatcher.post([this, offset, content, current_request] {
          if(current_request!=request_count)
            return;
          if(offset>=get_buffer()->get_char_count())
            return;
          type_tooltips.clear();
          auto create_tooltip_buffer=[this, offset, content]() {
            auto tooltip_buffer=Gtk::TextBuffer::create(get_buffer()->get_tag_table());
            tooltip_buffer->insert(tooltip_buffer->get_insert()->get_iter(), *content);
            
#ifdef JUCI_ENABLE_DEBUG
            if(language_id=="rust" && capabilities.definition) {
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
                    tooltip_buffer->insert(tooltip_buffer->get_insert()->get_iter(), "\n\n"+value_type+": "+debug_value.substr(pos+3, debug_value.size()-(pos+3)-1));
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
  });
}

void Source::LanguageProtocolView::tag_similar_symbols() {
  if(!capabilities.document_highlight && !capabilities.references)
    return;
  
  auto iter=get_buffer()->get_insert()->get_iter();
  std::string method;
  if(capabilities.document_highlight)
    method="textDocument/documentHighlight";
  else
    method="textDocument/references";
  
  static int request_count=0;
  request_count++;
  auto current_request=request_count;
  client->write_request(this, method, R"("textDocument":{"uri":")"+uri+R"("}, "position": {"line": )"+std::to_string(iter.get_line())+", \"character\": "+std::to_string(iter.get_line_offset())+R"(}, "context": {"includeDeclaration": true})", [this, current_request](const boost::property_tree::ptree &result, bool error) {
    if(!error) {
      std::vector<std::pair<Offset, Offset>> offsets;
      for(auto it=result.begin();it!=result.end();++it) {
        if(capabilities.document_highlight || it->second.get<std::string>("uri", "")==uri) {
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
      dispatcher.post([this, offsets=std::move(offsets), current_request] {
        if(current_request!=request_count)
          return;
        get_buffer()->remove_tag(similar_symbol_tag, get_buffer()->begin(), get_buffer()->end());
        for(auto &pair: offsets) {
          auto start=get_iter_at_line_pos(pair.first.line, pair.first.index);
          auto end=get_iter_at_line_pos(pair.second.line, pair.second.index);
          get_buffer()->apply_tag(similar_symbol_tag, start, end);
        }
      });
    }
  });
}

Source::Offset Source::LanguageProtocolView::get_declaration(const Gtk::TextIter &iter) {
  auto offset=std::make_shared<Offset>();
  std::promise<void> result_processed;
  client->write_request(this, "textDocument/definition", R"("textDocument":{"uri":")"+uri+R"("}, "position": {"line": )"+std::to_string(iter.get_line())+", \"character\": "+std::to_string(iter.get_line_offset())+"}", [offset, &result_processed](const boost::property_tree::ptree &result, bool error) {
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
  if(!capabilities.completion)
    return;
  
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
    const static std::regex dot_or_arrow(R"(^.*[a-zA-Z0-9_\)\]\>"'](\.)([a-zA-Z0-9_]*)$)");
    const static std::regex colon_colon(R"(^.*::([a-zA-Z0-9_]*)$)");
    const static std::regex part_of_symbol(R"(^.*[^a-zA-Z0-9_]+([a-zA-Z0-9_]{3,})$)");
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
      client->write_request(this, "textDocument/completion", R"("textDocument":{"uri":")"+uri+R"("}, "position": {"line": )"+std::to_string(line_number-1)+", \"character\": "+std::to_string(column-1)+"}", [this, &result_processed](const boost::property_tree::ptree &result, bool error) {
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
            auto documentation=it->second.get<std::string>("documentation", "");
            auto insert=it->second.get<std::string>("insertText", "");
            if(!insert.empty()) {
              // In case ( is missing in insert but is present in label
              if(label.size()>insert.size() && label.back()==')' && insert.find('(')==std::string::npos) {
                auto pos=label.find('(');
                if(pos!=std::string::npos && pos==insert.size() && pos+1<label.size()) {
                  if(pos+2==label.size()) // If no parameters
                    insert+="()";
                  else
                    insert+="(${1:"+label.substr(pos+1, label.size()-1-(pos+1))+"})";
                }
              }
            }
            else {
              insert=label;
              auto kind=it->second.get<unsigned>("kind", 0);
              if(kind>=2 && kind<=3) {
                bool found_bracket=false;
                for(auto &chr: insert) {
                  if(chr=='(' || chr=='{') {
                    found_bracket=true;
                    break;
                  }
                }
                if(!found_bracket)
                  insert+="(${1:})";
              }
            }
            if(!label.empty()) {
              autocomplete.rows.emplace_back(std::move(label));
              autocomplete_comment.emplace_back(std::move(detail));
              if(!documentation.empty()) {
                if(!autocomplete_comment.back().empty())
                  autocomplete_comment.back()+="\n\n";
                autocomplete_comment.back()+=documentation;
              }
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
      
      // Do not insert function/template parameters if they already exist
      auto iter=get_buffer()->get_insert()->get_iter();
      if(*iter=='(' || *iter=='<') {
        auto bracket_pos=insert.find(*iter);
        if(bracket_pos!=std::string::npos) {
          insert=insert.substr(0, bracket_pos);
        }
      }
      
      // Find and add position marks that one can move to using tab-key
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

void Source::LanguageProtocolView::add_flow_coverage_tooltips(bool called_in_thread) {
  std::stringstream stdin_stream, stderr_stream;
  auto stdout_stream=std::make_shared<std::stringstream>();
  auto exit_status=Terminal::get().process(stdin_stream, *stdout_stream, flow_coverage_executable.string()+" coverage --json "+file_path.string(), "", &stderr_stream);
  auto f=[this, exit_status, stdout_stream] {
    clear_diagnostic_tooltips();
    num_flow_coverage_warnings=0;
    for(auto &mark: flow_coverage_marks) {
      get_buffer()->delete_mark(mark.first);
      get_buffer()->delete_mark(mark.second);
    }
    flow_coverage_marks.clear();
    
    if(exit_status==0) {
      boost::property_tree::ptree pt;
      try {
        boost::property_tree::read_json(*stdout_stream, pt);
        auto uncovered_locs_pt=pt.get_child("expressions.uncovered_locs");
        for(auto it=uncovered_locs_pt.begin();it!=uncovered_locs_pt.end();++it) {
          auto start_pt=it->second.get_child("start");
          auto start=get_iter_at_line_offset(start_pt.get<int>("line")-1, start_pt.get<int>("column")-1);
          auto end_pt=it->second.get_child("end");
          auto end=get_iter_at_line_offset(end_pt.get<int>("line")-1, end_pt.get<int>("column"));
          
          add_diagnostic_tooltip(start, end, flow_coverage_message, false);
          ++num_flow_coverage_warnings;
          
          flow_coverage_marks.emplace_back(get_buffer()->create_mark(start), get_buffer()->create_mark(end));
        }
      }
      catch(...) {}
    }
    status_diagnostics=std::make_tuple(num_warnings+num_flow_coverage_warnings, num_errors, num_fix_its);
    if(update_status_diagnostics)
      update_status_diagnostics(this);
  };
  if(called_in_thread)
    dispatcher.post(std::move(f));
  else
    f();
}
