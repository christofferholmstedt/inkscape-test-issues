/**
 * \brief Filter Effects dialog
 *
 * Authors:
 *   Nicholas Bishop <nicholasbishop@gmail.org>
 *
 * Copyright (C) 2007 Authors
 *
 * Released under GNU GPL.  Read the file 'COPYING' for more information.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <gtk/gtktreeview.h>
#include <gtkmm/cellrenderertext.h>
#include <gtkmm/paned.h>
#include <gtkmm/scale.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/stock.h>
#include <glibmm/i18n.h>

#include "application/application.h"
#include "application/editor.h"
#include "desktop.h"
#include "desktop-handles.h"
#include "dialog-manager.h"
#include "document.h"
#include "filter-chemistry.h"
#include "filter-effects-dialog.h"
#include "inkscape.h"
#include "sp-feblend.h"
#include "sp-fecomposite.h"
#include "sp-fedisplacementmap.h"
#include "sp-filter-primitive.h"
#include "sp-gaussian-blur.h"
#include "sp-feoffset.h"
#include "verbs.h"
#include "xml/node.h"
#include "xml/repr.h"
#include <sstream>

#include <iostream>

namespace Inkscape {
namespace UI {
namespace Dialog {

Glib::RefPtr<Gtk::Menu> create_popup_menu(Gtk::Widget& parent, sigc::slot<void> dup,
                                          sigc::slot<void> rem)
{
    Glib::RefPtr<Gtk::Menu> menu(new Gtk::Menu);

    menu->items().push_back(Gtk::Menu_Helpers::MenuElem(_("_Duplicate"), dup));
    Gtk::MenuItem* mi = Gtk::manage(new Gtk::ImageMenuItem(Gtk::Stock::REMOVE));
    menu->append(*mi);
    mi->signal_activate().connect(rem);
    mi->show();
    menu->accelerate(parent);

    return menu;
}

static void try_id_change(SPObject* ob, const Glib::ustring& text)
{
    // FIXME: this needs more serious error checking...
    if(ob && !SP_ACTIVE_DOCUMENT->getObjectById(text.c_str())) {
        SPException ex;
        SP_EXCEPTION_INIT(&ex);
        sp_object_setAttribute(ob, "id", text.c_str(), &ex);
        sp_document_done(SP_ACTIVE_DOCUMENT, SP_VERB_DIALOG_FILTER_EFFECTS, _("Set object ID"));
    }
}

/*** FilterModifier ***/
FilterEffectsDialog::FilterModifier::FilterModifier()
    : _add(Gtk::Stock::ADD)
{
    Gtk::ScrolledWindow* sw = Gtk::manage(new Gtk::ScrolledWindow);
    pack_start(*sw);
    pack_start(_add, false, false);
    sw->add(_list);

    _list.set_model(_model);
    _list.append_column_editable(_("_Filter"), _columns.id);
    ((Gtk::CellRendererText*)_list.get_column(0)->get_first_cell_renderer())->
        signal_edited().connect(sigc::mem_fun(*this, &FilterEffectsDialog::FilterModifier::filter_name_edited));

    sw->set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
    sw->set_shadow_type(Gtk::SHADOW_IN);
    show_all_children();
    _add.signal_clicked().connect(sigc::mem_fun(*this, &FilterModifier::add_filter));
    _list.signal_button_release_event().connect_notify(
        sigc::mem_fun(*this, &FilterModifier::filter_list_button_release));
    _menu = create_popup_menu(*this, sigc::mem_fun(*this, &FilterModifier::duplicate_filter),
                              sigc::mem_fun(*this, &FilterModifier::remove_filter));

    update_filters();
}

Glib::SignalProxy0<void> FilterEffectsDialog::FilterModifier::signal_selection_changed()
{
    return _list.get_selection()->signal_changed();
}

SPFilter* FilterEffectsDialog::FilterModifier::get_selected_filter()
{
    if(_list.get_selection()) {
        Gtk::TreeModel::iterator i = _list.get_selection()->get_selected();

        if(i)
            return (*i)[_columns.filter];
    }

    return 0;
}

void FilterEffectsDialog::FilterModifier::select_filter(const SPFilter* filter)
{
    if(filter) {
        for(Gtk::TreeModel::iterator i = _model->children().begin();
            i != _model->children().end(); ++i) {
            if((*i)[_columns.filter] == filter) {
                _list.get_selection()->select(i);
                break;
            }
        }
    }
}

void FilterEffectsDialog::FilterModifier::filter_list_button_release(GdkEventButton* event)
{
    if((event->type == GDK_BUTTON_RELEASE) && (event->button == 3)) {
        const bool sensitive = get_selected_filter() != NULL;
        _menu->items()[0].set_sensitive(sensitive);
        _menu->items()[1].set_sensitive(sensitive);
        _menu->popup(event->button, event->time);
    }
}

