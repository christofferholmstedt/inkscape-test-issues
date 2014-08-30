/**
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *
 * Copyright (C) 2000 Lauris Kaplinski
 * Copyright (C) 2000-2001 Ximian, Inc.
 * Copyright (C) 2002 Lauris Kaplinski
 * Copyright (C) 2008 Johan Engelen
 *
 * Released under GNU GPL, see file 'COPYING' for more information
 */

#ifndef SEEN_DISPLAY_CURVE_H
#define SEEN_DISPLAY_CURVE_H

#include <glib.h>
#include <2geom/forward.h>
#include <boost/optional.hpp>

/**
 * Wrapper around a Geom::PathVector object.
 */
class SPCurve {
public:
    /* Constructors */
    explicit SPCurve();
    explicit SPCurve(Geom::PathVector const& pathv);
    static SPCurve * new_from_rect(Geom::Rect const &rect, bool all_four_sides = false);

    virtual ~SPCurve();

    void set_pathvector(Geom::PathVector const & new_pathv);
    Geom::PathVector const & get_pathvector() const;

    SPCurve * ref();
    SPCurve * unref();

    SPCurve * copy() const;

    guint get_segment_count() const;
    guint nodes_in_path() const;

    bool is_empty() const;
    bool is_closed() const;
    Geom::Curve const * last_segment() const;
    Geom::Path const * last_path() const;
    Geom::Curve const * first_segment() const;
    Geom::Path const * first_path() const;
    boost::optional<Geom::Point> first_point() const;
    boost::optional<Geom::Point> last_point() const;
    boost::optional<Geom::Point> second_point() const;
    boost::optional<Geom::Point> penultimate_point() const;

    void reset();

    void moveto(Geom::Point const &p);
    void moveto(gdouble x, gdouble y);
    void lineto(Geom::Point const &p);
    void lineto(gdouble x, gdouble y);
    void quadto(Geom::Point const &p1, Geom::Point const &p2);
    void quadto(gdouble x1, gdouble y1, gdouble x2, gdouble y2);
    void curveto(Geom::Point const &p0, Geom::Point const &p1, Geom::Point const &p2);
    void curveto(gdouble x0, gdouble y0, gdouble x1, gdouble y1, gdouble x2, gdouble y2);
    void closepath();
    void closepath_current();
    void backspace();

    void transform(Geom::Affine const &m);
    void stretch_endpoints(Geom::Point const &, Geom::Point const &);
    void move_endpoints(Geom::Point const &, Geom::Point const &);
    void last_point_additive_move(Geom::Point const & p);

    void append(SPCurve const *curve2, bool use_lineto);
    SPCurve * append_continuous(SPCurve const *c1, gdouble tolerance);
    SPCurve * create_reverse() const;

    GSList * split() const;
    static SPCurve * concat(GSList const *list);

protected:
    gint _refcount;

    Geom::PathVector _pathv;

private:
    // Don't implement these:
    SPCurve(const SPCurve&);
    SPCurve& operator=(const SPCurve&);
};

#endif // !SEEN_DISPLAY_CURVE_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8 :
