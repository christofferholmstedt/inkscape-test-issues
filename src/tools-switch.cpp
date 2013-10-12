/*
 * Utility functions for switching tools (= contexts)
 *
 * Authors:
 *   bulia byak <buliabyak@users.sf.net>
 *   Josh Andler <scislac@users.sf.net>
 *
 * Copyright (C) 2003-2007 authors
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */


#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <cstring>
#include <string>

#include "inkscape-private.h"
#include "desktop.h"
#include "desktop-handles.h"
#include <glibmm/i18n.h>

#include <xml/repr.h>

#include "select-context.h"
#include "ui/tool/node-tool.h"
#include "tweak-context.h"
#include "spray-context.h"
#include "sp-path.h"
#include "rect-context.h"
#include "sp-rect.h"
#include "box3d-context.h"
#include "box3d.h"
#include "arc-context.h"
#include "sp-ellipse.h"
#include "star-context.h"
#include "sp-star.h"
#include "spiral-context.h"
#include "sp-spiral.h"
#include "dyna-draw-context.h"
#include "eraser-context.h"
#include "pen-context.h"
#include "pencil-context.h"
#include "lpe-tool-context.h"
#include "text-context.h"
#include "sp-text.h"
#include "sp-flowtext.h"
#include "gradient-context.h"
#include "mesh-context.h"
#include "zoom-context.h"
#include "measure-context.h"
#include "dropper-context.h"
#include "connector-context.h"
#include "flood-context.h"
#include "sp-offset.h"
#include "message-context.h"

#include "tools-switch.h"

static char const *const tool_names[] = {
    NULL,
    "/tools/select",
    "/tools/nodes",
    "/tools/tweak",
    "/tools/spray",
    "/tools/shapes/rect",
    "/tools/shapes/3dbox",
    "/tools/shapes/arc",
    "/tools/shapes/star",
    "/tools/shapes/spiral",
    "/tools/freehand/pencil",
    "/tools/freehand/pen",
    "/tools/calligraphic",
    "/tools/text",
    "/tools/gradient",
    "/tools/mesh",
    "/tools/zoom",
    "/tools/measure",
    "/tools/dropper",
    "/tools/connector",
    "/tools/paintbucket",
    "/tools/eraser",
    "/tools/lpetool",
    NULL
};
static char const *const tool_msg[] = {
    NULL,
    N_("<b>Click</b> to Select and Transform objects, <b>Drag</b> to select many objects."),
    N_("Modify selected path points (nodes) directly."),
    N_("To tweak a path by pushing, select it and drag over it."),
    N_("<b>Drag</b>, <b>click</b> or <b>click and scroll</b> to spray the selected objects."),
    N_("<b>Drag</b> to create a rectangle. <b>Drag controls</b> to round corners and resize. <b>Click</b> to select."),
    N_("<b>Drag</b> to create a 3D box. <b>Drag controls</b> to resize in perspective. <b>Click</b> to select (with <b>Ctrl+Alt</b> for single faces)."),
    N_("<b>Drag</b> to create an ellipse. <b>Drag controls</b> to make an arc or segment. <b>Click</b> to select."),
    N_("<b>Drag</b> to create a star. <b>Drag controls</b> to edit the star shape. <b>Click</b> to select."),
    N_("<b>Drag</b> to create a spiral. <b>Drag controls</b> to edit the spiral shape. <b>Click</b> to select."),
    N_("<b>Drag</b> to create a freehand line. <b>Shift</b> appends to selected path, <b>Alt</b> activates sketch mode."),
    N_("<b>Click</b> or <b>click and drag</b> to start a path; with <b>Shift</b> to append to selected path. <b>Ctrl+click</b> to create single dots (straight line modes only)."),
    N_("<b>Drag</b> to draw a calligraphic stroke; with <b>Ctrl</b> to track a guide path. <b>Arrow keys</b> adjust width (left/right) and angle (up/down)."),
    N_("<b>Click</b> to select or create text, <b>drag</b> to create flowed text; then type."),
    N_("<b>Drag</b> or <b>double click</b> to create a gradient on selected objects, <b>drag handles</b> to adjust gradients."),
    N_("<b>Drag</b> or <b>double click</b> to create a mesh on selected objects, <b>drag handles</b> to adjust meshes."),
    N_("<b>Click</b> or <b>drag around an area</b> to zoom in, <b>Shift+click</b> to zoom out."),
    N_("<b>Drag</b> to measure the dimensions of objects."),
    N_("<b>Click</b> to set fill, <b>Shift+click</b> to set stroke; <b>drag</b> to average color in area; with <b>Alt</b> to pick inverse color; <b>Ctrl+C</b> to copy the color under mouse to clipboard"),
    N_("<b>Click and drag</b> between shapes to create a connector."),
    N_("<b>Click</b> to paint a bounded area, <b>Shift+click</b> to union the new fill with the current selection, <b>Ctrl+click</b> to change the clicked object's fill and stroke to the current setting."),
    N_("<b>Drag</b> to erase."),
    N_("Choose a subtool from the toolbar"),
};