void FilterEffectsDialog::FilterModifier::add_filter()
{
    SPDocument* doc = sp_desktop_document(SP_ACTIVE_DESKTOP);
    SPFilter* filter = new_filter(doc);

    update_filters();

    select_filter(filter);

    sp_document_done(doc, SP_VERB_DIALOG_FILTER_EFFECTS, _("Add filter"));
}

void FilterEffectsDialog::FilterModifier::remove_filter()
{
    SPFilter *filter = get_selected_filter();

    if(filter) {
        SPDocument* doc = filter->document;
        sp_repr_unparent(filter->repr);

        sp_document_done(doc, SP_VERB_DIALOG_FILTER_EFFECTS, _("Remove filter"));

        update_filters();
    }
}

void FilterEffectsDialog::FilterModifier::duplicate_filter()
{
    SPFilter *filter = get_selected_filter();

    if(filter) {
        //SPFilter *dupfilter = filter_duplicate(sp_desktop_document(SP_ACTIVE_DESKTOP), filter);

        sp_document_done(filter->document, SP_VERB_DIALOG_FILTER_EFFECTS, _("Duplicate filter"));

        update_filters();
    }
}

void FilterEffectsDialog::FilterModifier::filter_name_edited(const Glib::ustring& path, const Glib::ustring& text)
{
    Gtk::TreeModel::iterator i = _model->get_iter(path);

    if(i)
        try_id_change((SPObject*)(*i)[_columns.filter], text);
}

FilterEffectsDialog::CellRendererConnection::CellRendererConnection()
    : Glib::ObjectBase(typeid(CellRendererConnection)),
      _primitive(*this, "primitive", 0)
{
}

Glib::PropertyProxy<void*> FilterEffectsDialog::CellRendererConnection::property_primitive()
{
    return _primitive.get_proxy();
}

int FilterEffectsDialog::CellRendererConnection::input_count(const SPFilterPrimitive* prim)
{
    if(!prim)
        return 0;
    else if(SP_IS_FEBLEND(prim) || SP_IS_FECOMPOSITE(prim) || SP_IS_FEDISPLACEMENTMAP(prim))
        return 2;
    else
        return 1;
}

void FilterEffectsDialog::CellRendererConnection::get_size_vfunc(
    Gtk::Widget& widget, const Gdk::Rectangle* cell_area,
    int* x_offset, int* y_offset, int* width, int* height) const
{
    PrimitiveList& primlist = dynamic_cast<PrimitiveList&>(widget);

    if(x_offset)
        (*x_offset) = 0;
    if(y_offset)
        (*y_offset) = 0;
    if(width)
        (*width) = size * primlist.primitive_count();
    if(height) {
        // Scale the height depending on the number of inputs, unless it's
        // the first primitive, in which case their are no connections
        SPFilterPrimitive* prim = (SPFilterPrimitive*)_primitive.get_value();
        (*height) = primlist.is_first(prim) ? size : size * input_count(prim);
    }
}

/*** PrimitiveList ***/
FilterEffectsDialog::PrimitiveList::PrimitiveList(FilterEffectsDialog& d)
    : _dialog(d), _in_drag(0)
{
    add_events(Gdk::POINTER_MOTION_MASK | Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK);

    _model = Gtk::ListStore::create(_columns);

    // TODO: reenable this once it is possible to modify the order in the backend
    //set_reorderable(true);

    set_model(_model);
    append_column(_("_Type"), _columns.type);

    signal_selection_changed().connect(sigc::mem_fun(*this, &PrimitiveList::queue_draw));

    CellRendererConnection* cell = new CellRendererConnection;
    int cols_count = append_column("Connections", *cell);
    Gtk::TreeViewColumn* col = get_column(cols_count - 1);
    if(col)
       col->add_attribute(cell->property_primitive(), _columns.primitive);
}

Glib::SignalProxy0<void> FilterEffectsDialog::PrimitiveList::signal_selection_changed()
{
    return get_selection()->signal_changed();
}

/* Add all filter primitives in the current to the list.
   Keeps the same selection if possible, otherwise selects the first element */
void FilterEffectsDialog::PrimitiveList::update()
{
    SPFilter* f = _dialog._filter_modifier.get_selected_filter();
    const SPFilterPrimitive* active_prim = get_selected();
    bool active_found = false;

    _model->clear();

    if(f) {
        _dialog._primitive_box.set_sensitive(true);

        for(SPObject *prim_obj = f->children;
                prim_obj && SP_IS_FILTER_PRIMITIVE(prim_obj);
                prim_obj = prim_obj->next) {
            SPFilterPrimitive *prim = SP_FILTER_PRIMITIVE(prim_obj);
            if(prim) {
                Gtk::TreeModel::Row row = *_model->append();
                row[_columns.primitive] = prim;
                row[_columns.type_id] = FPConverter.get_id_from_name(prim->repr->name());
                row[_columns.type] = FPConverter.get_label(row[_columns.type_id]);
                row[_columns.id] = SP_OBJECT_ID(prim);

                if(prim == active_prim) {
                    get_selection()->select(row);
                    active_found = true;
                }
            }
        }

        if(!active_found && _model->children().begin())
            get_selection()->select(_model->children().begin());
    }
    else {
        _dialog._primitive_box.set_sensitive(false);
    }
}

