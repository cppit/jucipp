#include "usages_clang.h"
#include "compile_commands.h"
#include "config.h"
#include "dialogs.h"
#include "filesystem.h"
#include <chrono>
#include <fstream>
#include <regex>
#include <thread>

#ifdef _WIN32
#include <windows.h>
DWORD get_current_process_id() {
  return GetCurrentProcessId();
}
#else
#include <unistd.h>
pid_t get_current_process_id() {
  return getpid();
}
#endif

const boost::filesystem::path Usages::Clang::cache_folder = ".usages_clang";
std::map<boost::filesystem::path, Usages::Clang::Cache> Usages::Clang::caches;
std::mutex Usages::Clang::caches_mutex;
std::atomic<size_t> Usages::Clang::cache_in_progress_count(0);

bool Usages::Clang::Cache::Cursor::operator==(const Cursor &o) {
  for(auto &usr : usrs) {
    if(clangmm::Cursor::is_similar_kind(o.kind, kind) && o.usrs.count(usr))
      return true;
  }
  return false;
}

Usages::Clang::Cache::Cache(boost::filesystem::path project_path_, boost::filesystem::path build_path_, const boost::filesystem::path &path,
                            std::time_t before_parse_time, clangmm::TranslationUnit *translation_unit, clangmm::Tokens *clang_tokens)
    : project_path(std::move(project_path_)), build_path(std::move(build_path_)) {
  for(auto &clang_token : *clang_tokens) {
    tokens.emplace_back(Token{clang_token.get_spelling(), clang_token.get_source_range().get_offsets(), static_cast<size_t>(-1)});

    if(clang_token.is_identifier()) {
      auto clang_cursor = clang_token.get_cursor().get_referenced();
      if(clang_cursor) {
        Cursor cursor{clang_cursor.get_kind(), clang_cursor.get_all_usr_extended()};
        for(size_t c = 0; c < cursors.size(); ++c) {
          if(cursor == cursors[c]) {
            tokens.back().cursor_id = c;
            break;
          }
        }
        if(tokens.back().cursor_id == static_cast<size_t>(-1)) {
          cursors.emplace_back(cursor);
          tokens.back().cursor_id = cursors.size() - 1;
        }
      }
    }
  }
  boost::system::error_code ec;
  auto last_write_time = boost::filesystem::last_write_time(path, ec);
  if(ec)
    last_write_time = 0;
  if(last_write_time > before_parse_time)
    last_write_time = 0;
  paths_and_last_write_times.emplace(path, last_write_time);

  class VisitorData {
  public:
    const boost::filesystem::path &project_path;
    const boost::filesystem::path &path;
    std::time_t before_parse_time;
    std::map<boost::filesystem::path, std::time_t> &paths_and_last_write_times;
  };
  VisitorData visitor_data{this->project_path, path, before_parse_time, paths_and_last_write_times};

  clang_getInclusions(translation_unit->cx_tu, [](CXFile included_file, CXSourceLocation *inclusion_stack, unsigned include_len, CXClientData data) {
    auto visitor_data = static_cast<VisitorData *>(data);
    auto path = filesystem::get_normal_path(clangmm::to_string(clang_getFileName(included_file)));
    if(filesystem::file_in_path(path, visitor_data->project_path)) {
      for(unsigned c = 0; c < include_len; ++c) {
        auto from_path = filesystem::get_normal_path(clangmm::SourceLocation(inclusion_stack[c]).get_path());
        if(from_path == visitor_data->path) {
          boost::system::error_code ec;
          auto last_write_time = boost::filesystem::last_write_time(path, ec);
          if(ec)
            last_write_time = 0;
          if(last_write_time > visitor_data->before_parse_time)
            last_write_time = 0;
          visitor_data->paths_and_last_write_times.emplace(path, last_write_time);
          break;
        }
      }
    }
  },
                      &visitor_data);
}

