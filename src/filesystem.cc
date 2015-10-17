#include "filesystem.h"
#include "singletons.h"
#include <fstream>
#include <sstream>
#include <iostream>

const size_t buffer_size=131072;

//Only use on small files
std::string juci::filesystem::read(const std::string &path) {
  std::stringstream ss;
  std::ifstream input(path, std::ofstream::binary);
  if(input) {
    ss << input.rdbuf();
    input.close();
  }
  return ss.str();
}

std::string safe_get_env(const std::string &env) {
  auto ptr = std::getenv(env.c_str());
  return nullptr==ptr ? "" : std::string(ptr);
}

/**
 * Returns home folder, empty on error
 */
std::string juci::filesystem::get_home_folder() {
  auto home=safe_get_env("HOME");
  if(home.empty())
    home=safe_get_env("AppData");
  auto status = boost::filesystem::status(home);
  if((status.permissions() & 0222)>=2) {
    return home;
  } else {
    JERROR("No write permissions in home, var:");
    DEBUG_VAR(home);
    return "";
  }
}

/**
 * Returns tmp folder, empty on error.
 */
std::string juci::filesystem::get_tmp_folder() {
  boost::system::error_code code;
  auto path = boost::filesystem::temp_directory_path(code);
  if (code.value()!=0) {
    return "";
  }
  return path.string();
}

int juci::filesystem::read(const std::string &path, Glib::RefPtr<Gtk::TextBuffer> text_buffer) {
  std::ifstream input(path, std::ofstream::binary);
  
  if(input) {
    //need to read the whole file to make this work...
    std::stringstream ss;
    ss << input.rdbuf();
    Glib::ustring ustr=std::move(ss.str());
    
    bool valid=true;
    
    if(ustr.validate())
      text_buffer->insert_at_cursor(ustr);
    else
      valid=false;
    
    //TODO: maybe correct this, emphasis on maybe:
    /*std::vector<char> buffer(buffer_size);
    size_t read_length;
    while((read_length=input.read(&buffer[0], buffer_size).gcount())>0) {
      //auto ustr=Glib::ustring(std::string(&buffer[0], read_length)); //this works...

      //this does not work:
      Glib::ustring ustr;
      ustr.resize(read_length);
      ustr.replace(0, read_length, &buffer[0]);
      
      Glib::ustring::iterator iter;
      while(!ustr.validate(iter)) {
        auto next_char_iter=iter;
        next_char_iter++;
        ustr.replace(iter, next_char_iter, "?");
      }
      
      text_buffer->insert_at_cursor(ustr); //What if insert happens in the middle of an UTF-8 char???
    }*/
    input.close();
    if(valid)
      return 1;
    else
      return -1;
  }
  return 0;
}

int juci::filesystem::read_non_utf8(const std::string &path, Glib::RefPtr<Gtk::TextBuffer> text_buffer) {
  std::ifstream input(path, std::ofstream::binary);
  
  if(input) {
    //need to read the whole file to make this work...
    std::stringstream ss;
    ss << input.rdbuf();
    Glib::ustring ustr=std::move(ss.str());
    
    bool valid=true;
    Glib::ustring::iterator iter;
    while(!ustr.validate(iter)) {
      auto next_char_iter=iter;
      next_char_iter++;
      ustr.replace(iter, next_char_iter, "?");
      valid=false;
    }
    
    text_buffer->insert_at_cursor(ustr);
    
    input.close();
    if(valid)
      return 1;
    else
      return -1;
  }
  return 0;
}

//Only use on small files
std::vector<std::string> juci::filesystem::read_lines(const std::string &path) {
  std::vector<std::string> res;
  std::ifstream input(path, std::ofstream::binary);
  if (input) {
    do { res.emplace_back(); } while(getline(input, res.back()));
  }
  input.close();
  return res;
}

//Only use on small files
bool juci::filesystem::write(const std::string &path, const std::string &new_content) {
  std::ofstream output(path, std::ofstream::binary);
  if(output)
    output << new_content;
  else
    return false;
  output.close();
  return true;
}

bool juci::filesystem::write(const std::string &path, Glib::RefPtr<Gtk::TextBuffer> buffer) {
  std::ofstream output(path, std::ofstream::binary);
  if(output) {
    auto start_iter=buffer->begin();
    auto end_iter=start_iter;
    bool end_reached=false;
    while(!end_reached) {
      for(size_t c=0;c<buffer_size;c++) {
        if(!end_iter.forward_char()) {
          end_reached=true;
          break;
        }
      }
      output << buffer->get_text(start_iter, end_iter).c_str();
      start_iter=end_iter;
    }
    output.close();
    return true;
  }
  return false;
}