void FilterEffectsDialog::PrimitiveList::set_menu(Glib::RefPtr<Gtk::Menu> menu)
{
    _primitive_menu = menu;
}

SPFilterPrimitive* FilterEffectsDialog::PrimitiveList::get_selected()
{
    if(_dialog._filter_modifier.get_selected_filter()) {
        Gtk::TreeModel::iterator i = get_selection()->get_selected();
        if(i)
            return (*i)[_columns.primitive];
    }

    return 0;
}

void FilterEffectsDialog::PrimitiveList::select(SPFilterPrimitive* prim)
{
    for(Gtk::TreeIter i = _model->children().begin();
        i != _model->children().end(); ++i) {
        if((*i)[_columns.primitive] == prim)
            get_selection()->select(i);
    }
}

bool FilterEffectsDialog::PrimitiveList::on_expose_event(GdkEventExpose* e)
{
    Gtk::TreeView::on_expose_event(e);

    SPFilterPrimitive* prim = get_selected();

    int row_index = 0;
    int row_count = get_model()->children().size();
    int fheight = 0;
    for(Gtk::TreeIter row = get_model()->children().begin();
        row != get_model()->children().end(); ++row, ++row_index) {
        Gdk::Rectangle rct, clip(&e->area);
        get_cell_area(get_model()->get_path(row), *get_column(1), rct);
        const int x = rct.get_x(), y = rct.get_y(), h = rct.get_height();

        // For calculating the width of cells, the height of the first row is used
        if(row_index == 0)
            fheight = h;

        // Check mouse state
        int mx, my;
        Gdk::ModifierType mask;
        get_bin_window()->get_pointer(mx, my, mask);

        // Outline the bottom of the connection area
        const int outline_x = x + fheight * (row_count - row_index);
        get_bin_window()->draw_line(get_style()->get_dark_gc(Gtk::STATE_NORMAL),
                                    x, y + h, outline_x, y + h);

        // The first row can't have any inputs
        if(row_index == 0)
            continue;

        // Side outline
        get_bin_window()->draw_line(get_style()->get_dark_gc(Gtk::STATE_NORMAL),
                                    outline_x, y, outline_x, y + h);

        std::vector<Gdk::Point> con_poly;
        int con_drag_y;
        bool inside;
        // Draw "in" shape
        inside = do_connection_node(row, 0, con_poly, mx, my);
        con_drag_y = con_poly[2].get_y();
        get_bin_window()->draw_polygon(inside && mask & GDK_BUTTON1_MASK ?
                                       get_style()->get_dark_gc(Gtk::STATE_NORMAL) :
                                       get_style()->get_dark_gc(Gtk::STATE_ACTIVE),
                                       inside, con_poly);
        // Draw "in" connection
        draw_connection(find_result(row, SP_ATTR_IN), outline_x, con_poly[2].get_y(), row_count);

        const SPFilterPrimitive* row_prim = (*row)[_columns.primitive];
        if(CellRendererConnection::input_count(row_prim) == 2) {
            // Draw "in2" shape
            inside = do_connection_node(row, 1, con_poly, mx, my);
            if(_in_drag == 2)
                con_drag_y = con_poly[2].get_y();
            get_bin_window()->draw_polygon(inside && mask & GDK_BUTTON1_MASK ?
                                           get_style()->get_dark_gc(Gtk::STATE_NORMAL) :
                                           get_style()->get_dark_gc(Gtk::STATE_ACTIVE),
                                           inside, con_poly);
            // Draw "in2" connection
            draw_connection(find_result(row, SP_ATTR_IN2), outline_x, con_poly[2].get_y(), row_count);
        }

        // Draw drag connection
        if(row_prim == prim && _in_drag) {
            get_bin_window()->draw_line(get_style()->get_black_gc(), outline_x, con_drag_y,
                                        mx, con_drag_y);
            get_bin_window()->draw_line(get_style()->get_black_gc(), mx, con_drag_y, mx, my);
        }
    }

    return true;
}

void FilterEffectsDialog::PrimitiveList::draw_connection(const Gtk::TreeIter& input, const int x1, const int y1,
                                                         const int row_count)
{
    if(input != _model->children().end()) {
        Gdk::Rectangle rct;

        get_cell_area(get_model()->get_path(_model->children().begin()), *get_column(1), rct);
        const int fheight = rct.get_height();

        get_cell_area(get_model()->get_path(input), *get_column(1), rct);
        const int row_index = find_index(input);
        const int x2 = rct.get_x() + fheight * (row_count - row_index) - fheight / 2;
        const int y2 = rct.get_y() + rct.get_height();

        // Draw an 'L'-shaped connection
        get_bin_window()->draw_line(get_style()->get_black_gc(), x1, y1, x2, y1);
        get_bin_window()->draw_line(get_style()->get_black_gc(), x2, y1, x2, y2);
    }
}

