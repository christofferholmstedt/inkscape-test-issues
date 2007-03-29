/**
 * Inkscape::EventLog
 *
 * A simple log for maintaining a history of commited, undone and redone events along with their
 * type. It implements the UndoStackObserver and should be registered with a
 * CompositeUndoStackObserver for each document. The event log is then notified on all commit, undo
 * and redo events and will store a representation of them in an internal Gtk::TreeStore.
 *
 * Consecutive events of the same type are grouped with the first event as a parent and following
 * as its children.
 *
 * If a Gtk::TreeView is connected to the event log, the TreeView's selection and its nodes
 * expanded/collapsed state will be updated as events are commited, undone and redone. Whenever
 * this happens, the event log will block the TreeView's callbacks to prevent circular updates.
 *
 * Author:
 *   Gustav Broberg <broberg@kth.se>
 *
 * Copyright (c) 2006, 2007 Authors
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifndef INKSCAPE_EVENT_LOG_H
#define INKSCAPE_EVENT_LOG_H

#include <gdkmm/pixbuf.h>
#include <glibmm/refptr.h>
#include <gtkmm/treestore.h>
#include <gtkmm/treeselection.h>
#include <gtkmm/treeview.h>

#include "undo-stack-observer.h"
#include "event.h"

namespace Inkscape {

/**
 *     
 */
class EventLog : public UndoStackObserver {
        
public:
    typedef Gtk::TreeModel::iterator iterator;
    typedef Gtk::TreeModel::const_iterator const_iterator;

    EventLog(SPDocument* document);
    ~EventLog();

    /**
     * Event datatype
     */

    struct EventModelColumns : public Gtk::TreeModelColumnRecord
    {
        Gtk::TreeModelColumn<Event *> event;
        Gtk::TreeModelColumn<unsigned int> type;
        Gtk::TreeModelColumn<Glib::ustring> description;
        Gtk::TreeModelColumn<int> child_count;

        EventModelColumns()
        { 
            add(event); add(type); add(description); add(child_count);
        }
    };

    /**
     * Implementation of Inkscape::UndoStackObserver methods
     * \brief Modifies the log's entries and the view's selection when triggered
     */

    void notifyUndoEvent(Event *log);
    void notifyRedoEvent(Event *log);
    void notifyUndoCommitEvent(Event *log);
    void notifyClearUndoEvent();
    void notifyClearRedoEvent();

    /**
     * Accessor functions
     */

    Glib::RefPtr<Gtk::TreeModel> getEventListStore() const { return _event_list_store; }
    const EventModelColumns& getColumns() const            { return _columns; }
    iterator getCurrEvent() const                          { return _curr_event; }
    iterator getCurrEventParent() const                    { return _curr_event_parent; }

    void setCurrEvent(iterator event)          { _curr_event = event; }
    void setCurrEventParent(iterator event)    { _curr_event_parent = event; }
    void blockNotifications(bool status=true)  { _notifications_blocked = status; }

    /* 
     * Callback types for TreeView changes.
     */

    enum CallbackTypes { 
        CALLB_SELECTION_CHANGE, 
        CALLB_EXPAND, 
        CALLB_COLLAPSE, 
        CALLB_LAST 
    };

    typedef std::map<const CallbackTypes, sigc::connection> CallbackMap;

    /**
     * Connect with a TreeView.
     */
    void connectWithDialog(Gtk::TreeView *event_list_view, CallbackMap *callback_connections);

    /*
     * Updates the sensitivity and names of SP_VERB_EDIT_UNDO and SP_VERB_EDIT_REDO to reflect the
     * current state.
     */
    void updateUndoVerbs();

private:
    bool _connected;             //< connected with dialog
    SPDocument *_document;       //< document that is logged

    const EventModelColumns _columns;

    Glib::RefPtr<Gtk::TreeStore> _event_list_store; 
    Glib::RefPtr<Gtk::TreeSelection> _event_list_selection;
    Gtk::TreeView *_event_list_view;

    iterator _curr_event;        //< current event in _event_list_store
    iterator _last_event;        //< end position in _event_list_store
    iterator _curr_event_parent; //< parent to current event, if any

    bool _notifications_blocked; //< if notifications should be handled

    // Map of connections used to temporary block/unblock callbacks in a TreeView
    CallbackMap *_callback_connections;

    /**
     * Helper functions
     */

    const_iterator _getUndoEvent() const; //< returns the current undoable event or NULL if none
    const_iterator _getRedoEvent() const; //< returns the current redoable event or NULL if none

    void _clearUndo();  //< erase all previously commited events
    void _clearRedo();  //< erase all previously undone events

    // noncopyable, nonassignable
    EventLog(EventLog const &other);
    EventLog& operator=(EventLog const &other);

};

} // namespace Inkscape

#endif // INKSCAPE_EVENT_LOG_H

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
