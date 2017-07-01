#ifndef JUCI_AUTOCOMPLETE_H_
#define JUCI_AUTOCOMPLETE_H_
#include "dispatcher.h"
#include "selection_dialog.h"
#include "tooltips.h"
#include <thread>

template <class Suggestion>
class Autocomplete {
  Gtk::TextView *view;
  /// Some libraries/utilities, like libclang, require that autocomplete is started at the beginning of a word
  bool strip_word;

  Dispatcher dispatcher;

public:
  enum class State { IDLE, STARTING, RESTARTING, CANCELED };

  std::string prefix;
  std::mutex prefix_mutex;
  std::unordered_map<std::string, std::pair<std::string, std::string>> rows;
  Tooltips tooltips;

  std::atomic<State> state;

  std::thread thread;

  std::function<bool()> is_processing = [] { return true; };
  std::function<void()> reparse = [] {};
  std::function<void()> cancel_reparse = [] {};
  std::function<std::unique_ptr<std::lock_guard<std::mutex>>()> get_parse_lock = [] { return nullptr; };
  std::function<void()> stop_parse = [] {};

  std::function<bool(guint last_keyval)> is_continue_key = [](guint) { return false; };
  std::function<bool(guint last_keyval)> is_restart_key = [](guint) { return false; };
  std::function<bool()> run_check = [] { return false; };

  std::function<void()> before_get_suggestions = [] {};
  std::function<void()> after_get_suggestions = [] {};
  std::function<void()> on_get_suggestions_error = [] {};

  std::function<std::shared_ptr<std::vector<Suggestion>>(std::string &buffer, int line_number, int column)> get_suggestions = [](std::string &, int, int) { return std::make_shared<std::vector<Suggestion>>(); };

  std::function<void(Suggestion &)> foreach_suggestion = [](Suggestion &) {};

  std::function<void()> setup_dialog = [] {};

  Autocomplete(Gtk::TextView *view, bool &interactive_completion, guint &last_keyval, bool strip_word): view(view), state(State::IDLE), strip_word(strip_word) {
    view->get_buffer()->signal_changed().connect([this, &interactive_completion, &last_keyval] {
      if(CompletionDialog::get() && CompletionDialog::get()->is_visible()) {
        cancel_reparse();
        return;
      }
      if(!this->view->has_focus())
        return;
      if(is_continue_key(last_keyval) && (interactive_completion || state != State::IDLE))
        run();
      else {
        if(state == State::STARTING || state == State::RESTARTING)
          state = State::CANCELED;
        
        if(is_restart_key(last_keyval) && interactive_completion)
          run();
      }
    });

    view->get_buffer()->signal_mark_set().connect([this](const Gtk::TextBuffer::iterator &iterator, const Glib::RefPtr<Gtk::TextBuffer::Mark> &mark) {
      if(mark->get_name() == "insert") {
        if(state == State::STARTING || state == State::RESTARTING)
          state = State::CANCELED;
      }
    });

    view->signal_key_release_event().connect([this](GdkEventKey *key) {
      if(CompletionDialog::get() && CompletionDialog::get()->is_visible()) {
        if(CompletionDialog::get()->on_key_release(key))
          return true;
      }
      return false;
    }, false);

    view->signal_focus_out_event().connect([this](GdkEventFocus *event) {
      if(state == State::STARTING || state == State::RESTARTING)
        state = State::CANCELED;
      return false;
    });
  }

  void run() {
    if(run_check()) {
      if(!is_processing())
        return;
      
      if(state == State::CANCELED)
        state = State::RESTARTING;
      
      if(state != State::IDLE)
        return;
      
      state = State::STARTING;
      
      before_get_suggestions();
      
      if(thread.joinable())
        thread.join();
      auto buffer = std::make_shared<Glib::ustring>(view->get_buffer()->get_text());
      auto iter = view->get_buffer()->get_insert()->get_iter();
      auto line_nr = iter.get_line() + 1;
      auto column_nr = iter.get_line_index() + 1;
      if(strip_word) {
        auto pos = iter.get_offset() - 1;
        while(pos >= 0 && (((*buffer)[pos] >= 'a' && (*buffer)[pos] <= 'z') || ((*buffer)[pos] >= 'A' && (*buffer)[pos] <= 'Z') ||
                           ((*buffer)[pos] >= '0' && (*buffer)[pos] <= '9') || (*buffer)[pos] == '_')) {
          buffer->replace(pos, 1, " ");
          column_nr--;
          pos--;
        }
      }
      thread = std::thread([this, line_nr, column_nr, buffer]() {
        auto lock = get_parse_lock();
        if(!is_processing())
          return;
        stop_parse();
      
        auto &buffer_raw = const_cast<std::string &>(buffer->raw());
        auto suggestions = get_suggestions(buffer_raw, line_nr, column_nr);
      
        if(is_processing()) {
          dispatcher.post([this, suggestions] {
            if(state == State::CANCELED) {
              after_get_suggestions();
              reparse();
              state = State::IDLE;
            }
            else if(state == State::RESTARTING) {
              after_get_suggestions();
              reparse();
              state = State::IDLE;
              run();
            }
            else {
              auto start_iter = view->get_buffer()->get_insert()->get_iter();
              if(prefix.size() > 0 && !start_iter.backward_chars(prefix.size()))
                return;
              CompletionDialog::create(view, view->get_buffer()->create_mark(start_iter));
              rows.clear();
              setup_initial_dialog();
              setup_dialog();
      
              for(auto &suggestion : *suggestions)
                foreach_suggestion(suggestion);
              suggestions->clear();
              after_get_suggestions();
              state = State::IDLE;
      
              if(!rows.empty()) {
                view->get_buffer()->begin_user_action();
                CompletionDialog::get()->show();
              }
              else
                reparse();
            }
          });
        }
        else {
          dispatcher.post([this] {
            state = State::CANCELED;
            on_get_suggestions_error();
          });
        }
      });
    }
    
    if(state != State::IDLE)
      cancel_reparse();
  }

private:
  void setup_initial_dialog() {
    CompletionDialog::get()->on_hide = [this]() {
      view->get_buffer()->end_user_action();
      tooltips.hide();
      tooltips.clear();
      reparse();
    };

    CompletionDialog::get()->on_changed = [this](const std::string &selected) {
      if(selected.empty()) {
        tooltips.hide();
        return;
      }
      auto tooltip = std::make_shared<std::string>(rows.at(selected).second);
      if(tooltip->empty())
        tooltips.hide();
      else {
        tooltips.clear();
        auto create_tooltip_buffer = [this, tooltip]() {
          auto tooltip_buffer = Gtk::TextBuffer::create(view->get_buffer()->get_tag_table());

          tooltip_buffer->insert_with_tag(tooltip_buffer->get_insert()->get_iter(), *tooltip, "def:note");

          return tooltip_buffer;
        };

        auto iter = CompletionDialog::get()->start_mark->get_iter();
        tooltips.emplace_back(create_tooltip_buffer, view, view->get_buffer()->create_mark(iter), view->get_buffer()->create_mark(iter));

        tooltips.show(true);
      }
    };
  }
};

#endif // JUCI_AUTOCOMPLETE_H_
