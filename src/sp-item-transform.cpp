#define __SP_ITEM_TRANSFORM_C__

/*
 * Transforming single items
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Frank Felfe <innerspace@iname.com>
 *   bulia byak <buliabyak@gmail.com>
 *
 * Copyright (C) 1999-2005 authors
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include <libnr/nr-matrix-ops.h>
#include "libnr/nr-matrix-rotate-ops.h"
#include "libnr/nr-matrix-scale-ops.h"
#include "libnr/nr-matrix-translate-ops.h"
#include "sp-item.h"

static NR::translate inverse(NR::translate const m)
{
	/* TODO: Move this to nr-matrix-fns.h or the like. */
	return NR::translate(-m[0], -m[1]);
}

void
sp_item_rotate_rel(SPItem *item, NR::rotate const &rotation)
{
    NR::Point center = item->getCenter();
    NR::translate const s(item->getCenter());
    NR::Matrix affine = NR::Matrix(inverse(s)) * NR::Matrix(rotation) * NR::Matrix(s);

    // Rotate item.
    sp_item_set_i2d_affine(item, sp_item_i2d_affine(item) * (Geom::Matrix)affine);
    // Use each item's own transform writer, consistent with sp_selection_apply_affine()
    sp_item_write_transform(item, SP_OBJECT_REPR(item), item->transform);

    // Restore the center position (it's changed because the bbox center changed)
    if (item->isCenterSet()) {
        item->setCenter(center * affine);
    }
}

void
sp_item_scale_rel (SPItem *item, NR::scale const &scale)
{
    boost::optional<NR::Rect> bbox = sp_item_bbox_desktop(item);
    if (bbox) {
        Geom::Translate const s(bbox->midpoint()); // use getCenter?
        sp_item_set_i2d_affine(item, sp_item_i2d_affine(item) * s.inverse() * (Geom::Matrix)(NR::Matrix)scale * s);
        sp_item_write_transform(item, SP_OBJECT_REPR(item), item->transform);
    }
}

void
sp_item_skew_rel (SPItem *item, double skewX, double skewY)
{
    NR::Point center = item->getCenter();
    NR::translate const s(item->getCenter());

    NR::Matrix const skew(1, skewY, skewX, 1, 0, 0);
    NR::Matrix affine = NR::Matrix(inverse(s)) * skew * NR::Matrix(s);

    sp_item_set_i2d_affine(item, sp_item_i2d_affine(item) * (Geom::Matrix)affine);
    sp_item_write_transform(item, SP_OBJECT_REPR(item), item->transform);

    // Restore the center position (it's changed because the bbox center changed)
    if (item->isCenterSet()) {
        item->setCenter(center * affine);
    }
}

void sp_item_move_rel(SPItem *item, NR::translate const &tr)
{
	sp_item_set_i2d_affine(item, sp_item_i2d_affine(item) * tr);

	sp_item_write_transform(item, SP_OBJECT_REPR(item), item->transform);
}

/*
** Returns the matrix you need to apply to an object with given visual bbox and strokewidth to
scale/move it to the new visual bbox x0/y0/x1/y1. Takes into account the "scale stroke"
preference value passed to it. Has to solve a quadratic equation to make sure
the goal is met exactly and the stroke scaling is obeyed.
*/