std::vector<std::pair<clangmm::Offset, clangmm::Offset>> Usages::Clang::Cache::get_similar_token_offsets(clangmm::Cursor::Kind kind, const std::string &spelling,
                                                                                                         const std::unordered_set<std::string> &usrs) const {
  std::vector<std::pair<clangmm::Offset, clangmm::Offset>> offsets;
  for(auto &token : tokens) {
    if(token.cursor_id != static_cast<size_t>(-1)) {
      auto &cursor = cursors[token.cursor_id];
      if(clangmm::Cursor::is_similar_kind(cursor.kind, kind) && token.spelling == spelling) {
        for(auto &usr : cursor.usrs) {
          if(usrs.count(usr)) {
            offsets.emplace_back(token.offsets);
            break;
          }
        }
      }
    }
  }
  return offsets;
}

std::vector<Usages::Clang::Usages> Usages::Clang::get_usages(const boost::filesystem::path &project_path, const boost::filesystem::path &build_path, const boost::filesystem::path &debug_path,
                                                             const std::string &spelling, const clangmm::Cursor &cursor, const std::vector<clangmm::TranslationUnit *> &translation_units) {
  std::vector<Usages> usages;

  if(spelling.empty())
    return usages;

  PathSet visited;

  auto usr_extended = cursor.get_usr_extended();
  if(!usr_extended.empty() && usr_extended[0] >= '0' && usr_extended[0] <= '9') { //if declared within a function, return
    if(!translation_units.empty())
      add_usages(project_path, build_path, boost::filesystem::path(), usages, visited, spelling, cursor, translation_units.front(), false);
    return usages;
  }

  for(auto &translation_unit : translation_units)
    add_usages(project_path, build_path, boost::filesystem::path(), usages, visited, spelling, cursor, translation_unit, false);

  for(auto &translation_unit : translation_units)
    add_usages_from_includes(project_path, build_path, usages, visited, spelling, cursor, translation_unit, false);

  if(project_path.empty())
    return usages;

  auto paths = find_paths(project_path, build_path, debug_path);
  auto pair = parse_paths(spelling, paths);
  PathSet all_cursors_paths;
  auto canonical=cursor.get_canonical();
  all_cursors_paths.emplace(canonical.get_source_location().get_path());
  for(auto &cursor: canonical.get_all_overridden_cursors())
    all_cursors_paths.emplace(cursor.get_source_location().get_path());
  auto pair2 = find_potential_paths(all_cursors_paths, project_path, pair.first, pair.second);
  auto &potential_paths = pair2.first;
  auto &all_includes = pair2.second;

  // Remove visited paths
  for(auto it = potential_paths.begin(); it != potential_paths.end();) {
    if(visited.find(*it) != visited.end())
      it = potential_paths.erase(it);
    else
      ++it;
  }

  // Wait for current caching to finish
  std::unique_ptr<Dialog::Message> message;
  const std::string message_string = "Please wait while finding usages";
  if(cache_in_progress_count != 0) {
    message = std::make_unique<Dialog::Message>(message_string);
    while(cache_in_progress_count != 0) {
      while(Gtk::Main::events_pending())
        Gtk::Main::iteration(false);
    }
  }

  // Use cache
  for(auto it = potential_paths.begin(); it != potential_paths.end();) {
    std::unique_lock<std::mutex> lock(caches_mutex);
    auto caches_it = caches.find(*it);

    // Load cache from file if not found in memory and if cache file exists
    if(caches_it == caches.end()) {
      auto cache = read_cache(project_path, build_path, *it);
      if(cache) {
        auto pair = caches.emplace(*it, std::move(cache));
        caches_it = pair.first;
      }
    }

    if(caches_it != caches.end()) {
      if(add_usages_from_cache(caches_it->first, usages, visited, spelling, cursor, caches_it->second))
        it = potential_paths.erase(it);
      else {
        caches.erase(caches_it);
        ++it;
      }
    }
    else
      ++it;
  }

  // Remove paths that has been included
  for(auto it = potential_paths.begin(); it != potential_paths.end();) {
    if(all_includes.find(*it) != all_includes.end())
      it = potential_paths.erase(it);
    else
      ++it;
  }

  // Parse potential paths
  if(!potential_paths.empty()) {
    if(!message)
      message = std::make_unique<Dialog::Message>(message_string);

    std::vector<std::thread> threads;
    auto it = potential_paths.begin();
    auto number_of_threads = Config::get().source.clang_usages_threads;
    if(number_of_threads == static_cast<unsigned>(-1)) {
      number_of_threads = std::thread::hardware_concurrency();
      if(number_of_threads == 0)
        number_of_threads = 1;
    }
    for(unsigned thread_id = 0; thread_id < number_of_threads; ++thread_id) {
      threads.emplace_back([&potential_paths, &it, &build_path,
                            &project_path, &usages, &visited, &spelling, &cursor] {
        while(true) {
          boost::filesystem::path path;
          {
            static std::mutex mutex;
            std::unique_lock<std::mutex> lock(mutex);
            if(it == potential_paths.end())
              return;
            path = *it;
            ++it;
          }
          clangmm::Index index(0, 0);

          {
            static std::mutex mutex;
            std::unique_lock<std::mutex> lock(mutex);
            // std::cout << "parsing: " << path << std::endl;
          }
          // auto before_time = std::chrono::system_clock::now();

          std::ifstream stream(path.string(), std::ifstream::binary);
          std::string buffer;
          buffer.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());

          auto arguments = CompileCommands::get_arguments(build_path, path);
          arguments.emplace_back("-w"); // Disable all warnings
          for(auto it = arguments.begin(); it != arguments.end();) { // remove comments from system headers
            if(*it == "-fretain-comments-from-system-headers")
              it = arguments.erase(it);
            else
              ++it;
          }
          int flags = CXTranslationUnit_Incomplete;
#if CINDEX_VERSION_MAJOR > 0 || (CINDEX_VERSION_MAJOR == 0 && CINDEX_VERSION_MINOR >= 35)
          flags |= CXTranslationUnit_KeepGoing;
#endif

          clangmm::TranslationUnit translation_unit(index, path.string(), arguments, buffer, flags);

          {
            static std::mutex mutex;
            std::unique_lock<std::mutex> lock(mutex);
            add_usages(project_path, build_path, path, usages, visited, spelling, cursor, &translation_unit, true);
            add_usages_from_includes(project_path, build_path, usages, visited, spelling, cursor, &translation_unit, true);
          }

          // auto time = std::chrono::system_clock::now();
          // std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(time - before_time).count() << std::endl;
        }
      });
    }
    for(auto &thread : threads)
      thread.join();
  }

  if(message)
    message->hide();

  return usages;
}

