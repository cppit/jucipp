#include "source_diff.h"
#include "config.h"
#include "terminal.h"
#include <boost/version.hpp>

namespace sigc {
#ifndef SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
  template <typename Functor>
  struct functor_trait<Functor, false> {
    typedef decltype (::sigc::mem_fun(std::declval<Functor&>(),
                                      &Functor::operator())) _intermediate;
    typedef typename _intermediate::result_type result_type;
    typedef Functor functor_type;
  };
#else
  SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
#endif
}

Source::DiffView::Renderer::Renderer() : Gsv::GutterRenderer() {
  set_padding(4, 0);
}

void Source::DiffView::Renderer::draw_vfunc(const Cairo::RefPtr<Cairo::Context> &cr, const Gdk::Rectangle &background_area,
                                            const Gdk::Rectangle &cell_area, Gtk::TextIter &start, Gtk::TextIter &end,
                                            Gsv::GutterRendererState p6) {
  if(start.has_tag(tag_added) || end.has_tag(tag_added)) {
    cr->set_source_rgba(0.0, 1.0, 0.0, 0.5);
    cr->rectangle(cell_area.get_x(), cell_area.get_y(), 4, cell_area.get_height());
    cr->fill();
  }
  else if(start.has_tag(tag_modified) || end.has_tag(tag_modified)) {
    cr->set_source_rgba(0.9, 0.9, 0.0, 0.75);
    cr->rectangle(cell_area.get_x(), cell_area.get_y(), 4, cell_area.get_height());
    cr->fill();
  }
  if(start.has_tag(tag_removed_below) || end.has_tag(tag_removed_below)) {
    cr->set_source_rgba(0.75, 0.0, 0.0, 0.5);
    cr->rectangle(cell_area.get_x()-4, cell_area.get_y()+cell_area.get_height()-2, 8, 2);
    cr->fill();
  }
  if(start.has_tag(tag_removed_above) || end.has_tag(tag_removed_above)) {
    cr->set_source_rgba(0.75, 0.0, 0.0, 0.5);
    cr->rectangle(cell_area.get_x()-4, cell_area.get_y(), 8, 2);
    cr->fill();
  }
}

Source::DiffView::DiffView(const boost::filesystem::path &file_path) : Gsv::View(), file_path(file_path), renderer(new Renderer()) {
  renderer->tag_added=get_buffer()->create_tag("git_added");
  renderer->tag_modified=get_buffer()->create_tag("git_modified");
  renderer->tag_removed=get_buffer()->create_tag("git_removed");
  renderer->tag_removed_below=get_buffer()->create_tag();
  renderer->tag_removed_above=get_buffer()->create_tag();
  
  configure();
}

Source::DiffView::~DiffView() {
  dispatcher.disconnect();
  if(repository) {
    get_gutter(Gtk::TextWindowType::TEXT_WINDOW_LEFT)->remove(renderer.get());
    buffer_insert_connection.disconnect();
    buffer_erase_connection.disconnect();
    monitor_changed_connection.disconnect();
    delayed_buffer_changed_connection.disconnect();
    delayed_monitor_changed_connection.disconnect();
    
    parse_stop=true;
    if(parse_thread.joinable())
      parse_thread.join();
  }
}

