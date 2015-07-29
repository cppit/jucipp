#include "sourcefile.h"
#include <giomm.h>
#include <string>
#include <iostream>
#include <vector>

using namespace std;

sourcefile::sourcefile(const string &input_filename)
  : lines(), filename(input_filename) {
  open(input_filename);
}

/**
 * 
 */
void sourcefile::open(const string &filename) {
  Gio::init();

  // Creates/Opens a file specified by the input string.
  Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(filename);

  if (!file)  // Gio::File has overloaded operator
    cerr << "Was not able to open file: " << filename << endl;

  // Creates pointer for filestream

  if (!file->query_exists()) {
    file->create_file()->close();
  }

  Glib::RefPtr<Gio::FileInputStream> stream = file->read();

  if (!stream)  // error message on stream failure
    cerr << filename << " returned an empty stream" << endl;

  Glib::RefPtr<Gio::DataInputStream>
    datainput = Gio::DataInputStream::create(stream);

  string line;
  while (datainput->read_line(line)) {
    lines.push_back(line);
  }

  datainput->close();
  stream->close();
}

vector<string> sourcefile::get_lines() {
  return lines;
}

string sourcefile::get_line(int line_number) {
  return lines[line_number];
}

int sourcefile::save(const string &text) {
  Gio::init();

  // Creates/Opens a file specified by the input string.
  Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(filename);

  if (!file)  // Gio::File has overloaded operator
    cerr << "Was not able to open file: " << filename << endl;

  // Creates
  Glib::RefPtr<Gio::FileOutputStream> stream =
    file->query_exists() ? file->replace() : file->create_file();

  if (!stream)  // error message on stream failure
    cerr << filename << " returned an empty stream" << endl;

  Glib::RefPtr<Gio::DataOutputStream>
    output = Gio::DataOutputStream::create(stream);

  output->put_string(text);

  output->close();
  stream->close();


  return 0;
}

string sourcefile::get_content() {
  string res;
  for (auto line : lines) {
    res.append(line).append("\n");
  }
  return res;
}

