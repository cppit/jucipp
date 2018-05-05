#include <glib.h>
#include "source.h"
#include "filesystem.h"

std::string hello_world=R"(#include <iostream>  
    
int main() {  
  std::cout << "hello world\n";    
})";

std::string hello_world_cleaned=R"(#include <iostream>

int main() {
  std::cout << "hello world\n";
}
)";

//Requires display server to work
//However, it is possible to use the Broadway backend if the test is run in a pure terminal environment:
//broadwayd&
//make test

int main() {
  auto app=Gtk::Application::create();
  Gsv::init();
  
  auto tests_path=boost::filesystem::canonical(JUCI_TESTS_PATH);
  auto source_file=tests_path/"tmp"/"source_file.cpp";
  
  {
    Source::View source_view(source_file, Glib::RefPtr<Gsv::Language>());
    source_view.get_buffer()->set_text(hello_world);
    g_assert(source_view.save());
  }
  
  Source::View source_view(source_file, Glib::RefPtr<Gsv::Language>());
  g_assert(source_view.get_buffer()->get_text()==hello_world);
  source_view.cleanup_whitespace_characters();
  g_assert(source_view.get_buffer()->get_text()==hello_world_cleaned);
  
  g_assert(boost::filesystem::remove(source_file));
  g_assert(!boost::filesystem::exists(source_file));
  
  for(int c=0;c<2;++c) {
    size_t found=0;
    auto style_scheme_manager=Source::StyleSchemeManager::get_default();
    for(auto &search_path: style_scheme_manager->get_search_path()) {
      if(search_path=="styles") // found added style
        ++found;
    }
    g_assert(found==1);
  }
  
  // replace_text tests
  {
    auto buffer=source_view.get_buffer();
    {
      auto text="line 1\nline 2\nline3";
      buffer->set_text(text);
      buffer->place_cursor(buffer->begin());
      source_view.replace_text(text);
      assert(buffer->get_text()==text);
      assert(buffer->get_insert()->get_iter()==buffer->begin());
      
      buffer->place_cursor(buffer->end());
      source_view.replace_text(text);
      assert(buffer->get_text()==text);
      assert(buffer->get_insert()->get_iter()==buffer->end());
      
      source_view.place_cursor_at_line_offset(1, 0);
      source_view.replace_text(text);
      assert(buffer->get_text()==text);
      assert(buffer->get_insert()->get_iter().get_line()==1);
      assert(buffer->get_insert()->get_iter().get_line_offset()==0);
    }
    {
      auto old_text="line 1\nline3";
      auto new_text="line 1\nline 2\nline3";
      buffer->set_text(old_text);
      source_view.place_cursor_at_line_offset(1, 0);
      source_view.replace_text(new_text);
      assert(buffer->get_text()==new_text);
      assert(buffer->get_insert()->get_iter().get_line()==2);
      assert(buffer->get_insert()->get_iter().get_line_offset()==0);
      
      source_view.replace_text(old_text);
      assert(buffer->get_text()==old_text);
      assert(buffer->get_insert()->get_iter().get_line()==1);
      assert(buffer->get_insert()->get_iter().get_line_offset()==0);
      
      source_view.place_cursor_at_line_offset(0, 0);
      source_view.replace_text(new_text);
      assert(buffer->get_text()==new_text);
      assert(buffer->get_insert()->get_iter().get_line()==0);
      assert(buffer->get_insert()->get_iter().get_line_offset()==0);
      
      source_view.replace_text(old_text);
      assert(buffer->get_text()==old_text);
      assert(buffer->get_insert()->get_iter().get_line()==0);
      assert(buffer->get_insert()->get_iter().get_line_offset()==0);
      
      source_view.replace_text(new_text);
      assert(buffer->get_text()==new_text);
      
      source_view.place_cursor_at_line_offset(2, 0);
      source_view.replace_text(old_text);
      assert(buffer->get_text()==old_text);
      assert(buffer->get_insert()->get_iter().get_line()==1);
      assert(buffer->get_insert()->get_iter().get_line_offset()==0);
    }
    {
      auto old_text="line 1\nline 3";
      auto new_text="";
      buffer->set_text(old_text);
      source_view.replace_text(new_text);
      assert(buffer->get_text()==new_text);
      
      source_view.replace_text(old_text);
      assert(buffer->get_text()==old_text);
      assert(buffer->get_insert()->get_iter().get_line()==1);
      assert(buffer->get_insert()->get_iter().get_line_offset()==6);
    }
    {
      auto old_text="";
      auto new_text="";
      buffer->set_text(old_text);
      source_view.replace_text(new_text);
      assert(buffer->get_text()==new_text);
    }
    {
      auto old_text="line 1\nline 3\n";
      auto new_text="";
      buffer->set_text(old_text);
      source_view.replace_text(new_text);
      assert(buffer->get_text()==new_text);
      
      source_view.replace_text(old_text);
      assert(buffer->get_text()==old_text);
      assert(buffer->get_insert()->get_iter().get_line()==2);
      assert(buffer->get_insert()->get_iter().get_line_offset()==0);
    }
    {
      auto old_text="line 1\n\nline 3\nline 4\n\nline 5\n";
      auto new_text="line 1\n\nline 33\nline 44\n\nline 5\n";
      buffer->set_text(old_text);
      source_view.place_cursor_at_line_offset(2, 0);
      source_view.replace_text(new_text);
      assert(buffer->get_text()==new_text);
      assert(buffer->get_insert()->get_iter().get_line()==2);
      assert(buffer->get_insert()->get_iter().get_line_offset()==0);
      
      buffer->set_text(old_text);
      source_view.place_cursor_at_line_offset(3, 0);
      source_view.replace_text(new_text);
      assert(buffer->get_text()==new_text);
      assert(buffer->get_insert()->get_iter().get_line()==3);
      assert(buffer->get_insert()->get_iter().get_line_offset()==0);
    }
    {
      auto old_text="line 1\n\nline 3\nline 4\n\nline 5\n";
      auto new_text="line 1\n\nline 33\nline 44\nline 45\nline 46\n\nline 5\n";
      buffer->set_text(old_text);
      source_view.place_cursor_at_line_offset(2, 0);
      source_view.replace_text(new_text);
      assert(buffer->get_text()==new_text);
      assert(buffer->get_insert()->get_iter().get_line()==2);
      assert(buffer->get_insert()->get_iter().get_line_offset()==0);
      
      buffer->set_text(old_text);
      source_view.place_cursor_at_line_offset(3, 0);
      source_view.replace_text(new_text);
      assert(buffer->get_text()==new_text);
      assert(buffer->get_insert()->get_iter().get_line()==4);
      assert(buffer->get_insert()->get_iter().get_line_offset()==0);
    }
  }
}
