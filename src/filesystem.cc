#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>

#include "filesystem.h"

//Only use on small files
std::string filesystem::read(const std::string &path) {
  std::stringstream ss;
  std::ifstream input(path, std::ofstream::binary);
  if(input) {
    ss << input.rdbuf();
    input.close();
  }
  return ss.str();
}

//Only use on small files
std::vector<std::string> filesystem::read_lines(const std::string &path) {
  std::vector<std::string> res;
  std::ifstream input(path, std::ofstream::binary);
  if (input) {
    do { res.emplace_back(); } while(getline(input, res.back()));
  }
  input.close();
  return res;
}

//Only use on small files
bool filesystem::write(const std::string &path, const std::string &new_content) {
  std::ofstream output(path, std::ofstream::binary);
  if(output)
    output << new_content;
  else
    return false;
  output.close();
  return true;
}

std::string filesystem::escape_argument(const std::string &argument) {
  auto escaped=argument;
  for(size_t pos=0;pos<escaped.size();++pos) {
    if(escaped[pos]==' ' || escaped[pos]=='(' || escaped[pos]==')' || escaped[pos]=='\'' || escaped[pos]=='"') {
      escaped.insert(pos, "\\");
      ++pos;
    }
  }
  return escaped;
}

std::string filesystem::unescape_argument(const std::string &argument) {
  auto unescaped=argument;
  
  if(unescaped.size()>=2) {
    if((unescaped[0]=='\'' && unescaped[unescaped.size()-1]=='\'') ||
       (unescaped[0]=='"' && unescaped[unescaped.size()-1]=='"')) {
      char quotation_mark=unescaped[0];
      unescaped=unescaped.substr(1, unescaped.size()-2);
      size_t backslash_count=0;
      for(size_t pos=0;pos<unescaped.size();++pos) {
        if(backslash_count%2==1 && (unescaped[pos]=='\\' || unescaped[pos]==quotation_mark)) {
          unescaped.erase(pos-1, 1);
          --pos;
          backslash_count=0;
        }
        else if(unescaped[pos]=='\\')
          ++backslash_count;
        else
          backslash_count=0;
      }
      return unescaped;
    }
  }
  
  size_t backslash_count=0;
  for(size_t pos=0;pos<unescaped.size();++pos) {
    if(backslash_count%2==1 && (unescaped[pos]=='\\' || unescaped[pos]==' ' || unescaped[pos]=='(' || unescaped[pos]==')' || unescaped[pos]=='\'' || unescaped[pos]=='"')) {
      unescaped.erase(pos-1, 1);
      --pos;
      backslash_count=0;
    }
    else if(unescaped[pos]=='\\')
      ++backslash_count;
    else
      backslash_count=0;
  }
  return unescaped;
}

boost::filesystem::path filesystem::get_home_path() noexcept {
  std::vector<std::string> environment_variables = {"HOME", "AppData"};
  char *ptr = nullptr;
  for (auto &variable : environment_variables) {
    ptr=std::getenv(variable.c_str());
    boost::system::error_code ec;
    if (ptr!=nullptr && boost::filesystem::exists(ptr, ec))
      return ptr;
  }
  return boost::filesystem::path();
}

boost::filesystem::path filesystem::get_short_path(const boost::filesystem::path &path) noexcept {
#ifdef _WIN32
  return path;
#else
  static auto home_path=get_home_path();
  if(!home_path.empty()) {
    auto relative_path=filesystem::get_relative_path(path, home_path);
    if(!relative_path.empty())
      return "~"/relative_path;
  }
  return path;
#endif
}

bool filesystem::file_in_path(const boost::filesystem::path &file_path, const boost::filesystem::path &path) {
  if(std::distance(file_path.begin(), file_path.end())<std::distance(path.begin(), path.end()))
    return false;
  return std::equal(path.begin(), path.end(), file_path.begin());
}

