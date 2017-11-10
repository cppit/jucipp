#include "source.h"
#include <glib.h>

//Requires display server to work
//However, it is possible to use the Broadway backend if the test is run in a pure terminal environment:
//broadwayd&
//make test

int main() {
  auto app = Gtk::Application::create();
  Gsv::init();

  auto tests_path = boost::filesystem::canonical(JUCI_TESTS_PATH);
  auto source_file = tests_path / "tmp" / "source_file.cpp";

  auto language_manager = Gsv::LanguageManager::get_default();
  GdkEventKey event;
  event.state = 0;

  {
    auto language = language_manager->get_language("cpp");
    Source::View view(source_file, language);
    view.get_source_buffer()->set_highlight_syntax(true);
    view.get_source_buffer()->set_language(language);
    view.set_tab_char_and_size(' ', 2);
    event.keyval = GDK_KEY_Return;

    {
      view.get_buffer()->set_text("{");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "{\n"
                                                "  \n"
                                                "}");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 1);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 2);
    }
    {
      view.get_buffer()->set_text("{\n"
                                  "  \n"
                                  "}");
      auto iter = view.get_buffer()->begin();
      iter.forward_char();
      view.get_buffer()->place_cursor(iter);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "{\n"
                                                "  \n"
                                                "  \n"
                                                "}");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 1);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 2);
    }
    {
      view.get_buffer()->set_text("{\n"
                                  "  \n"
                                  "}");
      auto iter = view.get_buffer()->get_iter_at_line(2);
      iter.backward_char();
      view.get_buffer()->place_cursor(iter);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "{\n"
                                                "  \n"
                                                "  \n"
                                                "}");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 2);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 2);
    }
    {
      view.get_buffer()->set_text("{\n"
                                  "  {\n"
                                  "}");
      auto iter = view.get_buffer()->get_iter_at_line(2);
      iter.backward_char();
      view.get_buffer()->place_cursor(iter);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "{\n"
                                                "  {\n"
                                                "    \n"
                                                "  }\n"
                                                "}");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 2);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 4);
    }
    {
      view.get_buffer()->set_text("{\n"
                                  "  {\n"
                                  "    \n"
                                  "  }\n"
                                  "}");
      auto iter = view.get_buffer()->get_iter_at_line(2);
      iter.backward_char();
      view.get_buffer()->place_cursor(iter);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "{\n"
                                                "  {\n"
                                                "    \n"
                                                "    \n"
                                                "  }\n"
                                                "}");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 2);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 4);
    }
    {
      view.get_buffer()->set_text("{\n"
                                  "{\n"
                                  "}");
      auto iter = view.get_buffer()->get_iter_at_line(1);
      iter.forward_char();
      view.get_buffer()->place_cursor(iter);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "{\n"
                                                "{\n"
                                                "  \n"
                                                "}\n"
                                                "}");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 2);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 2);
    }
    {
      view.get_buffer()->set_text("{\n"
                                  "{\n"
                                  "  \n"
                                  "}\n"
                                  "}");
      auto iter = view.get_buffer()->get_iter_at_line(1);
      iter.forward_char();
      view.get_buffer()->place_cursor(iter);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "{\n"
                                                "{\n"
                                                "  \n"
                                                "  \n"
                                                "}\n"
                                                "}");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 2);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 2);
    }

    {
      view.get_buffer()->set_text("  int main() {");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  int main() {\n"
                                                "    \n"
                                                "  }");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 1);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 4);
    }
    {
      view.get_buffer()->set_text("  int main() {//comment");
      while(Gtk::Main::events_pending())
        Gtk::Main::iteration(false);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  int main() {//comment\n"
                                                "    \n"
                                                "  }");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 1);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 4);
    }
    {
      view.get_buffer()->set_text("  int main() {  ");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  int main() {\n"
                                                "    \n"
                                                "  }");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 1);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 4);
    }
    {
      view.get_buffer()->set_text("  int main() {\n"
                                  "  }");
      auto iter = view.get_buffer()->get_insert()->get_iter();
      iter.backward_chars(4);
      view.get_buffer()->place_cursor(iter);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  int main() {\n"
                                                "    \n"
                                                "  }");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 1);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 4);
    }
    {
      view.get_buffer()->set_text("  int main() {//comment\n"
                                  "  }");
      auto iter = view.get_buffer()->get_insert()->get_iter();
      iter.backward_chars(4);
      view.get_buffer()->place_cursor(iter);
      while(Gtk::Main::events_pending())
        Gtk::Main::iteration(false);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  int main() {//comment\n"
                                                "    \n"
                                                "  }");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 1);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 4);
    }
    {
      view.get_buffer()->set_text("  int main() {}");
      auto iter = view.get_buffer()->get_insert()->get_iter();
      iter.backward_chars(1);
      view.get_buffer()->place_cursor(iter);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  int main() {\n"
                                                "    \n"
                                                "  }");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 1);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 4);
    }
    {
      view.get_buffer()->set_text("  int main()\n"
                                  "  {");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  int main()\n"
                                                "  {\n"
                                                "    \n"
                                                "  }");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 2);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 4);
    }
    {
      view.get_buffer()->set_text("  int main()\n"
                                  "  {\n"
                                  "  }");
      auto iter = view.get_buffer()->get_insert()->get_iter();
      iter.backward_chars(4);
      view.get_buffer()->place_cursor(iter);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  int main()\n"
                                                "  {\n"
                                                "    \n"
                                                "  }");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 2);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 4);
    }
    {
      view.get_buffer()->set_text("  int main()\n"
                                  "  {}");
      auto iter = view.get_buffer()->get_insert()->get_iter();
      iter.backward_chars(1);
      view.get_buffer()->place_cursor(iter);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  int main()\n"
                                                "  {\n"
                                                "    \n"
                                                "  }");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 2);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 4);
    }
    {
      view.get_buffer()->set_text("  int main()\n"
                                  "  {/*comment*/}");
      auto iter = view.get_buffer()->get_insert()->get_iter();
      iter.backward_chars(1);
      view.get_buffer()->place_cursor(iter);
      while(Gtk::Main::events_pending())
        Gtk::Main::iteration(false);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  int main()\n"
                                                "  {/*comment*/\n"
                                                "    \n"
                                                "  }");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 2);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 4);
    }

    {
      view.get_buffer()->set_text("  else");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  else\n"
                                                "    ");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }
    {
      view.get_buffer()->set_text("  else // comment");
      while(Gtk::Main::events_pending())
        Gtk::Main::iteration(false);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  else // comment\n"
                                                "    ");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }
    {
      view.get_buffer()->set_text("  else;");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  else;\n"
                                                "  ");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }
    {
      view.get_buffer()->set_text("  else;//comment");
      while(Gtk::Main::events_pending())
        Gtk::Main::iteration(false);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  else;//comment\n"
                                                "  ");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }
    {
      view.get_buffer()->set_text("  else {}");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  else {}\n"
                                                "  ");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }
    {
      view.get_buffer()->set_text("  else {}//comment");
      while(Gtk::Main::events_pending())
        Gtk::Main::iteration(false);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  else {}//comment\n"
                                                "  ");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }

    {
      view.get_buffer()->set_text("  } else if(true)");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  } else if(true)\n"
                                                "    ");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }
    {
      view.get_buffer()->set_text("  } else if(true)//comment");
      while(Gtk::Main::events_pending())
        Gtk::Main::iteration(false);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  } else if(true)//comment\n"
                                                "    ");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }
    {
      view.get_buffer()->set_text("  } else if(true)\n"
                                  "    ;");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  } else if(true)\n"
                                                "    ;\n"
                                                "  ");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }
    {
      view.get_buffer()->set_text("  } else if(true)//comment\n"
                                  "    ;//comment");
      while(Gtk::Main::events_pending())
        Gtk::Main::iteration(false);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  } else if(true)//comment\n"
                                                "    ;//comment\n"
                                                "  ");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }

    {
      view.get_buffer()->set_text("  if(true) {\n"
                                  "    ;");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  if(true) {\n"
                                                "    ;\n"
                                                "    ");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }
    {
      view.get_buffer()->set_text("  if(true) { /*comment*/\n"
                                  "    ;");
      while(Gtk::Main::events_pending())
        Gtk::Main::iteration(false);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  if(true) { /*comment*/\n"
                                                "    ;\n"
                                                "    ");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }

    {
      view.get_buffer()->set_text("  if(true &&\n"
                                  "     false");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  if(true &&\n"
                                                "     false\n"
                                                "     ");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }
    {
      view.get_buffer()->set_text("  if(true &&\n"
                                  "     false)");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  if(true &&\n"
                                                "     false)\n"
                                                "    ");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }
    {
      view.get_buffer()->set_text("  if(true &&\n"
                                  "     false) {");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  if(true &&\n"
                                                "     false) {\n"
                                                "    \n"
                                                "  }");
      auto iter = view.get_buffer()->end();
      iter.backward_chars(4);
      g_assert(view.get_buffer()->get_insert()->get_iter() == iter);
    }
    {
      view.get_buffer()->set_text("  if(true && // comment\n"
                                  "     false) { // comment");
      while(Gtk::Main::events_pending())
        Gtk::Main::iteration(false);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  if(true && // comment\n"
                                                "     false) { // comment\n"
                                                "    \n"
                                                "  }");
      auto iter = view.get_buffer()->end();
      iter.backward_chars(4);
      g_assert(view.get_buffer()->get_insert()->get_iter() == iter);
    }
    {
      view.get_buffer()->set_text("  if(true &&\n"
                                  "     false)\n"
                                  "    ;");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  if(true &&\n"
                                                "     false)\n"
                                                "    ;\n"
                                                "  ");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }
    {
      view.get_buffer()->set_text("  if(true &&\n"
                                  "     false)//comment\n"
                                  "    ;//comment");
      while(Gtk::Main::events_pending())
        Gtk::Main::iteration(false);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  if(true &&\n"
                                                "     false)//comment\n"
                                                "    ;//comment\n"
                                                "  ");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }
    {
      view.get_buffer()->set_text("  if(true &&\n"
                                  "     false)\n"
                                  "    cout << endl <<\n"
                                  "         << endl <<");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  if(true &&\n"
                                                "     false)\n"
                                                "    cout << endl <<\n"
                                                "         << endl <<\n"
                                                "         ");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }
    {
      view.get_buffer()->set_text("  if(true &&\n"
                                  "     false)\n"
                                  "    cout << endl <<\n"
                                  "         << endl;");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  if(true &&\n"
                                                "     false)\n"
                                                "    cout << endl <<\n"
                                                "         << endl;\n"
                                                "  ");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }

    {
      view.get_buffer()->set_text("  func([");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  func([\n"
                                                "        ");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }
    {
      view.get_buffer()->set_text("  func([] {},");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  func([] {},\n"
                                                "       ");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }
    {
      view.get_buffer()->set_text("  func([]() -> std::vector<std::vector<int>> {},");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  func([]() -> std::vector<std::vector<int>> {},\n"
                                                "       ");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }
    {
      view.get_buffer()->set_text("  func([] {");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  func([] {\n"
                                                "    \n"
                                                "  }");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 1);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 4);
    }
    {
      view.get_buffer()->set_text("  func([]() -> std::vector<std::vector<int>> {");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  func([]() -> std::vector<std::vector<int>> {\n"
                                                "    \n"
                                                "  }");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 1);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 4);
    }
    {
      view.get_buffer()->set_text("  func([] {)");
      auto iter = view.get_buffer()->get_insert()->get_iter();
      iter.backward_char();
      view.get_buffer()->place_cursor(iter);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  func([] {\n"
                                                "    \n"
                                                "  })");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 1);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 4);
    }
    {
      view.get_buffer()->set_text("  func([]->std::vector<std::vector<int>>{)");
      auto iter = view.get_buffer()->get_insert()->get_iter();
      iter.backward_char();
      view.get_buffer()->place_cursor(iter);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  func([]->std::vector<std::vector<int>>{\n"
                                                "    \n"
                                                "  })");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 1);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 4);
    }
    {
      view.get_buffer()->set_text("  func([] {\n"
                                  "  })");
      auto iter = view.get_buffer()->get_insert()->get_iter();
      iter.backward_chars(5);
      view.get_buffer()->place_cursor(iter);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  func([] {\n"
                                                "    \n"
                                                "  })");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 1);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 4);
    }
    {
      view.get_buffer()->set_text("  func([]()->std::vector<std::vector<int>> {\n"
                                  "  })");
      auto iter = view.get_buffer()->get_insert()->get_iter();
      iter.backward_chars(5);
      view.get_buffer()->place_cursor(iter);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  func([]()->std::vector<std::vector<int>> {\n"
                                                "    \n"
                                                "  })");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 1);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 4);
    }
    {
      view.get_buffer()->set_text("  func([]->bool{return true;\n"
                                  "  });");
      auto iter = view.get_buffer()->get_insert()->get_iter();
      iter.backward_chars(18);
      view.get_buffer()->place_cursor(iter);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  func([]->bool{\n"
                                                "    return true;\n"
                                                "  });");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 1);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 4);
    }
    {
      view.get_buffer()->set_text("  func([] {}, [] {");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  func([] {}, [] {\n"
                                                "    \n"
                                                "  }");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 1);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 4);
    }
    {
      view.get_buffer()->set_text("  func([]()->bool {}, []()->bool {");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  func([]()->bool {}, []()->bool {\n"
                                                "    \n"
                                                "  }");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 1);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 4);
    }
    {
      view.get_buffer()->set_text("  func([] {}, [] {},");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  func([] {}, [] {},\n"
                                                "       ");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }
    {
      view.get_buffer()->set_text("  func([]()->bool {}, []()->bool {},");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  func([]()->bool {}, []()->bool {},\n"
                                                "       ");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }
    {
      view.get_buffer()->set_text("  func([] {}, [] {}");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  func([] {}, [] {}\n"
                                                "       ");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }
    {
      view.get_buffer()->set_text("  func([]->bool {}, []->bool {}");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  func([]->bool {}, []->bool {}\n"
                                                "       ");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }
    {
      view.get_buffer()->set_text("  func([] {}, [] {}) {");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  func([] {}, [] {}) {\n"
                                                "    \n"
                                                "  }");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 1);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 4);
    }
    {
      view.get_buffer()->set_text("  func([]->bool {}, []->bool {}) {");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  func([]->bool {}, []->bool {}) {\n"
                                                "    \n"
                                                "  }");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 1);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 4);
    }
    {
      view.get_buffer()->set_text("  func([] {\n"
                                  "    \n"
                                  "  }, [] {\n"
                                  "    \n"
                                  "  }, {);");
      auto iter = view.get_buffer()->get_insert()->get_iter();
      iter.backward_chars(2);
      view.get_buffer()->place_cursor(iter);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  func([] {\n"
                                                "    \n"
                                                "  }, [] {\n"
                                                "    \n"
                                                "  }, {\n"
                                                "    \n"
                                                "  });");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 5);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 4);
    }
    {
      view.get_buffer()->set_text("  func([]() -> std::vector<std::vector<int>> {\n"
                                  "    return std::vector<std::vector<int>>();\n"
                                  "  }, []() -> std::vector<std::vector<int>> {\n"
                                  "    return std::vector<std::vector<int>>();\n"
                                  "  }, {);");
      auto iter = view.get_buffer()->get_insert()->get_iter();
      iter.backward_chars(2);
      view.get_buffer()->place_cursor(iter);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  func([]() -> std::vector<std::vector<int>> {\n"
                                                "    return std::vector<std::vector<int>>();\n"
                                                "  }, []() -> std::vector<std::vector<int>> {\n"
                                                "    return std::vector<std::vector<int>>();\n"
                                                "  }, {\n"
                                                "    \n"
                                                "  });");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 5);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 4);
    }

    {
      view.get_buffer()->set_text("  auto func=[] {");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  auto func=[] {\n"
                                                "    \n"
                                                "  };");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 1);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 4);
    }
    {
      view.get_buffer()->set_text("  auto func=[] {//comment");
      while(Gtk::Main::events_pending())
        Gtk::Main::iteration(false);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  auto func=[] {//comment\n"
                                                "    \n"
                                                "  };");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 1);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 4);
    }

    {
      view.get_buffer()->set_text("  void Class::Class()\n"
                                  "      : var(1) {");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  void Class::Class()\n"
                                                "      : var(1) {\n"
                                                "    \n"
                                                "  }");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 2);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 4);
    }
    {
      view.get_buffer()->set_text("  void Class::Class() :\n"
                                  "      var(1) {");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  void Class::Class() :\n"
                                                "      var(1) {\n"
                                                "    \n"
                                                "  }");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 2);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 4);
    }
    {
      view.get_buffer()->set_text("  void Class::Class(int a,\n"
                                  "                    int b) {");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  void Class::Class(int a,\n"
                                                "                    int b) {\n"
                                                "    \n"
                                                "  }");
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line() == 2);
      g_assert(view.get_buffer()->get_insert()->get_iter().get_line_offset() == 4);
    }

    {
      view.get_buffer()->set_text("  class Class : BaseClass {\n"
                                  "  public:");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  class Class : BaseClass {\n"
                                                "  public:\n"
                                                "    ");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }
    {
      view.get_buffer()->set_text("  class Class : BaseClass {\n"
                                  "    public:");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  class Class : BaseClass {\n"
                                                "  public:\n"
                                                "    ");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }
    {
      view.get_buffer()->set_text("  class Class : BaseClass {\n"
                                  "    public://comment");
      while(Gtk::Main::events_pending())
        Gtk::Main::iteration(false);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  class Class : BaseClass {\n"
                                                "  public://comment\n"
                                                "    ");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }
    {
      view.get_buffer()->set_text("  class Class : BaseClass {\n"
                                  "    int a;\n"
                                  "  public:");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  class Class : BaseClass {\n"
                                                "    int a;\n"
                                                "  public:\n"
                                                "    ");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }
    {
      view.get_buffer()->set_text("  class Class : BaseClass {\n"
                                  "    int a;\n"
                                  "    public:");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  class Class : BaseClass {\n"
                                                "    int a;\n"
                                                "  public:\n"
                                                "    ");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }

    {
      view.get_buffer()->set_text("  /*");
      while(Gtk::Main::events_pending())
        Gtk::Main::iteration(false);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  /*\n"
                                                "   * ");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }
    {
      view.get_buffer()->set_text("  /*\n"
                                  "   */");
      while(Gtk::Main::events_pending())
        Gtk::Main::iteration(false);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  /*\n"
                                                "   */\n"
                                                "  ");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }
    {
      view.get_buffer()->set_text("   //comment");
      while(Gtk::Main::events_pending())
        Gtk::Main::iteration(false);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "   //comment\n"
                                                "   ");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }
    {
      view.get_buffer()->set_text("   //comment\n"
                                  "   //comment");
      while(Gtk::Main::events_pending())
        Gtk::Main::iteration(false);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "   //comment\n"
                                                "   //comment\n"
                                                "   ");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }

    {
      view.get_buffer()->set_text("if('a'=='a')");
      auto iter = view.get_buffer()->get_insert()->get_iter();
      iter.backward_char();
      view.get_buffer()->place_cursor(iter);
      while(Gtk::Main::events_pending())
        Gtk::Main::iteration(false);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "if('a'=='a'\n"
                                                "   )");
      iter=view.get_buffer()->end();
      iter.backward_char();
      g_assert(view.get_buffer()->get_insert()->get_iter() == iter);
    }


    event.keyval = GDK_KEY_braceleft;
    {
      view.get_buffer()->set_text("  int main()\n"
                                  "  ");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  int main()\n"
                                                "  {");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }
    {
      view.get_buffer()->set_text("  else if(true)\n"
                                  "    ");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  else if(true)\n"
                                                "  {");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }
    {
      view.get_buffer()->set_text("  else if(true)//comment\n"
                                  "    ");
      while(Gtk::Main::events_pending())
        Gtk::Main::iteration(false);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  else if(true)//comment\n"
                                                "  {");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }
    {
      view.get_buffer()->set_text("  if(true)\n"
                                  "  ");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  if(true)\n"
                                                "  {");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }


    event.keyval = GDK_KEY_braceright;
    {
      view.get_buffer()->set_text("  int main() {\n"
                                  "    ");
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  int main() {\n"
                                                "  }");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }
    {
      view.get_buffer()->set_text("  int main() {//comment\n"
                                  "    ");
      while(Gtk::Main::events_pending())
        Gtk::Main::iteration(false);
      view.on_key_press_event(&event);
      g_assert(view.get_buffer()->get_text() == "  int main() {//comment\n"
                                                "  }");
      g_assert(view.get_buffer()->get_insert()->get_iter() == view.get_buffer()->end());
    }
    
    
    event.keyval = GDK_KEY_BackSpace;
    {
      view.get_buffer()->set_text("  int main()\n");
      auto iter=view.get_buffer()->begin();
      iter.forward_chars(2);
      view.get_buffer()->place_cursor(iter);
      assert(view.on_key_press_event_basic(&event)==false);
      assert(view.on_key_press_event_bracket_language(&event)==false);
    }
    {
      view.get_buffer()->set_text("  int main()");
      auto iter=view.get_buffer()->begin();
      iter.forward_chars(2);
      view.get_buffer()->place_cursor(iter);
      assert(view.on_key_press_event_basic(&event)==false);
      assert(view.on_key_press_event_bracket_language(&event)==false);
    }
  }
}
