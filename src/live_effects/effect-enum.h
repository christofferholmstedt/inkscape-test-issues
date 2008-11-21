#ifndef INKSCAPE_LIVEPATHEFFECT_ENUM_H
#define INKSCAPE_LIVEPATHEFFECT_ENUM_H

/*
 * Inkscape::LivePathEffect::EffectType
 *
* Copyright (C) Johan Engelen 2008 <j.b.c.engelen@utwente.nl>
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "util/enums.h"

#define LPE_ENABLE_TEST_EFFECTS

namespace Inkscape {
namespace LivePathEffect {

enum EffectType {
    BEND_PATH = 0,
    PATTERN_ALONG_PATH,
    FREEHAND_SHAPE,
    SKETCH,
    HATCHES,
    VONKOCH,
    KNOT,
#ifdef LPE_ENABLE_TEST_EFFECTS
    DOEFFECTSTACK_TEST,
#endif
    GEARS,
    CURVE_STITCH,
    CIRCLE_WITH_RADIUS,
    PERSPECTIVE_PATH,
    SPIRO,
    LATTICE,
    ENVELOPE,
    CONSTRUCT_GRID,
    PERP_BISECTOR,
    TANGENT_TO_CURVE,
    MIRROR_SYMMETRY,
    CIRCLE_3PTS,
    ANGLE_BISECTOR,
    PARALLEL,
    COPY_ROTATE,
    OFFSET,
    RULER,
    BOOLOPS,
    INTERPOLATE,
    TEXT_LABEL,
    PATH_LENGTH,
    LINE_SEGMENT,
    INVALID_LPE // This must be last
};

extern const Util::EnumData<EffectType> LPETypeData[];  /// defined in effect.cpp
extern const Util::EnumDataConverter<EffectType> LPETypeConverter; /// defined in effect.cpp

} //namespace LivePathEffect
} //namespace Inkscape

#endif

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