void Usages::Clang::cache(const boost::filesystem::path &project_path, const boost::filesystem::path &build_path, const boost::filesystem::path &path,
                          std::time_t before_parse_time, const PathSet &project_paths_in_use, clangmm::TranslationUnit *translation_unit, clangmm::Tokens *tokens) {
  class ScopeExit {
  public:
    std::function<void()> f;
    ~ScopeExit() {
      f();
    }
  };
  ScopeExit scope_exit{[] {
    --cache_in_progress_count;
  }};

  if(project_path.empty())
    return;

  {
    std::unique_lock<std::mutex> lock(caches_mutex);
    if(project_paths_in_use.count(project_path)) {
      caches.erase(path);
      caches.emplace(path, Cache(project_path, build_path, path, before_parse_time, translation_unit, tokens));
    }
    else
      write_cache(path, Cache(project_path, build_path, path, before_parse_time, translation_unit, tokens));
  }

  class VisitorData {
  public:
    const boost::filesystem::path &project_path;
    PathSet paths;
  };
  VisitorData visitor_data{project_path, {}};

  auto translation_unit_cursor = clang_getTranslationUnitCursor(translation_unit->cx_tu);
  clang_visitChildren(translation_unit_cursor, [](CXCursor cx_cursor, CXCursor cx_parent, CXClientData data) {
    auto visitor_data = static_cast<VisitorData *>(data);

    auto path = filesystem::get_normal_path(clangmm::Cursor(cx_cursor).get_source_location().get_path());
    if(filesystem::file_in_path(path, visitor_data->project_path))
      visitor_data->paths.emplace(path);

    return CXChildVisit_Continue;
  },
                      &visitor_data);

  visitor_data.paths.erase(path);

  for(auto &path : visitor_data.paths) {
    boost::system::error_code ec;
    auto file_size = boost::filesystem::file_size(path, ec);
    if(file_size == static_cast<boost::uintmax_t>(-1) || ec)
      continue;
    auto tokens = translation_unit->get_tokens(path.string(), 0, file_size - 1);
    std::unique_lock<std::mutex> lock(caches_mutex);
    if(project_paths_in_use.count(project_path)) {
      caches.erase(path);
      caches.emplace(path, Cache(project_path, build_path, path, before_parse_time, translation_unit, tokens.get()));
    }
    else
      write_cache(path, Cache(project_path, build_path, path, before_parse_time, translation_unit, tokens.get()));
  }
}

