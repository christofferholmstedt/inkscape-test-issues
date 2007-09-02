/**
 * \brief Base class for dialogs in Inkscape.  This class provides certain
 *        common behaviors and styles wanted of all dialogs in the application.
 *
 * Authors:
 *   Bryce W. Harrington <bryce@bryceharrington.org>
 *   buliabyak@gmail.com 
 *   Johan Engelen <j.b.c.engelen@ewi.utwente.nl>
 *   Gustav Broberg <broberg@kth.se>
 *
 * Copyright (C) 2004--2007 Authors
 *
 * Released under GNU GPL.  Read the file 'COPYING' for more information.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <gtkmm/stock.h>
#include <gtk/gtk.h>

#include "application/application.h"
#include "application/editor.h"
#include "inkscape.h"
#include "event-context.h"
#include "desktop.h"
#include "desktop-handles.h"
#include "dialog-manager.h"
#include "dialogs/dialog-events.h"
#include "shortcuts.h"
#include "prefs-utils.h"
#include "interface.h"
#include "verbs.h"

#define MIN_ONSCREEN_DISTANCE 50


namespace Inkscape {
namespace UI {
namespace Dialog {

void
sp_retransientize (Inkscape::Application *inkscape, SPDesktop *desktop, gpointer dlgPtr)
{
    Dialog *dlg = (Dialog *)dlgPtr;
    dlg->onDesktopActivated (desktop);
}

gboolean
sp_retransientize_again (gpointer dlgPtr)
{
    Dialog *dlg = (Dialog *)dlgPtr;
    dlg->retransientize_suppress = false;
    return FALSE; // so that it is only called once
}

void
sp_dialog_shutdown (GtkObject *object, gpointer dlgPtr)
{
    Dialog *dlg = (Dialog *)dlgPtr;
    dlg->onShutdown();
}

void
Dialog::save_geometry()
{
    int y, x, w, h;

    get_position(x, y);
    get_size(w, h);

    // g_print ("write %d %d %d %d\n", x, y, w, h);

    if (x<0) x=0;
    if (y<0) y=0;

    prefs_set_int_attribute (_prefs_path, "x", x);
    prefs_set_int_attribute (_prefs_path, "y", y);
    prefs_set_int_attribute (_prefs_path, "w", w);
    prefs_set_int_attribute (_prefs_path, "h", h);

}

void hideCallback(GtkObject *object, gpointer dlgPtr)
{
    g_return_if_fail( dlgPtr != NULL );

    Dialog *dlg = (Dialog *)dlgPtr;
    dlg->onHideF12();
}

void unhideCallback(GtkObject *object, gpointer dlgPtr)
{
    g_return_if_fail( dlgPtr != NULL );

    Dialog *dlg = (Dialog *)dlgPtr;
    dlg->onShowF12();
}



void
Dialog::read_geometry()
{
    _user_hidden = false;

    int x = prefs_get_int_attribute (_prefs_path, "x", -1000);
    int y = prefs_get_int_attribute (_prefs_path, "y", -1000);
    int w = prefs_get_int_attribute (_prefs_path, "w", 0);
    int h = prefs_get_int_attribute (_prefs_path, "h", 0);

    // g_print ("read %d %d %d %d\n", x, y, w, h);

    // If there are stored height and width values for the dialog,
    // resize the window to match; otherwise we leave it at its default
    if (w != 0 && h != 0) {
        resize(w, h);
    }
    
    // If there are stored values for where the dialog should be
    // located, then restore the dialog to that position.
    // also check if (x,y) is actually onscreen with the current screen dimensions
    if ( (x >= 0) && (y >= 0) && (x < (gdk_screen_width()-MIN_ONSCREEN_DISTANCE)) && (y < (gdk_screen_height()-MIN_ONSCREEN_DISTANCE)) ) {
        move(x, y);
    } else {
        // ...otherwise just put it in the middle of the screen
        set_position(Gtk::WIN_POS_CENTER);
    }

}

inline Dialog::operator Gtk::Widget&()                           { return *_behavior; }
inline GtkWidget *Dialog::gobj()                                 { return _behavior->gobj(); }
inline void Dialog::present()                                    { _behavior->present(); }
inline Gtk::VBox *Dialog::get_vbox()                             {  return _behavior->get_vbox(); }
inline void Dialog::show_all_children()                          { _behavior->show_all_children(); }
inline void Dialog::hide()                                       { _behavior->hide(); }
inline void Dialog::show()                                       { _behavior->show(); }
inline void Dialog::set_size_request(int width, int height)      { _behavior->set_size_request(width, height); }
inline void Dialog::size_request(Gtk::Requisition& requisition)  { _behavior->size_request(requisition); }
inline void Dialog::get_position(int& x, int& y)                 { _behavior->get_position(x, y); }
inline void Dialog::get_size(int& width, int& height)            { _behavior->get_size(width, height); }
inline void Dialog::resize(int width, int height)                { _behavior->resize(width, height); }
inline void Dialog::move(int x, int y)                           { _behavior->move(x, y); }
inline void Dialog::set_position(Gtk::WindowPosition position)   { _behavior->set_position(position); }
inline void Dialog::set_title(Glib::ustring title)               { _behavior->set_title(title); }
inline void Dialog::set_sensitive(bool sensitive)                { _behavior->set_sensitive(sensitive); }

inline void Dialog::set_response_sensitive(int response_id, bool setting)
{ _behavior->set_response_sensitive(response_id, setting); }

void Dialog::set_resizable(bool) { }
void Dialog::set_default(Gtk::Widget&) { }

inline void Dialog::set_default_response(int response_id) { _behavior->set_default_response(response_id); }

Glib::SignalProxy0<void> Dialog::signal_show () { return _behavior->signal_show(); }
Glib::SignalProxy0<void> Dialog::signal_hide () { return _behavior->signal_hide(); }
Glib::SignalProxy1<void, int> Dialog::signal_response () { return _behavior->signal_response(); }

Gtk::Button* Dialog::add_button (const Glib::ustring& button_text, int response_id) 
{ return _behavior->add_button(button_text, response_id); }

Gtk::Button* Dialog::add_button (const Gtk::StockID& stock_id, int response_id)
{ return _behavior->add_button(stock_id, response_id); }

Dialog::Dialog(const char *prefs_path, int verb_num, const char *apply_label)
{

}

//=====================================================================

/**
 * UI::Dialog::Dialog is a base class for all dialogs in Inkscape.  The
 * purpose of this class is to provide a unified place for ensuring
 * style and behavior.  Specifically, this class provides functionality
 * for saving and restoring the size and position of dialogs (through
 * the user's preferences file).
 *
 * It also provides some general purpose signal handlers for things like
 * showing and hiding all dialogs.
 */
