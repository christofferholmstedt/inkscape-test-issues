/**
 * Inkscape::ConsoleOutputUndoObserver - observer for tracing calls to
 * sp_document_undo, sp_document_redo, sp_document_maybe_done
 *
 * Authors:
 * David Yip <yipdw@alumni.rose-hulman.edu>
 *
 * Copyright (c) 2006 Authors
 *
 * Released under GNU GPL, see the file 'COPYING' for more information
 */

#include <glibmm.h>

#include "console-output-undo-observer.h"

namespace Inkscape {

void
ConsoleOutputUndoObserver::notifyUndoEvent(XML::Event* log)
{
    // g_message("notifyUndoEvent (sp_document_undo) called; log=%p\n", log->event);
}

void
ConsoleOutputUndoObserver::notifyRedoEvent(XML::Event* log)
{
    // g_message("notifyRedoEvent (sp_document_redo) called; log=%p\n", log->event);
}

void
ConsoleOutputUndoObserver::notifyUndoCommitEvent(XML::Event* log)
{
    //g_message("notifyUndoCommitEvent (sp_document_maybe_done) called; log=%p\n", log->event);
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=99 :
