#include "info.h"

Info::Info() {
  set_hexpand(false);
  set_halign(Gtk::Align::ALIGN_END);
  
  auto content_area=dynamic_cast<Gtk::Container*>(get_content_area());
  label.set_max_width_chars(40);
  label.set_line_wrap(true);
  content_area->add(label);
    
  auto provider = Gtk::CssProvider::create();
  provider->load_from_data("* {border-radius: 5px;}");
  get_style_context()->add_provider(provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

void Info::print(const std::string &text) {
  if(!enabled)
    return;
  
  timeout_connection.disconnect();
  timeout_connection=Glib::signal_timeout().connect([this]() {
    hide();
    return false;
  }, 3000);
  
  label.set_text(text);
  show();
}