Dialog::Dialog(Behavior::BehaviorFactory behavior_factory, const char *prefs_path, int verb_num,
               const char *apply_label) 
    : _hiddenF12 (false),
      _prefs_path (prefs_path),
      _verb_num(verb_num),
      _apply_label (apply_label)
{
    gchar title[500];

    if (verb_num)
        sp_ui_dialog_title_string (Inkscape::Verb::get(verb_num), title);

    _title = title;

    _behavior = behavior_factory(*this);

    gtk_signal_connect( GTK_OBJECT (gobj()), "event", GTK_SIGNAL_FUNC(sp_dialog_event_handler), gobj() );

    if (Inkscape::NSApplication::Application::getNewGui())
    {
        _desktop_activated_connection = Inkscape::NSApplication::Editor::connectDesktopActivated (sigc::mem_fun (*this, &Dialog::onDesktopActivated));
        _dialogs_hidden_connection = Inkscape::NSApplication::Editor::connectDialogsHidden (sigc::mem_fun (*this, &Dialog::onHideF12));
        _dialogs_unhidden_connection = Inkscape::NSApplication::Editor::connectDialogsUnhidden (sigc::mem_fun (*this, &Dialog::onShowF12));
        _shutdown_connection = Inkscape::NSApplication::Editor::connectShutdown (sigc::mem_fun (*this, &Dialog::onShutdown));
    }
    else
    {
        g_signal_connect (G_OBJECT (INKSCAPE), "activate_desktop", G_CALLBACK (sp_retransientize), (void *)this);
        g_signal_connect ( G_OBJECT(INKSCAPE), "dialogs_hide", G_CALLBACK(hideCallback), (void *)this );
        g_signal_connect ( G_OBJECT(INKSCAPE), "dialogs_unhide", G_CALLBACK(unhideCallback), (void *)this );
        g_signal_connect (G_OBJECT (INKSCAPE), "shut_down", G_CALLBACK (sp_dialog_shutdown), (void *)this);
    }

    Glib::wrap(gobj())->signal_key_press_event().connect(sigc::ptr_fun(&windowKeyPress));

    if (prefs_get_int_attribute ("dialogs", "showclose", 0) || apply_label) {
        // TODO: make the order of buttons obey the global preference
        if (apply_label) {
            add_button(Glib::ustring(apply_label), Gtk::RESPONSE_APPLY);
            set_default_response(Gtk::RESPONSE_APPLY);
        }
        add_button(Gtk::Stock::CLOSE, Gtk::RESPONSE_CLOSE);
    }

    read_geometry();
    present();
}