boost::filesystem::path filesystem::find_file_in_path_parents(const std::string &file_name, const boost::filesystem::path &path) {
  auto current_path=path;
  while(true) {
    auto test_path=current_path/file_name;
    if(boost::filesystem::exists(test_path))
      return test_path;
    if(current_path==current_path.root_directory())
      return boost::filesystem::path();
    current_path=current_path.parent_path();
  }
}

boost::filesystem::path filesystem::get_normal_path(const boost::filesystem::path &path) noexcept {
  boost::filesystem::path normal_path;
  
  for(auto &e: path) {
    if(e==".")
      continue;
    else if(e=="..") {
      auto parent_path=normal_path.parent_path();
      if(!parent_path.empty())
        normal_path=parent_path;
      else
        normal_path/=e;
    }
    else if(e.empty())
      continue;
    else
      normal_path/=e;
  }
  
  return normal_path;
}

boost::filesystem::path filesystem::get_relative_path(const boost::filesystem::path &path, const boost::filesystem::path &base) noexcept {
  boost::filesystem::path relative_path;

  if(std::distance(path.begin(), path.end())<std::distance(base.begin(), base.end()))
    return boost::filesystem::path();
  
  auto base_it=base.begin();
  auto path_it=path.begin();
  while(path_it!=path.end() && base_it!=base.end()) {
    if(*path_it!=*base_it)
      return boost::filesystem::path();
    ++path_it;
    ++base_it;
  }
  for(;path_it!=path.end();++path_it)
    relative_path/=*path_it;

  return relative_path;
}

boost::filesystem::path filesystem::get_executable(const boost::filesystem::path &executable_name) noexcept {
#if defined(__APPLE__) || defined(_WIN32)
  return executable_name;
#endif

  static std::vector<boost::filesystem::path> bin_paths={"/usr/bin", "/usr/local/bin"};
  
  try {
    for(auto &path: bin_paths) {
      if(boost::filesystem::exists(path/executable_name))
        return executable_name;
    }
    
    auto &executable_name_str = executable_name.string();
    for(auto &path: bin_paths) {
      boost::filesystem::path executable;
      for(boost::filesystem::directory_iterator it(path), end; it != end; ++it) {
        auto it_path = it->path();
        auto it_path_filename_str = it_path.filename().string();
        if(!it_path_filename_str.empty() && it_path_filename_str.compare(0, executable_name_str.size(), executable_name_str)==0) {
          if(it_path > executable &&
             ((it_path_filename_str.size() > executable_name_str.size() &&
               it_path_filename_str[executable_name_str.size()]>='0' &&
               it_path_filename_str[executable_name_str.size()]<='9') ||
              (it_path_filename_str.size() > executable_name_str.size()+1 &&
               it_path_filename_str[executable_name_str.size()]=='-' &&
               it_path_filename_str[executable_name_str.size()+1]>='0' &&
               it_path_filename_str[executable_name_str.size()+1]<='9')) &&
             !boost::filesystem::is_directory(it_path))
            executable=it_path;
        }
      }
      if(!executable.empty())
        return executable;
    }
  }
  catch(...) {}
  
  return executable_name;
}

// Based on https://stackoverflow.com/a/11295568
const std::vector<boost::filesystem::path> &filesystem::get_executable_search_paths() {
  static std::vector<boost::filesystem::path> result;
  if(!result.empty())
    return result;

  const std::string env = getenv("PATH");
  const char delimiter = ':';

  size_t previous = 0;
  size_t pos;
  while((pos = env.find(delimiter, previous)) != std::string::npos) {
    result.emplace_back(env.substr(previous, pos - previous));
    previous = pos + 1;
  }
  result.emplace_back(env.substr(previous));

  return result;
}

boost::filesystem::path filesystem::find_executable(const std::string &executable_name) {
  for(auto &path: get_executable_search_paths()) {
    boost::system::error_code ec;
    auto executable_path=path/executable_name;
    if(boost::filesystem::exists(executable_path, ec))
      return executable_path;
  }
  return boost::filesystem::path();
}