// Creates a triangle outline of the connection node and returns true if (x,y) is inside the node
bool FilterEffectsDialog::PrimitiveList::do_connection_node(const Gtk::TreeIter& row, const int input,
                                                            std::vector<Gdk::Point>& points,
                                                            const int ix, const int iy)
{
    Gdk::Rectangle rct;

    get_cell_area(get_model()->get_path(_model->children().begin()), *get_column(1), rct);
    const int h = rct.get_height();

    get_cell_area(_model->get_path(row), *get_column(1), rct);

    const int x = rct.get_x() + h * (_model->children().size() - find_index(row));
    const int con_w = (int)(h * 0.35f);
    const int con_y = rct.get_y() + h / 2 - con_w + input * h;
    points.clear();
    points.push_back(Gdk::Point(x, con_y));
    points.push_back(Gdk::Point(x, con_y + con_w * 2));
    points.push_back(Gdk::Point(x - con_w, con_y + con_w));

    return ix >= x - h && iy >= con_y && ix <= x && iy <= points[1].get_y();
}

const Gtk::TreeIter FilterEffectsDialog::PrimitiveList::find_result(const Gtk::TreeIter& start,
                                                                    const SPAttributeEnum attr)
{
    SPFilterPrimitive* prim = (*start)[_columns.primitive];
    Gtk::TreeIter target = _model->children().end();
    int image;

    if(attr == SP_ATTR_IN)
        image = prim->image_in;
    else if(attr == SP_ATTR_IN2) {
        if(SP_IS_FEBLEND(prim))
            image = SP_FEBLEND(prim)->in2;
        else if(SP_IS_FECOMPOSITE(prim))
            image = SP_FECOMPOSITE(prim)->in2;
        /*else if(SP_IS_FEDISPLACEMENTMAP(prim))
        image = SP_FEDISPLACEMENTMAP(prim)->in2;*/
        else
            return target;
    }
    else
        return target;

    if(image >= 0) {
        for(Gtk::TreeIter i = _model->children().begin();
            i != start; ++i) {
            if(((SPFilterPrimitive*)(*i)[_columns.primitive])->image_out == image)
                target = i;
        }
    }

    return target;
}

int FilterEffectsDialog::PrimitiveList::find_index(const Gtk::TreeIter& target)
{
    int i = 0;
    for(Gtk::TreeIter iter = _model->children().begin();
        iter != target; ++iter, ++i);
    return i;
}

bool FilterEffectsDialog::PrimitiveList::on_button_press_event(GdkEventButton* e)
{
    Gtk::TreePath path;
    Gtk::TreeViewColumn* col;
    const int x = (int)e->x, y = (int)e->y;
    int cx, cy;
    
    if(get_path_at_pos(x, y, path, col, cx, cy)) {
        std::vector<Gdk::Point> points;
        if(do_connection_node(_model->get_iter(path), 0, points, x, y))
            _in_drag = 1;
        else if(do_connection_node(_model->get_iter(path), 1, points, x, y))
            _in_drag = 2;

        queue_draw();
    }

    return Gtk::TreeView::on_button_press_event(e);
}

bool FilterEffectsDialog::PrimitiveList::on_motion_notify_event(GdkEventMotion* e)
{
    queue_draw();

    return Gtk::TreeView::on_motion_notify_event(e);
}

bool FilterEffectsDialog::PrimitiveList::on_button_release_event(GdkEventButton* e)
{
    SPFilterPrimitive *prim = get_selected(), *target;

    if(_in_drag && prim) {
        Gtk::TreePath path;
        Gtk::TreeViewColumn* col;
        int cx, cy;
        
        if(get_path_at_pos((int)e->x, (int)e->y, path, col, cx, cy)) {
            target = (*_model->get_iter(path))[_columns.primitive];
            Inkscape::XML::Node *repr = SP_OBJECT_REPR(target);
            // Make sure the target has a result
            const gchar *gres = repr->attribute("result");
            Glib::ustring result = gres ? gres : "";
            if(!gres) {
                result = "result" + Glib::Ascii::dtostr(SP_FILTER(prim->parent)->_image_number_next);
                repr->setAttribute("result", result.c_str());
            }

            if(_in_drag == 1)
                SP_OBJECT_REPR(prim)->setAttribute("in", result.c_str());
            else if(_in_drag == 2)
                SP_OBJECT_REPR(prim)->setAttribute("in2", result.c_str());
        }

        _in_drag = 0;
        queue_draw();

        _dialog.update_settings_view();
    }

    if((e->type == GDK_BUTTON_RELEASE) && (e->button == 3)) {
        const bool sensitive = get_selected() != NULL;
        _primitive_menu->items()[0].set_sensitive(sensitive);
        _primitive_menu->items()[1].set_sensitive(sensitive);
        _primitive_menu->popup(e->button, e->time);

        return true;
    }
    else
        return Gtk::TreeView::on_button_release_event(e);
}