void Source::DiffView::configure() {
  if(Config::get().source.show_git_diff) {
    if(repository)
      return;
  }
  else if(repository) {
    get_gutter(Gtk::TextWindowType::TEXT_WINDOW_LEFT)->remove(renderer.get());
    buffer_insert_connection.disconnect();
    buffer_erase_connection.disconnect();
    monitor_changed_connection.disconnect();
    delayed_buffer_changed_connection.disconnect();
    delayed_monitor_changed_connection.disconnect();
    
    parse_stop=true;
    if(parse_thread.joinable())
      parse_thread.join();
    repository=nullptr;
    diff=nullptr;
    
    return;
  }
  else
    return;
  
  try {
    repository=Git::get().get_repository(this->file_path.parent_path());
  }
  catch(const std::exception &) {
    return;
  }
  
  get_gutter(Gtk::TextWindowType::TEXT_WINDOW_LEFT)->insert(renderer.get(), -40);
  parse_state=ParseState::STARTING;
  parse_stop=false;
  monitor_changed=false;
  
  buffer_insert_connection=get_buffer()->signal_insert().connect([this](const Gtk::TextBuffer::iterator &iter ,const Glib::ustring &text, int) {
    //Do not perform git diff if no newline is added and line is already marked as added
    if(!iter.starts_line() && iter.has_tag(renderer->tag_added)) {
      bool newline=false;
      for(auto &c: text.raw()) {
        if(c=='\n') {
          newline=true;
          break;
        }
      }
      if(!newline)
        return;
    }
    //Remove tag_removed_above/below if newline is inserted
    else if(!text.empty() && text[0]=='\n' && iter.has_tag(renderer->tag_removed)) {
      auto start_iter=get_buffer()->get_iter_at_line(iter.get_line());
      auto end_iter=get_iter_at_line_end(iter.get_line());
      end_iter.forward_char();
      get_buffer()->remove_tag(renderer->tag_removed_above, start_iter, end_iter);
      get_buffer()->remove_tag(renderer->tag_removed_below, start_iter, end_iter);
    }
    parse_state=ParseState::IDLE;
    delayed_buffer_changed_connection.disconnect();
    delayed_buffer_changed_connection=Glib::signal_timeout().connect([this]() {
      parse_state=ParseState::STARTING;
      return false;
    }, 250);
  }, false);
  
  buffer_erase_connection=get_buffer()->signal_erase().connect([this](const Gtk::TextBuffer::iterator &start_iter, const Gtk::TextBuffer::iterator &end_iter) {
    //Do not perform git diff if start_iter and end_iter is at the same line in addition to the line is tagged added
    if(start_iter.get_line()==end_iter.get_line() && start_iter.has_tag(renderer->tag_added))
      return;
    
    parse_state=ParseState::IDLE;
    delayed_buffer_changed_connection.disconnect();
    delayed_buffer_changed_connection=Glib::signal_timeout().connect([this]() {
      parse_state=ParseState::STARTING;
      return false;
    }, 250);
  }, false);
  
  monitor_changed_connection=repository->monitor->signal_changed().connect([this](const Glib::RefPtr<Gio::File> &file,
                                                                                  const Glib::RefPtr<Gio::File>&,
                                                                                  Gio::FileMonitorEvent monitor_event) {
    if(monitor_event!=Gio::FileMonitorEvent::FILE_MONITOR_EVENT_CHANGES_DONE_HINT) {
      delayed_monitor_changed_connection.disconnect();
      delayed_monitor_changed_connection=Glib::signal_timeout().connect([this]() {
        monitor_changed=true;
        parse_state=ParseState::STARTING;
        std::unique_lock<std::mutex> lock(parse_mutex);
        diff=nullptr;
        return false;
      }, 500);
    }
  });
  
  parse_thread=std::thread([this]() {
    try {
      diff=get_diff();
    }
    catch(const std::exception &) {}
    
    try {
      while(true) {
        while(!parse_stop && parse_state!=ParseState::STARTING && parse_state!=ParseState::PROCESSING)
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if(parse_stop)
          break;
        std::unique_lock<std::mutex> parse_lock(parse_mutex, std::defer_lock);
        auto expected=ParseState::STARTING;
        if(parse_state.compare_exchange_strong(expected, ParseState::PREPROCESSING)) {
          dispatcher.post([this] {
            auto expected=ParseState::PREPROCESSING;
            std::unique_lock<std::mutex> parse_lock(parse_mutex, std::defer_lock);
            if(parse_lock.try_lock()) {
              if(parse_state.compare_exchange_strong(expected, ParseState::PROCESSING))
                parse_buffer=get_buffer()->get_text();
              parse_lock.unlock();
            }
            else
              parse_state.compare_exchange_strong(expected, ParseState::STARTING);
          });
        }
        else if (parse_state==ParseState::PROCESSING && parse_lock.try_lock()) {
          bool expected_monitor_changed=true;
          if(monitor_changed.compare_exchange_strong(expected_monitor_changed, false)) {
            try {
              diff=get_diff();
            }
            catch(const std::exception &) {
              dispatcher.post([this] {
                get_buffer()->remove_tag(renderer->tag_added, get_buffer()->begin(), get_buffer()->end());
                get_buffer()->remove_tag(renderer->tag_modified, get_buffer()->begin(), get_buffer()->end());
                get_buffer()->remove_tag(renderer->tag_removed, get_buffer()->begin(), get_buffer()->end());
                get_buffer()->remove_tag(renderer->tag_removed_below, get_buffer()->begin(), get_buffer()->end());
                get_buffer()->remove_tag(renderer->tag_removed_above, get_buffer()->begin(), get_buffer()->end());
                renderer->queue_draw();
              });
            }
          }
          if(diff)
            lines=diff->get_lines(parse_buffer.raw());
          else {
            lines.added.clear();
            lines.modified.clear();
            lines.removed.clear();
          }
          auto expected=ParseState::PROCESSING;
          if(parse_state.compare_exchange_strong(expected, ParseState::POSTPROCESSING)) {
            parse_lock.unlock();
            dispatcher.post([this] {
              std::unique_lock<std::mutex> parse_lock(parse_mutex, std::defer_lock);
              if(parse_lock.try_lock()) {
                auto expected=ParseState::POSTPROCESSING;
                if(parse_state.compare_exchange_strong(expected, ParseState::IDLE))
                  update_lines();
              }
            });
          }
        }
      }
    }
    catch(const std::exception &e) {
      auto e_what=std::make_shared<std::string>(e.what());
      dispatcher.post([this, e_what] {
        get_buffer()->remove_tag(renderer->tag_added, get_buffer()->begin(), get_buffer()->end());
        get_buffer()->remove_tag(renderer->tag_modified, get_buffer()->begin(), get_buffer()->end());
        get_buffer()->remove_tag(renderer->tag_removed, get_buffer()->begin(), get_buffer()->end());
        get_buffer()->remove_tag(renderer->tag_removed_below, get_buffer()->begin(), get_buffer()->end());
        get_buffer()->remove_tag(renderer->tag_removed_above, get_buffer()->begin(), get_buffer()->end());
        renderer->queue_draw();
        Terminal::get().print("Error (git): "+*e_what+'\n', true);
      });
    }
  });
}

