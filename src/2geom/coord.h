/** @file
 * @brief Integral and real coordinate types and some basic utilities
 *//*
 * Copyright 2006 Nathan Hurst <njh@mail.csse.monash.edu.au>
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 */

#ifndef LIB2GEOM_SEEN_COORD_H
#define LIB2GEOM_SEEN_COORD_H

#include <cmath>
#include <limits>
#include <string>
#include <functional>
#include <boost/operators.hpp>
#include <2geom/forward.h>

namespace Geom {

/// 2D axis enumeration (X or Y).
enum Dim2 { X=0, Y=1 };

/// Get the other (perpendicular) dimension.
inline Dim2 other_dimension(Dim2 d) { return d == Y ? X : Y; }

// TODO: make a smarter implementation with C++11
template <typename T>
struct D2Traits {
    typedef typename T::D1Value D1Value;
    typedef typename T::D1Reference D1Reference;
    typedef typename T::D1ConstReference D1ConstReference;
};

// for use with things such as transform_iterator
template <typename T>
struct GetX {
    typedef typename D2Traits<T>::D1Value result_type;
    typedef T argument_type;
    typename D2Traits<T>::D1Value operator()(T const &a) const {
        return a[X];
    }
};
template <typename T>
struct GetY {
    typedef typename D2Traits<T>::D1Value result_type;
    typedef T argument_type;
    typename D2Traits<T>::D1Value operator()(T const &a) const {
        return a[Y];
    }
};

/**
 * @brief Floating point type used to store coordinates.
 *
 * You may safely assume that double (or even float) provides enough precision for storing
 * on-canvas points, and hence that double provides enough precision for dot products of
 * differences of on-canvas points.
 */
typedef double Coord;
typedef int IntCoord;

const Coord EPSILON = 1e-5; //1e-18;

inline Coord infinity() {  return std::numeric_limits<Coord>::infinity();  }

//IMPL: NearConcept
inline bool are_near(Coord a, Coord b, double eps=EPSILON) { return a-b <= eps && a-b >= -eps; }
inline bool rel_error_bound(Coord a, Coord b, double eps=EPSILON) { return a <= eps*b && a >= -eps*b; }

/// Numerically stable linear interpolation.
inline Coord lerp(Coord t, Coord a, Coord b) {
    return (1 - t) * a + t * b;
}

template <typename C>
struct CoordTraits {
    typedef D2<C> PointType;
    typedef GenericInterval<C> IntervalType;
    typedef GenericOptInterval<C> OptIntervalType;
    typedef GenericRect<C> RectType;
    typedef GenericOptRect<C> OptRectType;

    typedef
      boost::equality_comparable< IntervalType
    , boost::orable< IntervalType
      > >
        IntervalOps;

    typedef
      boost::equality_comparable< RectType
    , boost::orable< RectType
    , boost::orable< RectType, OptRectType
      > > >
        RectOps;
};

// NOTE: operator helpers for Rect and Interval are defined here.
// This is to avoid increasing their size through multiple inheritance.

template<>
struct CoordTraits<IntCoord> {
    typedef IntPoint PointType;
    typedef IntInterval IntervalType;
    typedef OptIntInterval OptIntervalType;
    typedef IntRect RectType;
    typedef OptIntRect OptRectType;

    typedef
      boost::equality_comparable< IntInterval
    , boost::additive< IntInterval
    , boost::additive< IntInterval, IntCoord
    , boost::orable< IntInterval
      > > > >
        IntervalOps;

    typedef
      boost::equality_comparable< IntRect
    , boost::orable< IntRect
    , boost::orable< IntRect, OptIntRect
    , boost::additive< IntRect, IntPoint
      > > > >
        RectOps;
};

template<>
struct CoordTraits<Coord> {
    typedef Point PointType;
    typedef Interval IntervalType;
    typedef OptInterval OptIntervalType;
    typedef Rect RectType;
    typedef OptRect OptRectType;

    typedef
      boost::equality_comparable< Interval
    , boost::equality_comparable< Interval, IntInterval
    , boost::additive< Interval
    , boost::multipliable< Interval
    , boost::orable< Interval
    , boost::arithmetic< Interval, Coord
      > > > > > >
        IntervalOps;

    typedef
      boost::equality_comparable< Rect
    , boost::equality_comparable< Rect, IntRect
    , boost::orable< Rect
    , boost::orable< Rect, OptRect
    , boost::additive< Rect, Point
    , boost::multipliable< Rect, Affine
      > > > > > >
        RectOps;
};

/** @brief Convert coordinate to shortest possible string
 * @return The shortest string that parses back to the original value. */
std::string format_coord_shortest(Coord x);

/** @brief Convert coordinate to human-readable string
 * Unlike format_coord_shortest, this function will not omit a leading zero
 * before a decimal point or use small negative exponents. The output format
 * is similar to Javascript functions. */
std::string format_coord_nice(Coord x);

/** @brief Parse coordinate string
 * When using this function in conjunction with format_coord_shortest()
 * or format_coord_nice(), the value is guaranteed to be preserved exactly. */
Coord parse_coord(std::string const &s);

} // end namespace Geom

#endif // LIB2GEOM_SEEN_COORD_H

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
