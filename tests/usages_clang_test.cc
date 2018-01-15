#include "clangmm.h"
#include "compile_commands.h"
#include "meson.h"
#include "project.h"
#include "usages_clang.h"
#include <cassert>
#include <fstream>

#include <iostream>

int main() {
  auto tests_path = boost::filesystem::canonical(JUCI_TESTS_PATH);
  auto project_path = boost::filesystem::canonical(tests_path / "usages_clang_test_files");
  auto build_path = project_path / "build";

  auto build = Project::Build::create(project_path);
  g_assert(build->project_path == project_path);

  {
    clangmm::Index index(0, 0);
    auto path = project_path / "main.cpp";
    std::ifstream stream(path.string(), std::ifstream::binary);
    assert(stream);
    std::string buffer;
    buffer.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    auto arguments = CompileCommands::get_arguments(build_path, path);
    clangmm::TranslationUnit translation_unit(index, path.string(), arguments, buffer);
    auto tokens = translation_unit.get_tokens();
    clangmm::Token *found_token = nullptr;
    for(auto &token : *tokens) {
      if(token.get_spelling() == "a") {
        found_token = &token;
        break;
      }
    }
    assert(found_token);
    auto spelling = found_token->get_spelling();
    auto cursor = found_token->get_cursor().get_referenced();
    std::vector<Usages::Clang::Usages> usages;
    Usages::Clang::PathSet visited;

    Usages::Clang::add_usages(project_path, build_path, boost::filesystem::path(), usages, visited, spelling, cursor, &translation_unit, false);
    assert(usages.size() == 1);
    assert(usages[0].path == path);
    assert(usages[0].lines.size() == 1);
    assert(usages[0].lines[0] == "  test.a=2;");
    assert(usages[0].offsets.size() == 1);
    assert(usages[0].offsets[0].first.line == 6);
    assert(usages[0].offsets[0].first.index == 8);
    assert(usages[0].offsets[0].second.line == 6);
    assert(usages[0].offsets[0].second.index == 9);

    Usages::Clang::add_usages_from_includes(project_path, build_path, usages, visited, spelling, cursor, &translation_unit, false);
    assert(usages.size() == 2);
    assert(usages[1].path == project_path / "test.hpp");
    assert(usages[1].lines.size() == 2);
    assert(usages[1].lines[0] == "  int a=0;");
    assert(usages[1].lines[1] == "    ++a;");
    assert(usages[1].offsets.size() == 2);
    assert(usages[1].offsets[0].first.line == 6);
    assert(usages[1].offsets[0].first.index == 7);
    assert(usages[1].offsets[0].second.line == 6);
    assert(usages[1].offsets[0].second.index == 8);
    assert(usages[1].offsets[1].first.line == 8);
    assert(usages[1].offsets[1].first.index == 7);
    assert(usages[1].offsets[1].second.line == 8);
    assert(usages[1].offsets[1].second.index == 8);

    auto paths = Usages::Clang::find_paths(project_path, build_path, build_path / "debug");
    auto pair = Usages::Clang::parse_paths(spelling, paths);

    auto &paths_includes = pair.first;
    assert(paths_includes.size() == 3);
    assert(paths_includes.find(project_path / "main.cpp") != paths_includes.end());
    assert(paths_includes.find(project_path / "test.hpp") != paths_includes.end());
    assert(paths_includes.find(project_path / "test2.hpp") != paths_includes.end());

    auto &paths_with_spelling = pair.second;
    assert(paths_with_spelling.size() == 3);
    assert(paths_with_spelling.find(project_path / "main.cpp") != paths_with_spelling.end());
    assert(paths_with_spelling.find(project_path / "test.hpp") != paths_with_spelling.end());
    assert(paths_with_spelling.find(project_path / "test2.hpp") != paths_with_spelling.end());

    auto pair2 = Usages::Clang::find_potential_paths({cursor.get_canonical().get_source_location().get_path()}, project_path, pair.first, pair.second);

    auto &potential_paths = pair2.first;
    assert(potential_paths.size() == 3);
    assert(potential_paths.find(project_path / "main.cpp") != potential_paths.end());
    assert(potential_paths.find(project_path / "test.hpp") != potential_paths.end());
    assert(potential_paths.find(project_path / "test2.hpp") != potential_paths.end());

    auto &all_includes = pair2.second;
    assert(all_includes.size() == 1);
    assert(*all_includes.begin() == project_path / "test.hpp");

    // Remove visited paths
    for(auto it = potential_paths.begin(); it != potential_paths.end();) {
      if(visited.find(*it) != visited.end())
        it = potential_paths.erase(it);
      else
        ++it;
    }

    assert(potential_paths.size() == 1);
    assert(*potential_paths.begin() == project_path / "test2.hpp");

    {
      auto path = *potential_paths.begin();
      std::ifstream stream(path.string(), std::ifstream::binary);
      std::string buffer;
      buffer.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
      clangmm::TranslationUnit translation_unit(index, path.string(), arguments, buffer);
      Usages::Clang::add_usages(project_path, build_path, path, usages, visited, spelling, cursor, &translation_unit, true);
      Usages::Clang::add_usages_from_includes(project_path, build_path, usages, visited, spelling, cursor, &translation_unit, true);
      assert(usages.size() == 3);
      assert(usages[2].path == path);
      assert(usages[2].lines.size() == 1);
      assert(usages[2].lines[0] == "    ++a;");
      assert(usages[2].offsets.size() == 1);
      assert(usages[2].offsets[0].first.line == 5);
      assert(usages[2].offsets[0].first.index == 7);
      assert(usages[2].offsets[0].second.line == 5);
      assert(usages[2].offsets[0].second.index == 8);
    }

    assert(Usages::Clang::caches.size() == 1);
    auto cache_it = Usages::Clang::caches.begin();
    assert(cache_it->first == project_path / "test2.hpp");
    assert(cache_it->second.build_path == build_path);
    assert(cache_it->second.paths_and_last_write_times.size() == 2);
    assert(cache_it->second.paths_and_last_write_times.find(project_path / "test2.hpp") != cache_it->second.paths_and_last_write_times.end());
    assert(cache_it->second.paths_and_last_write_times.find(project_path / "test.hpp") != cache_it->second.paths_and_last_write_times.end());
    assert(cache_it->second.tokens.size());
    assert(cache_it->second.cursors.size());
    {
      std::vector<Usages::Clang::Usages> usages;
      Usages::Clang::PathSet visited;
      Usages::Clang::add_usages_from_cache(cache_it->first, usages, visited, spelling, cursor, cache_it->second);
      assert(usages.size() == 1);
      assert(usages[0].path == cache_it->first);
      assert(usages[0].lines.size() == 1);
      assert(usages[0].lines[0] == "    ++a;");
      assert(usages[0].offsets.size() == 1);
      assert(usages[0].offsets[0].first.line == 5);
      assert(usages[0].offsets[0].first.index == 7);
      assert(usages[0].offsets[0].second.line == 5);
      assert(usages[0].offsets[0].second.index == 8);
    }

    Usages::Clang::erase_unused_caches({project_path});
    Usages::Clang::cache(project_path, build_path, path, time(nullptr), {project_path}, &translation_unit, tokens.get());
    assert(Usages::Clang::caches.size() == 3);
    assert(Usages::Clang::caches.find(project_path / "main.cpp") != Usages::Clang::caches.end());
    assert(Usages::Clang::caches.find(project_path / "test.hpp") != Usages::Clang::caches.end());
    assert(Usages::Clang::caches.find(project_path / "test2.hpp") != Usages::Clang::caches.end());

    Usages::Clang::erase_unused_caches({});
    Usages::Clang::cache(project_path, build_path, path, time(nullptr), {}, &translation_unit, tokens.get());
    assert(Usages::Clang::caches.size() == 0);

    auto cache = Usages::Clang::read_cache(project_path, build_path, project_path / "main.cpp");
    assert(cache);
    Usages::Clang::caches.emplace(project_path / "main.cpp", std::move(cache));
    assert(!cache);
    assert(Usages::Clang::caches.size() == 1);
    cache = Usages::Clang::read_cache(project_path, build_path, project_path / "test.hpp");
    assert(cache);
    Usages::Clang::caches.emplace(project_path / "test.hpp", std::move(cache));
    assert(!cache);
    assert(Usages::Clang::caches.size() == 2);
    cache = Usages::Clang::read_cache(project_path, build_path, project_path / "test2.hpp");
    assert(cache);
    Usages::Clang::caches.emplace(project_path / "test2.hpp", std::move(cache));
    assert(!cache);
    assert(Usages::Clang::caches.size() == 3);
    cache = Usages::Clang::read_cache(project_path, build_path, project_path / "test_not_existing.hpp");
    assert(!cache);
    assert(Usages::Clang::caches.size() == 3);
    {
      auto cache_it = Usages::Clang::caches.find(project_path / "main.cpp");
      assert(cache_it != Usages::Clang::caches.end());
      assert(cache_it->first == project_path / "main.cpp");
      assert(cache_it->second.build_path == build_path);
      assert(cache_it->second.paths_and_last_write_times.size() == 2);
      assert(cache_it->second.paths_and_last_write_times.find(project_path / "main.cpp") != cache_it->second.paths_and_last_write_times.end());
      assert(cache_it->second.paths_and_last_write_times.find(project_path / "test.hpp") != cache_it->second.paths_and_last_write_times.end());
      assert(cache_it->second.tokens.size());
      assert(cache_it->second.cursors.size());
      {
        std::vector<Usages::Clang::Usages> usages;
        Usages::Clang::PathSet visited;
        Usages::Clang::add_usages_from_cache(cache_it->first, usages, visited, spelling, cursor, cache_it->second);
        assert(usages.size() == 1);
        assert(usages[0].path == cache_it->first);
        assert(usages[0].lines.size() == 1);
        assert(usages[0].lines[0] == "  test.a=2;");
        assert(usages[0].offsets.size() == 1);
        assert(usages[0].offsets[0].first.line == 6);
        assert(usages[0].offsets[0].first.index == 8);
        assert(usages[0].offsets[0].second.line == 6);
        assert(usages[0].offsets[0].second.index == 9);
      }
    }
    {
      auto cache_it = Usages::Clang::caches.find(project_path / "test.hpp");
      assert(cache_it != Usages::Clang::caches.end());
      assert(cache_it->first == project_path / "test.hpp");
      assert(cache_it->second.build_path == build_path);
      assert(cache_it->second.paths_and_last_write_times.size() == 1);
      assert(cache_it->second.paths_and_last_write_times.find(project_path / "test.hpp") != cache_it->second.paths_and_last_write_times.end());
      assert(cache_it->second.tokens.size());
      assert(cache_it->second.cursors.size());
      {
        std::vector<Usages::Clang::Usages> usages;
        Usages::Clang::PathSet visited;
        Usages::Clang::add_usages_from_cache(cache_it->first, usages, visited, spelling, cursor, cache_it->second);
        assert(usages.size() == 1);
        assert(usages[0].path == cache_it->first);
        assert(usages[0].lines.size() == 2);
        assert(usages[0].lines[0] == "  int a=0;");
        assert(usages[0].lines[1] == "    ++a;");
        assert(usages[0].offsets.size() == 2);
        assert(usages[0].offsets[0].first.line == 6);
        assert(usages[0].offsets[0].first.index == 7);
        assert(usages[0].offsets[0].second.line == 6);
        assert(usages[0].offsets[0].second.index == 8);
        assert(usages[0].offsets[1].first.line == 8);
        assert(usages[0].offsets[1].first.index == 7);
        assert(usages[0].offsets[1].second.line == 8);
        assert(usages[0].offsets[1].second.index == 8);
      }
    }
    {
      auto cache_it = Usages::Clang::caches.find(project_path / "test2.hpp");
      assert(cache_it != Usages::Clang::caches.end());
      assert(cache_it->first == project_path / "test2.hpp");
      assert(cache_it->second.build_path == build_path);
      assert(cache_it->second.paths_and_last_write_times.size() == 2);
      assert(cache_it->second.paths_and_last_write_times.find(project_path / "test2.hpp") != cache_it->second.paths_and_last_write_times.end());
      assert(cache_it->second.paths_and_last_write_times.find(project_path / "test.hpp") != cache_it->second.paths_and_last_write_times.end());
      assert(cache_it->second.tokens.size());
      assert(cache_it->second.cursors.size());
      {
        std::vector<Usages::Clang::Usages> usages;
        Usages::Clang::PathSet visited;
        Usages::Clang::add_usages_from_cache(cache_it->first, usages, visited, spelling, cursor, cache_it->second);
        assert(usages.size() == 1);
        assert(usages[0].path == cache_it->first);
        assert(usages[0].lines.size() == 1);
        assert(usages[0].lines[0] == "    ++a;");
        assert(usages[0].offsets.size() == 1);
        assert(usages[0].offsets[0].first.line == 5);
        assert(usages[0].offsets[0].first.index == 7);
        assert(usages[0].offsets[0].second.line == 5);
        assert(usages[0].offsets[0].second.index == 8);
      }
    }
  }
  {
    assert(!Usages::Clang::caches.empty());
    assert(boost::filesystem::exists(build_path/Usages::Clang::cache_folder));
    assert(boost::filesystem::exists(build_path/Usages::Clang::cache_folder/"main.cpp.usages"));
    assert(boost::filesystem::exists(build_path/Usages::Clang::cache_folder/"test.hpp.usages"));
    assert(boost::filesystem::exists(build_path/Usages::Clang::cache_folder/"test2.hpp.usages"));
    
    Usages::Clang::erase_all_caches_for_project(project_path, build_path);
    assert(Usages::Clang::caches.empty());
    assert(boost::filesystem::exists(build_path/Usages::Clang::cache_folder));
    assert(!boost::filesystem::exists(build_path/Usages::Clang::cache_folder/"main.cpp.usages"));
    assert(!boost::filesystem::exists(build_path/Usages::Clang::cache_folder/"test.hpp.usages"));
    assert(!boost::filesystem::exists(build_path/Usages::Clang::cache_folder/"test2.hpp.usages"));
  }
}