static int
tools_prefpath2num(char const *id)
{
    int i = 1;
    while (tool_names[i]) {
        if (strcmp(tool_names[i], id) == 0)
            return i;
        else i++;
    }
    g_assert( 0 == TOOLS_INVALID );
    return 0; //nothing found
}

int
tools_isactive(SPDesktop *dt, unsigned num)
{
    g_assert( num < G_N_ELEMENTS(tool_names) );
    if (SP_IS_EVENT_CONTEXT(dt->event_context))
        return dt->event_context->pref_observer->observed_path == tool_names[num];
    else return FALSE;
}

int
tools_active(SPDesktop *dt)
{
    return tools_prefpath2num(dt->event_context->pref_observer->observed_path.data());
}

void
tools_switch(SPDesktop *dt, int num)
{
    dt->tipsMessageContext()->set(Inkscape::NORMAL_MESSAGE, gettext( tool_msg[num] ) );
    if (dt) {
        // This event may change the above message
        dt->_tool_changed.emit(num);
    }

    dt->set_event_context2(tool_names[num]);
    /* fixme: This is really ugly hack. We should bind and unbind class methods */
    /* First 4 tools use guides, first is undefined but we don't care */
    dt->activate_guides(num < 5);
    inkscape_eventcontext_set(dt->getEventContext());
}

void tools_switch_by_item(SPDesktop *dt, SPItem *item, Geom::Point const p)
{
    if (SP_IS_RECT(item)) {
        tools_switch(dt, TOOLS_SHAPES_RECT);
    } else if (SP_IS_BOX3D(item)) {
        tools_switch(dt, TOOLS_SHAPES_3DBOX);
    } else if (SP_IS_GENERICELLIPSE(item)) {
        tools_switch(dt, TOOLS_SHAPES_ARC);
    } else if (SP_IS_STAR(item)) {
        tools_switch(dt, TOOLS_SHAPES_STAR);
    } else if (SP_IS_SPIRAL(item)) {
        tools_switch(dt, TOOLS_SHAPES_SPIRAL);
    } else if (SP_IS_PATH(item)) {
        if (cc_item_is_connector(item)) {
            tools_switch(dt, TOOLS_CONNECTOR);
        }
        else {
            tools_switch(dt, TOOLS_NODES);
        }
    } else if (SP_IS_TEXT(item) || SP_IS_FLOWTEXT(item))  {
        tools_switch(dt, TOOLS_TEXT);
        sp_text_context_place_cursor_at (SP_TEXT_CONTEXT(dt->event_context), SP_OBJECT(item), p);
    } else if (SP_IS_OFFSET(item))  {
        tools_switch(dt, TOOLS_NODES);
    }
}

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