void Usages::Clang::erase_unused_caches(const PathSet &project_paths_in_use) {
  std::unique_lock<std::mutex> lock(caches_mutex);
  for(auto it = caches.begin(); it != caches.end();) {
    bool found = false;
    for(auto &project_path : project_paths_in_use) {
      if(filesystem::file_in_path(it->first, project_path)) {
        found = true;
        break;
      }
    }
    if(!found) {
      write_cache(it->first, it->second);
      it = caches.erase(it);
    }
    else
      ++it;
  }
}

void Usages::Clang::erase_cache(const boost::filesystem::path &path) {
  std::unique_lock<std::mutex> lock(caches_mutex);

  auto it = caches.find(path);
  if(it == caches.end())
    return;

  auto paths_and_last_write_times = std::move(it->second.paths_and_last_write_times);
  for(auto &path_and_last_write_time : paths_and_last_write_times)
    caches.erase(path_and_last_write_time.first);
}

void Usages::Clang::erase_all_caches_for_project(const boost::filesystem::path &project_path, const boost::filesystem::path &build_path) {
  if(project_path.empty())
    return;

  if(cache_in_progress_count != 0)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

  std::unique_lock<std::mutex> lock(caches_mutex);
  boost::system::error_code ec;
  auto usages_clang_path = build_path / cache_folder;
  if(boost::filesystem::exists(usages_clang_path, ec) && boost::filesystem::is_directory(usages_clang_path, ec)) {
    for(boost::filesystem::directory_iterator it(usages_clang_path), end; it != end; ++it) {
      if(it->path().extension() == ".usages")
        boost::filesystem::remove(it->path(), ec);
    }
  }

  for(auto it = caches.begin(); it != caches.end();) {
    if(filesystem::file_in_path(it->first, project_path))
      it = caches.erase(it);
    else
      ++it;
  }
}

void Usages::Clang::cache_in_progress() {
  ++cache_in_progress_count;
}

void Usages::Clang::add_usages(const boost::filesystem::path &project_path, const boost::filesystem::path &build_path, const boost::filesystem::path &path_,
                               std::vector<Usages> &usages, PathSet &visited, const std::string &spelling, clangmm::Cursor cursor,
                               clangmm::TranslationUnit *translation_unit, bool store_in_cache) {
  std::unique_ptr<clangmm::Tokens> tokens;
  boost::filesystem::path path;
  auto before_parse_time = std::time(nullptr);
  auto all_usr_extended = cursor.get_all_usr_extended();
  if(path_.empty()) {
    path = clangmm::to_string(clang_getTranslationUnitSpelling(translation_unit->cx_tu));
    if(visited.find(path) != visited.end() || !filesystem::file_in_path(path, project_path))
      return;
    tokens = translation_unit->get_tokens();
  }
  else {
    path = path_;
    if(visited.find(path) != visited.end() || !filesystem::file_in_path(path, project_path))
      return;
    boost::system::error_code ec;
    auto file_size = boost::filesystem::file_size(path, ec);
    if(file_size == static_cast<boost::uintmax_t>(-1) || ec)
      return;
    tokens = translation_unit->get_tokens(path.string(), 0, file_size - 1);
  }

  auto offsets = tokens->get_similar_token_offsets(cursor.get_kind(), spelling, all_usr_extended);
  std::vector<std::string> lines;
  for(auto &offset : offsets) {
    std::string line;
    auto line_nr = offset.second.line;
    for(auto &token : *tokens) {
      auto offset = token.get_source_location().get_offset();
      if(offset.line == line_nr) {
        while(line.size() < offset.index - 1)
          line += ' ';
        line += token.get_spelling();
      }
    }
    lines.emplace_back(std::move(line));
  }

  if(store_in_cache && filesystem::file_in_path(path, project_path)) {
    std::unique_lock<std::mutex> lock(caches_mutex);
    caches.erase(path);
    caches.emplace(path, Cache(project_path, build_path, path, before_parse_time, translation_unit, tokens.get()));
  }

  visited.emplace(path);
  if(!offsets.empty())
    usages.emplace_back(Usages{std::move(path), std::move(offsets), lines});
}