// Reorder the filter primitives to match the list order
void FilterEffectsDialog::PrimitiveList::on_drag_end(const Glib::RefPtr<Gdk::DragContext>&)
{
    SPFilter* filter = _dialog._filter_modifier.get_selected_filter();

    for(Gtk::TreeModel::iterator iter = _model->children().begin();
        iter != _model->children().end(); ++iter) {
        SPFilterPrimitive* prim = (*iter)[_columns.primitive];
        if(prim)
            ;//reorder_primitive(filter, prim->repr->position(), ndx); /* FIXME */
    }

    sp_document_done(filter->document, SP_VERB_DIALOG_FILTER_EFFECTS, _("Reorder filter primitive"));
}

int FilterEffectsDialog::PrimitiveList::primitive_count() const
{
    return _model->children().size();
}

bool FilterEffectsDialog::PrimitiveList::is_first(const SPFilterPrimitive* prim) const
{
    return (*_model->children().begin())[_columns.primitive] == prim;
}

/*** SettingsGroup ***/
FilterEffectsDialog::SettingsGroup::SettingsGroup()
{
    show();
}

void FilterEffectsDialog::SettingsGroup::init(FilterEffectsDialog* dlg, Glib::RefPtr<Gtk::SizeGroup> sg)
{
    _dialog = dlg;
    _dialog->_settings.pack_start(*this, false, false);
    _sizegroup = sg;
}

/* Adds a new settings widget using the specified label. The label will be formatted with a colon
   and all widgets within the setting group are aligned automatically. */
void FilterEffectsDialog::SettingsGroup::add_setting_generic(Gtk::Widget& w, const Glib::ustring& label)
{
    Gtk::Label *lbl = Gtk::manage(new Gtk::Label(label + (label == "" ? "" : ":"), Gtk::ALIGN_LEFT));
    Gtk::HBox *hb = Gtk::manage(new Gtk::HBox);
    hb->set_spacing(12);
    hb->pack_start(*lbl, false, false);
    hb->pack_start(w);
    pack_start(*hb);

    _sizegroup->add_widget(*lbl);

    hb->show();
    lbl->show();

    w.show();
}

/* For SpinSlider settings */
void FilterEffectsDialog::SettingsGroup::add_setting(SpinSlider& ss, const SPAttributeEnum attr,
                                                     const Glib::ustring& label)
{
    add_setting_generic(ss, label);
    ss.signal_value_changed().connect(
        sigc::bind(sigc::mem_fun(_dialog, &FilterEffectsDialog::set_attr_spinslider), attr, &ss));
}

/* For subgroups of settings */
void FilterEffectsDialog::SettingsGroup::add_setting(std::vector<Gtk::Widget*>& w, const Glib::ustring& label)
{
    Gtk::HBox *hb = Gtk::manage(new Gtk::HBox);
    for(unsigned int i = 0; i < w.size(); ++i)
        hb->pack_start(*w[i]);
    hb->set_spacing(12);
    add_setting_generic(*hb, label);
}

/*** FilterEffectsDialog ***/