Gtk::TextIter Source::DiffView::get_iter_at_line_end(int line_nr) {
  if(line_nr>=get_buffer()->get_line_count())
    return get_buffer()->end();
  else if(line_nr+1<get_buffer()->get_line_count()) {
    auto iter=get_buffer()->get_iter_at_line(line_nr+1);
    iter.backward_char();
    return iter;
  }
  else {
    auto iter=get_buffer()->get_iter_at_line(line_nr);
    while(!iter.ends_line() && iter.forward_char()) {}
    return iter;
  }
}

void Source::DiffView::git_goto_next_diff() {
  auto iter=get_buffer()->get_insert()->get_iter();
  auto insert_iter=iter;
  bool wrapped=false;
  iter.forward_char();
  while(!wrapped || iter<insert_iter) {
    auto toggled_tags=iter.get_toggled_tags();
    for(auto &toggled_tag: toggled_tags) {
      if(toggled_tag->property_name()=="git_added" ||
         toggled_tag->property_name()=="git_modified" ||
         toggled_tag->property_name()=="git_removed") {
        get_buffer()->place_cursor(iter);
        scroll_to(get_buffer()->get_insert(), 0.0, 1.0, 0.5);
        return;
      }
    }
    iter.forward_char();
    if(!wrapped && iter==get_buffer()->end()) {
      iter=get_buffer()->begin();
      wrapped=true;
    }
  }
}