bool Usages::Clang::add_usages_from_cache(const boost::filesystem::path &path, std::vector<Usages> &usages, PathSet &visited,
                                          const std::string &spelling, const clangmm::Cursor &cursor, const Cache &cache) {
  for(auto &path_and_last_write_time : cache.paths_and_last_write_times) {
    boost::system::error_code ec;
    auto last_write_time = boost::filesystem::last_write_time(path_and_last_write_time.first, ec);
    if(ec || last_write_time != path_and_last_write_time.second) {
      // std::cout << "updated file: " << path_and_last_write_time.first << ", included from " << path << std::endl;
      return false;
    }
  }

  auto offsets = cache.get_similar_token_offsets(cursor.get_kind(), spelling, cursor.get_all_usr_extended());

  std::vector<std::string> lines;
  for(auto &offset : offsets) {
    std::string line;
    auto line_nr = offset.second.line;
    for(auto &token : cache.tokens) {
      auto &offset = token.offsets.first;
      if(offset.line == line_nr) {
        while(line.size() < offset.index - 1)
          line += ' ';
        line += token.spelling;
      }
    }
    lines.emplace_back(std::move(line));
  }

  visited.emplace(path);
  if(!offsets.empty())
    usages.emplace_back(Usages{path, std::move(offsets), lines});
  return true;
}

void Usages::Clang::add_usages_from_includes(const boost::filesystem::path &project_path, const boost::filesystem::path &build_path,
                                             std::vector<Usages> &usages, PathSet &visited, const std::string &spelling, const clangmm::Cursor &cursor,
                                             clangmm::TranslationUnit *translation_unit, bool store_in_cache) {
  if(project_path.empty())
    return;

  class VisitorData {
  public:
    const boost::filesystem::path &project_path;
    const std::string &spelling;
    PathSet &visited;
    PathSet paths;
  };
  VisitorData visitor_data{project_path, spelling, visited, {}};

  auto translation_unit_cursor = clang_getTranslationUnitCursor(translation_unit->cx_tu);
  clang_visitChildren(translation_unit_cursor, [](CXCursor cx_cursor, CXCursor cx_parent, CXClientData data) {
    auto visitor_data = static_cast<VisitorData *>(data);

    auto path = filesystem::get_normal_path(clangmm::Cursor(cx_cursor).get_source_location().get_path());
    if(visitor_data->visited.find(path) == visitor_data->visited.end() && filesystem::file_in_path(path, visitor_data->project_path))
      visitor_data->paths.emplace(path);

    return CXChildVisit_Continue;
  },
                      &visitor_data);

  for(auto &path : visitor_data.paths)
    add_usages(project_path, build_path, path, usages, visited, spelling, cursor, translation_unit, store_in_cache);
}

