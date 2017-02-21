#include "source.h"
#include "config.h"
#include "filesystem.h"
#include "terminal.h"
#include "info.h"
#include "directories.h"
#include "menu.h"
#include "selection_dialog.h"
#include <gtksourceview/gtksource.h>
#include <boost/property_tree/json_parser.hpp>
#include <boost/spirit/home/qi/char.hpp>
#include <boost/spirit/home/qi/operator.hpp>
#include <boost/spirit/home/qi/string.hpp>
#include <iostream>
#include <numeric>
#include <set>

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

Glib::RefPtr<Gsv::Language> Source::guess_language(const boost::filesystem::path &file_path) {
  auto language_manager=Gsv::LanguageManager::get_default();
  bool result_uncertain = false;
  auto content_type = Gio::content_type_guess(file_path.string(), nullptr, 0, result_uncertain);
  if(result_uncertain) {
    content_type.clear();
  }
  auto language=language_manager->guess_language(file_path.string(), content_type);
  if(!language) {
    auto filename=file_path.filename().string();
    if(filename=="CMakeLists.txt")
      language=language_manager->get_language("cmake");
    else if(filename=="Makefile")
      language=language_manager->get_language("makefile");
    else if(file_path.extension()==".tcc")
      language=language_manager->get_language("cpphdr");
  }
  return language;
}

Source::FixIt::FixIt(const std::string &source, const std::pair<Offset, Offset> &offsets) : source(source), offsets(offsets) {
  if(source.size()==0)
    type=Type::ERASE;
  else {
    if(this->offsets.first==this->offsets.second)
      type=Type::INSERT;
    else
      type=Type::REPLACE;
  }
}

std::string Source::FixIt::string(Glib::RefPtr<Gtk::TextBuffer> buffer) {
  auto iter=buffer->get_iter_at_line_index(offsets.first.line, offsets.first.index);
  unsigned first_line_offset=iter.get_line_offset()+1;
  iter=buffer->get_iter_at_line_index(offsets.second.line, offsets.second.index);
  unsigned second_line_offset=iter.get_line_offset()+1;
  
  std::string text;
  if(type==Type::INSERT) {
    text+="Insert "+source+" at ";
    text+=std::to_string(offsets.first.line+1)+":"+std::to_string(first_line_offset);
  }
  else if(type==Type::REPLACE) {
    text+="Replace ";
    text+=std::to_string(offsets.first.line+1)+":"+std::to_string(first_line_offset)+" - ";
    text+=std::to_string(offsets.second.line+1)+":"+std::to_string(second_line_offset);
    text+=" with "+source;
  }
  else {
    text+="Erase ";
    text+=std::to_string(offsets.first.line+1)+":"+std::to_string(first_line_offset)+" - ";
    text+=std::to_string(offsets.second.line+1)+":"+std::to_string(second_line_offset);
  }
  
  return text;
}

//////////////
//// View ////
//////////////
const std::regex Source::View::bracket_regex("^([ \\t]*).*\\{ *$");
const std::regex Source::View::no_bracket_statement_regex("^([ \\t]*)(if|for|else if|while) *\\(.*[^;}] *$");
const std::regex Source::View::no_bracket_no_para_statement_regex("^([ \\t]*)(else) *$");

Source::View::View(const boost::filesystem::path &file_path, Glib::RefPtr<Gsv::Language> language): Gsv::View(), SpellCheckView(), DiffView(file_path), language(language), status_diagnostics(0, 0, 0) {
  load();
  
  get_buffer()->place_cursor(get_buffer()->get_iter_at_offset(0)); 
  
  search_settings = gtk_source_search_settings_new();
  gtk_source_search_settings_set_wrap_around(search_settings, true);
  search_context = gtk_source_search_context_new(get_source_buffer()->gobj(), search_settings);
  gtk_source_search_context_set_highlight(search_context, true);
  //TODO: why does this not work?: Might be best to use the styles from sourceview. These has to be read from file, search-matches got style "search-match"
  //TODO: in header if trying again: GtkSourceStyle* search_match_style;
  //TODO: We can drop this, only work on newer versions of gtksourceview.
  //search_match_style=(GtkSourceStyle*)g_object_new(GTK_SOURCE_TYPE_STYLE, "background-set", 1, "background", "#00FF00", nullptr);
  //gtk_source_search_context_set_match_style(search_context, search_match_style);
  
  //TODO: either use lambda if possible or create a gtkmm wrapper around search_context (including search_settings):
  //TODO: (gtkmm's Gtk::Object has connect_property_changed, so subclassing this might be an idea)
  g_signal_connect(search_context, "notify::occurrences-count", G_CALLBACK(search_occurrences_updated), this);
  
  get_buffer()->create_tag("def:warning");
  get_buffer()->create_tag("def:warning_underline");
  get_buffer()->create_tag("def:error");
  get_buffer()->create_tag("def:error_underline");
  get_buffer()->create_tag("def:note_background");
  get_buffer()->create_tag("def:note");
  
  auto mark_attr_debug_breakpoint=Gsv::MarkAttributes::create();
  Gdk::RGBA rgba;
  rgba.set_red(1.0);
  rgba.set_green(0.5);
  rgba.set_blue(0.5);
  rgba.set_alpha(0.3);
  mark_attr_debug_breakpoint->set_background(rgba);
  set_mark_attributes("debug_breakpoint", mark_attr_debug_breakpoint, 100);
  auto mark_attr_debug_stop=Gsv::MarkAttributes::create();
  rgba.set_red(0.5);
  rgba.set_green(0.5);
  rgba.set_blue(1.0);
  mark_attr_debug_stop->set_background(rgba);
  set_mark_attributes("debug_stop", mark_attr_debug_stop, 101);
  auto mark_attr_debug_breakpoint_and_stop=Gsv::MarkAttributes::create();
  rgba.set_red(0.75);
  rgba.set_green(0.5);
  rgba.set_blue(0.75);
  mark_attr_debug_breakpoint_and_stop->set_background(rgba);
  set_mark_attributes("debug_breakpoint_and_stop", mark_attr_debug_breakpoint_and_stop, 102);
  
  get_buffer()->signal_changed().connect([this](){
    if(update_status_location)
      update_status_location(this);
  });
  
  signal_realize().connect([this] {
    auto gutter=get_gutter(Gtk::TextWindowType::TEXT_WINDOW_LEFT);
    auto renderer=gutter->get_renderer_at_pos(15, 0);
    if(renderer) {
      renderer_activate_connection.disconnect();
      renderer_activate_connection=renderer->signal_activate().connect([this](const Gtk::TextIter& iter, const Gdk::Rectangle&, GdkEvent*) {
        if(toggle_breakpoint)
          toggle_breakpoint(iter.get_line());
      });
    }
  });
  
  set_tooltip_and_dialog_events();
  
  if(language && (language->get_id()=="chdr" || language->get_id()=="cpphdr" || language->get_id()=="c" ||
                  language->get_id()=="cpp" || language->get_id()=="objc" || language->get_id()=="java" ||
                  language->get_id()=="js" || language->get_id()=="ts" || language->get_id()=="proto" ||
                  language->get_id()=="c-sharp" || language->get_id()=="html" || language->get_id()=="cuda" ||
                  language->get_id()=="php" || language->get_id()=="rust" || language->get_id()=="swift" ||
                  language->get_id()=="go" || language->get_id()=="scala" || language->get_id()=="opencl")) {
    is_bracket_language=true;
    
    format_style=[this]() {
      auto command=Config::get().terminal.clang_format_command+" -output-replacements-xml -assume-filename="+filesystem::escape_argument(this->file_path.string());
      
      if(get_buffer()->get_has_selection()) {
        Gtk::TextIter start, end;
        get_buffer()->get_selection_bounds(start, end);
        command+=" -lines="+std::to_string(start.get_line()+1)+':'+std::to_string(end.get_line()+1);
      }
      
      bool use_style_file=false;
      auto style_file_search_path=this->file_path.parent_path();
      while(true) {
        if(boost::filesystem::exists(style_file_search_path/".clang-format") || boost::filesystem::exists(style_file_search_path/"_clang-format")) {
          use_style_file=true;
          break;
        }
        if(style_file_search_path==style_file_search_path.root_directory())
          break;
        style_file_search_path=style_file_search_path.parent_path();
      }
      
      if(use_style_file)
        command+=" -style=file";
      else {
        unsigned indent_width;
        std::string tab_style;
        if(tab_char=='\t') {
          indent_width=tab_size*8;
          tab_style="UseTab: Always";
        }
        else {
          indent_width=tab_size;
          tab_style="UseTab: Never";
        }
        command+=" -style=\"{IndentWidth: "+std::to_string(indent_width);
        command+=", "+tab_style;
        command+=", "+std::string("AccessModifierOffset: -")+std::to_string(indent_width);
        if(Config::get().source.clang_format_style!="")
          command+=", "+Config::get().source.clang_format_style;
        command+="}\"";
      }
      
      std::stringstream stdin_stream(get_buffer()->get_text()), stdout_stream;
      
      auto exit_status=Terminal::get().process(stdin_stream, stdout_stream, command, this->file_path.parent_path());
      if(exit_status==0) {
        // The following code is complex due to clang-format returning offsets in byte offsets instead of char offsets
        
        // Create bytes_in_lines cache to significantly speed up the processing of finding iterators from byte offsets
        std::vector<size_t> bytes_in_lines;
        auto line_count=get_buffer()->get_line_count();
        for(int line_nr=0;line_nr<line_count;++line_nr) {
          auto iter=get_buffer()->get_iter_at_line(line_nr);
          bytes_in_lines.emplace_back(iter.get_bytes_in_line());
        }
        
        get_buffer()->begin_user_action();
        try {
          boost::property_tree::ptree pt;
          boost::property_tree::xml_parser::read_xml(stdout_stream, pt);
          auto replacements_pt=pt.get_child("replacements");
          for(auto it=replacements_pt.rbegin();it!=replacements_pt.rend();++it) {
            if(it->first=="replacement") {
              auto offset=it->second.get<size_t>("<xmlattr>.offset");
              auto length=it->second.get<size_t>("<xmlattr>.length");
              auto replacement_str=it->second.get<std::string>("");
              
              size_t bytes=0;
              for(size_t c=0;c<bytes_in_lines.size();++c) {
                auto previous_bytes=bytes;
                bytes+=bytes_in_lines[c];
                if(offset<bytes || (c==bytes_in_lines.size()-1 && offset==bytes)) {
                  std::pair<size_t, size_t> line_index(c, offset-previous_bytes);
                  auto start=get_buffer()->get_iter_at_line_index(line_index.first, line_index.second);
                  
                  if(length>0) {
                    auto offset_end=offset+length;
                    size_t bytes=0;
                    for(size_t c=0;c<bytes_in_lines.size();++c) {
                      auto previous_bytes=bytes;
                      bytes+=bytes_in_lines[c];
                      if(offset_end<bytes || (c==bytes_in_lines.size()-1 && offset_end==bytes)) {
                        auto end=get_buffer()->get_iter_at_line_index(c, offset_end-previous_bytes);
                        get_buffer()->erase(start, end);
                        start=get_buffer()->get_iter_at_line_index(line_index.first, line_index.second);
                        break;
                      }
                    }
                  }
                  get_buffer()->insert(start, replacement_str);
                  break;
                }
              }
            }
          }
        }
        catch(const std::exception &e) {
          Terminal::get().print(std::string("Error: error parsing clang-format output: ")+e.what()+'\n', true);
        }
        get_buffer()->end_user_action();
      }
    };
  }
  
#ifndef __APPLE__
  set_tab_width(4); //Visual size of a \t hardcoded to be equal to visual size of 4 spaces. Buggy on OS X
#endif
  tab_char=Config::get().source.default_tab_char;
  tab_size=Config::get().source.default_tab_size;
  if(Config::get().source.auto_tab_char_and_size) {
    auto tab_char_and_size=find_tab_char_and_size();
    if(tab_char_and_size.second!=0) {
      if(tab_char!=tab_char_and_size.first || tab_size!=tab_char_and_size.second) {
        std::string tab_str;
        if(tab_char_and_size.first==' ')
          tab_str="<space>";
        else
          tab_str="<tab>";
      }
      
      tab_char=tab_char_and_size.first;
      tab_size=tab_char_and_size.second;
    }
  }
  set_tab_char_and_size(tab_char, tab_size);
  
  std::shared_ptr<std::string> comment_characters;
  if(is_bracket_language)
    comment_characters=std::make_shared<std::string>("//");
  else if(language) {
    if(language->get_id()=="cmake" || language->get_id()=="makefile" || language->get_id()=="python" ||
       language->get_id()=="python3" || language->get_id()=="sh" || language->get_id()=="perl" ||
       language->get_id()=="ruby" || language->get_id()=="r" || language->get_id()=="asm" ||
       language->get_id()=="automake")
      comment_characters=std::make_shared<std::string>("#");
    else if(language->get_id()=="latex" || language->get_id()=="matlab" || language->get_id()=="octave" ||
            language->get_id()=="bibtex")
      comment_characters=std::make_shared<std::string>("%");
    else if(language->get_id()=="fortran")
      comment_characters=std::make_shared<std::string>("!");
    else if(language->get_id()=="pascal")
      comment_characters=std::make_shared<std::string>("//");
    else if(language->get_id()=="lua")
      comment_characters=std::make_shared<std::string>("--");
  }
  if(comment_characters) {
    toggle_comments=[this, comment_characters] {
      std::vector<int> lines;
      Gtk::TextIter selection_start, selection_end;
      get_buffer()->get_selection_bounds(selection_start, selection_end);
      auto line_start=selection_start.get_line();
      auto line_end=selection_end.get_line();
      if(line_start!=line_end && selection_end.starts_line())
        --line_end;
      bool lines_commented=true;
      bool extra_spaces=true;
      int min_indentation=-1;
      for(auto line=line_start;line<=line_end;++line) {
        auto iter=get_buffer()->get_iter_at_line(line);
        bool line_added=false;
        bool line_commented=false;
        bool extra_space=false;
        int indentation=0;
        for(;;) {
          if(iter.ends_line())
            break;
          else if(*iter==' ' || *iter=='\t') {
            ++indentation;
            iter.forward_char();
            continue;
          }
          else {
            lines.emplace_back(line);
            line_added=true;
            for(size_t c=0;c<comment_characters->size();++c) {
              if(iter.ends_line()) {
                break;
              }
              else if(*iter==static_cast<unsigned int>((*comment_characters)[c])) {
                if(c<comment_characters->size()-1) {
                  iter.forward_char();
                  continue;
                }
                else {
                  line_commented=true;
                  if(!iter.ends_line()) {
                    iter.forward_char();
                    if(*iter==' ')
                      extra_space=true;
                  }
                  break;
                }
              }
              else
                break;
            }
            break;
          }
        }
        if(line_added) {
          lines_commented&=line_commented;
          extra_spaces&=extra_space;
          if(min_indentation==-1 || indentation<min_indentation)
            min_indentation=indentation;
        }
      }
      if(lines.size()) {
        auto comment_characters_and_space=*comment_characters+' ';
        get_buffer()->begin_user_action();
        for(auto &line: lines) {
          auto iter=get_buffer()->get_iter_at_line(line);
          iter.forward_chars(min_indentation);
          if(lines_commented) {
            auto end_iter=iter;
            end_iter.forward_chars(comment_characters->size()+static_cast<int>(extra_spaces));
            while(*iter==' ' || *iter=='\t') {
              iter.forward_char();
              end_iter.forward_char();
            }
            get_buffer()->erase(iter, end_iter);
          }
          else
            get_buffer()->insert(iter, comment_characters_and_space);
        }
        get_buffer()->end_user_action();
      }
    };
  }
}