std::string Source::DiffView::git_get_diff_details() {
  if(!diff)
    return std::string();
  auto line_nr=get_buffer()->get_insert()->get_iter().get_line();
  auto iter=get_buffer()->get_iter_at_line(line_nr);
  if(iter.has_tag(renderer->tag_removed_above))
    --line_nr;
  std::unique_lock<std::mutex> lock(parse_mutex);
  parse_buffer=get_buffer()->get_text();
  return diff->get_details(parse_buffer.raw(), line_nr);
}

///Return repository diff instance. Throws exception on error
std::unique_ptr<Git::Repository::Diff> Source::DiffView::get_diff() {
  auto work_path=boost::filesystem::canonical(repository->get_work_path());
  boost::filesystem::path relative_path;
  {
    std::unique_lock<std::mutex> lock(file_path_mutex);
#if BOOST_VERSION>=106000
    relative_path=boost::filesystem::relative(file_path, work_path);
#else
    if(std::distance(file_path.begin(), file_path.end())<std::distance(work_path.begin(), work_path.end()))
      throw std::runtime_error("not a relative path");
    
    auto work_path_it=work_path.begin();
    auto file_path_it=file_path.begin();
    while(file_path_it!=file_path.end() && work_path_it!=work_path.end()) {
      if(*file_path_it!=*work_path_it)
        throw std::runtime_error("not a relative path");
      ++file_path_it;
      ++work_path_it;
    }
    for(;file_path_it!=file_path.end();++file_path_it)
      relative_path/=*file_path_it;
#endif
  }
  return std::unique_ptr<Git::Repository::Diff>(new Git::Repository::Diff(repository->get_diff(relative_path)));
}

void Source::DiffView::update_lines() {
  get_buffer()->remove_tag(renderer->tag_added, get_buffer()->begin(), get_buffer()->end());
  get_buffer()->remove_tag(renderer->tag_modified, get_buffer()->begin(), get_buffer()->end());
  get_buffer()->remove_tag(renderer->tag_removed, get_buffer()->begin(), get_buffer()->end());
  get_buffer()->remove_tag(renderer->tag_removed_below, get_buffer()->begin(), get_buffer()->end());
  get_buffer()->remove_tag(renderer->tag_removed_above, get_buffer()->begin(), get_buffer()->end());
  
  for(auto &added: lines.added) {
    auto start_iter=get_buffer()->get_iter_at_line(added.first);
    auto end_iter=get_iter_at_line_end(added.second-1);
    end_iter.forward_char();
    get_buffer()->apply_tag(renderer->tag_added, start_iter, end_iter);
  }
  for(auto &modified: lines.modified) {
    auto start_iter=get_buffer()->get_iter_at_line(modified.first);
    auto end_iter=get_iter_at_line_end(modified.second-1);
    end_iter.forward_char();
    get_buffer()->apply_tag(renderer->tag_modified, start_iter, end_iter);
  }
  for(auto &line_nr: lines.removed) {
    Gtk::TextIter removed_start, removed_end;
    if(line_nr>=0) {
      auto start_iter=get_buffer()->get_iter_at_line(line_nr);
      removed_start=start_iter;
      auto end_iter=get_iter_at_line_end(line_nr);
      end_iter.forward_char();
      removed_end=end_iter;
      get_buffer()->apply_tag(renderer->tag_removed_below, start_iter, end_iter);
    }
    if(line_nr+1<get_buffer()->get_line_count()) {
      auto start_iter=get_buffer()->get_iter_at_line(line_nr+1);
      if(line_nr<0)
        removed_start=start_iter;
      auto end_iter=get_iter_at_line_end(line_nr+1);
      end_iter.forward_char();
      removed_end=end_iter;
      get_buffer()->apply_tag(renderer->tag_removed_above, start_iter, end_iter);
    }
    get_buffer()->apply_tag(renderer->tag_removed, removed_start, removed_end);
  }
  
  renderer->queue_draw();
}