Usages::Clang::PathSet Usages::Clang::find_paths(const boost::filesystem::path &project_path,
                                                 const boost::filesystem::path &build_path, const boost::filesystem::path &debug_path) {
  PathSet paths;

  CompileCommands compile_commands(build_path);

  for(boost::filesystem::recursive_directory_iterator it(project_path), end; it != end; ++it) {
    auto &path = it->path();
    if(!boost::filesystem::is_regular_file(path)) {
      if(path == build_path || path == debug_path || path.filename() == ".git")
        it.no_push();
      continue;
    }

    if(is_header(path))
      paths.emplace(path);
    else if(is_source(path)) {
      for(auto &command : compile_commands.commands) {
        if(filesystem::get_normal_path(command.file) == path) {
          paths.emplace(path);
          break;
        }
      }
    }
  }

  return paths;
}

bool Usages::Clang::is_header(const boost::filesystem::path &path) {
  auto ext = path.extension();
  if(ext == ".h" || // c headers
     ext == ".hh" || ext == ".hp" || ext == ".hpp" || ext == ".h++" || ext == ".tcc" || // c++ headers
     ext == ".cuh") // CUDA headers
    return true;
  else
    return false;
}

bool Usages::Clang::is_source(const boost::filesystem::path &path) {
  auto ext = path.extension();
  if(ext == ".c" || // c sources
     ext == ".cpp" || ext == ".cxx" || ext == ".cc" || ext == ".C" || ext == ".c++" || // c++ sources
     ext == ".cu" || // CUDA sources
     ext == ".cl") // OpenCL sources
    return true;
  else
    return false;
}