FilterEffectsDialog::FilterEffectsDialog() 
    : Dialog ("dialogs.filtereffects", SP_VERB_DIALOG_FILTER_EFFECTS),
      _primitive_list(*this),
      _add_primitive_type(FPConverter),
      _add_primitive(Gtk::Stock::ADD),
      _settings_labels(Gtk::SizeGroup::create(Gtk::SIZE_GROUP_HORIZONTAL)),
      _empty_settings("No primitive selected", Gtk::ALIGN_LEFT),
      // TODO: Find better range/climb-rate/digits values for the SpinSliders,
      //       many of the current values are just guesses
      _blend_mode(BlendModeConverter),
      _composite_operator(CompositeOperatorConverter),
      _composite_k1(0, -10, 10, 1, 0.01, 1),
      _composite_k2(0, -10, 10, 1, 0.01, 1),
      _composite_k3(0, -10, 10, 1, 0.01, 1),
      _composite_k4(0, -10, 10, 1, 0.01, 1),
      _gaussianblur_stddeviation(1, 0, 100, 1, 0.01, 1),
      _morphology_radius(1, 0, 100, 1, 0.01, 1),
      _offset_dx(0, -100, 100, 1, 0.01, 1),
      _offset_dy(0, -100, 100, 1, 0.01, 1),
      _turbulence_basefrequency(1, 0, 100, 1, 0.01, 1),
      _turbulence_numoctaves(1, 1, 10, 1, 1, 0),
      _turbulence_seed(1, 0, 100, 1, 0.01, 1),
      _turbulence_stitchtiles(_("Stitch Tiles")),
      _turbulence_fractalnoise(_turbulence_type, _("Fractal Noise")),
      _turbulence_turbulence(_turbulence_type, _("Turbulence"))
{
    // Initialize widget hierarchy
    Gtk::HPaned* hpaned = Gtk::manage(new Gtk::HPaned);
    Gtk::ScrolledWindow* sw_prims = Gtk::manage(new Gtk::ScrolledWindow);
    Gtk::HBox* hb_prims = Gtk::manage(new Gtk::HBox);
    Gtk::Frame* fr_settings = Gtk::manage(new Gtk::Frame("<b>Settings</b>"));
    Gtk::Alignment* al_settings = Gtk::manage(new Gtk::Alignment);
    get_vbox()->add(*hpaned);
    hpaned->pack1(_filter_modifier);
    hpaned->pack2(_primitive_box);
    _primitive_box.pack_start(*sw_prims);
    _primitive_box.pack_start(*hb_prims, false, false);
    sw_prims->add(_primitive_list);
    hb_prims->pack_end(_add_primitive, false, false);
    hb_prims->pack_end(_add_primitive_type, false, false);
    get_vbox()->pack_start(*fr_settings, false, false);
    fr_settings->add(*al_settings);
    al_settings->add(_settings);

    _primitive_list.signal_selection_changed().connect(
        sigc::mem_fun(*this, &FilterEffectsDialog::update_settings_view));
    _filter_modifier.signal_selection_changed().connect(
        sigc::mem_fun(_primitive_list, &PrimitiveList::update));

    sw_prims->set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
    sw_prims->set_shadow_type(Gtk::SHADOW_IN);
    al_settings->set_padding(0, 0, 12, 0);
    fr_settings->set_shadow_type(Gtk::SHADOW_NONE);
    ((Gtk::Label*)fr_settings->get_label_widget())->set_use_markup();
    _add_primitive.signal_clicked().connect(sigc::mem_fun(*this, &FilterEffectsDialog::add_primitive));
    _primitive_list.set_menu(create_popup_menu(*this, sigc::mem_fun(*this, &FilterEffectsDialog::duplicate_primitive),
                                               sigc::mem_fun(*this, &FilterEffectsDialog::remove_primitive)));
    _settings_labels->set_ignore_hidden(true);
    
    show_all_children();
    init_settings_widgets();
    _primitive_list.update();
    update_settings_view();
}

FilterEffectsDialog::~FilterEffectsDialog()
{
}

void FilterEffectsDialog::init_settings_widgets()
{
    _empty_settings.set_sensitive(false);
    _settings.pack_start(_empty_settings);

    _generic_settings.init(this, _settings_labels);
    _generic_settings.add_setting_generic(_primitive_input1, "Input");
    _primitive_input1.signal_changed().connect(
        sigc::bind(sigc::mem_fun(*this, &FilterEffectsDialog::set_attr_special), SP_ATTR_IN));
    _primitive_input1.append_text("Default");
    _primitive_input1.append_text("Source Graphic");
    _primitive_input1.append_text("Source Alpha");
    _primitive_input1.append_text("Background Image");
    _primitive_input1.append_text("Background Alpha");
    _primitive_input1.append_text("Fill Paint");
    _primitive_input1.append_text("Stroke Paint");
    _primitive_input1.append_text("Connection");

    _blend.init(this, _settings_labels);
    _blend.add_setting(_blend_mode, SP_ATTR_MODE, "Mode");

    _colormatrix.init(this, _settings_labels);
    //_colormatrix.add_setting(_colormatrix_type, "Type");

    _componenttransfer.init(this, _settings_labels);

    _composite.init(this, _settings_labels);
    _composite.add_setting(_composite_operator, SP_ATTR_OPERATOR, "Operator");
    _composite.add_setting(_composite_k1, SP_ATTR_K1, "K1");
    _composite.add_setting(_composite_k2, SP_ATTR_K2, "K2");
    _composite.add_setting(_composite_k3, SP_ATTR_K3, "K3");
    _composite.add_setting(_composite_k4, SP_ATTR_K4, "K4");

    _convolvematrix.init(this, _settings_labels);
    
    _diffuselighting.init(this, _settings_labels);

    _displacementmap.init(this, _settings_labels);

    _flood.init(this, _settings_labels);

    _gaussianblur.init(this, _settings_labels);
    _gaussianblur.add_setting(_gaussianblur_stddeviation, SP_ATTR_STDDEVIATION, "Standard Deviation");

    _image.init(this, _settings_labels);
    
    _merge.init(this, _settings_labels);

    _morphology.init(this, _settings_labels);
    //_morphology.add_setting(_morphology_operator, "Operator");
    //_morphology.add_setting(_morphology_radius, "Radius");

    _offset.init(this, _settings_labels);
    _offset.add_setting(_offset_dx, SP_ATTR_DX, "Delta X");
    _offset.add_setting(_offset_dy, SP_ATTR_DY, "Delta Y");

    _specularlighting.init(this, _settings_labels);

    _tile.init(this, _settings_labels);

    _turbulence.init(this, _settings_labels);
    std::vector<Gtk::Widget*> trb_grp;
    trb_grp.push_back(&_turbulence_fractalnoise);
    trb_grp.push_back(&_turbulence_turbulence);
    _turbulence.add_setting(trb_grp);
    /*_turbulence.add_setting(_turbulence_numoctaves, "Octaves");
    _turbulence.add_setting(_turbulence_basefrequency, "Base Frequency");
    _turbulence.add_setting(_turbulence_seed, "Seed");
    _turbulence.add_setting(_turbulence_stitchtiles);*/
}

