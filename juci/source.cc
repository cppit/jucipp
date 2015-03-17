#include "source.h"
#include <iostream>
#include "sourcefile.h"
#include <boost/property_tree/json_parser.hpp>
#include <fstream>

//////////////
//// View ////
//////////////
Source::View::View() {
  std::cout << "View constructor run" << std::endl;
  override_font(Pango::FontDescription("Monospace"));
}
// Source::View::UpdateLine
// returns the new line
string Source::View::UpdateLine() {
  Gtk::TextIter line(get_buffer()->get_insert()->get_iter());
  std::cout << line.get_line() << std::endl;
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
void Source::View::ApplyTheme(const Source::Theme &theme) {
  for (auto &item : theme.tagtable()) {
    //    std::cout << "Apply: f: " << item.first << ", s: " <<
    //      item.second << std::endl;
    get_buffer()->create_tag(item.first)->property_foreground() = item.second;
  }
}

void Source::View::OnOpenFile(std::vector<Clang::SourceLocation> &locations,
           const Source::Theme &theme) {
  ApplyTheme(theme);
  Glib::RefPtr<Gtk::TextBuffer> buffer = get_buffer();
  for (auto &loc : locations) {
    string type = std::to_string(loc.kind());
    try {
      theme.typetable().at(type);
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
        buffer->apply_tag_by_name(theme.typetable().at(type),
                                  begin_iter, end_iter);
    }
  }
}
// Source::View::Theme::tagtable()
// returns a const refrence to the tagtable
const std::unordered_map<string, string>& Source::Theme::tagtable() const {
  return tagtable_;
}

// Source::View::Theme::tagtable()
// returns a const refrence to the tagtable
const std::unordered_map<string, string>& Source::Theme::typetable() const {
  return typetable_;
}


void Source::Theme::InsertTag(const string &key, const string &value) {
  tagtable_[key] = value;
}
// Source::View::Theme::SetTagTable()
// sets the tagtable for the view
void Source::Theme::SetTypeTable(
                        const std::unordered_map<string, string> &typetable) {
  typetable_ = typetable;
}

void Source::Theme::InsertType(const string &key, const string &value) {
  typetable_[key] = value;
}
// Source::View::Theme::SetTagTable()
// sets the tagtable for the view
void Source::Theme::SetTagTable(
                        const std::unordered_map<string, string> &tagtable) {
  tagtable_ = tagtable;
}

///////////////
//// Model ////
///////////////
Source::Model::Model() :
  theme_() {
  std::cout << "Model constructor run" << std::endl;
  boost::property_tree::ptree pt;
  boost::property_tree::json_parser::read_json("config.json", pt);
  for ( auto &i : pt ) {
    boost::property_tree::ptree props =  pt.get_child(i.first);
    for (auto &pi : props) {
      if (i.first.compare("syntax")) {  // checks the config-file
        theme_.InsertTag(pi.first, pi.second.get_value<std::string>());
        //  std::cout << "inserting tag. " << pi.first << pi.second.get_value<std::string>() << std::endl;
      }
      if (i.first.compare("colors")) {  // checks the config-file
        theme_.InsertType(pi.first, pi.second.get_value<std::string>());
        //   std::cout << "inserting type. " << pi.first << pi.second.get_value<std::string>() << std::endl;
      }
    }
  }
}

Source::Theme& Source::Model::theme() {
  return theme_;
}

const string Source::Model::filepath() {
  return filepath_;
}

void Source::Model::SetFilePath(const string &filepath) {
  filepath_ = filepath;
}

void Source::Model::
SetSourceLocations(const std::vector<Clang::SourceLocation> &locations) {
  locations_ = locations;
}
////////////////////
//// Controller ////
////////////////////

// Source::Controller::Controller()
// Constructor for Controller
Source::Controller::Controller() {
  std::cout << "Controller constructor run" << std::endl;
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

void Source::Controller::OnOpenFile(const string &filename) {
  sourcefile s(filename);
  view().get_buffer()->set_text(s.get_content());
  int linums = view().get_buffer()->end().get_line();
  int offset = view().get_buffer()->end().get_line_offset();
  Clang::TranslationUnit tu(filename.c_str(), linums, offset);
  model().SetSourceLocations(tu.getSourceLocations());
  view().OnOpenFile(model().getSourceLocations(), model().theme());
}
