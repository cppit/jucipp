#include "source_base.h"
#include "info.h"
#include "terminal.h"
#include "git.h"
#include <fstream>

Source::BaseView::BaseView(const boost::filesystem::path &file_path, Glib::RefPtr<Gsv::Language> language): file_path(file_path), language(language), status_diagnostics(0, 0, 0) {
  boost::system::error_code ec;
  canonical_file_path=boost::filesystem::canonical(file_path, ec);
  if(ec)
    canonical_file_path=file_path;
  
  last_write_time=boost::filesystem::last_write_time(file_path, ec);
  if(ec)
    last_write_time=static_cast<std::time_t>(-1);
  
  signal_focus_in_event().connect([this](GdkEventFocus *event) {
    check_last_write_time();
    return false;
  });
}

bool Source::BaseView::load() {
  disable_spellcheck=true;
  get_source_buffer()->begin_not_undoable_action();
  bool status=true;
  if(language) {
    std::ifstream input(file_path.string(), std::ofstream::binary);
    if(input) {
      std::stringstream ss;
      ss << input.rdbuf();
      Glib::ustring ustr=ss.str();
      
      bool valid=true;
      Glib::ustring::iterator iter;
      while(!ustr.validate(iter)) {
        auto next_char_iter=iter;
        next_char_iter++;
        ustr.replace(iter, next_char_iter, "?");
        valid=false;
      }
      if(get_buffer()->size()==0)
        get_buffer()->insert_at_cursor(ustr);
      else
        replace_text(ustr.raw());
    
      if(!valid)
        Terminal::get().print("Warning: "+file_path.string()+" is not a valid UTF-8 file. Saving might corrupt the file.\n");
    }
  }
  else {
    std::ifstream input(file_path.string(), std::ofstream::binary);
    if(input) {
      std::stringstream ss;
      ss << input.rdbuf();
      Glib::ustring ustr=ss.str();
      
      bool valid=true;
      if(ustr.validate()) {
        if(get_buffer()->size()==0)
          get_buffer()->insert_at_cursor(ustr);
        else
          replace_text(ustr.raw());
      }
      else
        valid=false;
      
      if(!valid)
        Terminal::get().print("Error: "+file_path.string()+" is not a valid UTF-8 file.\n", true);
      status=false;
    }
  }
  get_source_buffer()->end_not_undoable_action();
  disable_spellcheck=false;
  
  boost::system::error_code ec;
  last_write_time=boost::filesystem::last_write_time(file_path, ec);
  if(ec)
    last_write_time=static_cast<std::time_t>(-1);
  
  return status;
}

void Source::BaseView::replace_text(const std::string &new_text) {
  get_buffer()->begin_user_action();
  
  if(get_buffer()->size()==0) {
    get_buffer()->insert_at_cursor(new_text);
    get_buffer()->end_user_action();
    return;
  }
  else if(new_text.empty()) {
    get_buffer()->set_text(new_text);
    get_buffer()->end_user_action();
    return;
  }
  
  auto iter=get_buffer()->get_insert()->get_iter();
  int cursor_line_nr=iter.get_line();
  int cursor_line_offset=iter.ends_line() ? std::numeric_limits<int>::max() : iter.get_line_offset();
  
  std::vector<std::pair<const char*, const char*>> new_lines;
  
  const char* line_start=new_text.c_str();
  for(size_t i=0;i<new_text.size();++i) {
    if(new_text[i]=='\n') {
      new_lines.emplace_back(line_start, &new_text[i]+1);
      line_start = &new_text[i]+1;
    }
  }
  if(new_text.empty() || new_text.back()!='\n')
    new_lines.emplace_back(line_start, &new_text[new_text.size()]);
  
  try {
    auto hunks = Git::Repository::Diff::get_hunks(get_buffer()->get_text().raw(), new_text);
    
    for(auto it=hunks.rbegin();it!=hunks.rend();++it) {
      bool place_cursor=false;
      Gtk::TextIter start;
      if(it->old_lines.second!=0) {
        start=get_buffer()->get_iter_at_line(it->old_lines.first-1);
        auto end=get_buffer()->get_iter_at_line(it->old_lines.first-1+it->old_lines.second);
        
        if(cursor_line_nr>=start.get_line() && cursor_line_nr<end.get_line()) {
          if(it->new_lines.second!=0) {
            place_cursor = true;
            int line_diff=cursor_line_nr-start.get_line();
            cursor_line_nr+=static_cast<int>(0.5+(static_cast<float>(line_diff)/it->old_lines.second)*it->new_lines.second)-line_diff;
          }
        }
        
        get_buffer()->erase(start, end);
        start=get_buffer()->get_iter_at_line(it->old_lines.first-1);
      }
      else
        start=get_buffer()->get_iter_at_line(it->old_lines.first);
      if(it->new_lines.second!=0) {
        get_buffer()->insert(start, new_lines[it->new_lines.first-1].first, new_lines[it->new_lines.first-1+it->new_lines.second-1].second);
        if(place_cursor)
          place_cursor_at_line_offset(cursor_line_nr, cursor_line_offset);
      }
    }
  }
  catch(...) {
    Terminal::get().print("Error: Could not replace text in buffer\n", true);
  }
  
  get_buffer()->end_user_action();
}

void Source::BaseView::rename(const boost::filesystem::path &path) {
  {
    std::unique_lock<std::mutex> lock(file_path_mutex);
    file_path=path;
    
    boost::system::error_code ec;
    canonical_file_path=boost::filesystem::canonical(file_path, ec);
    if(ec)
      canonical_file_path=file_path;
  }
  if(update_status_file_path)
    update_status_file_path(this);
  if(update_tab_label)
    update_tab_label(this);
}