std::pair<std::map<boost::filesystem::path, Usages::Clang::PathSet>, Usages::Clang::PathSet> Usages::Clang::parse_paths(const std::string &spelling, const PathSet &paths) {
  std::map<boost::filesystem::path, PathSet> paths_includes;
  PathSet paths_with_spelling;

  const static std::regex include_regex(R"R(^[ \t]*#[ \t]*include[ \t]*"([^"]+)".*$)R");

  auto is_spelling_char = [](char chr) {
    return (chr >= 'a' && chr <= 'z') || (chr >= 'A' && chr <= 'Z') || (chr >= '0' && chr <= '9') || chr == '_';
  };

  for(auto &path : paths) {
    auto paths_includes_it = paths_includes.emplace(path, PathSet()).first;
    bool paths_with_spelling_emplaced = false;

    std::ifstream stream(path.string(), std::ifstream::binary);
    if(!stream)
      continue;
    std::string line;
    while(std::getline(stream, line)) {
      std::smatch sm;
      if(std::regex_match(line, sm, include_regex)) {
        boost::filesystem::path path(sm[1].str());
        boost::filesystem::path include_path;
        // remove .. and .
        for(auto &part : path) {
          if(part == "..")
            include_path = include_path.parent_path();
          else if(part == ".")
            continue;
          else
            include_path /= part;
        }
        auto distance = std::distance(include_path.begin(), include_path.end());
        for(auto &path : paths) {
          auto path_distance = std::distance(path.begin(), path.end());
          if(path_distance >= distance) {
            auto it = path.begin();
            std::advance(it, path_distance - distance);
            if(std::equal(it, path.end(), include_path.begin(), include_path.end()))
              paths_includes_it->second.emplace(path);
          }
        }
      }
      else if(!paths_with_spelling_emplaced) {
        auto pos = line.find(spelling);
        if(pos != std::string::npos &&
           ((!spelling.empty() && !is_spelling_char(spelling[0])) ||
            ((pos == 0 || !is_spelling_char(line[pos - 1])) &&
             (pos + spelling.size() >= line.size() - 1 || !is_spelling_char(line[pos + spelling.size()]))))) {
          paths_with_spelling.emplace(path);
          paths_with_spelling_emplaced = true;
        }
      }
    }
  }
  return {paths_includes, paths_with_spelling};
}

Usages::Clang::PathSet Usages::Clang::get_all_includes(const boost::filesystem::path &path, const std::map<boost::filesystem::path, PathSet> &paths_includes) {
  PathSet all_includes;

  class Recursive {
  public:
    static void f(PathSet &all_includes, const boost::filesystem::path &path,
                  const std::map<boost::filesystem::path, PathSet> &paths_includes) {
      auto paths_includes_it = paths_includes.find(path);
      if(paths_includes_it != paths_includes.end()) {
        for(auto &include : paths_includes_it->second) {
          auto pair = all_includes.emplace(include);
          if(pair.second)
            f(all_includes, include, paths_includes);
        }
      }
    }
  };
  Recursive::f(all_includes, path, paths_includes);

  return all_includes;
}

std::pair<Usages::Clang::PathSet, Usages::Clang::PathSet> Usages::Clang::find_potential_paths(const PathSet &paths, const boost::filesystem::path &project_path,
                                                                                              const std::map<boost::filesystem::path, PathSet> &paths_includes, const PathSet &paths_with_spelling) {
  PathSet potential_paths;
  PathSet all_includes;

  bool first=true;
  for(auto &path: paths) {
    if(filesystem::file_in_path(path, project_path)) {
      for(auto &path_with_spelling : paths_with_spelling) {
        auto path_all_includes = get_all_includes(path_with_spelling, paths_includes);
        if((path_all_includes.find(path) != path_all_includes.end() || path_with_spelling == path)) {
          potential_paths.emplace(path_with_spelling);
  
          for(auto &include : path_all_includes)
            all_includes.emplace(include);
        }
      }
    }
    else {
      if(first) {
        for(auto &path_with_spelling : paths_with_spelling) {
          potential_paths.emplace(path_with_spelling);
    
          auto path_all_includes = get_all_includes(path_with_spelling, paths_includes);
          for(auto &include : path_all_includes)
            all_includes.emplace(include);
        }
        first=false;
      }
    }
  }

  return {potential_paths, all_includes};
}

void Usages::Clang::write_cache(const boost::filesystem::path &path, const Clang::Cache &cache) {
  auto cache_path = cache.build_path / cache_folder;
  boost::system::error_code ec;
  if(!boost::filesystem::exists(cache_path, ec)) {
    boost::filesystem::create_directory(cache_path, ec);
    if(ec)
      return;
  }
  else if(!boost::filesystem::is_directory(cache_path, ec) || ec)
    return;

  auto path_str = filesystem::get_relative_path(path, cache.project_path).string();
  for(auto &chr : path_str) {
    if(chr == '/' || chr == '\\')
      chr = '_';
  }
  path_str += ".usages";

  auto full_cache_path = cache_path / path_str;
  auto tmp_file = boost::filesystem::temp_directory_path(ec);
  if(ec)
    return;
  tmp_file /= ("jucipp" + std::to_string(get_current_process_id()) + path_str);

  std::ofstream stream(tmp_file.string());
  if(stream) {
    try {
      boost::archive::text_oarchive text_oarchive(stream);
      text_oarchive << cache;
      stream.close();
      boost::filesystem::rename(tmp_file, full_cache_path, ec);
      if(ec) {
        boost::filesystem::copy_file(tmp_file, full_cache_path, boost::filesystem::copy_option::overwrite_if_exists);
        boost::filesystem::remove(tmp_file, ec);
      }
    }
    catch(...) {
      boost::filesystem::remove(tmp_file, ec);
    }
  }
}

Usages::Clang::Cache Usages::Clang::read_cache(const boost::filesystem::path &project_path, const boost::filesystem::path &build_path, const boost::filesystem::path &path) {
  auto path_str = filesystem::get_relative_path(path, project_path).string();
  for(auto &chr : path_str) {
    if(chr == '/' || chr == '\\')
      chr = '_';
  }
  auto cache_path = build_path / cache_folder / (path_str + ".usages");

  boost::system::error_code ec;
  if(boost::filesystem::exists(cache_path, ec)) {
    std::ifstream stream(cache_path.string());
    if(stream) {
      Cache cache;
      boost::archive::text_iarchive text_iarchive(stream);
      try {
        text_iarchive >> cache;
        return cache;
      }
      catch(...) {
      }
    }
  }
  return Cache();
}
