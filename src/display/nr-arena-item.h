/*
 * RGBA display list system for inkscape
 *
 * Author:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *
 * Copyright (C) 2001-2002 Lauris Kaplinski
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifndef SEEN_DISPLAY_NR_ARENA_ITEM_H
#define SEEN_DISPLAY_NR_ARENA_ITEM_H

#include <cairo.h>
#include <2geom/matrix.h>
#include "libnr/nr-rect-l.h"
#include "libnr/nr-object.h"
#include "gc-soft-ptr.h"
#include "nr-arena-forward.h"

namespace Inkscape {
namespace Filters {
class Filter;
} }

#define NR_TYPE_ARENA_ITEM (nr_arena_item_get_type ())
#define NR_ARENA_ITEM(o) (NR_CHECK_INSTANCE_CAST ((o), NR_TYPE_ARENA_ITEM, NRArenaItem))
#define NR_IS_ARENA_ITEM(o) (NR_CHECK_INSTANCE_TYPE ((o), NR_TYPE_ARENA_ITEM))

#define NR_ARENA_ITEM_VIRTUAL(i,m) (((NRArenaItemClass *) NR_OBJECT_GET_CLASS (i))->m)

/*
 * NRArenaItem state flags
 */

/*
 * NR_ARENA_ITEM_STATE_INVALID
 *
 * If set or retuned indicates, that given object is in error.
 * Calling method has to return immediately, with appropriate
 * error flag.
 */

#define NR_ARENA_ITEM_STATE_INVALID  (1 << 0)


#define NR_ARENA_ITEM_STATE_BBOX     (1 << 1)
#define NR_ARENA_ITEM_STATE_COVERAGE (1 << 2)
#define NR_ARENA_ITEM_STATE_DRAFT    (1 << 3)
#define NR_ARENA_ITEM_STATE_RENDER   (1 << 4)
#define NR_ARENA_ITEM_STATE_CLIP     (1 << 5)
#define NR_ARENA_ITEM_STATE_MASK     (1 << 6)
#define NR_ARENA_ITEM_STATE_PICK     (1 << 7)
#define NR_ARENA_ITEM_STATE_IMAGE    (1 << 8)

#define NR_ARENA_ITEM_STATE_NONE  0x0000
#define NR_ARENA_ITEM_STATE_ALL   0x01fe

#define NR_ARENA_ITEM_STATE(i,s) (NR_ARENA_ITEM (i)->state & (s))
#define NR_ARENA_ITEM_SET_STATE(i,s) (NR_ARENA_ITEM (i)->state |= (s))
#define NR_ARENA_ITEM_UNSET_STATE(i,s) (NR_ARENA_ITEM (i)->state &= ~(s))

#define NR_ARENA_ITEM_RENDER_NO_CACHE (1 << 0)

struct NRGC {
    NRGC(NRGC const *p) : parent(p) {}
    NRGC const *parent;
    Geom::Matrix transform;
};

struct NRArenaItem : public NRObject {

    NRArena *arena;
    Inkscape::GC::soft_ptr<NRArenaItem> parent;
    NRArenaItem *next;
    Inkscape::GC::soft_ptr<NRArenaItem> prev;

    /* Item state */
    unsigned int state : 16;
    unsigned int propagate : 1;
    unsigned int sensitive : 1;
    unsigned int visible : 1;
    /* Whether items renders opacity itself */
    unsigned int render_opacity : 1;
    /* Opacity itself */
    unsigned int opacity : 8;

    unsigned int key; ///< Some SPItems can have more than one NRArenaItem,
                      ///this value is a hack used to distinguish between them

    NRRectL bbox; ///< Bounding box in pixel grid coordinates; (0,0) is at page origin
    NRRectL drawbox; ///< Bounding box enlarged by filters, shrinked by clips and masks
    Geom::OptRect item_bbox; ///< Bounding box in item coordinates, required by filters
    Geom::Matrix *transform; ///< Incremental transform of this item, as given by the transform= attribute
    Geom::Matrix ctm; ///< Total transform from pixel grid to item coords
    NRArenaItem *clip; ///< Clipping path
    NRArenaItem *mask; ///< Mask
    Inkscape::Filters::Filter *filter; ///< Filter
    unsigned char *px; ///< Render cache; unused

    void *data; ///< Anonymous data member - this is used to associate SPItems with arena items

    NRPixBlock *background_pb; ///< Background for filters; unused
    bool background_new;

    void init(NRArena *arena) {
        this->arena = arena;
    }
};