Gtk::TextIter Source::BaseView::get_iter_at_line_end(int line_nr) {
  if(line_nr>=get_buffer()->get_line_count())
    return get_buffer()->end();
  else if(line_nr+1<get_buffer()->get_line_count()) {
    auto iter=get_buffer()->get_iter_at_line(line_nr+1);
    iter.backward_char();
    if(!iter.ends_line()) // for CR+LF
      iter.backward_char();
    return iter;
  }
  else {
    auto iter=get_buffer()->get_iter_at_line(line_nr);
    while(!iter.ends_line() && iter.forward_char()) {}
    return iter;
  }
}

Gtk::TextIter Source::BaseView::get_iter_at_line_pos(int line, int pos) {
  line=std::min(line, get_buffer()->get_line_count()-1);
  if(line<0)
    line=0;
  auto iter=get_iter_at_line_end(line);
  pos=std::min(pos, iter.get_line_index());
  if(pos<0)
    pos=0;
  return get_buffer()->get_iter_at_line_index(line, pos);
}

void Source::BaseView::place_cursor_at_line_pos(int line, int pos) {
  get_buffer()->place_cursor(get_iter_at_line_pos(line, pos));
}

void Source::BaseView::place_cursor_at_line_offset(int line, int offset) {
  line=std::min(line, get_buffer()->get_line_count()-1);
  if(line<0)
    line=0;
  auto iter=get_iter_at_line_end(line);
  offset=std::min(offset, iter.get_line_offset());
  get_buffer()->place_cursor(get_buffer()->get_iter_at_line_offset(line, offset));
}

void Source::BaseView::place_cursor_at_line_index(int line, int index) {
  line=std::min(line, get_buffer()->get_line_count()-1);
  if(line<0)
    line=0;
  auto iter=get_iter_at_line_end(line);
  index=std::min(index, iter.get_line_index());
  get_buffer()->place_cursor(get_buffer()->get_iter_at_line_index(line, index));
}

Gtk::TextIter Source::BaseView::get_smart_home_iter(const Gtk::TextIter &iter) {
  auto start_line_iter=get_buffer()->get_iter_at_line(iter.get_line());
  auto start_sentence_iter=start_line_iter;
  while(!start_sentence_iter.ends_line() && 
        (*start_sentence_iter==' ' || *start_sentence_iter=='\t') &&
        start_sentence_iter.forward_char()) {}
  
  if(iter>start_sentence_iter || iter==start_line_iter)
    return start_sentence_iter;
  else
    return start_line_iter;
}
Gtk::TextIter Source::BaseView::get_smart_end_iter(const Gtk::TextIter &iter) {
  auto end_line_iter=get_iter_at_line_end(iter.get_line());
  auto end_sentence_iter=end_line_iter;
  while(!end_sentence_iter.starts_line() && 
        (*end_sentence_iter==' ' || *end_sentence_iter=='\t' || end_sentence_iter.ends_line()) &&
        end_sentence_iter.backward_char()) {}
  if(!end_sentence_iter.ends_line() && *end_sentence_iter!=' ' && *end_sentence_iter!='\t')
    end_sentence_iter.forward_char();
  
  if(iter==end_line_iter)
    return end_sentence_iter;
  else
    return end_line_iter;
}

std::string Source::BaseView::get_line(const Gtk::TextIter &iter) {
  auto line_start_it = get_buffer()->get_iter_at_line(iter.get_line());
  auto line_end_it = get_iter_at_line_end(iter.get_line());
  std::string line(get_buffer()->get_text(line_start_it, line_end_it));
  return line;
}
std::string Source::BaseView::get_line(Glib::RefPtr<Gtk::TextBuffer::Mark> mark) {
  return get_line(mark->get_iter());
}
std::string Source::BaseView::get_line(int line_nr) {
  return get_line(get_buffer()->get_iter_at_line(line_nr));
}
std::string Source::BaseView::get_line() {
  return get_line(get_buffer()->get_insert());
}

std::string Source::BaseView::get_line_before(const Gtk::TextIter &iter) {
  auto line_it = get_buffer()->get_iter_at_line(iter.get_line());
  std::string line(get_buffer()->get_text(line_it, iter));
  return line;
}
std::string Source::BaseView::get_line_before(Glib::RefPtr<Gtk::TextBuffer::Mark> mark) {
  return get_line_before(mark->get_iter());
}
std::string Source::BaseView::get_line_before() {
  return get_line_before(get_buffer()->get_insert());
}

Gtk::TextIter Source::BaseView::get_tabs_end_iter(const Gtk::TextIter &iter) {
  return get_tabs_end_iter(iter.get_line());
}
Gtk::TextIter Source::BaseView::get_tabs_end_iter(Glib::RefPtr<Gtk::TextBuffer::Mark> mark) {
  return get_tabs_end_iter(mark->get_iter());
}
Gtk::TextIter Source::BaseView::get_tabs_end_iter(int line_nr) {
  auto sentence_iter = get_buffer()->get_iter_at_line(line_nr);
  while((*sentence_iter==' ' || *sentence_iter=='\t') && !sentence_iter.ends_line() && sentence_iter.forward_char()) {}
  return sentence_iter;
}
Gtk::TextIter Source::BaseView::get_tabs_end_iter() {
  return get_tabs_end_iter(get_buffer()->get_insert());
}

void Source::BaseView::check_last_write_time() {
  if(has_focus()) {
    boost::system::error_code ec;
    auto last_write_time=boost::filesystem::last_write_time(file_path, ec);
    if(!ec && this->last_write_time!=static_cast<std::time_t>(-1) && last_write_time!=this->last_write_time)
      Info::get().print("Caution: " + file_path.filename().string() + " was changed outside of juCi++");
  }
}
