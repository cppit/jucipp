#include "tooltips.h"

Gdk::Rectangle Tooltips::drawn_tooltips_rectangle=Gdk::Rectangle();

Tooltip::Tooltip(std::function<Glib::RefPtr<Gtk::TextBuffer>()> create_tooltip_buffer,
                 Gtk::TextView *text_view,
                 Glib::RefPtr<Gtk::TextBuffer::Mark> start_mark,
                 Glib::RefPtr<Gtk::TextBuffer::Mark> end_mark): text_view(text_view) {}

Tooltip::~Tooltip() {}

void Tooltips::show(Gdk::Rectangle const&, bool) {}

void Tooltips::show(bool) {}

void Tooltips::hide(const std::pair<int, int> &, const std::pair<int, int> &) {}
