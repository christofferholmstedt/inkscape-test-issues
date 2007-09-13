/*
 * Copyright (C) 2005-2007 Authors:
 *   Ted Gould <ted@gould.cx>
 *   Johan Engelen <johan@shouraizou.nl>
 *   Christopher Brown <audiere@gmail.com>
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <iostream>
#include <sstream>

#include <gtkmm/adjustment.h>
#include <gtkmm/box.h>
#include <gtkmm/spinbutton.h>

#include <xml/node.h>

#include "extension.h"
#include "paramcolor.h"

#include "color.h"
#include "widgets/sp-color-selector.h"
#include "widgets/sp-color-notebook.h"


namespace Inkscape {
namespace Extension {
	
void sp_color_param_changed(SPColorSelector *csel, GObject *cp);

     
/** \brief  Free the allocated data. */
ParamColor::~ParamColor(void)
{
    
}
     
guint32 
ParamColor::set (guint32 in, SPDocument * doc, Inkscape::XML::Node * node)
{
    _value = in;

    gchar * prefname = this->pref_name();
    prefs_set_string_attribute(PREF_DIR, prefname, this->string()->c_str());
    g_free(prefname);

    return _value;
}

/** \brief  Initialize the object, to do that, copy the data. */
ParamColor::ParamColor (const gchar * name, const gchar * guitext, const gchar * desc, const Parameter::_scope_t scope, Inkscape::Extension::Extension * ext, Inkscape::XML::Node * xml) :
    Parameter(name, guitext, desc, scope, ext)
{
    const char * defaulthex = NULL;
    if (sp_repr_children(xml) != NULL)
        defaulthex = sp_repr_children(xml)->content();

    gchar * pref_name = this->pref_name();
    const gchar * paramval = prefs_get_string_attribute(PREF_DIR, pref_name);
    g_free(pref_name);

    if (paramval != NULL)
        defaulthex = paramval;
		
	_value = atoi(defaulthex);

    return;
}

/** \brief  Return the value as a string */
Glib::ustring *
ParamColor::string (void)
{
    char str[16];
	sprintf(str, "%i", _value);
	
	return new Glib::ustring(str);
}

Gtk::Widget *
ParamColor::get_widget (SPDocument * doc, Inkscape::XML::Node * node, sigc::signal<void> * changeSignal)
{
	_changeSignal = new sigc::signal<void>(*changeSignal);
	Gtk::HBox * hbox = Gtk::manage(new Gtk::HBox(false, 4));
	SPColorSelector* spColorSelector = (SPColorSelector*)sp_color_selector_new(SP_TYPE_COLOR_NOTEBOOK, SP_COLORSPACE_TYPE_RGB);
	
	ColorSelector* colorSelector = spColorSelector->base;
	if (_value < 1) {
		_value = 0xFF000000;
	}
	SPColor *color = new SPColor();
	sp_color_set_rgb_rgba32(color, _value);
	float alpha = (_value & 0xff) / 255.0F;
    colorSelector->setColorAlpha(*color, alpha);

	hbox->pack_start (*Glib::wrap(&spColorSelector->vbox), true, true, 0);
	g_signal_connect(G_OBJECT(spColorSelector), "changed",  G_CALLBACK(sp_color_param_changed), (void*)this);

	gtk_widget_show(GTK_WIDGET(spColorSelector));
	hbox->show();
    
    return dynamic_cast<Gtk::Widget *>(hbox);
}

void
sp_color_param_changed(SPColorSelector *csel, GObject *obj)
{
	const SPColor color = csel->base->getColor();
	float alpha = csel->base->getAlpha();

    ParamColor* ptr = (ParamColor*)obj;
	ptr->set(sp_color_get_rgba32_falpha(&color, alpha), NULL, NULL);
	
	ptr->_changeSignal->emit();
}

};  /* namespace Extension */
};  /* namespace Inkscape */
