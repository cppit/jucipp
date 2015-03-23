#include "source.h"
#include <iostream>
#include "sourcefile.h"
#include <boost/property_tree/json_parser.hpp>
#include <fstream>
#include <boost/timer/timer.hpp>

//////////////
//// View ////
//////////////
Source::View::View() {
  //  std::cout << "View constructor run" << std::endl;
  override_font(Pango::FontDescription("Monospace"));
}
// Source::View::UpdateLine
// returns the new line
string Source::View::UpdateLine() {
  Gtk::TextIter line(get_buffer()->get_insert()->get_iter());
  // std::cout << line.get_line() << std::endl;
  // for each word --> check what it is --> apply appropriate tag
  return "";
}

string Source::View::GetLine(const Gtk::TextIter &begin) {
  Gtk::TextIter end(begin);
  while (!end.ends_line())
    end++;
  return begin.get_text(end);
}

// Source::View::ApplyTheme()
// Applies theme in textview
void Source::View::ApplyConfig(const Source::Config &config) {
  for (auto &item : config.tagtable()) {
    //    std::cout << "Apply: f: " << item.first << ", s: " <<
    //      item.second << std::endl;
    get_buffer()->create_tag(item.first)->property_foreground() = item.second;
  }
}

void Source::View::OnOpenFile(std::vector<Clang::SourceLocation> &locations,
                              const Source::Config &config) {
  ApplyConfig(config);
  Glib::RefPtr<Gtk::TextBuffer> buffer = get_buffer();
  for (auto &loc : locations) {
    string type = std::to_string(loc.kind());
    try {
      config.typetable().at(type);
    } catch (std::exception) {
      continue;
    }

    int linum_start = loc.line_number_begin();
    int linum_end = loc.line_number_end();
    int begin = loc.begin();
    int end = loc.end();
    if (end < 0) end = 0;
    if (begin < 0) begin = 0;

    // std::cout << "Libc: ";
    // std::cout << "type: " << type;
    // std::cout << " linum_s: " << linum_start+1 << " linum_e: " << linum_end+1;
    // std::cout << ", begin: " << begin << ", end: " << end  << std::endl;

    Gtk::TextIter begin_iter = buffer->get_iter_at_line_offset(linum_start,
                                                               begin);
    Gtk::TextIter end_iter  = buffer->get_iter_at_line_offset(linum_end, end);
    //    std::cout << get_buffer()->get_text(begin_iter, end_iter) << std::endl;
    if (begin_iter.get_line() ==  end_iter.get_line()) {
      buffer->apply_tag_by_name(config.typetable().at(type),
                                begin_iter, end_iter);
    }
  }
}


// Source::View::Config::Config(Config &config)
// copy-constructor
Source::Config::Config(const Source::Config &original) {
  SetTagTable(original.tagtable());
  SetTypeTable(original.typetable());
}

Source::Config::Config(){}

// Source::View::Config::tagtable()
// returns a const refrence to the tagtable
const std::unordered_map<string, string>& Source::Config::tagtable() const {
  return tagtable_;
}

// Source::View::Config::tagtable()
// returns a const refrence to the tagtable
const std::unordered_map<string, string>& Source::Config::typetable() const {
  return typetable_;
}


void Source::Config::InsertTag(const string &key, const string &value) {
  tagtable_[key] = value;
}
// Source::View::Config::SetTagTable()
// sets the tagtable for the view
void Source::Config::SetTypeTable(
                                  const std::unordered_map<string, string> &typetable) {
  typetable_ = typetable;
}

void Source::Config::InsertType(const string &key, const string &value) {
  typetable_[key] = value;
}
// Source::View::Config::SetTagTable()
// sets the tagtable for the view
void Source::Config::SetTagTable(
                                 const std::unordered_map<string, string> &tagtable) {
  tagtable_ = tagtable;
}

///////////////
//// Model ////
///////////////
Source::Model::Model(const Source::Config &config) :
  config_(config) {
}

Source::Config& Source::Model::config() {
  return config_;
}

const string Source::Model::filepath() {
  return filepath_;
}

void Source::Model::SetFilePath(const string &filepath) {
  filepath_ = filepath;
}

void Source::Model::
SetSourceLocations(const std::vector<clang::SourceLocation> &locations) {
  locations_ = locations;
}
////////////////////
//// Controller ////
////////////////////

// Source::Controller::Controller()
// Constructor for Controller
Source::Controller::Controller(const Source::Config &config) :
  model_(config) {
  view().get_buffer()->signal_changed().connect([this](){
      this->OnLineEdit();
    });
}

// Source::Controller::view()
// return shared_ptr to the view
Source::View& Source::Controller::view() {
  return view_;
}
// Source::Controller::model()
// return shared_ptr to the model()
Source::Model& Source::Controller::model() {
  return model_;
}
// Source::Controller::OnLineEdit()
// fired when a line in the buffer is edited
void Source::Controller::OnLineEdit() {
  view().UpdateLine();
}

void Source::Controller::OnNewEmptyFile() {
  string filename("/tmp/juci_t");
  sourcefile s(filename);
  model().SetFilePath(filename);
  s.save("");
}

void Source::Controller::OnOpenFile(const string &filepath) {
  sourcefile s(filepath);
  buffer()->set_text(s.get_content());
  int start_offset = buffer()->begin().get_offset();
  int end_offset = buffer()->end().get_offset();

  std::string project_path =
    filepath.substr(0, filepath.find_last_of('/'));

  clang::CompilationDatabase db(project_path);
  clang::CompileCommands commands(filepath, &db);
  std::vector<clang::CompileCommand> cmds = commands.get_commands();
  std::vector<const char*> arguments;
  for (auto &i : cmds) {
    std::vector<std::string> lol = i.get_command_as_args();
    for (int a = 1; a < lol.size()-4; a++) {
      arguments.emplace_back(lol[a].c_str());
    }
  }
  boost::timer::auto_cpu_timer timer;
  clang::TranslationUnit tu(true, filepath, arguments);
  timer.~auto_cpu_timer();
  boost::timer::auto_cpu_timer timer2;
  clang::SourceLocation start(&tu, filepath, start_offset);
  clang::SourceLocation end(&tu, filepath, end_offset);
  clang::SourceRange range(&start, &end);
  clang::Tokens tokens(&tu, &range);
  std::vector<clang::Token> tks = tokens.tokens();

  for (auto &t : tks) {
    clang::SourceLocation loc = t.get_source_location(&tu);
    unsigned line;
    unsigned column;
    loc.get_location_info(NULL, &line, &column, NULL);
  }

  timer2.~auto_cpu_timer();
  //  std::cout << t.elapsed().user << std::endl;
  //  model().SetSourceLocations(tu.getSourceLocations());
  //  view().OnOpenFile(model().getSourceLocations(), model().theme());
}

Glib::RefPtr<Gtk::TextBuffer> Source::Controller::buffer() {
  return view().get_buffer();
}
