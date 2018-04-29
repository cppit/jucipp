#include "info.h"

Info::Info() {
  set_hexpand(false);
  set_halign(Gtk::Align::ALIGN_END);
  
  auto content_area=dynamic_cast<Gtk::Container*>(get_content_area());
  label.set_max_width_chars(40);
  label.set_line_wrap(true);
  content_area->add(label);
  
  get_style_context()->add_class("juci_info");
  
  //Workaround from https://bugzilla.gnome.org/show_bug.cgi?id=710888
  //Issue described at the same issue report
  //TODO: remove later
  auto revealer = gtk_widget_get_template_child (GTK_WIDGET (gobj()), GTK_TYPE_INFO_BAR, "revealer");
  if (revealer) {
    gtk_revealer_set_transition_type (GTK_REVEALER (revealer), GTK_REVEALER_TRANSITION_TYPE_NONE);
    gtk_revealer_set_transition_duration (GTK_REVEALER (revealer), 0);
  }
}

void Info::print(const std::string &text) {
  timeout_connection.disconnect();
  //Timeout based on https://en.wikipedia.org/wiki/Words_per_minute
  //(average_words_per_minute*average_letters_per_word)/60 => (228*4.5)/60 = 17.1
  double timeout=1000.0*std::max(3.0, 1.0+text.size()/17.1);
  timeout_connection=Glib::signal_timeout().connect([this]() {
    hide();
    return false;
  }, timeout);
  
  label.set_text(text);
  show();
}