void FilterEffectsDialog::add_primitive()
{
    SPFilter* filter = _filter_modifier.get_selected_filter();
    const EnumData<NR::FilterPrimitiveType>* data = _add_primitive_type.get_active_data();
    
    if(filter && data) {
        SPFilterPrimitive* prim = filter_add_primitive(filter, data->name.c_str());

        // Set default values
        switch(data->id) {
            case NR::NR_FILTER_BLEND:
                sp_object_set(prim, SP_ATTR_MODE, BlendModeConverter.get_name(NR::BLEND_NORMAL).c_str());
                break;
            case NR::NR_FILTER_COLORMATRIX:
                break;
            case NR::NR_FILTER_COMPONENTTRANSFER:
                break;
            case NR::NR_FILTER_COMPOSITE:
                break;
            case NR::NR_FILTER_CONVOLVEMATRIX:
                break;
            case NR::NR_FILTER_DIFFUSELIGHTING:
                break;
            case NR::NR_FILTER_DISPLACEMENTMAP:
                break;
            case NR::NR_FILTER_FLOOD:
                break;
            case NR::NR_FILTER_GAUSSIANBLUR:
                sp_object_set(prim, SP_ATTR_STDDEVIATION, "1");
                break;
            case NR::NR_FILTER_IMAGE:
                break;
            case NR::NR_FILTER_MERGE:
                break;
            case NR::NR_FILTER_MORPHOLOGY:
                break;
            case NR::NR_FILTER_OFFSET:
                sp_object_set(prim, SP_ATTR_DX, "0");
                sp_object_set(prim, SP_ATTR_DY, "0");
                break;
            case NR::NR_FILTER_SPECULARLIGHTING:
                break;
            case NR::NR_FILTER_TILE:
                break;
            case NR::NR_FILTER_TURBULENCE:
                break;
            default:
                break;
        }

        _primitive_list.update();
        _primitive_list.select(prim);

        sp_document_done(filter->document, SP_VERB_DIALOG_FILTER_EFFECTS, _("Add filter primitive"));
    }
}

void FilterEffectsDialog::remove_primitive()
{
    SPFilterPrimitive* prim = _primitive_list.get_selected();

    if(prim) {
        sp_repr_unparent(prim->repr);

        sp_document_done(sp_desktop_document(SP_ACTIVE_DESKTOP), SP_VERB_DIALOG_FILTER_EFFECTS,
                         _("Remove filter primitive"));

        _primitive_list.update();
    }
}

void FilterEffectsDialog::duplicate_primitive()
{
    SPFilter* filter = _filter_modifier.get_selected_filter();
    SPFilterPrimitive* origprim = _primitive_list.get_selected();

    if(filter && origprim) {
        Inkscape::XML::Node *repr;
        repr = SP_OBJECT_REPR(origprim)->duplicate(SP_OBJECT_REPR(origprim)->document());
        SP_OBJECT_REPR(filter)->appendChild(repr);

        sp_document_done(filter->document, SP_VERB_DIALOG_FILTER_EFFECTS, _("Duplicate filter primitive"));

        _primitive_list.update();
    }
}

void FilterEffectsDialog::set_attr_spinslider(const SPAttributeEnum attr, const SpinSlider* input)
{
    if(input->is_sensitive()) {
        std::ostringstream os;
        os << input->get_value();
        set_attr(attr, os.str());
    }
}

void FilterEffectsDialog::set_attr_special(const SPAttributeEnum attr)
{
    Glib::ustring val;

    switch(attr) {
        case SP_ATTR_IN:
            val = _primitive_input1.get_active_text();
            if(val == "Default") {
                val = "";
            }
            else if(val == "Connection") {
                return;
            }
            else {
                val.erase(val.find(" "), 1);
                for(Glib::ustring::size_type i = 0; i < val.size(); ++i) {
                    if(val[i] == ' ') {
                        val.erase(i, i + 1);
                        break;
                    }
                }
            }
            break;
        default:
            return;
    }

    set_attr(attr, val);
}

