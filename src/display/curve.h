#ifndef SEEN_DISPLAY_CURVE_H
#define SEEN_DISPLAY_CURVE_H

/** \file
 * Wrapper around an array of NArtBpath objects.
 *
 * Author:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *
 * Copyright (C) 2000 Lauris Kaplinski
 * Copyright (C) 2000-2001 Ximian, Inc.
 * Copyright (C) 2002 Lauris Kaplinski
 * Copyright (C) 2008 Johan Engelen
 *
 * Released under GNU GPL
 */

#include <glib/gtypes.h>
#include <glib/gslist.h>

#include <2geom/forward.h>
#include <2geom/point.h>

#include "libnr/nr-forward.h"
#include "libnr/nr-rect.h"

#define SP_CURVE_LENSTEP 32

struct SPObject;

/// Wrapper around Geom::PathVector.
class SPCurve {
public:
    /* Constructors */
    SPCurve(guint length = SP_CURVE_LENSTEP);
    SPCurve(Geom::PathVector const& pathv);
    static SPCurve * new_from_bpath(NArtBpath *bpath);
    static SPCurve * new_from_foreign_bpath(NArtBpath const *bpath);
    static SPCurve * new_from_rect(NR::Maybe<NR::Rect> const &rect);

    virtual ~SPCurve();

    void set_pathv(Geom::PathVector const & new_pathv);
    NArtBpath const * get_bpath() const;
    Geom::PathVector const & get_pathvector() const;

    guint get_length() const;

    SPCurve * ref();
    SPCurve * unref();

    SPCurve * copy() const;

    GSList * split() const;
    void transform(Geom::Matrix const &m);
    void transform(NR::Matrix const &);
    void transform(NR::translate const &);
    void stretch_endpoints(NR::Point const &, NR::Point const &);
    void move_endpoints(NR::Point const &, NR::Point const &);
    void last_point_additive_move(Geom::Point const & p);

    void reset();

    void moveto(Geom::Point const &p);
    void moveto(NR::Point const &p);
    void moveto(gdouble x, gdouble y);
    void lineto(Geom::Point const &p);
    void lineto(NR::Point const &p);
    void lineto(gdouble x, gdouble y);
    void curveto(Geom::Point const &p0, Geom::Point const &p1, Geom::Point const &p2);
    void curveto(NR::Point const &p0, NR::Point const &p1, NR::Point const &p2);
    void curveto(gdouble x0, gdouble y0, gdouble x1, gdouble y1, gdouble x2, gdouble y2);
    void closepath();
    void closepath_current();

    SPCurve * append_continuous(SPCurve const *c1, gdouble tolerance);

    bool is_empty() const;
    bool is_closed() const;
    NArtBpath const * last_bpath() const;
    Geom::Path const * last_path() const;
    NArtBpath const * first_bpath() const;
    Geom::Path const * first_path() const;
    NR::Point first_point() const;
    NR::Point last_point() const;
    NR::Point second_point() const;
    NR::Point penultimate_point() const;
    guint nodes_in_path() const;

    void append(SPCurve const *curve2, bool use_lineto);
    SPCurve * create_reverse() const;
    void backspace();

    static SPCurve * concat(GSList const *list);

    void ensure_space(guint space);

protected:
    gint _refcount;

    NArtBpath *_bpath;
    Geom::PathVector _pathv;

    /// Index in bpath[] of NR_END element.
    guint _end;

    /// Allocated size (i.e., capacity) of bpath[] array.  Not to be confused 
    /// with the SP_CURVE_LENGTH macro, which returns the logical length of 
    /// the path (i.e., index of NR_END).
    guint _length;

    /// Index in bpath[] of the start (i.e., moveto element) of the last 
    /// subpath in this path.
    guint _substart;

    /// Previous moveto position.
    /// \note This is used for coalescing moveto's, whereas if we're to 
    /// conform to the SVG spec then we mustn't coalesce movetos if we have 
    /// midpoint markers.  Ref:
    /// http://www.w3.org/TR/SVG11/implnote.html#PathElementImplementationNotes
    /// (first subitem of the item about zero-length path segments)
    NR::Point _movePos;

    /// True iff current point is defined.  Initially false for a new curve; 
    /// becomes true after moveto; becomes false on closepath.  Curveto, 
    /// lineto etc. require hascpt; hascpt remains true after lineto/curveto.
    bool _hascpt : 1;
    
    /// True iff previous was moveto.
    bool _posSet : 1;

    /// True iff bpath end is moving.
    bool _moving : 1;
    
    /// True iff all subpaths are closed.
    bool _closed : 1;

private:
    // Don't implement these:
    SPCurve(const SPCurve&);
    SPCurve& operator=(const SPCurve&);

//friends:
    friend double sp_curve_distance_including_space(SPCurve const *const curve, double seg2len[]);
    friend double sp_curve_nonzero_distance_including_space(SPCurve const *const curve, double seg2len[]);
    template<class M> friend void tmpl_curve_transform(SPCurve *const curve, M const &m);
    // this function is the only one who needs read access to _movePos and _posSet
    friend void sp_polygon_set(SPObject *object, unsigned int key, const gchar *value);

    static void debug_check( char const * text, SPCurve const * curve);
    static void debug_check( char const * text, bool a);
};

#define SP_CURVE_LENGTH(c) (((SPCurve const *)(c))->get_length())
#define SP_CURVE_BPATH(c) (((SPCurve const *)(c))->get_bpath())
#define SP_CURVE_SEGMENT(c,i) (((SPCurve const *)(c))->get_bpath() + (i))

#endif /* !SEEN_DISPLAY_CURVE_H */

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
