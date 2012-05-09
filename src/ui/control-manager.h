/*
 * Inkscape::ControlManager - Coordinates creation and styling of nodes, handles, etc.
 *
 * Author:
 *   Jon A. Cruz <jon@joncruz.org>
 *
 * Copyright 2012 Authors
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */
#ifndef SEEN_INKSCAPE_CONTROL_MANAGER_H
#define SEEN_INKSCAPE_CONTROL_MANAGER_H

#include <memory>
#include <sigc++/sigc++.h>

#include "display/sp-canvas-item.h"

struct SPCanvasGroup;
struct SPCanvasItem;
struct SPCtrlLine;

namespace Inkscape {

enum CtrlLineType {
    CTLINE_PRIMARY,
    CTLINE_SECONDARY,
    CTLINE_TERTIARY,
};


class ControlManagerImpl;

class ControlManager
{
public:

    static ControlManager &getManager();

    ~ControlManager();

    sigc::connection connectCtrlSizeChanged(const sigc::slot<void> &slot);

    SPCanvasItem *createControl(SPCanvasGroup *parent, ControlType type);

    SPCtrlLine *createControlLine(SPCanvasGroup *parent, CtrlLineType type = CTLINE_PRIMARY);

    SPCtrlLine *createControlLine(SPCanvasGroup *parent, Geom::Point const &p1, Geom::Point const &p2, CtrlLineType type = CTLINE_PRIMARY);

    void track(SPCanvasItem *item);

    void updateItem(SPCanvasItem *item);

private:
    ControlManager();

    std::auto_ptr<ControlManagerImpl> _impl;

    friend class ControlManagerImpl;
};

} // namespace Inkscape

#endif // SEEN_INKSCAPE_CONTROL_MANAGER_H
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