void FilterEffectsDialog::set_attr(const SPAttributeEnum attr, const Glib::ustring& val)
{
    SPFilter *filter = _filter_modifier.get_selected_filter();
    SPFilterPrimitive* prim = _primitive_list.get_selected();

    if(filter && prim) {
        update_settings_sensitivity();

        SP_OBJECT_REPR(prim)->setAttribute((gchar*)sp_attribute_name(attr), val.c_str());
        sp_object_set(prim, attr, val.c_str());
        filter->requestModified(SP_OBJECT_MODIFIED_FLAG);

        sp_document_done(filter->document, SP_VERB_DIALOG_FILTER_EFFECTS, _("Set filter primitive attribute"));
    }
}

void FilterEffectsDialog::update_settings_view()
{
    SPFilterPrimitive* prim = _primitive_list.get_selected();

    // Hide all the settings
    _settings.hide_all();
    _settings.show();

    _settings.set_sensitive(false);
    _empty_settings.show();

    if(prim) {
        const NR::FilterPrimitiveType tid = FPConverter.get_id_from_name(prim->repr->name());

        _generic_settings.show_all();
        const gchar *attr_in_g = SP_OBJECT_REPR(prim)->attribute("in");
        const Glib::ustring attr_in = attr_in_g ? attr_in_g : "";
        if(attr_in == "")
            _primitive_input1.set_active(0);
        else if(attr_in == "SourceGraphic")
            _primitive_input1.set_active(1);
        else if(attr_in == "SourceAlpha")
            _primitive_input1.set_active(2);
        else if(attr_in == "BackgroundImage")
            _primitive_input1.set_active(3);
        else if(attr_in == "BackgroundAlpha")
            _primitive_input1.set_active(4);
        else if(attr_in == "Fill Paint")
            _primitive_input1.set_active(5);
        else if(attr_in == "Stroke Paint")
            _primitive_input1.set_active(6);
        else
            _primitive_input1.set_active(7);

        if(tid == NR::NR_FILTER_BLEND) {
            _blend.show_all();
            const gchar* val = prim->repr->attribute("mode");
            if(val)
                _blend_mode.set_active(BlendModeConverter.get_id_from_name(val));
        }
        else if(tid == NR::NR_FILTER_COLORMATRIX)
            _colormatrix.show_all();
        else if(tid == NR::NR_FILTER_COMPONENTTRANSFER)
            _componenttransfer.show_all();
        else if(tid == NR::NR_FILTER_COMPOSITE) {
            _composite.show_all();
            SPFeComposite* comp = SP_FECOMPOSITE(prim);
            _composite_operator.set_active(comp->composite_operator);
            _composite_k1.set_value(comp->k1);
            _composite_k2.set_value(comp->k2);
            _composite_k3.set_value(comp->k3);
            _composite_k4.set_value(comp->k4);
        }
        else if(tid == NR::NR_FILTER_CONVOLVEMATRIX)
            _convolvematrix.show_all();
        else if(tid == NR::NR_FILTER_DIFFUSELIGHTING)
            _diffuselighting.show_all();
        else if(tid == NR::NR_FILTER_DISPLACEMENTMAP)
            _displacementmap.show_all();
        else if(tid == NR::NR_FILTER_FLOOD)
            _flood.show_all();
        else if(tid == NR::NR_FILTER_GAUSSIANBLUR) {
            _gaussianblur.show_all();
            _gaussianblur_stddeviation.set_value(((SPGaussianBlur*)prim)->stdDeviation.getNumber());
        }
        else if(tid == NR::NR_FILTER_IMAGE)
            _image.show_all();
        else if(tid == NR::NR_FILTER_MERGE)
            _merge.show_all();
        else if(tid == NR::NR_FILTER_MORPHOLOGY)
            _morphology.show_all();
        else if(tid == NR::NR_FILTER_OFFSET) {
            _offset.show_all();
            _offset_dx.set_value(((SPFeOffset*)prim)->dx);
            _offset_dy.set_value(((SPFeOffset*)prim)->dy);
        }
        else if(tid == NR::NR_FILTER_SPECULARLIGHTING)
            _specularlighting.show_all();
        else if(tid == NR::NR_FILTER_TILE)
            _tile.show_all();
        else if(tid == NR::NR_FILTER_TURBULENCE)
            _turbulence.show_all();

        _settings.set_sensitive(true);
        _empty_settings.hide();
    }

    update_settings_sensitivity();
}

void FilterEffectsDialog::update_settings_sensitivity()
{
    const bool use_k = _composite_operator.get_active_data()->id == COMPOSITE_ARITHMETIC;
    _composite_k1.set_sensitive(use_k);
    _composite_k2.set_sensitive(use_k);
    _composite_k3.set_sensitive(use_k);
    _composite_k4.set_sensitive(use_k);
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