struct NRArenaItemClass : public NRObjectClass {
    NRArenaItem * (* children) (NRArenaItem *item);
    NRArenaItem * (* last_child) (NRArenaItem *item);
    void (* add_child) (NRArenaItem *item, NRArenaItem *child, NRArenaItem *ref);
    void (* remove_child) (NRArenaItem *item, NRArenaItem *child);
    void (* set_child_position) (NRArenaItem *item, NRArenaItem *child, NRArenaItem *ref);

    unsigned int (* update) (NRArenaItem *item, NRRectL *area, NRGC *gc, unsigned int state, unsigned int reset);
    unsigned int (* render) (cairo_t *ct, NRArenaItem *item, NRRectL *area, NRPixBlock *pb, unsigned int flags);
    unsigned int (* clip) (cairo_t *ct, NRArenaItem *item, NRRectL *area);
    NRArenaItem * (* pick) (NRArenaItem *item, Geom::Point p, double delta, unsigned int sticky);
};

#define NR_ARENA_ITEM_ARENA(ai) (((NRArenaItem *) (ai))->arena)

NRType nr_arena_item_get_type (void);

NRArenaItem *nr_arena_item_ref (NRArenaItem *item);
NRArenaItem *nr_arena_item_unref (NRArenaItem *item);

NRArenaItem *nr_arena_item_children (NRArenaItem *item);
NRArenaItem *nr_arena_item_last_child (NRArenaItem *item);
void nr_arena_item_add_child (NRArenaItem *item, NRArenaItem *child, NRArenaItem *ref);
void nr_arena_item_remove_child (NRArenaItem *item, NRArenaItem *child);
void nr_arena_item_set_child_position (NRArenaItem *item, NRArenaItem *child, NRArenaItem *ref);

/*
 * Invoke update to given state, if item is inside area
 *
 * area == NULL is infinite
 * gc is PARENT gc for invoke, CHILD gc in corresponding virtual method
 * state - requested to state (bitwise or of requested flags)
 * reset - reset to state (bitwise or of flags to reset)
 */

unsigned int nr_arena_item_invoke_update (NRArenaItem *item, NRRectL *area, NRGC *gc, unsigned int state, unsigned int reset);

unsigned int nr_arena_item_invoke_render(cairo_t *ct, NRArenaItem *item, NRRectL const *area, NRPixBlock *pb, unsigned int flags);

unsigned int nr_arena_item_invoke_clip (cairo_t *ct, NRArenaItem *item, NRRectL *area);
NRArenaItem *nr_arena_item_invoke_pick (NRArenaItem *item, Geom::Point p, double delta, unsigned int sticky);

void nr_arena_item_request_update (NRArenaItem *item, unsigned int reset, unsigned int propagate);
void nr_arena_item_request_render (NRArenaItem *item);

/* Public */

NRArenaItem *nr_arena_item_unparent (NRArenaItem *item);

void nr_arena_item_append_child (NRArenaItem *parent, NRArenaItem *child);

void nr_arena_item_set_transform(NRArenaItem *item, Geom::Matrix const &transform);
void nr_arena_item_set_transform(NRArenaItem *item, Geom::Matrix const *transform);
void nr_arena_item_set_opacity (NRArenaItem *item, double opacity);
void nr_arena_item_set_sensitive (NRArenaItem *item, unsigned int sensitive);
void nr_arena_item_set_visible (NRArenaItem *item, unsigned int visible);
void nr_arena_item_set_clip (NRArenaItem *item, NRArenaItem *clip);
void nr_arena_item_set_mask (NRArenaItem *item, NRArenaItem *mask);
void nr_arena_item_set_order (NRArenaItem *item, int order);
void nr_arena_item_set_item_bbox (NRArenaItem *item, Geom::OptRect &bbox);

NRPixBlock *nr_arena_item_get_background (NRArenaItem const *item);

/* Helpers */

NRArenaItem *nr_arena_item_attach (NRArenaItem *parent, NRArenaItem *child, NRArenaItem *prev, NRArenaItem *next);
NRArenaItem *nr_arena_item_detach (NRArenaItem *parent, NRArenaItem *child);

#define NR_ARENA_ITEM_SET_DATA(i,v) (((NRArenaItem *) (i))->data = (v))
#define NR_ARENA_ITEM_GET_DATA(i) (((NRArenaItem *) (i))->data)

#define NR_ARENA_ITEM_SET_KEY(i,k) (((NRArenaItem *) (i))->key = (k))
#define NR_ARENA_ITEM_GET_KEY(i) (((NRArenaItem *) (i))->key)

#endif /* !SEEN_DISPLAY_NR_ARENA_ITEM_H */

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