Dialog::~Dialog()
{
    if (Inkscape::NSApplication::Application::getNewGui())
    {
        _desktop_activated_connection.disconnect();
        _dialogs_hidden_connection.disconnect();
        _dialogs_unhidden_connection.disconnect();
        _shutdown_connection.disconnect();
    }
    
    save_geometry();
    delete _behavior;
}


bool Dialog::windowKeyPress(GdkEventKey *event)
{
    unsigned int shortcut = 0;
    shortcut = get_group0_keyval (event) |
        ( event->state & GDK_SHIFT_MASK ?
          SP_SHORTCUT_SHIFT_MASK : 0 ) |
        ( event->state & GDK_CONTROL_MASK ?
          SP_SHORTCUT_CONTROL_MASK : 0 ) |
        ( event->state & GDK_MOD1_MASK ?
          SP_SHORTCUT_ALT_MASK : 0 );

    return sp_shortcut_invoke( shortcut, SP_ACTIVE_DESKTOP );
}

//---------------------------------------------------------------------

void
Dialog::on_response(int response_id)
{
    switch (response_id) {
        case Gtk::RESPONSE_APPLY: {
            _apply();
            break;
        }
        case Gtk::RESPONSE_CLOSE: {
            _close();
            break;
        }
    }
}

bool
Dialog::on_delete_event (GdkEventAny *event)
{
    save_geometry();
    _user_hidden = true;

    return false;
}

void
Dialog::onHideF12()
{
    _hiddenF12 = true;
    _behavior->onHideF12();
}

void
Dialog::onShowF12()
{
    if (_user_hidden)
        return;

    if (_hiddenF12) {
        _behavior->onShowF12();
    }

    _hiddenF12 = false;
}

void 
Dialog::onShutdown()
{
    save_geometry();
    _user_hidden = true;
    _behavior->onShutdown();
}

void
Dialog::onDesktopActivated(SPDesktop *desktop)
{
    _behavior->onDesktopActivated(desktop);
}

Inkscape::Selection*
Dialog::_getSelection()
{
    return sp_desktop_selection(SP_ACTIVE_DESKTOP);
}

void
Dialog::_apply()
{
    g_warning("Apply button clicked for dialog [Dialog::_apply()]");
}

void
Dialog::_close()
{
    GtkWidget *dlg = GTK_WIDGET(_behavior->gobj());

    /* this code sends a delete_event to the dialog,
     * instead of just destroying it, so that the
     * dialog can do some housekeeping, such as remember
     * its position.
     */

    GdkEventAny event;
    event.type = GDK_DELETE;
    event.window = dlg->window;
    event.send_event = TRUE;

    if (event.window) 
        g_object_ref (G_OBJECT (event.window));

    gtk_main_do_event ((GdkEvent*)&event);

    if (event.window) 
        g_object_unref (G_OBJECT (event.window));
}

} // namespace Dialog
} // namespace UI
} // namespace Inkscape

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=99 :