bool Source::View::load() {
  get_source_buffer()->begin_not_undoable_action();
  get_buffer()->erase(get_buffer()->begin(), get_buffer()->end());
  bool status=true;
  if(language) {
    if(filesystem::read_non_utf8(file_path, get_buffer())==-1)
      Terminal::get().print("Warning: "+file_path.string()+" is not a valid UTF-8 file. Saving might corrupt the file.\n");
  }
  else {
    if(filesystem::read(file_path, get_buffer())==-1) {
      Terminal::get().print("Error: "+file_path.string()+" is not a valid UTF-8 file.\n", true);
      status=false;
    }
  }
  get_source_buffer()->end_not_undoable_action();
  
  boost::system::error_code ec;
  last_write_time=boost::filesystem::last_write_time(file_path, ec);
  if(ec)
    last_write_time=static_cast<std::time_t>(-1);
  
  return status;
}

void Source::View::rename(const boost::filesystem::path &path) {
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

void Source::View::set_tab_char_and_size(char tab_char, unsigned tab_size) {
  this->tab_char=tab_char;
  this->tab_size=tab_size;
      
  tab.clear();
  for(unsigned c=0;c<tab_size;c++)
    tab+=tab_char;
}

void Source::View::cleanup_whitespace_characters() {
  auto buffer=get_buffer();
  buffer->begin_user_action();
  for(int line=0;line<buffer->get_line_count();line++) {
    auto iter=buffer->get_iter_at_line(line);
    auto end_iter=get_iter_at_line_end(line);
    if(iter==end_iter)
      continue;
    iter=end_iter;
    while(!iter.starts_line() && (*iter==' ' || *iter=='\t' || iter.ends_line()))
      iter.backward_char();
    if(*iter!=' ' && *iter!='\t')
      iter.forward_char();
    if(iter==end_iter)
      continue;
    buffer->erase(iter, end_iter);
  }
  auto iter=buffer->end();
  if(!iter.starts_line())
    buffer->insert(buffer->end(), "\n");
  buffer->end_user_action();
}

Gsv::DrawSpacesFlags Source::View::parse_show_whitespace_characters(const std::string &text) {
  namespace qi = boost::spirit::qi;
  
  qi::symbols<char, Gsv::DrawSpacesFlags> options;
  options.add
    ("space",    Gsv::DRAW_SPACES_SPACE)
    ("tab",      Gsv::DRAW_SPACES_TAB)
    ("newline",  Gsv::DRAW_SPACES_NEWLINE)
    ("nbsp",     Gsv::DRAW_SPACES_NBSP)
    ("leading",  Gsv::DRAW_SPACES_LEADING)
    ("text",     Gsv::DRAW_SPACES_TEXT)
    ("trailing", Gsv::DRAW_SPACES_TRAILING)
    ("all",      Gsv::DRAW_SPACES_ALL);

  std::set<Gsv::DrawSpacesFlags> out;
  
  // parse comma-separated list of options
  qi::phrase_parse(text.begin(), text.end(), options % ',', qi::space, out);
  
  return out.count(Gsv::DRAW_SPACES_ALL)>0 ?
    Gsv::DRAW_SPACES_ALL :
    static_cast<Gsv::DrawSpacesFlags>(std::accumulate(out.begin(), out.end(), 0));
}

bool Source::View::save(const std::vector<Source::View*> &views) {
  if(file_path.empty() || !get_buffer()->get_modified())
    return false;
  if(Config::get().source.cleanup_whitespace_characters)
    cleanup_whitespace_characters();
  
  if(Config::get().source.format_style_on_save && format_style)
    format_style();
  
  if(filesystem::write(file_path, get_buffer())) {
    boost::system::error_code ec;
    last_write_time=boost::filesystem::last_write_time(file_path, ec);
    if(ec)
      last_write_time=static_cast<std::time_t>(-1);
    get_buffer()->set_modified(false);
    Directories::get().on_save_file(file_path);
    return true;
  }
  else {
    Terminal::get().print("Error: could not save file "+file_path.string()+"\n", true);
    return false;
  }
}

void Source::View::replace_text(const std::string &text) {
  get_buffer()->begin_user_action();
  auto iter=get_buffer()->get_insert()->get_iter();
  auto cursor_line_nr=iter.get_line();
  auto cursor_line_offset=iter.get_line_offset();
  
  size_t start_line_index=0;
  int line_nr=0;
  for(size_t c=0;c<text.size();++c) {
    if(text[c]=='\n' || c==text.size()-1) {
      std::string line=text.substr(start_line_index, c-start_line_index);
      if(text[c]!='\n')
        line+=text[c];
      //Remove carriage return
      for(auto it=line.begin();it!=line.end();) {
        if(*it=='\r')
          it=line.erase(it);
        else
          ++it;
      }
      Gtk::TextIter iter;
      if(line_nr<get_buffer()->get_line_count()) {
        auto start_iter=get_buffer()->get_iter_at_line(line_nr);
        auto end_iter=get_iter_at_line_end(line_nr);
        
        if(get_buffer()->get_text(start_iter, end_iter)!=line) {
          get_buffer()->erase(start_iter, end_iter);
          iter=get_buffer()->get_iter_at_line(line_nr);
          get_buffer()->insert(iter, line);
        }
      }
      else {
        iter=get_buffer()->end();
        get_buffer()->insert(iter, '\n'+line);
      }
      
      ++line_nr;
      start_line_index=c+1;
    }
  }
  
  if(text[text.size()-1]=='\n') {
    Gtk::TextIter iter;
    get_buffer()->insert(get_iter_at_line_end(line_nr-1), "\n");
    ++line_nr;
  }
  
  if(line_nr<get_buffer()->get_line_count()) {
    auto iter=get_iter_at_line_end(line_nr-1);
    get_buffer()->erase(iter, get_buffer()->end());
  }
  
  get_buffer()->end_user_action();
  
  place_cursor_at_line_offset(cursor_line_nr, cursor_line_offset);
}

void Source::View::configure() {
  SpellCheckView::configure();
  DiffView::configure();
  
  //TODO: Move this to notebook? Might take up too much memory doing this for every tab.
  auto style_scheme_manager=Gsv::StyleSchemeManager::get_default();
  style_scheme_manager->prepend_search_path((Config::get().home_juci_path/"styles").string());
  
  if(Config::get().source.style.size()>0) {
    auto scheme = style_scheme_manager->get_scheme(Config::get().source.style);
  
    if(scheme)
      get_source_buffer()->set_style_scheme(scheme);
    else
      Terminal::get().print("Error: Could not find gtksourceview style: "+Config::get().source.style+'\n', true);
  }
  
  set_draw_spaces(parse_show_whitespace_characters(Config::get().source.show_whitespace_characters));
  
  if(Config::get().source.wrap_lines)
    set_wrap_mode(Gtk::WrapMode::WRAP_CHAR);
  else
    set_wrap_mode(Gtk::WrapMode::WRAP_NONE);
  property_highlight_current_line() = Config::get().source.highlight_current_line;
  property_show_line_numbers() = Config::get().source.show_line_numbers;
  if(Config::get().source.font.size()>0)
    override_font(Pango::FontDescription(Config::get().source.font));
#if GTKSOURCEVIEWMM_MAJOR_VERSION > 3 || (GTKSOURCEVIEWMM_MAJOR_VERSION == 3 && GTKSOURCEVIEWMM_MINOR_VERSION >= 16)
  if(Config::get().source.show_background_pattern)
    gtk_source_view_set_background_pattern(this->gobj(), GTK_SOURCE_BACKGROUND_PATTERN_TYPE_GRID);
  else
    gtk_source_view_set_background_pattern(this->gobj(), GTK_SOURCE_BACKGROUND_PATTERN_TYPE_NONE);
#endif
  
  //Create tags for diagnostic warnings and errors:
  auto scheme = get_source_buffer()->get_style_scheme();
  auto tag_table=get_buffer()->get_tag_table();
  auto style=scheme->get_style("def:warning");
  auto diagnostic_tag=get_buffer()->get_tag_table()->lookup("def:warning");
  auto diagnostic_tag_underline=get_buffer()->get_tag_table()->lookup("def:warning_underline");
  if(style && (style->property_foreground_set() || style->property_background_set())) {
    Glib::ustring warning_property;
    if(style->property_foreground_set()) {
      warning_property=style->property_foreground().get_value();
      diagnostic_tag->property_foreground() = warning_property;
    }
    else if(style->property_background_set())
      warning_property=style->property_background().get_value();
  
    diagnostic_tag_underline->property_underline()=Pango::Underline::UNDERLINE_ERROR;
    auto tag_class=G_OBJECT_GET_CLASS(diagnostic_tag_underline->gobj()); //For older GTK+ 3 versions:
    auto param_spec=g_object_class_find_property(tag_class, "underline-rgba");
    if(param_spec!=nullptr) {
      diagnostic_tag_underline->set_property("underline-rgba", Gdk::RGBA(warning_property));
    }
  }
  style=scheme->get_style("def:error");
  diagnostic_tag=get_buffer()->get_tag_table()->lookup("def:error");
  diagnostic_tag_underline=get_buffer()->get_tag_table()->lookup("def:error_underline");
  if(style && (style->property_foreground_set() || style->property_background_set())) {
    Glib::ustring error_property;
    if(style->property_foreground_set()) {
      error_property=style->property_foreground().get_value();
      diagnostic_tag->property_foreground() = error_property;
    }
    else if(style->property_background_set())
      error_property=style->property_background().get_value();
    
    diagnostic_tag_underline->property_underline()=Pango::Underline::UNDERLINE_ERROR;
#if GTK_VERSION_GE(3, 16)
    diagnostic_tag_underline->set_property("underline-rgba", Gdk::RGBA(error_property));
#endif
  }
  //TODO: clear tag_class and param_spec?

  //Add tooltip foreground and background
  style = scheme->get_style("def:note");
  auto note_tag=get_buffer()->get_tag_table()->lookup("def:note_background");
  if(style->property_background_set()) {
    note_tag->property_background()=style->property_background();
  }
  note_tag=get_buffer()->get_tag_table()->lookup("def:note");
  if(style->property_foreground_set()) {
    note_tag->property_foreground()=style->property_foreground();
  }
  
  if(Config::get().menu.keys["source_show_completion"].empty()) {
    get_completion()->unblock_interactive();
    interactive_completion=true;
  }
  else {
    get_completion()->block_interactive();
    interactive_completion=false;
  }
}

void Source::View::set_tooltip_and_dialog_events() {
  get_buffer()->signal_changed().connect([this] {
    hide_tooltips();
  });
  
  signal_motion_notify_event().connect([this](GdkEventMotion* event) {
    if(on_motion_last_x!=event->x || on_motion_last_y!=event->y) {
      delayed_tooltips_connection.disconnect();
      if((event->state&GDK_BUTTON1_MASK)==0) {
        gdouble x=event->x;
        gdouble y=event->y;
        delayed_tooltips_connection=Glib::signal_timeout().connect([this, x, y]() {
          Tooltips::init();
          Gdk::Rectangle rectangle(x, y, 1, 1);
          if(parsed) {
            show_type_tooltips(rectangle);
            show_diagnostic_tooltips(rectangle);
          }
          return false;
        }, 100);
      }
      type_tooltips.hide();
      diagnostic_tooltips.hide();
    }
    on_motion_last_x=event->x;
    on_motion_last_y=event->y;
    return false;
  });
  
  get_buffer()->signal_mark_set().connect([this](const Gtk::TextBuffer::iterator& iterator, const Glib::RefPtr<Gtk::TextBuffer::Mark>& mark) {
    if(get_buffer()->get_has_selection() && mark->get_name()=="selection_bound")
      delayed_tooltips_connection.disconnect();
    
    if(mark->get_name()=="insert") {
      hide_tooltips();
      delayed_tooltips_connection=Glib::signal_timeout().connect([this]() {
        Tooltips::init();
        Gdk::Rectangle rectangle;
        get_iter_location(get_buffer()->get_insert()->get_iter(), rectangle);
        int location_window_x, location_window_y;
        buffer_to_window_coords(Gtk::TextWindowType::TEXT_WINDOW_TEXT, rectangle.get_x(), rectangle.get_y(), location_window_x, location_window_y);
        rectangle.set_x(location_window_x-2);
        rectangle.set_y(location_window_y);
        rectangle.set_width(5);
        if(parsed) {
          show_type_tooltips(rectangle);
          show_diagnostic_tooltips(rectangle);
        }
        return false;
      }, 500);
      
      if(SelectionDialog::get())
        SelectionDialog::get()->hide();
      if(CompletionDialog::get())
        CompletionDialog::get()->hide();
      
      if(update_status_location)
        update_status_location(this);
    }
  });

  signal_scroll_event().connect([this](GdkEventScroll* event) {
    hide_tooltips();
    hide_dialogs();
    return false;
  });
  
  signal_focus_out_event().connect([this](GdkEventFocus* event) {
    hide_tooltips();
    return false;
  });
  
  signal_leave_notify_event().connect([this](GdkEventCrossing*) {
    delayed_tooltips_connection.disconnect();
    return false;
  });
}

void Source::View::search_occurrences_updated(GtkWidget* widget, GParamSpec* property, gpointer data) {
  auto view=static_cast<Source::View*>(data);
  if(view->update_search_occurrences)
    view->update_search_occurrences(gtk_source_search_context_get_occurrences_count(view->search_context));
}

Source::View::~View() {
  g_clear_object(&search_context);
  g_clear_object(&search_settings);
  
  delayed_tooltips_connection.disconnect();
  renderer_activate_connection.disconnect();
}

void Source::View::search_highlight(const std::string &text, bool case_sensitive, bool regex) {
  gtk_source_search_settings_set_case_sensitive(search_settings, case_sensitive);
  gtk_source_search_settings_set_regex_enabled(search_settings, regex);
  gtk_source_search_settings_set_search_text(search_settings, text.c_str());
  search_occurrences_updated(nullptr, nullptr, this);
}

void Source::View::search_forward() {
  Gtk::TextIter insert, selection_bound;
  get_buffer()->get_selection_bounds(insert, selection_bound);
  auto& start=selection_bound;
  Gtk::TextIter match_start, match_end;
#if defined(GTK_SOURCE_MAJOR_VERSION) && (GTK_SOURCE_MAJOR_VERSION > 3 || (GTK_SOURCE_MAJOR_VERSION == 3 && GTK_SOURCE_MINOR_VERSION >= 22))
  gboolean has_wrapped_around;
  if(gtk_source_search_context_forward2(search_context, start.gobj(), match_start.gobj(), match_end.gobj(), &has_wrapped_around)) {
#else
  if(gtk_source_search_context_forward(search_context, start.gobj(), match_start.gobj(), match_end.gobj())) {
#endif
    get_buffer()->select_range(match_start, match_end);
    scroll_to(get_buffer()->get_insert());
  }
}

void Source::View::search_backward() {
  Gtk::TextIter insert, selection_bound;
  get_buffer()->get_selection_bounds(insert, selection_bound);
  auto &start=insert;
  Gtk::TextIter match_start, match_end;
#if defined(GTK_SOURCE_MAJOR_VERSION) && (GTK_SOURCE_MAJOR_VERSION > 3 || (GTK_SOURCE_MAJOR_VERSION == 3 && GTK_SOURCE_MINOR_VERSION >= 22))
  gboolean has_wrapped_around;
  if(gtk_source_search_context_backward2(search_context, start.gobj(), match_start.gobj(), match_end.gobj(), &has_wrapped_around)) {
#else
  if(gtk_source_search_context_backward(search_context, start.gobj(), match_start.gobj(), match_end.gobj())) {
#endif
    get_buffer()->select_range(match_start, match_end);
    scroll_to(get_buffer()->get_insert());
  }
}

void Source::View::replace_forward(const std::string &replacement) {
  Gtk::TextIter insert, selection_bound;
  get_buffer()->get_selection_bounds(insert, selection_bound);
  auto &start=insert;
  Gtk::TextIter match_start, match_end;
#if defined(GTK_SOURCE_MAJOR_VERSION) && (GTK_SOURCE_MAJOR_VERSION > 3 || (GTK_SOURCE_MAJOR_VERSION == 3 && GTK_SOURCE_MINOR_VERSION >= 22))
  gboolean has_wrapped_around;
  if(gtk_source_search_context_forward2(search_context, start.gobj(), match_start.gobj(), match_end.gobj(), &has_wrapped_around)) {
#else
  if(gtk_source_search_context_forward(search_context, start.gobj(), match_start.gobj(), match_end.gobj())) {
#endif
    auto offset=match_start.get_offset();
#if defined(GTK_SOURCE_MAJOR_VERSION) && (GTK_SOURCE_MAJOR_VERSION > 3 || (GTK_SOURCE_MAJOR_VERSION == 3 && GTK_SOURCE_MINOR_VERSION >= 22))
    gtk_source_search_context_replace2(search_context, match_start.gobj(), match_end.gobj(), replacement.c_str(), replacement.size(), nullptr);
#else
    gtk_source_search_context_replace(search_context, match_start.gobj(), match_end.gobj(), replacement.c_str(), replacement.size(), nullptr);
#endif
    
    Glib::ustring replacement_ustring=replacement;
    get_buffer()->select_range(get_buffer()->get_iter_at_offset(offset), get_buffer()->get_iter_at_offset(offset+replacement_ustring.size()));
    scroll_to(get_buffer()->get_insert());
  }
}

void Source::View::replace_backward(const std::string &replacement) {
  Gtk::TextIter insert, selection_bound;
  get_buffer()->get_selection_bounds(insert, selection_bound);
  auto &start=selection_bound;
  Gtk::TextIter match_start, match_end;
#if defined(GTK_SOURCE_MAJOR_VERSION) && (GTK_SOURCE_MAJOR_VERSION > 3 || (GTK_SOURCE_MAJOR_VERSION == 3 && GTK_SOURCE_MINOR_VERSION >= 22))
  gboolean has_wrapped_around;
  if(gtk_source_search_context_backward2(search_context, start.gobj(), match_start.gobj(), match_end.gobj(), &has_wrapped_around)) {
#else
  if(gtk_source_search_context_backward(search_context, start.gobj(), match_start.gobj(), match_end.gobj())) {
#endif
    auto offset=match_start.get_offset();
#if defined(GTK_SOURCE_MAJOR_VERSION) && (GTK_SOURCE_MAJOR_VERSION > 3 || (GTK_SOURCE_MAJOR_VERSION == 3 && GTK_SOURCE_MINOR_VERSION >= 22))
    gtk_source_search_context_replace2(search_context, match_start.gobj(), match_end.gobj(), replacement.c_str(), replacement.size(), nullptr);
#else
    gtk_source_search_context_replace(search_context, match_start.gobj(), match_end.gobj(), replacement.c_str(), replacement.size(), nullptr);
#endif

    get_buffer()->select_range(get_buffer()->get_iter_at_offset(offset), get_buffer()->get_iter_at_offset(offset+replacement.size()));
    scroll_to(get_buffer()->get_insert());
  }
}

void Source::View::replace_all(const std::string &replacement) {
  gtk_source_search_context_replace_all(search_context, replacement.c_str(), replacement.size(), nullptr);
}

void Source::View::paste() {
  std::string text=Gtk::Clipboard::get()->wait_for_text();
  
  //Replace carriage returns (which leads to crash) with newlines
  for(size_t c=0;c<text.size();c++) {
    if(text[c]=='\r') {
      if((c+1)<text.size() && text[c+1]=='\n')
        text.replace(c, 2, "\n");
      else
        text.replace(c, 1, "\n");
    }
  }
  
  //Exception for when pasted text is only whitespaces
  bool only_whitespaces=true;
  for(auto &chr: text) {
    if(chr!='\n' && chr!='\r' && chr!=' ' && chr!='\t') {
      only_whitespaces=false;
      break;
    }
  }
  if(only_whitespaces) {
    Gtk::Clipboard::get()->set_text(text);
    get_buffer()->paste_clipboard(Gtk::Clipboard::get());
    scroll_to_cursor_delayed(this, false, false);
    return;
  }
  
  get_buffer()->begin_user_action();
  if(get_buffer()->get_has_selection()) {
    Gtk::TextIter start, end;
    get_buffer()->get_selection_bounds(start, end);
    get_buffer()->erase(start, end);
  }
  auto line=get_line_before(); 
  std::string prefix_tabs;
  auto tabs_end_iter=get_tabs_end_iter();
  prefix_tabs=get_line_before(tabs_end_iter);

  size_t start_line=0;
  size_t end_line=0;
  bool paste_line=false;
  bool first_paste_line=true;
  size_t paste_line_tabs=-1;
  bool first_paste_line_has_tabs=false;
  for(size_t c=0;c<text.size();c++) {
    if(text[c]=='\n') {
      end_line=c;
      paste_line=true;
    }
    else if(c==text.size()-1) {
      end_line=c+1;
      paste_line=true;
    }
    if(paste_line) {
      bool empty_line=true;
      std::string line=text.substr(start_line, end_line-start_line);
      size_t tabs=0;
      for(auto chr: line) {
        if(chr==tab_char)
          tabs++;
        else {
          empty_line=false;
          break;
        }
      }
      if(first_paste_line) {
        if(tabs!=0) {
          first_paste_line_has_tabs=true;
          paste_line_tabs=tabs;
        }
        first_paste_line=false;
      }
      else if(!empty_line)
        paste_line_tabs=std::min(paste_line_tabs, tabs);

      start_line=end_line+1;
      paste_line=false;
    }
  }
  if(paste_line_tabs==static_cast<size_t>(-1))
    paste_line_tabs=0;
  start_line=0;
  end_line=0;
  paste_line=false;
  first_paste_line=true;
  for(size_t c=0;c<text.size();c++) {
    if(text[c]=='\n') {
      end_line=c;
      paste_line=true;
    }
    else if(c==text.size()-1) {
      end_line=c+1;
      paste_line=true;
    }
    if(paste_line) {
      std::string line=text.substr(start_line, end_line-start_line);
      size_t line_tabs=0;
      for(auto chr: line) {
        if(chr==tab_char)
          line_tabs++;
        else
          break;
      }
      auto tabs=paste_line_tabs;
      if(!(first_paste_line && !first_paste_line_has_tabs) && line_tabs<paste_line_tabs) {
        tabs=line_tabs;
      }
      
      if(first_paste_line) {
        if(first_paste_line_has_tabs)
          get_buffer()->insert_at_cursor(text.substr(start_line+tabs, end_line-start_line-tabs));
        else
          get_buffer()->insert_at_cursor(text.substr(start_line, end_line-start_line));
        first_paste_line=false;
      }
      else
        get_buffer()->insert_at_cursor('\n'+prefix_tabs+text.substr(start_line+tabs, end_line-start_line-tabs));
      start_line=end_line+1;
      paste_line=false;
    }
  }
  get_buffer()->place_cursor(get_buffer()->get_insert()->get_iter());
  get_buffer()->end_user_action();
  scroll_to_cursor_delayed(this, false, false);
}

Gtk::TextIter Source::View::get_iter_for_dialog() {
  auto iter=get_buffer()->get_insert()->get_iter();
  Gdk::Rectangle visible_rect;
  get_visible_rect(visible_rect);
  Gdk::Rectangle iter_rect;
  get_iter_location(iter, iter_rect);
  iter_rect.set_width(1);
  if(iter.get_line_offset()>=80) {
    get_iter_at_location(iter, visible_rect.get_x(), iter_rect.get_y());
    get_iter_location(iter, iter_rect);
  }
  if(!visible_rect.intersects(iter_rect))
    get_iter_at_location(iter, visible_rect.get_x(), visible_rect.get_y()+visible_rect.get_height()/3);
  return iter;
}

void Source::View::place_cursor_at_line_offset(int line, int offset) {
  line=std::min(line, get_buffer()->get_line_count()-1);
  if(line<0)
    line=0;
  auto iter=get_iter_at_line_end(line);
  offset=std::min(offset, iter.get_line_offset());
  get_buffer()->place_cursor(get_buffer()->get_iter_at_line_offset(line, offset));
}

void Source::View::place_cursor_at_line_index(int line, int index) {
  line=std::min(line, get_buffer()->get_line_count()-1);
  if(line<0)
    line=0;
  auto iter=get_iter_at_line_end(line);
  index=std::min(index, iter.get_line_index());
  get_buffer()->place_cursor(get_buffer()->get_iter_at_line_index(line, index));
}

void Source::View::hide_tooltips() {
  delayed_tooltips_connection.disconnect();
  type_tooltips.hide();
  diagnostic_tooltips.hide();
}

void Source::View::hide_dialogs() {
  SpellCheckView::hide_dialogs();
  if(SelectionDialog::get())
    SelectionDialog::get()->hide();
  if(CompletionDialog::get())
    CompletionDialog::get()->hide();
}

std::string Source::View::get_line(const Gtk::TextIter &iter) {
  auto line_start_it = get_buffer()->get_iter_at_line(iter.get_line());
  auto line_end_it = get_iter_at_line_end(iter.get_line());
  std::string line(get_buffer()->get_text(line_start_it, line_end_it));
  return line;
}
std::string Source::View::get_line(Glib::RefPtr<Gtk::TextBuffer::Mark> mark) {
  return get_line(mark->get_iter());
}
std::string Source::View::get_line(int line_nr) {
  return get_line(get_buffer()->get_iter_at_line(line_nr));
}
std::string Source::View::get_line() {
  return get_line(get_buffer()->get_insert());
}

std::string Source::View::get_line_before(const Gtk::TextIter &iter) {
  auto line_it = get_buffer()->get_iter_at_line(iter.get_line());
  std::string line(get_buffer()->get_text(line_it, iter));
  return line;
}
std::string Source::View::get_line_before(Glib::RefPtr<Gtk::TextBuffer::Mark> mark) {
  return get_line_before(mark->get_iter());
}
std::string Source::View::get_line_before() {
  return get_line_before(get_buffer()->get_insert());
}

Gtk::TextIter Source::View::get_tabs_end_iter(const Gtk::TextIter &iter) {
  return get_tabs_end_iter(iter.get_line());
}
Gtk::TextIter Source::View::get_tabs_end_iter(Glib::RefPtr<Gtk::TextBuffer::Mark> mark) {
  return get_tabs_end_iter(mark->get_iter());
}
Gtk::TextIter Source::View::get_tabs_end_iter(int line_nr) {
  auto sentence_iter = get_buffer()->get_iter_at_line(line_nr);
  while((*sentence_iter==' ' || *sentence_iter=='\t') && !sentence_iter.ends_line() && sentence_iter.forward_char()) {}
  return sentence_iter;
}
Gtk::TextIter Source::View::get_tabs_end_iter() {
  return get_tabs_end_iter(get_buffer()->get_insert());
}

bool Source::View::find_open_non_curly_bracket_backward(Gtk::TextIter iter, Gtk::TextIter &found_iter) {
  long para_count=0;
  long square_count=0;
  long curly_count=0;
  
  do {
    if(*iter=='(' && is_code_iter(iter))
      para_count++;
    else if(*iter==')' && is_code_iter(iter))
      para_count--;
    else if(*iter=='[' && is_code_iter(iter))
      square_count++;
    else if(*iter==']' && is_code_iter(iter))
      square_count--;
    else if(*iter=='{' && is_code_iter(iter))
      curly_count++;
    else if(*iter=='}' && is_code_iter(iter))
      curly_count--;
    
    if(curly_count>0)
      break;
    
    if(para_count>0 || square_count>0) {
      found_iter=iter;
      return true;
    }
  } while(iter.backward_char());
  return false;
}

Gtk::TextIter Source::View::find_start_of_sentence(Gtk::TextIter iter) {
  int para_count=0;
  int square_count=0;
  long curly_count=0;
  
  do {
    if(*iter=='(' && is_code_iter(iter))
      para_count++;
    else if(*iter==')' && is_code_iter(iter))
      para_count--;
    else if(*iter=='[' && is_code_iter(iter))
      square_count++;
    else if(*iter==']' && is_code_iter(iter))
      square_count--;
    else if(*iter=='{' && is_code_iter(iter))
      curly_count++;
    else if(*iter=='}' && is_code_iter(iter))
      curly_count--;
    
    if(curly_count>0)
      break;
    
    if(iter.starts_line() && para_count==0 && square_count==0) {
      //Handle : and , on previous line
      auto previous_iter=iter;
      previous_iter.backward_char();
      while(!previous_iter.starts_line() && (*previous_iter==' ' || previous_iter.ends_line()) && previous_iter.backward_char()) {}
      if(*previous_iter!=',' && *previous_iter!=':')
        return iter;
      else if(*previous_iter==':') {
        previous_iter.backward_char();
        while(!previous_iter.starts_line() && *previous_iter==' ' && previous_iter.backward_char()) {}
        if(*previous_iter==')') {
          auto token=get_token(get_tabs_end_iter(get_buffer()->get_iter_at_line(previous_iter.get_line())));
          if(token=="case")
            return iter;
        }
        else
          return iter;
      }
    }
  } while(iter.backward_char());
  
  return iter;
}

bool Source::View::find_open_curly_bracket_backward(Gtk::TextIter iter, Gtk::TextIter &found_iter) {
  long count=0;
  
  do {
    if(*iter=='{') {
      if(count==0 && is_code_iter(iter)) {
        found_iter=iter;
        return true;
      }
      count++;
    }
    else if(*iter=='}' && is_code_iter(iter))
      count--;
  } while(iter.backward_char());
  return false;
}

bool Source::View::find_close_curly_bracket_forward(Gtk::TextIter iter, Gtk::TextIter &found_iter) {
  long count=0;
  
  do {
    if(*iter=='}' && is_code_iter(iter)) {
      if(count==0) {
        found_iter=iter;
        return true;
      }
      count--;
    }
    else if(*iter=='{' && is_code_iter(iter))
      count++;
  } while(iter.forward_char());
  return false;
}

long Source::View::symbol_count(Gtk::TextIter iter, unsigned int positive_char, unsigned int negative_char) {
  auto iter_stored=iter;
  long symbol_count=0;
  long curly_count=0;
  bool break_on_curly=true;
  if(positive_char=='{' || negative_char=='}')
    break_on_curly=false;
  bool check_if_next_iter_is_code_iter=false;
  if(positive_char=='\'' || negative_char=='\'' || positive_char=='"' || negative_char=='"')
    check_if_next_iter_is_code_iter=true;
  
  Gtk::TextIter previous_iter;
  do {
    if(*iter==positive_char && is_code_iter(iter))
      symbol_count++;
    else if(*iter==negative_char && is_code_iter(iter))
      symbol_count--;
    else if(*iter=='{' && is_code_iter(iter))
      curly_count++;
    else if(*iter=='}' && is_code_iter(iter))
      curly_count--;
    else if(check_if_next_iter_is_code_iter) {
      auto next_iter=iter;
      if(next_iter.forward_char()) {
        if(*iter==positive_char && is_code_iter(next_iter))
          symbol_count++;
        else if(*iter==negative_char && is_code_iter(next_iter))
          symbol_count--;
      }
    }
    
    if(break_on_curly && curly_count>0)
      break;
  } while(iter.backward_char());
  
  iter=iter_stored;
  if(!iter.forward_char()) {
    return symbol_count;
  }
  
  curly_count=0;
  do {
    if(*iter==positive_char && is_code_iter(iter))
      symbol_count++;
    else if(*iter==negative_char && is_code_iter(iter))
      symbol_count--;
    else if(*iter=='{' && is_code_iter(iter))
      curly_count++;
    else if(*iter=='}' && is_code_iter(iter))
      curly_count--;
    else if(check_if_next_iter_is_code_iter) {
      auto next_iter=iter;
      if(next_iter.forward_char()) {
        if(*iter==positive_char && is_code_iter(next_iter))
          symbol_count++;
        else if(*iter==negative_char && is_code_iter(next_iter))
          symbol_count--;
      }
    }
    
    if(break_on_curly && curly_count<0)
      break;
  } while(iter.forward_char());
  
  return symbol_count;
}

bool Source::View::is_templated_function(Gtk::TextIter iter, Gtk::TextIter &parenthesis_end_iter) {
  auto iter_stored=iter;
  long bracket_count=0;
  long curly_count=0;
  
  if(!(iter.backward_char() && *iter=='>' && *iter_stored=='('))
    return false;
  
  do {
    if(*iter=='<' && is_code_iter(iter))
      bracket_count++;
    else if(*iter=='>' && is_code_iter(iter))
      bracket_count--;
    else if(*iter=='{' && is_code_iter(iter))
      curly_count++;
    else if(*iter=='}' && is_code_iter(iter))
      curly_count--;
    
    if(bracket_count==0)
      break;
    
    if(curly_count>0)
      break;
  } while(iter.backward_char());
  
  if(bracket_count!=0)
    return false;
  
  iter=iter_stored;
  bracket_count=0;
  curly_count=0;
  do {
    if(*iter=='(' && is_code_iter(iter))
      bracket_count++;
    else if(*iter==')' && is_code_iter(iter))
      bracket_count--;
    else if(*iter=='{' && is_code_iter(iter))
      curly_count++;
    else if(*iter=='}' && is_code_iter(iter))
      curly_count--;
    
    if(bracket_count==0) {
      parenthesis_end_iter=iter;
      return true;
    }
    
    if(curly_count<0)
      return false;
  } while(iter.forward_char());
  
  return false;
}

std::string Source::View::get_token(Gtk::TextIter iter) {
  auto start=iter;
  auto end=iter;
  
  while((*iter>='A' && *iter<='Z') || (*iter>='a' && *iter<='z') || (*iter>='0' && *iter<='9') || *iter=='_') {
    start=iter;
    if(!iter.backward_char())
      break;
  }
  while((*end>='A' && *end<='Z') || (*end>='a' && *end<='z') || (*end>='0' && *end<='9') || *end=='_') {
    if(!end.forward_char())
      break;
  }
  
  return get_buffer()->get_text(start, end);
}

void Source::View::cleanup_whitespace_characters_on_return(const Gtk::TextIter &iter) {
  auto start_blank_iter=iter;
  auto end_blank_iter=iter;
  while((*end_blank_iter==' ' || *end_blank_iter=='\t') &&
        !end_blank_iter.ends_line() && end_blank_iter.forward_char()) {}
  if(!start_blank_iter.starts_line()) {
    start_blank_iter.backward_char();
    while((*start_blank_iter==' ' || *start_blank_iter=='\t') &&
          !start_blank_iter.starts_line() && start_blank_iter.backward_char()) {}
    if(*start_blank_iter!=' ' && *start_blank_iter!='\t')
      start_blank_iter.forward_char();
  }

  if(start_blank_iter.starts_line())
    get_buffer()->erase(iter, end_blank_iter);
  else
    get_buffer()->erase(start_blank_iter, end_blank_iter);
}

bool Source::View::on_key_press_event(GdkEventKey* key) {
  if(SelectionDialog::get() && SelectionDialog::get()->is_visible()) {
    if(SelectionDialog::get()->on_key_press(key))
      return true;
  }
  if(CompletionDialog::get() && CompletionDialog::get()->is_visible()) {
    if(CompletionDialog::get()->on_key_press(key))
      return true;
  }
  
  if(last_keyval<GDK_KEY_Shift_L || last_keyval>GDK_KEY_Hyper_R)
    previous_non_modifier_keyval=last_keyval;
  last_keyval=key->keyval;
  
  //Move cursor one paragraph down
  if((key->keyval==GDK_KEY_Down || key->keyval==GDK_KEY_KP_Down) && (key->state&GDK_CONTROL_MASK)>0) {
    auto selection_start_iter=get_buffer()->get_selection_bound()->get_iter();
    auto iter=get_buffer()->get_iter_at_line(get_buffer()->get_insert()->get_iter().get_line());
    bool empty_line=false;
    bool text_found=false;
    for(;;) {
      if(!iter)
        break;
      if(iter.starts_line())
        empty_line=true;
      if(empty_line && !iter.ends_line() && *iter!='\n' && *iter!=' ' && *iter!='\t')
        empty_line=false;
      if(!text_found && !iter.ends_line() && *iter!='\n' && *iter!=' ' && *iter!='\t')
        text_found=true;
      if(empty_line && text_found && iter.ends_line())
        break;
      iter.forward_char();
    }
    iter=get_buffer()->get_iter_at_line(iter.get_line());
    if((key->state&GDK_SHIFT_MASK)>0)
      get_buffer()->select_range(iter, selection_start_iter);
    else
      get_buffer()->place_cursor(iter);
    scroll_to(get_buffer()->get_insert());
    return true;
  }
  //Move cursor one paragraph up
  else if((key->keyval==GDK_KEY_Up || key->keyval==GDK_KEY_KP_Up) && (key->state&GDK_CONTROL_MASK)>0) {
    auto selection_start_iter=get_buffer()->get_selection_bound()->get_iter();
    auto iter=get_buffer()->get_iter_at_line(get_buffer()->get_insert()->get_iter().get_line());
    iter.backward_char();
    bool empty_line=false;
    bool text_found=false;
    bool move_to_start=false;
    for(;;) {
      if(!iter)
        break;
      if(iter.ends_line())
        empty_line=true;
      if(empty_line && !iter.ends_line() && *iter!='\n' && *iter!=' ' && *iter!='\t')
        empty_line=false;
      if(!text_found && !iter.ends_line() && *iter!='\n' && *iter!=' ' && *iter!='\t')
        text_found=true;
      if(empty_line && text_found && iter.starts_line())
        break;
      if(iter.is_start()) {
        move_to_start=true;
        break;
      }
      iter.backward_char();
    }
    if(empty_line && !move_to_start) {
      iter=get_iter_at_line_end(iter.get_line());
      iter.forward_char();
      if(!iter.starts_line()) // For CR+LF
        iter.forward_char();
    }
    if((key->state&GDK_SHIFT_MASK)>0)
      get_buffer()->select_range(iter, selection_start_iter);
    else
      get_buffer()->place_cursor(iter);
    scroll_to(get_buffer()->get_insert());
    return true;
  }
  
  get_buffer()->begin_user_action();
  
  if(Config::get().source.smart_brackets && on_key_press_event_smart_brackets(key)) {
    get_buffer()->end_user_action();
    return true;
  }
  if(Config::get().source.smart_inserts && on_key_press_event_smart_inserts(key)) {
    get_buffer()->end_user_action();
    return true;
  }
  
  if(is_bracket_language && on_key_press_event_bracket_language(key)) {
    get_buffer()->end_user_action();
    return true;
  }
  else if(on_key_press_event_basic(key)) {
    get_buffer()->end_user_action();
    return true;
  }
  else {
    get_buffer()->end_user_action();
    return Gsv::View::on_key_press_event(key);
  }
}

//Basic indentation
bool Source::View::on_key_press_event_basic(GdkEventKey* key) {
  auto iter=get_buffer()->get_insert()->get_iter();

  //Indent as in next or previous line
  if((key->keyval==GDK_KEY_Return || key->keyval==GDK_KEY_KP_Enter) && !get_buffer()->get_has_selection() && !iter.starts_line()) {
    cleanup_whitespace_characters_on_return(iter);
    
    iter=get_buffer()->get_insert()->get_iter();
    auto tabs=get_line_before(get_tabs_end_iter(iter));
    
    int line_nr=iter.get_line();
    if(iter.ends_line() && (line_nr+1)<get_buffer()->get_line_count()) {
      auto next_line_tabs=get_line_before(get_tabs_end_iter(line_nr+1));
      if(next_line_tabs.size()>tabs.size()) {
        get_buffer()->insert_at_cursor("\n"+next_line_tabs);
        scroll_to(get_buffer()->get_insert());
        return true;
      }
    }
    get_buffer()->insert_at_cursor("\n"+tabs);
    scroll_to(get_buffer()->get_insert());
    return true;
  }
  else if(key->keyval==GDK_KEY_Tab && (key->state&GDK_SHIFT_MASK)==0) {
    if(!Config::get().source.tab_indents_line && !get_buffer()->get_has_selection()) {
      get_buffer()->insert_at_cursor(tab);
      return true;
    }
    //Indent right when clicking tab, no matter where in the line the cursor is. Also works on selected text.
    //Special case if insert is at beginning of empty line:
    if(iter.starts_line() && iter.ends_line() && !get_buffer()->get_has_selection()) {
      auto prev_line_iter=iter;
      while(prev_line_iter.starts_line() && prev_line_iter.backward_char()) {}
      auto prev_line_tabs_end_iter=get_tabs_end_iter(prev_line_iter);
      auto previous_line_tabs=get_line_before(prev_line_tabs_end_iter);

      auto next_line_iter=iter;
      while(next_line_iter.starts_line() && next_line_iter.forward_char()) {}
      auto next_line_tabs_end_iter=get_tabs_end_iter(next_line_iter);
      auto next_line_tabs=get_line_before(next_line_tabs_end_iter);
      
      std::string tabs;
      if(previous_line_tabs.size()<next_line_tabs.size())
        tabs=previous_line_tabs;
      else
        tabs=next_line_tabs;
      if(tabs.size()>=tab_size) {
        get_buffer()->insert_at_cursor(tabs);
        return true;
      }
    }
    
    Gtk::TextIter selection_start, selection_end;
    get_buffer()->get_selection_bounds(selection_start, selection_end);
    auto selection_end_mark=get_buffer()->create_mark(selection_end);
    int line_start=selection_start.get_line();
    int line_end=selection_end.get_line();
    for(int line=line_start;line<=line_end;line++) {
      Gtk::TextIter line_it = get_buffer()->get_iter_at_line(line);
      if(!get_buffer()->get_has_selection() || line_it!=selection_end_mark->get_iter())
        get_buffer()->insert(line_it, tab);
    }
    get_buffer()->delete_mark(selection_end_mark);
    return true;
  }
  //Indent left when clicking shift-tab, no matter where in the line the cursor is. Also works on selected text.
  else if((key->keyval==GDK_KEY_ISO_Left_Tab || key->keyval==GDK_KEY_Tab) && (key->state&GDK_SHIFT_MASK)>0) {
    Gtk::TextIter selection_start, selection_end;
    get_buffer()->get_selection_bounds(selection_start, selection_end);
    int line_start=selection_start.get_line();
    int line_end=selection_end.get_line();
    
    unsigned indent_left_steps=tab_size;
    std::vector<bool> ignore_line;
    for(int line_nr=line_start;line_nr<=line_end;line_nr++) {
      auto line_it = get_buffer()->get_iter_at_line(line_nr);
      if(!get_buffer()->get_has_selection() || line_it!=selection_end) {        
        auto tabs_end_iter=get_tabs_end_iter(line_nr);
        if(tabs_end_iter.starts_line() && tabs_end_iter.ends_line())
          ignore_line.push_back(true);
        else {
          auto line_tabs=get_line_before(tabs_end_iter);
          
          if(line_tabs.size()>0) {
            indent_left_steps=std::min(indent_left_steps, static_cast<unsigned>(line_tabs.size()));
            ignore_line.push_back(false);
          }
          else
            return true;
        }
      }
    }
    
    for(int line_nr=line_start;line_nr<=line_end;line_nr++) {
      Gtk::TextIter line_it = get_buffer()->get_iter_at_line(line_nr);
      Gtk::TextIter line_plus_it=line_it;
      if(!get_buffer()->get_has_selection() || line_it!=selection_end) {
        line_plus_it.forward_chars(indent_left_steps);
        if(!ignore_line.at(line_nr-line_start))
          get_buffer()->erase(line_it, line_plus_it);
      }
    }
    return true;
  }
  //"Smart" backspace key
  else if(key->keyval==GDK_KEY_BackSpace && !get_buffer()->get_has_selection()) {
    auto line=get_line_before();
    bool do_smart_backspace=true;
    for(auto &chr: line) {
      if(chr!=' ' && chr!='\t') {
        do_smart_backspace=false;
        break;
      }
    }
    if(do_smart_backspace) {
      auto previous_line_end_iter=iter;
      if(previous_line_end_iter.backward_chars(line.size()+1)) {
        if(!previous_line_end_iter.ends_line()) // For CR+LF
          previous_line_end_iter.backward_char();
        get_buffer()->erase(previous_line_end_iter, iter);
        return true;
      }
    }
  }
  //"Smart" delete key
  else if(key->keyval==GDK_KEY_Delete && !get_buffer()->get_has_selection()) {
    auto insert_iter=iter;
    bool do_smart_delete=true;
    do {
      if(*iter!=' ' && *iter!='\t' && !iter.ends_line()) {
        do_smart_delete=false;
        break;
      }
      if(iter.ends_line()) {
        if(*iter=='\r') // For CR+LF
          iter.forward_char();
        if(!iter.forward_char())
          do_smart_delete=false;
        break;
      }
    } while(iter.forward_char());
    if(do_smart_delete) {
      if(!insert_iter.starts_line()) {
        while((*iter==' ' || *iter=='\t') && iter.forward_char()) {}
      }
      get_buffer()->erase(insert_iter, iter);
      return true;
    }
  }
  //Next two are smart home/end keys that works with wrapped lines
  //Note that smart end goes FIRST to end of line to avoid hiding empty chars after expressions
  else if((key->keyval==GDK_KEY_End || key->keyval==GDK_KEY_KP_End) && (key->state&GDK_CONTROL_MASK)==0) {
    auto end_line_iter=get_iter_at_line_end(iter.get_line());
    auto end_sentence_iter=end_line_iter;
    while(!end_sentence_iter.starts_line() && 
          (*end_sentence_iter==' ' || *end_sentence_iter=='\t' || end_sentence_iter.ends_line()) &&
          end_sentence_iter.backward_char()) {}
    if(!end_sentence_iter.ends_line() && *end_sentence_iter!=' ' && *end_sentence_iter!='\t')
      end_sentence_iter.forward_char();
    if(iter==end_line_iter) {
      if((key->state&GDK_SHIFT_MASK)>0)
        get_buffer()->move_mark_by_name("insert", end_sentence_iter);
      else
        get_buffer()->place_cursor(end_sentence_iter);
    }
    else {
      if((key->state&GDK_SHIFT_MASK)>0)
        get_buffer()->move_mark_by_name("insert", end_line_iter);
      else
        get_buffer()->place_cursor(end_line_iter);
    }
    scroll_to(get_buffer()->get_insert());
    return true;
  }
  else if((key->keyval==GDK_KEY_Home || key->keyval==GDK_KEY_KP_Home) && (key->state&GDK_CONTROL_MASK)==0) {
    auto start_line_iter=get_buffer()->get_iter_at_line(iter.get_line());
    auto start_sentence_iter=start_line_iter;
    while(!start_sentence_iter.ends_line() && 
          (*start_sentence_iter==' ' || *start_sentence_iter=='\t') &&
          start_sentence_iter.forward_char()) {}
    
    if(iter>start_sentence_iter || iter==start_line_iter) {
      if((key->state&GDK_SHIFT_MASK)>0)
        get_buffer()->move_mark_by_name("insert", start_sentence_iter);
      else
        get_buffer()->place_cursor(start_sentence_iter);
    }
    else {
      if((key->state&GDK_SHIFT_MASK)>0)
        get_buffer()->move_mark_by_name("insert", start_line_iter);
      else
        get_buffer()->place_cursor(start_line_iter);
    }
    scroll_to(get_buffer()->get_insert());
    return true;
  }

  //Workaround for TextView::on_key_press_event bug sometimes causing segmentation faults
  //TODO: figure out the bug and create pull request to gtk
  //Have only experienced this on OS X
  //Note: valgrind reports issues on TextView::on_key_press_event as well
  auto unicode=gdk_keyval_to_unicode(key->keyval);
  if((key->state&(GDK_CONTROL_MASK|GDK_META_MASK))==0 && unicode>=32 && unicode!=127 &&
     (previous_non_modifier_keyval<GDK_KEY_dead_grave || previous_non_modifier_keyval>GDK_KEY_dead_greek)) {
    if(get_buffer()->get_has_selection()) {
      Gtk::TextIter selection_start, selection_end;
      get_buffer()->get_selection_bounds(selection_start, selection_end);
      get_buffer()->erase(selection_start, selection_end);
    }
    get_buffer()->insert_at_cursor(Glib::ustring(1, unicode));
    scroll_to(get_buffer()->get_insert());
    
    //Trick to make the cursor visible right after insertion:
    set_cursor_visible(false);
    set_cursor_visible();
    
    return true;
  }

  return false;
}

//Bracket language indentation
bool Source::View::on_key_press_event_bracket_language(GdkEventKey* key) {
  auto iter=get_buffer()->get_insert()->get_iter();

  if(get_buffer()->get_has_selection())
    return false;

  if(!is_code_iter(iter)) {
    // Add * at start of line in comment blocks
    if((key->keyval==GDK_KEY_Return || key->keyval==GDK_KEY_KP_Enter) && !iter.starts_line()) {
      cleanup_whitespace_characters_on_return(iter);
      
      iter=get_buffer()->get_insert()->get_iter();
      auto start_iter=get_tabs_end_iter(iter.get_line());
      auto end_iter=start_iter;
      end_iter.forward_chars(2);
      auto start_of_sentence=get_buffer()->get_text(start_iter, end_iter);
      if(!start_of_sentence.empty()) {
        if(start_of_sentence=="/*" || start_of_sentence[0]=='*') {
          auto tabs=get_line_before(start_iter);
          auto insert_str="\n"+tabs;
          if(start_of_sentence[0]=='/')
            insert_str+=' ';
          insert_str+="* ";
          
          get_buffer()->insert_at_cursor(insert_str);
          return true;
        }
      }
    }
    return false;
  }
  
  //Indent depending on if/else/etc and brackets
  if((key->keyval==GDK_KEY_Return || key->keyval==GDK_KEY_KP_Enter) && !iter.starts_line()) {
    cleanup_whitespace_characters_on_return(iter);
    
    iter=get_buffer()->get_insert()->get_iter();
    auto previous_iter=iter;
    previous_iter.backward_char();
    auto start_iter=previous_iter;
    if(*start_iter=='{')
      start_iter.backward_char();
    Gtk::TextIter open_non_curly_bracket_iter;
    bool open_non_curly_bracket_iter_found=false;
    if(find_open_non_curly_bracket_backward(start_iter, open_non_curly_bracket_iter)) {
      open_non_curly_bracket_iter_found=true;
      start_iter=get_tabs_end_iter(get_buffer()->get_iter_at_line(open_non_curly_bracket_iter.get_line()));
    }
    else
      start_iter=get_tabs_end_iter(get_buffer()->get_iter_at_line(find_start_of_sentence(start_iter).get_line()));
    auto tabs=get_line_before(start_iter);
    
    /*
     * Change tabs after ending comment block with an extra space (as in this case)
     */
    if(tabs.size()%tab_size==1) {
      iter=get_buffer()->get_insert()->get_iter();
      auto tabs_end_iter=get_tabs_end_iter(iter);
      if(!tabs_end_iter.ends_line() && !is_code_iter(tabs_end_iter)) {
        auto end_of_line_iter=tabs_end_iter;
        end_of_line_iter.forward_to_line_end();
        auto line=get_buffer()->get_text(tabs_end_iter, end_of_line_iter);
        if(!line.empty() && line.compare(0, 2, "*/")==0)
          tabs.pop_back();
      }
    }
    
    if(*previous_iter=='{') {
      Gtk::TextIter found_iter;
      bool found_right_bracket=find_close_curly_bracket_forward(iter, found_iter);
      
      bool has_right_curly_bracket=false;
      if(found_right_bracket) {
        auto tabs_end_iter=get_tabs_end_iter(found_iter);
        auto line_tabs=get_line_before(tabs_end_iter);
        if(tabs.size()==line_tabs.size())
          has_right_curly_bracket=true;
      }
      if(*get_buffer()->get_insert()->get_iter()=='}') {
        get_buffer()->insert_at_cursor("\n"+tabs+tab+"\n"+tabs);
        auto insert_it = get_buffer()->get_insert()->get_iter();
        if(insert_it.backward_chars(tabs.size()+1)) {
          scroll_to(get_buffer()->get_insert());
          get_buffer()->place_cursor(insert_it);
        }
        return true;
      }
      else if(!has_right_curly_bracket) {
        //Insert new lines with bracket end
        bool add_semicolon=false;
        if(language && (language->get_id()=="chdr" || language->get_id()=="cpphdr" ||
                        language->get_id()=="c" || language->get_id()=="cpp")) {
          auto token=get_token(start_iter);
          if(token.empty()) {
            auto iter=start_iter;
            while(!iter.starts_line() && iter.backward_char()) {}
            if(iter.backward_char())
              token=get_token(get_tabs_end_iter(get_buffer()->get_iter_at_line(iter.get_line())));
          }
          //Add semicolon after class or struct
          if(token=="class" || token=="struct")
            add_semicolon=true;
          //Add semicolon after lambda unless it's a parameter
          else if(!open_non_curly_bracket_iter_found) {
            auto it=previous_iter;
            long para_count=0;
            long square_count=0;
            bool square_outside_para_found=false;
            while(it.backward_char()) {
              if(*it==']' && is_code_iter(it)) {
                --square_count;
                if(para_count==0)
                  square_outside_para_found=true;
              }
              else if(*it=='[' && is_code_iter(it))
                ++square_count;
              else if(*it==')' && is_code_iter(it))
                --para_count;
              else if(*it=='(' && is_code_iter(it))
                ++para_count;
              
              if(square_outside_para_found && square_count==0 && para_count==0) {
                add_semicolon=true;
                break;
              }
              if(it==start_iter)
                break;
              if(!square_outside_para_found && square_count==0 && para_count==0) {
                if((*it>='A' && *it<='Z') || (*it>='a' && *it<='z') || (*it>='0' && *it<='9') || *it=='_' ||
                   *it=='-' || *it==' ' || *it=='\t' || *it=='<' || *it=='>' || *it=='(' || *it==':' ||
                   *it=='*' || *it=='&' || *it=='/' || it.ends_line() || !is_code_iter(it)) {
                  continue;
                }
                else
                  break;
              }
            }
          }
        }
        get_buffer()->insert_at_cursor("\n"+tabs+tab+"\n"+tabs+(add_semicolon?"};":"}"));
        auto insert_it = get_buffer()->get_insert()->get_iter();
        if(insert_it.backward_chars(tabs.size()+(add_semicolon?3:2))) {
          scroll_to(get_buffer()->get_insert());
          get_buffer()->place_cursor(insert_it);
        }
        return true;
      }
      else {
        get_buffer()->insert_at_cursor("\n"+tabs+tab);
        scroll_to(get_buffer()->get_insert());
        return true;
      }
    }
    
    //Indent multiline expressions
    if(open_non_curly_bracket_iter_found) {
      auto tabs_end_iter=get_tabs_end_iter(open_non_curly_bracket_iter);
      auto tabs=get_line_before(get_tabs_end_iter(open_non_curly_bracket_iter));
      auto iter=tabs_end_iter;
      while(iter<=open_non_curly_bracket_iter) {
        tabs+=' ';
        iter.forward_char();
      }
      get_buffer()->insert_at_cursor("\n"+tabs);
      scroll_to(get_buffer()->get_insert());
      return true;
    }
    auto line=get_line_before();
    std::smatch sm;
    if(std::regex_match(line, sm, no_bracket_statement_regex)) {
      get_buffer()->insert_at_cursor("\n"+tabs+tab);
      scroll_to(get_buffer()->get_insert());
      return true;
    }
    else if(std::regex_match(line, sm, no_bracket_no_para_statement_regex)) {
      get_buffer()->insert_at_cursor("\n"+tabs+tab);
      scroll_to(get_buffer()->get_insert());
      return true;
    }
    //Indenting after for instance if(...)\n...;\n
    else if(iter.backward_char() && *iter==';') {
      std::smatch sm2;
      size_t line_nr=get_buffer()->get_insert()->get_iter().get_line();
      if(line_nr>0 && tabs.size()>=tab_size) {
        std::string previous_line=get_line(line_nr-1);
        if(!std::regex_match(previous_line, sm2, bracket_regex)) {
          if(std::regex_match(previous_line, sm2, no_bracket_statement_regex)) {
            get_buffer()->insert_at_cursor("\n"+sm2[1].str());
            scroll_to(get_buffer()->get_insert());
            return true;
          }
          else if(std::regex_match(previous_line, sm2, no_bracket_no_para_statement_regex)) {
            get_buffer()->insert_at_cursor("\n"+sm2[1].str());
            scroll_to(get_buffer()->get_insert());
            return true;
          }
        }
      }
    }
    //Indenting after ':'
    else if(*iter==':') {
      bool perform_indent=true;
      auto previous_iter=iter;
      previous_iter.backward_char();
      while(!previous_iter.starts_line() && *previous_iter==' ' && previous_iter.backward_char()) {}
      if(*previous_iter==')') {
        auto token=get_token(get_tabs_end_iter(get_buffer()->get_iter_at_line(previous_iter.get_line())));
        if(token!="case")
          perform_indent=false;
      }
      if(perform_indent) {
        Gtk::TextIter found_curly_iter;
        if(find_open_curly_bracket_backward(iter, found_curly_iter)) {
          auto tabs_end_iter=get_tabs_end_iter(get_buffer()->get_iter_at_line(found_curly_iter.get_line()));
          auto tabs_start_of_sentence=get_line_before(tabs_end_iter);
          if(tabs.size()==(tabs_start_of_sentence.size()+tab_size)) {
            auto start_line_iter=get_buffer()->get_iter_at_line(iter.get_line());
            auto start_line_plus_tab_size=start_line_iter;
            for(size_t c=0;c<tab_size;c++)
              start_line_plus_tab_size.forward_char();
            get_buffer()->erase(start_line_iter, start_line_plus_tab_size);
          }
          else {
            get_buffer()->insert_at_cursor("\n"+tabs+tab);
            scroll_to(get_buffer()->get_insert());
            return true;
          }
        }
      }
    }
    get_buffer()->insert_at_cursor("\n"+tabs);
    scroll_to(get_buffer()->get_insert());
    return true;
  }
  //Indent left when writing } on a new line
  else if(key->keyval==GDK_KEY_braceright) {
    std::string line=get_line_before();
    if(line.size()>=tab_size && iter.ends_line()) {
      bool indent_left=true;
      for(auto c: line) {
        if(c!=tab_char) {
          indent_left=false;
          break;
        }
      }
      if(indent_left) {
        Gtk::TextIter insert_it = get_buffer()->get_insert()->get_iter();
        Gtk::TextIter line_it = get_buffer()->get_iter_at_line(insert_it.get_line());
        Gtk::TextIter line_plus_it=line_it;
        line_plus_it.forward_chars(tab_size);
        get_buffer()->erase(line_it, line_plus_it);
        get_buffer()->insert_at_cursor("}");
        return true;
      }
    }
  }
  //Indent left when writing { on a new line after for instance if(...)\n...
  else if(key->keyval==GDK_KEY_braceleft) {
    auto iter=get_buffer()->get_insert()->get_iter();
    auto tabs_end_iter=get_tabs_end_iter();
    auto tabs=get_line_before(tabs_end_iter);
    size_t line_nr=iter.get_line();
    if(line_nr>0 && tabs.size()>=tab_size && iter==tabs_end_iter) {
      std::string previous_line=get_line(line_nr-1);
      std::smatch sm;
      if(!std::regex_match(previous_line, sm, bracket_regex)) {
        auto start_iter=iter;
        start_iter.backward_chars(tab_size);
        if(std::regex_match(previous_line, sm, no_bracket_statement_regex) ||
           std::regex_match(previous_line, sm, no_bracket_no_para_statement_regex)) {
          if((tabs.size()-tab_size)==sm[1].str().size()) {
            get_buffer()->erase(start_iter, iter);
            get_buffer()->insert_at_cursor("{");
            scroll_to(get_buffer()->get_insert());
            return true;
          }
        }
      }
    }
  }
  
  return false;
}

bool Source::View::on_key_press_event_smart_brackets(GdkEventKey *key) {
  if(get_buffer()->get_has_selection())
    return false;
  
  auto iter=get_buffer()->get_insert()->get_iter();
  auto previous_iter=iter;
  previous_iter.backward_char();
  if(is_code_iter(iter)) {
    //Move after ')' if closed expression
    if(key->keyval==GDK_KEY_parenright) {
      if(*iter==')' && symbol_count(iter, '(', ')')==0) {
        iter.forward_char();
        get_buffer()->place_cursor(iter);
        scroll_to(get_buffer()->get_insert());
        return true;
      }
    }
    //Move after '>' if >( and closed expression
    else if(key->keyval==GDK_KEY_greater) {
      if(*iter=='>') {
        iter.forward_char();
        Gtk::TextIter parenthesis_end_iter;
        if(*iter=='(' && is_templated_function(iter, parenthesis_end_iter)) {
          get_buffer()->place_cursor(iter);
          scroll_to(get_buffer()->get_insert());
          return true;
        }
      }
    }
    //Move after '(' if >( and select text inside parentheses
    else if(key->keyval==GDK_KEY_parenleft) {
      auto previous_iter=iter;
      previous_iter.backward_char();
      if(*previous_iter=='>') {
        Gtk::TextIter parenthesis_end_iter;
        if(*iter=='(' && is_templated_function(iter, parenthesis_end_iter)) {
          iter.forward_char();
          get_buffer()->select_range(iter, parenthesis_end_iter);
          scroll_to(iter);
          return true;
        }
      }
    }
  }
  
  return false;
}

bool Source::View::on_key_press_event_smart_inserts(GdkEventKey *key) {
  if(get_buffer()->get_has_selection()) {
    bool perform_insertion=false;
    char left_char, right_char;
    // Insert () around selection
    if(key->keyval==GDK_KEY_parenleft) {
      perform_insertion=true;
      left_char='(';
      right_char=')';
    }
    // Insert [] around selection
    else if(key->keyval==GDK_KEY_bracketleft) {
      perform_insertion=true;
      left_char='[';
      right_char=']';
    }
    // Insert {} around selection
    else if(key->keyval==GDK_KEY_braceleft) {
      perform_insertion=true;
      left_char='{';
      right_char='}';
    }
    // Insert <> around selection
    else if(key->keyval==GDK_KEY_less) {
      perform_insertion=true;
      left_char='<';
      right_char='>';
    }
    // Insert '' around selection
    else if(key->keyval==GDK_KEY_apostrophe) {
      perform_insertion=true;
      left_char='\'';
      right_char='\'';
    }
    // Insert "" around selection
    else if(key->keyval==GDK_KEY_quotedbl) {
      perform_insertion=true;
      left_char='"';
      right_char='"';
    }
    else if(language && language->get_id()=="markdown") {
      if(key->keyval==GDK_KEY_dead_grave) {
        perform_insertion=true;
        left_char='`';
        right_char='`';
      }
      if(key->keyval==GDK_KEY_asterisk) {
        perform_insertion=true;
        left_char='*';
        right_char='*';
      }
      if(key->keyval==GDK_KEY_underscore) {
        perform_insertion=true;
        left_char='_';
        right_char='_';
      }
      if(key->keyval==GDK_KEY_dead_tilde) {
        perform_insertion=true;
        left_char='~';
        right_char='~';
      }
    }
    if(perform_insertion) {
      Gtk::TextIter start, end;
      get_buffer()->get_selection_bounds(start, end);
      auto start_mark=get_buffer()->create_mark(start);
      auto end_mark=get_buffer()->create_mark(end);
      get_buffer()->insert(start, Glib::ustring()+left_char);
      get_buffer()->insert(end_mark->get_iter(), Glib::ustring()+right_char);
      auto start_mark_next_iter=start_mark->get_iter();
      start_mark_next_iter.forward_char();
      get_buffer()->select_range(start_mark_next_iter, end_mark->get_iter());
      get_buffer()->delete_mark(start_mark);
      get_buffer()->delete_mark(end_mark);
      return true;
    }
    return false;
  }
  
  auto iter=get_buffer()->get_insert()->get_iter();
  auto previous_iter=iter;
  previous_iter.backward_char();
  auto next_iter=iter;
  next_iter.forward_char();
  
  auto allow_insertion=[](const Gtk::TextIter &iter) {
    if(iter.ends_line() || *iter==' ' || *iter=='\t' || *iter==';' || *iter==')' || *iter==']' || *iter=='[' || *iter=='{' || *iter=='}')
      return true;
    return false;
  };
  
  // Move right when clicking ' before a ' or when clicking " before a "
  if(((key->keyval==GDK_KEY_apostrophe && *iter=='\'') ||
      (key->keyval==GDK_KEY_quotedbl && *iter=='\"')) && is_code_iter(next_iter)) {
    bool perform_move=false;
    if(*previous_iter!='\\')
      perform_move=true;
    else {
      auto it=previous_iter;
      long backslash_count=1;
      while(it.backward_char() && *it=='\\') {
        ++backslash_count;
      }
      if(backslash_count%2==0)
        perform_move=true;
    }
    if(perform_move) {
      get_buffer()->place_cursor(next_iter);
      scroll_to(get_buffer()->get_insert());
      return true;
    }
  }
  // When to delete '' or ""
  else if(key->keyval==GDK_KEY_BackSpace) {
    if(((*previous_iter=='\'' && *iter=='\'') ||
        (*previous_iter=='"' && *iter=='"')) && is_code_iter(previous_iter)) {
      get_buffer()->erase(previous_iter, next_iter);
      scroll_to(get_buffer()->get_insert());
      return true;
    }
  }
  
  if(is_code_iter(iter)) {
    // Insert ()
    if(key->keyval==GDK_KEY_parenleft && allow_insertion(iter)) {
      if(symbol_count(iter, '(', ')')==0) {
        get_buffer()->insert_at_cursor("()");
        auto iter=get_buffer()->get_insert()->get_iter();
        iter.backward_char();
        get_buffer()->place_cursor(iter);
        scroll_to(get_buffer()->get_insert());
        return true;
      }
    }
    // Insert []
    else if(key->keyval==GDK_KEY_bracketleft && allow_insertion(iter)) {
      if(symbol_count(iter, '[', ']')==0) {
        get_buffer()->insert_at_cursor("[]");
        auto iter=get_buffer()->get_insert()->get_iter();
        iter.backward_char();
        get_buffer()->place_cursor(iter);
        scroll_to(get_buffer()->get_insert());
        return true;
      }
    }
    // Move left on ] in []
    else if(key->keyval==GDK_KEY_bracketright) {
      if(*iter==']' && symbol_count(iter, '[', ']')==0) {
        iter.forward_char();
        get_buffer()->place_cursor(iter);
        scroll_to(get_buffer()->get_insert());
        return true;
      }
    }
    // Insert ''
    else if(key->keyval==GDK_KEY_apostrophe && allow_insertion(iter) && symbol_count(iter, '\'', -1)%2==0) {
      get_buffer()->insert_at_cursor("''");
      auto iter=get_buffer()->get_insert()->get_iter();
      iter.backward_char();
      get_buffer()->place_cursor(iter);
      scroll_to(get_buffer()->get_insert());
      return true;
    }
    // Insert ""
    else if(key->keyval==GDK_KEY_quotedbl && allow_insertion(iter) && symbol_count(iter, '"', -1)%2==0) {
      get_buffer()->insert_at_cursor("\"\"");
      auto iter=get_buffer()->get_insert()->get_iter();
      iter.backward_char();
      get_buffer()->place_cursor(iter);
      scroll_to(get_buffer()->get_insert());
      return true;
    }
    // Insert ; at the end of line, if iter is at the last )
    else if(key->keyval==GDK_KEY_semicolon) {
      if(*iter==')' && symbol_count(iter, '(', ')')==0) {
        if(next_iter.ends_line()) {
          Gtk::TextIter open_non_curly_bracket_iter;
          if(find_open_non_curly_bracket_backward(previous_iter, open_non_curly_bracket_iter)) {
            open_non_curly_bracket_iter.backward_char();
            if(*open_non_curly_bracket_iter==' ')
              open_non_curly_bracket_iter.backward_char();
            if(get_token(open_non_curly_bracket_iter)!="for") {
              iter.forward_char();
              get_buffer()->place_cursor(iter);
              get_buffer()->insert_at_cursor(";");
              scroll_to(get_buffer()->get_insert());
              return true;
            }
          }
        }
      }
    }
    // Delete ()
    else if(key->keyval==GDK_KEY_BackSpace) {
      if(*previous_iter=='(' && *iter==')' && symbol_count(iter, '(', ')')==0) {
        auto next_iter=iter;
        next_iter.forward_char();
        get_buffer()->erase(previous_iter, next_iter);
        scroll_to(get_buffer()->get_insert());
        return true;
      }
      // Delete []
      else if(*previous_iter=='[' && *iter==']' && symbol_count(iter, '[', ']')==0) {
        auto next_iter=iter;
        next_iter.forward_char();
        get_buffer()->erase(previous_iter, next_iter);
        scroll_to(get_buffer()->get_insert());
        return true;
      }
    }
  }
  
  return false;
}

bool Source::View::on_button_press_event(GdkEventButton *event) {
  // Select range when double clicking
  if(event->type==GDK_2BUTTON_PRESS) {
    Gtk::TextIter start, end;
    get_buffer()->get_selection_bounds(start, end);
    auto iter=start;
    while((*iter>=48 && *iter<=57) || (*iter>=65 && *iter<=90) || (*iter>=97 && *iter<=122) || *iter==95) {
      start=iter;
      if(!iter.backward_char())
        break;
    }
    while((*end>=48 && *end<=57) || (*end>=65 && *end<=90) || (*end>=97 && *end<=122) || *end==95) {
      if(!end.forward_char())
        break;
    }
    get_buffer()->select_range(start, end);
    return true;
  }

  // Go to implementation or declaration
  if((event->type == GDK_BUTTON_PRESS) && (event->button == 1)){
#ifdef __APPLE__
    GdkModifierType mask=GDK_MOD2_MASK;
#else
    GdkModifierType mask=GDK_CONTROL_MASK;
#endif
    if(event->state & mask) {
      int x, y;
      window_to_buffer_coords(Gtk::TextWindowType::TEXT_WINDOW_TEXT, event->x, event->y, x, y);
      Gtk::TextIter iter;
      get_iter_at_location(iter, x, y);
      if(iter)
        get_buffer()->place_cursor(iter);
      
      Menu::get().actions["source_goto_declaration_or_implementation"]->activate();
      return true;
    }
  }

  // Open right click menu
  if((event->type == GDK_BUTTON_PRESS) && (event->button == 3)){
    hide_tooltips();
    if(!get_buffer()->get_has_selection()){
      int x,y;
      window_to_buffer_coords(Gtk::TextWindowType::TEXT_WINDOW_TEXT,event->x,event->y,x,y);
      Gtk::TextIter iter;
      get_iter_at_location(iter,x,y);
      if(iter)
        get_buffer()->place_cursor(iter);
      Menu::get().right_click_line_menu->popup(event->button,event->time);
    } else {
      Menu::get().right_click_selected_menu->popup(event->button,event->time);
    }
    return true;
  }
  return Gsv::View::on_button_press_event(event);
}

std::pair<char, unsigned> Source::View::find_tab_char_and_size() {
  std::unordered_map<char, size_t> tab_chars;
  std::unordered_map<unsigned, size_t> tab_sizes;
  auto iter=get_buffer()->begin();
  long tab_count=-1;
  long last_tab_count=0;
  bool single_quoted=false;
  bool double_quoted=false;
  //For bracket languages, TODO: add more language ids
  if(is_bracket_language && !(language && language->get_id()=="html")) {
    bool line_comment=false;
    bool comment=false;
    bool bracket_last_line=false;
    char last_char=0;
    long last_tab_diff=-1;
    while(iter) {
      if(iter.starts_line()) {
        line_comment=false;
        single_quoted=false;
        double_quoted=false;
        tab_count=0;
        if(last_char=='{')
          bracket_last_line=true;
        else
          bracket_last_line=false;
      }
      if(bracket_last_line && tab_count!=-1) {
        if(*iter==' ') {
          tab_chars[' ']++;
          tab_count++;
        }
        else if(*iter=='\t') {
          tab_chars['\t']++;
          tab_count++;
        }
        else {
          auto line_iter=iter;
          char last_line_char=0;
          while(line_iter && !line_iter.ends_line()) {
            if(*line_iter!=' ' && *line_iter!='\t')
              last_line_char=*line_iter;
            if(*line_iter=='(')
              break;
            line_iter.forward_char();
          }
          if(last_line_char==':' || *iter=='#') {
            tab_count=0;
            if((iter.get_line()+1) < get_buffer()->get_line_count()) {
              iter=get_buffer()->get_iter_at_line(iter.get_line()+1);
              continue;
            }
          }
          else if(!iter.ends_line()) {
            if(tab_count!=last_tab_count)
              tab_sizes[std::abs(tab_count-last_tab_count)]++;
            last_tab_diff=std::abs(tab_count-last_tab_count);
            last_tab_count=tab_count;
            last_char=0;
          }
        }
      }

      auto prev_iter=iter;
      prev_iter.backward_char();
      auto prev_prev_iter=prev_iter;
      prev_prev_iter.backward_char();
      if(!double_quoted && *iter=='\'' && !(*prev_iter=='\\' && *prev_prev_iter!='\\'))
        single_quoted=!single_quoted;
      else if(!single_quoted && *iter=='\"' && !(*prev_iter=='\\' && *prev_prev_iter!='\\'))
        double_quoted=!double_quoted;
      else if(!single_quoted && !double_quoted) {
        auto next_iter=iter;
        next_iter.forward_char();
        if(*iter=='/' && *next_iter=='/')
          line_comment=true;
        else if(*iter=='/' && *next_iter=='*')
          comment=true;
        else if(*iter=='*' && *next_iter=='/') {
          iter.forward_char();
          iter.forward_char();
          comment=false;
        }
      }
      if(!single_quoted && !double_quoted && !comment && !line_comment && *iter!=' ' && *iter!='\t' && !iter.ends_line())
        last_char=*iter;
      if(!single_quoted && !double_quoted && !comment && !line_comment && *iter=='}' && tab_count!=-1 && last_tab_diff!=-1)
        last_tab_count-=last_tab_diff;
      if(*iter!=' ' && *iter!='\t')
        tab_count=-1;

      iter.forward_char();
    }
  }
  else {
    long para_count=0;
    while(iter) {
      if(iter.starts_line())
        tab_count=0;
      if(tab_count!=-1 && para_count==0 && single_quoted==false && double_quoted==false) {
        if(*iter==' ') {
          tab_chars[' ']++;
          tab_count++;
        }
        else if(*iter=='\t') {
          tab_chars['\t']++;
          tab_count++;
        }
        else if(!iter.ends_line()) {
          if(tab_count!=last_tab_count)
            tab_sizes[std::abs(tab_count-last_tab_count)]++;
          last_tab_count=tab_count;
        }
      }
      auto prev_iter=iter;
      prev_iter.backward_char();
      auto prev_prev_iter=prev_iter;
      prev_prev_iter.backward_char();
      if(!double_quoted && *iter=='\'' && !(*prev_iter=='\\' && *prev_prev_iter!='\\'))
        single_quoted=!single_quoted;
      else if(!single_quoted && *iter=='\"' && !(*prev_iter=='\\' && *prev_prev_iter!='\\'))
        double_quoted=!double_quoted;
      else if(!single_quoted && !double_quoted) {
        if(*iter=='(')
          para_count++;
        else if(*iter==')')
          para_count--;
      }
      if(*iter!=' ' && *iter!='\t')
        tab_count=-1;

      iter.forward_char();
    }
  }

  char found_tab_char=0;
  size_t occurences=0;
  for(auto &tab_char: tab_chars) {
    if(tab_char.second>occurences) {
      found_tab_char=tab_char.first;
      occurences=tab_char.second;
    }
  }
  unsigned found_tab_size=0;
  occurences=0;
  for(auto &tab_size: tab_sizes) {
    if(tab_size.second>occurences) {
      found_tab_size=tab_size.first;
      occurences=tab_size.second;
    }
  }
  return {found_tab_char, found_tab_size};
}


/////////////////////
//// GenericView ////
/////////////////////
Source::GenericView::GenericView(const boost::filesystem::path &file_path, Glib::RefPtr<Gsv::Language> language) : View(file_path, language) {
  configure();
  spellcheck_all=true;
  
  if(language)
    get_source_buffer()->set_language(language);
  
  auto completion=get_completion();
  completion->property_show_headers()=false;
  completion->property_show_icons()=false;
  completion->property_accelerators()=0;
  
  auto completion_words=Gsv::CompletionWords::create("", Glib::RefPtr<Gdk::Pixbuf>());
  completion_words->register_provider(get_buffer());
  completion->add_provider(completion_words);
  
  if(language) {
    auto language_manager=Gsv::LanguageManager::get_default();
    auto search_paths=language_manager->get_search_path();
    bool found_language_file=false;
    boost::filesystem::path language_file;
    for(auto &search_path: search_paths) {
      boost::filesystem::path p(static_cast<std::string>(search_path)+'/'+static_cast<std::string>(language->get_id())+".lang");
      if(boost::filesystem::exists(p) && boost::filesystem::is_regular_file(p)) {
        language_file=p;
        found_language_file=true;
        break;
      }
    }
    if(found_language_file) {
      auto completion_buffer_keywords=CompletionBuffer::create();
      boost::property_tree::ptree pt;
      try {
        boost::property_tree::xml_parser::read_xml(language_file.string(), pt);
      }
      catch(const std::exception &e) {
        Terminal::get().print("Error: error parsing language file "+language_file.string()+": "+e.what()+'\n', true);
      }
      bool has_context_class=false;
      parse_language_file(completion_buffer_keywords, has_context_class, pt);
      if(!has_context_class)
        spellcheck_all=false;
      completion_words->register_provider(completion_buffer_keywords);
    }
  }
}

void Source::GenericView::parse_language_file(Glib::RefPtr<CompletionBuffer> &completion_buffer, bool &has_context_class, const boost::property_tree::ptree &pt) {
  bool case_insensitive=false;
  for(auto &node: pt) {
    if(node.first=="<xmlcomment>") {
      auto data=static_cast<std::string>(node.second.data());
      std::transform(data.begin(), data.end(), data.begin(), ::tolower);
      if(data.find("case insensitive")!=std::string::npos)
        case_insensitive=true;
    }
    else if(node.first=="keyword") {
      auto data=static_cast<std::string>(node.second.data());
      completion_buffer->insert_at_cursor(data+'\n');
      if(case_insensitive) {
        std::transform(data.begin(), data.end(), data.begin(), ::tolower);
        completion_buffer->insert_at_cursor(data+'\n');
      }
    }
    else if(!has_context_class && node.first=="context") {
      auto class_attribute=node.second.get<std::string>("<xmlattr>.class", "");
      auto class_disabled_attribute=node.second.get<std::string>("<xmlattr>.class-disabled", "");
      if(class_attribute.size()>0 || class_disabled_attribute.size()>0)
        has_context_class=true;
    }
    try {
      parse_language_file(completion_buffer, has_context_class, node.second);
    }
    catch(const std::exception &e) {        
    }
  }
}
