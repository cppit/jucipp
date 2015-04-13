#include "window.h"

Window::Window() :
  window_box_(Gtk::ORIENTATION_VERTICAL),
  main_config_(),
  keybindings_(main_config_.keybindings_cfg()),
  notebook_(keybindings(), main_config_.source_cfg(), main_config_.dir_cfg()),
  menu_(keybindings()) {
  set_title("juCi++");
  set_default_size(600, 400);
  add(window_box_);
  keybindings_.action_group_menu()->add(Gtk::Action::create("FileQuit",
							    Gtk::Stock::QUIT),
					[this]() {
					  OnWindowHide();
					});
  keybindings_.action_group_menu()->add(Gtk::Action::create("FileOpenFile",
							    Gtk::Stock::OPEN),
					[this]() {
					  OnOpenFile();
					});
  keybindings_.action_group_menu()->add(Gtk::Action::create("FileOpenFolder",
                                                           "Open folder"),
                                       Gtk::AccelKey(keybindings_.config_
						     .key_map()["open_folder"]),
                                       [this]() {
                                         OnFileOpenFolder();
                                       });
  PluginApi::menu_ = &menu_;
  PluginApi::notebook_ = &notebook_;
  PluginApi::InitPlugins();

  add_accel_group(keybindings_.ui_manager_menu()->get_accel_group());
  add_accel_group(keybindings_.ui_manager_hidden()->get_accel_group());
  keybindings_.BuildMenu();

  window_box_.pack_start(menu_.view(), Gtk::PACK_SHRINK);
  window_box_.pack_start(notebook_.entry_view(), Gtk::PACK_SHRINK);
  window_box_.pack_start(notebook_.view());
  show_all_children();
  } // Window constructor

void Window::OnWindowHide() {
  hide();
}
void Window::OnFileOpenFolder() {
  Gtk::FileChooserDialog dialog("Please choose a folder",
          Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER);
  
  dialog.set_transient_for(*this);
  //Add response buttons the the dialog:
  dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
  dialog.add_button("Select", Gtk::RESPONSE_OK);

  int result = dialog.run();

  //Handle the response:
  switch(result)
  {
    case(Gtk::RESPONSE_OK):
    {
      std::cout << "Folder selected: " << dialog.get_filename()
          << std::endl;
      notebook_.directories().open_folder(dialog.get_filename());
      break;
    }
    case(Gtk::RESPONSE_CANCEL):
    {
      std::cout << "Cancel clicked." << std::endl;
      break;
    }
    default:
    {
      std::cout << "Unexpected button clicked." << std::endl;
      break;
    }
  }
}


void Window::OnOpenFile() {
  Gtk::FileChooserDialog dialog("Please choose a file",
            Gtk::FILE_CHOOSER_ACTION_OPEN);
    dialog.set_transient_for(*this);
    dialog.set_position(Gtk::WindowPosition::WIN_POS_CENTER_ALWAYS);

    //Add response buttons the the dialog:
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("_Open", Gtk::RESPONSE_OK);

    //Add filters, so that only certain file types can be selected:
    Glib::RefPtr<Gtk::FileFilter> filter_text = Gtk::FileFilter::create();
    filter_text->set_name("Text files");
    filter_text->add_mime_type("text/plain");
    dialog.add_filter(filter_text);

    Glib::RefPtr<Gtk::FileFilter> filter_cpp = Gtk::FileFilter::create();
    filter_cpp->set_name("C/C++ files");
    filter_cpp->add_mime_type("text/x-c");
    filter_cpp->add_mime_type("text/x-c++");
    filter_cpp->add_mime_type("text/x-c-header");
    dialog.add_filter(filter_cpp);

    Glib::RefPtr<Gtk::FileFilter> filter_any = Gtk::FileFilter::create();
    filter_any->set_name("Any files");
    filter_any->add_pattern("*");
    dialog.add_filter(filter_any);

    int result = dialog.run();

    switch (result) {
        case(Gtk::RESPONSE_OK): {
            std::cout << "Open clicked." << std::endl;
            std::string path = dialog.get_filename();
            std::cout << "File selected: " << path << std::endl;
	    notebook_.OnOpenFile(path);
            break;
        }
        case(Gtk::RESPONSE_CANCEL): {
            std::cout << "Cancel clicked." << std::endl;
            break;
        }
        default: {
            std::cout << "Unexpected button clicked." << std::endl;
            break;
        }
    }
}