NR::Matrix
get_scale_transform_with_stroke (NR::Rect &bbox_param, gdouble strokewidth, bool transform_stroke, gdouble x0, gdouble y0, gdouble x1, gdouble y1)
{
    NR::Rect bbox (bbox_param);

    NR::Matrix p2o = NR::Matrix (NR::translate (-bbox.min()));
    NR::Matrix o2n = NR::Matrix (NR::translate (x0, y0));

    NR::Matrix scale = NR::Matrix (NR::scale (1, 1)); // scale component
    NR::Matrix unbudge = NR::Matrix (NR::translate (0, 0)); // move component to compensate for the drift caused by stroke width change

    gdouble w0 = bbox.extent(NR::X); // will return a value >= 0, as required further down the road
    gdouble h0 = bbox.extent(NR::Y);
    gdouble w1 = x1 - x0; // can have any sign
    gdouble h1 = y1 - y0;
    gdouble r0 = strokewidth;

    if (bbox.isEmpty()) {
        NR::Matrix move = NR::Matrix(NR::translate(x0 - bbox.min()[NR::X], y0 - bbox.min()[NR::Y]));
        return (move); // cannot scale from empty boxes at all, so only translate
    }

    NR::Matrix direct = NR::Matrix (NR::scale(w1 / w0,   h1 / h0));

    if (fabs(w0 - r0) < 1e-6 || fabs(h0 - r0) < 1e-6 || (!transform_stroke && (fabs(w1 - r0) < 1e-6 || fabs(h1 - r0) < 1e-6))) {
        return (p2o * direct * o2n); // can't solve the equation: one of the dimensions is equal to stroke width, so return the straightforward scaler
    }

    int flip_x = (w1 > 0) ? 1 : -1;
    int flip_y = (h1 > 0) ? 1 : -1;
    
    // w1 and h1 will be negative when mirroring, but if so then e.g. w1-r0 won't make sense
    // Therefore we will use the absolute values from this point on
    w1 = fabs(w1);
    h1 = fabs(h1);
    r0 = fabs(r0);
    // w0 and h0 will always be positive due to the definition extent()

    gdouble ratio_x = (w1 - r0) / (w0 - r0);
    gdouble ratio_y = (h1 - r0) / (h0 - r0);
    
    NR::Matrix direct_constant_r = NR::Matrix (NR::scale(flip_x * ratio_x, flip_y * ratio_y));

    if (transform_stroke && r0 != 0 && r0 != NR_HUGE) { // there's stroke, and we need to scale it
        // These coefficients are obtained from the assumption that scaling applies to the
        // non-stroked "shape proper" and that stroke scale is scaled by the expansion of that
        // matrix. We're trying to solve this equation:
        // r1 = r0 * sqrt (((w1-r0)/(w0-r0))*((h1-r0)/(h0-r0)))
        // The operant of the sqrt() must be positive, which is ensured by the fabs() a few lines above
        gdouble A = -w0*h0 + r0*(w0 + h0);
        gdouble B = -(w1 + h1) * r0*r0;
        gdouble C = w1 * h1 * r0*r0;
        if (B*B - 4*A*C > 0) {
            gdouble r1 = fabs((-B - sqrt(B*B - 4*A*C))/(2*A));
            //gdouble r2 = (-B + sqrt (B*B - 4*A*C))/(2*A);
            //std::cout << "r0" << r0 << " r1" << r1 << " r2" << r2 << "\n";
            //
            // If w1 < 0 then the scale will be wrong if we just do
            // gdouble scale_x = (w1 - r1)/(w0 - r0);
            // Here we also need the absolute values of w0, w1, h0, h1, and r1
            gdouble scale_x = (w1 - r1)/(w0 - r0);
            gdouble scale_y = (h1 - r1)/(h0 - r0);
            scale *= NR::scale(flip_x * scale_x, flip_y * scale_y);
            unbudge *= NR::translate (-flip_x * 0.5 * (r0 * scale_x - r1), -flip_y * 0.5 * (r0 * scale_y - r1));
        } else {
            scale *= direct;
        }
    } else {
        if (r0 == 0 || r0 == NR_HUGE) { // no stroke to scale
            scale *= direct;
        } else {// nonscaling strokewidth
            scale *= direct_constant_r;
            unbudge *= NR::translate (flip_x * 0.5 * r0 * (1 - ratio_x), flip_y * 0.5 * r0 * (1 - ratio_y));
        }
    }

    return (p2o * scale * unbudge * o2n);
}

NR::Rect
get_visual_bbox (boost::optional<NR::Rect> const &initial_geom_bbox, NR::Matrix const &abs_affine, gdouble const initial_strokewidth, bool const transform_stroke)
{
    
    g_assert(initial_geom_bbox);
    
    // Find the new geometric bounding box; Do this by transforming each corner of
    // the initial geometric bounding box individually and fitting a new boundingbox
    // around the transformerd corners  
    NR::Point const p0 = initial_geom_bbox->corner(0) * abs_affine;    
    NR::Rect new_geom_bbox = NR::Rect(p0, p0);
    for (unsigned i = 1 ; i < 4 ; i++) {
        new_geom_bbox.expandTo(initial_geom_bbox->corner(i) * abs_affine);
    }

    NR::Rect new_visual_bbox = new_geom_bbox; 
    if (initial_strokewidth > 0 && initial_strokewidth < NR_HUGE) {
        if (transform_stroke) {
            // scale stroke by: sqrt (((w1-r0)/(w0-r0))*((h1-r0)/(h0-r0))) (for visual bboxes, see get_scale_transform_with_stroke)
            // equals scaling by: sqrt ((w1/w0)*(h1/h0)) for geometrical bboxes            
            // equals scaling by: sqrt (area1/area0) for geometrical bboxes
            gdouble const new_strokewidth = initial_strokewidth * sqrt (new_geom_bbox.area() / initial_geom_bbox->area());
            new_visual_bbox.growBy(0.5 * new_strokewidth);        
        } else {
            // Do not transform the stroke
            new_visual_bbox.growBy(0.5 * initial_strokewidth);   
        }
    }
    
    return new_visual_bbox;
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
