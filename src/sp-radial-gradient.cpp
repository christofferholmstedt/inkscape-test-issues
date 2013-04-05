#include "sp-radial-gradient.h"

#include "attributes.h"
#include "xml/repr.h"

#include "2geom/transforms.h"

#include "sp-factory.h"

namespace {
	SPObject* createRadialGradient() {
		return new SPRadialGradient();
	}

	bool radialGradientRegistered = SPFactory::instance().registerObject("svg:radialGradient", createRadialGradient);
}

/*
 * Radial Gradient
 */
SPRadialGradient::SPRadialGradient() : SPGradient() {
	this->cobject = this;

    this->cx.unset(SVGLength::PERCENT, 0.5, 0.5);
    this->cy.unset(SVGLength::PERCENT, 0.5, 0.5);
    this->r.unset(SVGLength::PERCENT, 0.5, 0.5);
    this->fx.unset(SVGLength::PERCENT, 0.5, 0.5);
    this->fy.unset(SVGLength::PERCENT, 0.5, 0.5);
}

SPRadialGradient::~SPRadialGradient() {
}

/**
 * Set radial gradient attributes from associated repr.
 */
void SPRadialGradient::build(SPDocument *document, Inkscape::XML::Node *repr) {
    SPGradient::build(document, repr);

    this->readAttr( "cx" );
    this->readAttr( "cy" );
    this->readAttr( "r" );
    this->readAttr( "fx" );
    this->readAttr( "fy" );
}

/**
 * Set radial gradient attribute.
 */
void SPRadialGradient::set(unsigned key, gchar const *value) {
    switch (key) {
        case SP_ATTR_CX:
            if (!this->cx.read(value)) {
                this->cx.unset(SVGLength::PERCENT, 0.5, 0.5);
            }

            if (!this->fx._set) {
                this->fx.value = this->cx.value;
                this->fx.computed = this->cx.computed;
            }

            this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;

        case SP_ATTR_CY:
            if (!this->cy.read(value)) {
                this->cy.unset(SVGLength::PERCENT, 0.5, 0.5);
            }

            if (!this->fy._set) {
                this->fy.value = this->cy.value;
                this->fy.computed = this->cy.computed;
            }

            this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;

        case SP_ATTR_R:
            if (!this->r.read(value)) {
                this->r.unset(SVGLength::PERCENT, 0.5, 0.5);
            }

            this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;

        case SP_ATTR_FX:
            if (!this->fx.read(value)) {
                this->fx.unset(this->cx.unit, this->cx.value, this->cx.computed);
            }

            this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;

        case SP_ATTR_FY:
            if (!this->fy.read(value)) {
                this->fy.unset(this->cy.unit, this->cy.value, this->cy.computed);
            }

            this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;

        default:
            SPGradient::set(key, value);
            break;
    }
}

/**
 * Write radial gradient attributes to associated repr.
 */
Inkscape::XML::Node* SPRadialGradient::write(Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, guint flags) {
    if ((flags & SP_OBJECT_WRITE_BUILD) && !repr) {
        repr = xml_doc->createElement("svg:radialGradient");
    }

    if ((flags & SP_OBJECT_WRITE_ALL) || this->cx._set) {
    	sp_repr_set_svg_double(repr, "cx", this->cx.computed);
    }

    if ((flags & SP_OBJECT_WRITE_ALL) || this->cy._set) {
    	sp_repr_set_svg_double(repr, "cy", this->cy.computed);
    }

    if ((flags & SP_OBJECT_WRITE_ALL) || this->r._set) {
    	sp_repr_set_svg_double(repr, "r", this->r.computed);
    }

    if ((flags & SP_OBJECT_WRITE_ALL) || this->fx._set) {
    	sp_repr_set_svg_double(repr, "fx", this->fx.computed);
    }

    if ((flags & SP_OBJECT_WRITE_ALL) || this->fy._set) {
    	sp_repr_set_svg_double(repr, "fy", this->fy.computed);
    }

    SPGradient::write(xml_doc, repr, flags);

    return repr;
}

/**
 * Directly set properties of radial gradient and request modified.
 */
void
sp_radialgradient_set_position(SPRadialGradient *rg,
                               gdouble cx, gdouble cy, gdouble fx, gdouble fy, gdouble r)
{
    g_return_if_fail(rg != NULL);
    g_return_if_fail(SP_IS_RADIALGRADIENT(rg));

    /* fixme: units? (Lauris)  */
    rg->cx.set(SVGLength::NONE, cx, cx);
    rg->cy.set(SVGLength::NONE, cy, cy);
    rg->fx.set(SVGLength::NONE, fx, fx);
    rg->fy.set(SVGLength::NONE, fy, fy);
    rg->r.set(SVGLength::NONE, r, r);

    rg->requestModified(SP_OBJECT_MODIFIED_FLAG);
}


cairo_pattern_t* SPRadialGradient::pattern_new(cairo_t *ct, Geom::OptRect const &bbox, double opacity) {
    this->ensureVector();

    Geom::Point focus(this->fx.computed, this->fy.computed);
    Geom::Point center(this->cx.computed, this->cy.computed);

    double radius = this->r.computed;
    double scale = 1.0;
    double tolerance = cairo_get_tolerance(ct);

    // NOTE: SVG2 will allow the use of a focus circle which can
    // have its center outside the first circle.

    // code below suggested by Cairo devs to overcome tolerance problems
    // more: https://bugs.freedesktop.org/show_bug.cgi?id=40918

    // Corrected for
    // https://bugs.launchpad.net/inkscape/+bug/970355

    Geom::Affine gs2user = this->gradientTransform;
    Geom::Scale  gs2user_scale;

    if (this->getUnits() == SP_GRADIENT_UNITS_OBJECTBOUNDINGBOX && bbox) {
        Geom::Affine bbox2user(bbox->width(), 0, 0, bbox->height(), bbox->left(), bbox->top());
        gs2user *= bbox2user;
        gs2user_scale = Geom::Scale( gs2user[0], gs2user[3] );
    }

    Geom::Point d = focus - center;
    Geom::Point d_user = d * gs2user_scale;
    Geom::Point r_user( radius, 0 );
    r_user *= gs2user_scale;

    if (d_user.length() + tolerance > r_user.length()) {
        scale = r_user.length() / d_user.length();
        double dx = d_user.x(), dy = d_user.y();
        cairo_user_to_device_distance(ct, &dx, &dy);

        if (!Geom::are_near(dx, 0, tolerance) || !Geom::are_near(dy, 0, tolerance))
        {
            scale *= 1.0 - 2.0 * tolerance / hypot(dx, dy);
        }
    }

    cairo_pattern_t *cp = cairo_pattern_create_radial(
        scale * d.x() + center.x(), scale * d.y() + center.y(), 0,
        center.x(), center.y(), radius);

    sp_gradient_pattern_common_setup(cp, this, bbox, opacity);

    return cp;
}
