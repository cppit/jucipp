#pragma once
#include "clangmm.h"
#include <atomic>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/filesystem.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/unordered_set.hpp>
#include <boost/serialization/vector.hpp>
#include <map>
#include <mutex>
#include <regex>
#include <set>
#include <unordered_set>

namespace boost {
  namespace serialization {
    template <class Archive>
    void serialize(Archive &ar, boost::filesystem::path &path, const unsigned int version) {
      std::string path_str;
      if(Archive::is_saving::value)
        path_str = path.string();
      ar &path_str;
      if(Archive::is_loading::value)
        path = path_str;
    }
  } // namespace serialization
} // namespace boost

namespace Usages {
  class Clang {
  public:
    using PathSet = std::set<boost::filesystem::path>;

    class Usages {
    public:
      boost::filesystem::path path;
      std::vector<std::pair<clangmm::Offset, clangmm::Offset>> offsets;
      std::vector<std::string> lines;
    };

    class Cache {
      friend class boost::serialization::access;
      template <class Archive>
      void serialize(Archive &ar, const unsigned int version) {
        ar &project_path;
        ar &build_path;
        ar &tokens;
        ar &cursors;
        ar &paths_and_last_write_times;
      }

    public:
      class Cursor {
        friend class boost::serialization::access;
        template <class Archive>
        void serialize(Archive &ar, const unsigned int version) {
          ar &kind;
          ar &usrs;
        }

      public:
        clangmm::Cursor::Kind kind;
        std::unordered_set<std::string> usrs;

        bool operator==(const Cursor &o);
      };

      class Token {
        friend class boost::serialization::access;
        template <class Archive>
        void serialize(Archive &ar, const unsigned int version) {
          ar &spelling;
          ar &offsets.first.line &offsets.first.index;
          ar &offsets.second.line &offsets.second.index;
          ar &cursor_id;
        }

      public:
        std::string spelling;
        std::pair<clangmm::Offset, clangmm::Offset> offsets;
        size_t cursor_id;
      };

      boost::filesystem::path project_path;
      boost::filesystem::path build_path;

      std::vector<Token> tokens;
      std::vector<Cursor> cursors;
      std::map<boost::filesystem::path, std::time_t> paths_and_last_write_times;

      Cache() = default;
      Cache(boost::filesystem::path project_path_, boost::filesystem::path build_path_, const boost::filesystem::path &path,
            std::time_t before_parse_time, clangmm::TranslationUnit *translation_unit, clangmm::Tokens *clang_tokens);

      operator bool() const { return !paths_and_last_write_times.empty(); }

      std::vector<std::pair<clangmm::Offset, clangmm::Offset>> get_similar_token_offsets(clangmm::Cursor::Kind kind, const std::string &spelling,
                                                                                         const std::unordered_set<std::string> &usrs) const;
    };

  private:
    const static boost::filesystem::path cache_folder;

    static std::map<boost::filesystem::path, Cache> caches;
    static std::mutex caches_mutex;

    static std::atomic<size_t> cache_in_progress_count;

  public:
    static std::vector<Usages> get_usages(const boost::filesystem::path &project_path, const boost::filesystem::path &build_path, const boost::filesystem::path &debug_path,
                                          const std::string &spelling, const clangmm::Cursor &cursor, const std::vector<clangmm::TranslationUnit *> &translation_units);

    static void cache(const boost::filesystem::path &project_path, const boost::filesystem::path &build_path, const boost::filesystem::path &path,
                      std::time_t before_parse_time, const PathSet &project_paths_in_use, clangmm::TranslationUnit *translation_unit, clangmm::Tokens *tokens);
    static void erase_unused_caches(const PathSet &project_paths_in_use);
    static void erase_cache(const boost::filesystem::path &path);
    static void erase_all_caches_for_project(const boost::filesystem::path &project_path, const boost::filesystem::path &build_path);
    static void cache_in_progress();

  private:
    static void add_usages(const boost::filesystem::path &project_path, const boost::filesystem::path &build_path, const boost::filesystem::path &path_,
                           std::vector<Usages> &usages, PathSet &visited, const std::string &spelling, clangmm::Cursor cursor,
                           clangmm::TranslationUnit *translation_unit, bool store_in_cache);

    static bool add_usages_from_cache(const boost::filesystem::path &path, std::vector<Usages> &usages, PathSet &visited,
                                      const std::string &spelling, const clangmm::Cursor &cursor, const Cache &cache);

    static void add_usages_from_includes(const boost::filesystem::path &project_path, const boost::filesystem::path &build_path,
                                         std::vector<Usages> &usages, PathSet &visited, const std::string &spelling, const clangmm::Cursor &cursor,
                                         clangmm::TranslationUnit *translation_unit, bool store_in_cache);

    static PathSet find_paths(const boost::filesystem::path &project_path,
                              const boost::filesystem::path &build_path, const boost::filesystem::path &debug_path);

    static bool is_header(const boost::filesystem::path &path);
    static bool is_source(const boost::filesystem::path &path);

    static std::pair<std::map<boost::filesystem::path, PathSet>, PathSet> parse_paths(const std::string &spelling, const PathSet &paths);

    /// Recursively find and return all the include paths of path
    static PathSet get_all_includes(const boost::filesystem::path &path, const std::map<boost::filesystem::path, PathSet> &paths_includes);

    /// Based on cursor paths, paths_includes and paths_with_spelling return potential paths that might contain the sought after symbol
    static std::pair<Clang::PathSet, Clang::PathSet> find_potential_paths(const PathSet &paths, const boost::filesystem::path &project_path,
                                                                          const std::map<boost::filesystem::path, PathSet> &paths_includes, const PathSet &paths_with_spelling);

    static void write_cache(const boost::filesystem::path &path, const Cache &cache);
    static Cache read_cache(const boost::filesystem::path &project_path, const boost::filesystem::path &build_path, const boost::filesystem::path &path);
  };
} // namespace Usages
