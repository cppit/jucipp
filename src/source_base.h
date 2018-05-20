#pragma once

#include <gtksourceviewmm.h>
#include <mutex>
#include <set>
#include <boost/filesystem.hpp>

namespace Source {
  class BaseView : public Gsv::View {
  public:
    BaseView(const boost::filesystem::path &file_path, const Glib::RefPtr<Gsv::Language> &language);
    ~BaseView() override;
    boost::filesystem::path file_path;
    
    Glib::RefPtr<Gsv::Language> language;
    
    bool load(bool not_undoable_action=false);
    /// Set new text more optimally and without unnecessary scrolling
    void replace_text(const std::string &new_text);
    virtual void rename(const boost::filesystem::path &path);
    virtual bool save() = 0;
    
    Glib::RefPtr<Gio::FileMonitor> monitor;
    sigc::connection monitor_changed_connection;
    sigc::connection delayed_monitor_changed_connection;
    
    virtual void configure() = 0;
    virtual void hide_tooltips() = 0;
    virtual void hide_dialogs() = 0;
    
    std::function<void(BaseView* view, bool center, bool show_tooltips)> scroll_to_cursor_delayed=[](BaseView* view, bool center, bool show_tooltips) {};
    
    /// Safely returns iter given line and an offset using either byte index or character offset. Defaults to using byte index.
    virtual Gtk::TextIter get_iter_at_line_pos(int line, int pos);
    /// Safely returns iter given line and character offset
    Gtk::TextIter get_iter_at_line_offset(int line, int offset);
    /// Safely returns iter given line and byte index
    Gtk::TextIter get_iter_at_line_index(int line, int index);
    
    Gtk::TextIter get_iter_at_line_end(int line_nr);
    Gtk::TextIter get_iter_for_dialog();
    
    /// Safely places cursor at line using get_iter_at_line_pos.
    void place_cursor_at_line_pos(int line, int pos);
    /// Safely places cursor at line offset
    void place_cursor_at_line_offset(int line, int offset);
    /// Safely places cursor at line index
    void place_cursor_at_line_index(int line, int index);
    
  protected:
    std::time_t last_write_time;
    void monitor_file();
    void check_last_write_time(std::time_t last_write_time_=static_cast<std::time_t>(-1));
    
    /// Move iter to line start. Depending on iter position, before or after indentation.
    /// Works with wrapped lines. 
    Gtk::TextIter get_smart_home_iter(const Gtk::TextIter &iter);
    /// Move iter to line end. Depending on iter position, before or after indentation.
    /// Works with wrapped lines. 
    /// Note that smart end goes FIRST to end of line to avoid hiding empty chars after expressions.
    Gtk::TextIter get_smart_end_iter(const Gtk::TextIter &iter);
    
    std::string get_line(const Gtk::TextIter &iter);
    std::string get_line(const Glib::RefPtr<Gtk::TextBuffer::Mark> &mark);
    std::string get_line(int line_nr);
    std::string get_line();
    std::string get_line_before(const Gtk::TextIter &iter);
    std::string get_line_before(const Glib::RefPtr<Gtk::TextBuffer::Mark> &mark);
    std::string get_line_before();
    Gtk::TextIter get_tabs_end_iter(const Gtk::TextIter &iter);
    Gtk::TextIter get_tabs_end_iter(const Glib::RefPtr<Gtk::TextBuffer::Mark> &mark);
    Gtk::TextIter get_tabs_end_iter(int line_nr);
    Gtk::TextIter get_tabs_end_iter();
    
    std::set<int> diagnostic_offsets;
    void place_cursor_at_next_diagnostic();
  public:
    std::function<void(BaseView *view)> update_tab_label;
    std::function<void(BaseView *view)> update_status_location;
    std::function<void(BaseView *view)> update_status_file_path;
    std::function<void(BaseView *view)> update_status_diagnostics;
    std::function<void(BaseView *view)> update_status_state;
    std::tuple<size_t, size_t, size_t> status_diagnostics;
    std::string status_state;
    std::function<void(BaseView *view)> update_status_branch;
    std::string status_branch;
    
    bool disable_spellcheck=false;
  };
}
