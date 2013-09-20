/** @file
 * @brief Enhanced Metafile printing
 *//*
 * Authors:
 *   Ulf Erikson <ulferikson@users.sf.net>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Abhishek Sharma
 *   David Mathog
 *
 * Copyright (C) 2006-2009 Authors
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 *
 * References:
 *  - How to Create & Play Enhanced Metafiles in Win32
 *      http://support.microsoft.com/kb/q145999/
 *  - INFO: Windows Metafile Functions & Aldus Placeable Metafiles
 *      http://support.microsoft.com/kb/q66949/
 *  - Metafile Functions
 *      http://msdn.microsoft.com/library/en-us/gdi/metafile_0whf.asp
 *  - Metafile Structures
 *      http://msdn.microsoft.com/library/en-us/gdi/metafile_5hkj.asp
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>
#include <glibmm/miscutils.h>
#include <libuemf/symbol_convert.h>
#include <2geom/sbasis-to-bezier.h>
#include <2geom/path.h>
#include <2geom/pathvector.h>
#include <2geom/rect.h>
#include <2geom/curves.h>

#include "helper/geom.h"
#include "helper/geom-curves.h"
#include "sp-item.h"
#include "util/units.h"

#include "style.h"
#include "inkscape-version.h"
#include "sp-root.h"

#include "extension/system.h"
#include "extension/print.h"
#include "document.h"
#include "path-prefix.h"
#include "sp-pattern.h"
#include "sp-image.h"
#include "sp-gradient.h"
#include "sp-radial-gradient.h"
#include "sp-linear-gradient.h"
#include "display/cairo-utils.h"

#include "splivarot.h"             // pieces for union on shapes
#include "2geom/svg-path-parser.h" // to get from SVG text to Geom::Path
#include "display/canvas-bpath.h"  // for SPWindRule

#include "emf-print.h"


namespace Inkscape {
namespace Extension {
namespace Internal {

#define PXPERMETER 2835


/* globals */
static double       PX2WORLD = 20.0f;
static U_XFORM      worldTransform;
static bool         FixPPTCharPos, FixPPTDashLine, FixPPTGrad2Polys, FixPPTPatternAsHatch, FixImageRot;
static EMFTRACK    *et               = NULL;
static EMFHANDLES  *eht              = NULL;

void PrintEmf::smuggle_adxkyrtl_out(const char *string, uint32_t **adx, double *ky, int *rtl, int *ndx, float scale)
{
    float       fdx;
    int         i;
    uint32_t   *ladx;
    const char *cptr = &string[strlen(string) + 1]; // this works because of the first fake terminator

    *adx = NULL;
    *ky  = 0.0;       // set a default value
    sscanf(cptr, "%7d", ndx);
    if (!*ndx) {
        return;    // this could happen with an empty string
    }
    cptr += 7;
    ladx = (uint32_t *) malloc(*ndx * sizeof(uint32_t));
    if (!ladx) {
        g_message("Out of memory");
    }
    *adx = ladx;
    for (i = 0; i < *ndx; i++, cptr += 7, ladx++) {
        sscanf(cptr, "%7f", &fdx);
        *ladx = (uint32_t) round(fdx * scale);
    }
    cptr++; // skip 2nd fake terminator
    sscanf(cptr, "%7f", &fdx);
    *ky = fdx;
    cptr += 7;  // advance over ky and its space
    sscanf(cptr, "%07d", rtl);
}

PrintEmf::PrintEmf()
{
    // all of the class variables are initialized elsewhere, many in PrintEmf::Begin,
}


unsigned int PrintEmf::setup(Inkscape::Extension::Print * /*mod*/)
{
    return TRUE;
}


unsigned int PrintEmf::begin(Inkscape::Extension::Print *mod, SPDocument *doc)
{
    U_SIZEL         szlDev, szlMm;
    U_RECTL         rclBounds, rclFrame;
    char           *rec;
    gchar const    *utf8_fn = mod->get_param_string("destination");

    FixPPTCharPos        = mod->get_param_bool("FixPPTCharPos");
    FixPPTDashLine       = mod->get_param_bool("FixPPTDashLine");
    FixPPTGrad2Polys     = mod->get_param_bool("FixPPTGrad2Polys");
    FixPPTPatternAsHatch = mod->get_param_bool("FixPPTPatternAsHatch");
    FixImageRot          = mod->get_param_bool("FixImageRot");

    (void) emf_start(utf8_fn, 1000000, 250000, &et);  // Initialize the et structure
    (void) htable_create(128, 128, &eht);             // Initialize the eht structure

    char *ansi_uri = (char *) utf8_fn;


    // width and height in px
    _width  = doc->getWidth().value("px");
    _height = doc->getHeight().value("px");

    // initialize a few global variables
    hbrush = hbrushOld = hpen = 0;
    htextalignment = U_TA_BASELINE | U_TA_LEFT;
    use_stroke = use_fill = simple_shape = usebk = false;

    Inkscape::XML::Node *nv = sp_repr_lookup_name(doc->rroot, "sodipodi:namedview");
    if (nv) {
        const char *p1 = nv->attribute("pagecolor");
        char *p2;
        uint32_t lc = strtoul(&p1[1], &p2, 16);    // it looks like "#ABC123"
        if (*p2) {
            lc = 0;
        }
        gv.bgc = _gethexcolor(lc);
        gv.rgb[0] = (float) U_RGBAGetR(gv.bgc) / 255.0;
        gv.rgb[1] = (float) U_RGBAGetG(gv.bgc) / 255.0;
        gv.rgb[2] = (float) U_RGBAGetB(gv.bgc) / 255.0;
    }

    bool pageBoundingBox;
    pageBoundingBox = mod->get_param_bool("pageBoundingBox");

    Geom::Rect d;
    if (pageBoundingBox) {
        d = Geom::Rect::from_xywh(0, 0, _width, _height);
    } else {
        SPItem *doc_item = doc->getRoot();
        Geom::OptRect bbox = doc_item->desktopVisualBounds();
        if (bbox) {
            d = *bbox;
        }
    }

    d *= Geom::Scale(Inkscape::Util::Quantity::convert(1, "px", "in"));

    float dwInchesX = d.width();
    float dwInchesY = d.height();

    // dwInchesX x dwInchesY in micrometer units, dpi=90 -> 3543.3 dpm
    (void) drawing_size((int) ceil(dwInchesX * 25.4), (int) ceil(dwInchesY * 25.4), 3.543307, &rclBounds, &rclFrame);

    // set up the reference device as 100 X A4 horizontal, (1200 dpi/25.4 -> dpmm).  Extra digits maintain dpi better in EMF
    int MMX = 21600;
    int MMY = 27900;
    (void) device_size(MMX, MMY, 1200.0 / 25.4, &szlDev, &szlMm);
    int PixelsX = szlDev.cx;
    int PixelsY = szlDev.cy;

    // set up the description:  (version string)0(file)00
    char buff[1024];
    memset(buff, 0, sizeof(buff));
    char *p1 = strrchr(ansi_uri, '\\');
    char *p2 = strrchr(ansi_uri, '/');
    char *p = MAX(p1, p2);
    if (p) {
        p++;
    } else {
        p = ansi_uri;
    }
    snprintf(buff, sizeof(buff) - 1, "Inkscape %s (%s)\1%s\1", Inkscape::version_string, __DATE__, p);
    uint16_t *Description = U_Utf8ToUtf16le(buff, 0, NULL);
    int cbDesc = 2 + wchar16len(Description);      // also count the final terminator
    (void) U_Utf16leEdit(Description, '\1', '\0'); // swap the temporary \1 characters for nulls

    // construct the EMRHEADER record and append it to the EMF in memory
    rec = U_EMRHEADER_set(rclBounds,  rclFrame,  NULL, cbDesc, Description, szlDev, szlMm, 0);
    free(Description);
    if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
        g_error("Fatal programming error in PrintEmf::begin at EMRHEADER");
    }


    // Simplest mapping mode, supply all coordinates in pixels
    rec = U_EMRSETMAPMODE_set(U_MM_TEXT);
    if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
        g_error("Fatal programming error in PrintEmf::begin at EMRSETMAPMODE");
    }


    //  Correct for dpi in EMF (1200) vs dpi in Inkscape (always 90).
    //  Also correct for the scaling in PX2WORLD, which is set to 20.

    worldTransform.eM11 = 1200. / (90.0 * PX2WORLD);
    worldTransform.eM12 = 0.0;
    worldTransform.eM21 = 0.0;
    worldTransform.eM22 = 1200. / (90.0 * PX2WORLD);
    worldTransform.eDx  = 0;
    worldTransform.eDy  = 0;

    rec = U_EMRMODIFYWORLDTRANSFORM_set(worldTransform, U_MWT_LEFTMULTIPLY);
    if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
        g_error("Fatal programming error in PrintEmf::begin at EMRMODIFYWORLDTRANSFORM");
    }


    if (1) {
        snprintf(buff, sizeof(buff) - 1, "Screen=%dx%dpx, %dx%dmm", PixelsX, PixelsY, MMX, MMY);
        rec = textcomment_set(buff);
        if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
            g_error("Fatal programming error in PrintEmf::begin at textcomment_set 1");
        }

        snprintf(buff, sizeof(buff) - 1, "Drawing=%.1lfx%.1lfpx, %.1lfx%.1lfmm", _width, _height, Inkscape::Util::Quantity::convert(dwInchesX, "in", "mm"), Inkscape::Util::Quantity::convert(dwInchesY, "in", "mm"));
        rec = textcomment_set(buff);
        if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
            g_error("Fatal programming error in PrintEmf::begin at textcomment_set 1");
        }
    }

    /* set some parameters, else the program that reads the EMF may default to other values */

    rec = U_EMRSETBKMODE_set(U_TRANSPARENT);
    if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
        g_error("Fatal programming error in PrintEmf::begin at U_EMRSETBKMODE_set");
    }

    hpolyfillmode = U_WINDING;
    rec = U_EMRSETPOLYFILLMODE_set(U_WINDING);
    if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
        g_error("Fatal programming error in PrintEmf::begin at U_EMRSETPOLYFILLMODE_set");
    }

    // Text alignment:  (only changed if RTL text is encountered )
    //   - (x,y) coordinates received by this filter are those of the point where the text
    //     actually starts, and already takes into account the text object's alignment;
    //   - for this reason, the EMF text alignment must always be TA_BASELINE|TA_LEFT.
    htextalignment = U_TA_BASELINE | U_TA_LEFT;
    rec = U_EMRSETTEXTALIGN_set(U_TA_BASELINE | U_TA_LEFT);
    if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
        g_error("Fatal programming error in PrintEmf::begin at U_EMRSETTEXTALIGN_set");
    }

    htextcolor_rgb[0] = htextcolor_rgb[1] = htextcolor_rgb[2] = 0.0;
    rec = U_EMRSETTEXTCOLOR_set(U_RGB(0, 0, 0));
    if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
        g_error("Fatal programming error in PrintEmf::begin at U_EMRSETTEXTCOLOR_set");
    }

    rec = U_EMRSETROP2_set(U_R2_COPYPEN);
    if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
        g_error("Fatal programming error in PrintEmf::begin at U_EMRSETROP2_set");
    }

    /* miterlimit is set with eah pen, so no need to check for it changes as in WMF */

    return 0;
}


unsigned int PrintEmf::finish(Inkscape::Extension::Print * /*mod*/)
{
    char *rec;
    if (!et) {
        return 0;
    }


    // earlier versions had flush of fill here, but it never executed and was removed

    rec = U_EMREOF_set(0, NULL, et); // generate the EOF record
    if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
        g_error("Fatal programming error in PrintEmf::finish");
    }
    (void) emf_finish(et, eht); // Finalize and write out the EMF
    emf_free(&et);              // clean up
    htable_free(&eht);          // clean up

    return 0;
}


unsigned int PrintEmf::comment(
    Inkscape::Extension::Print * /*module*/,
    const char * /*comment*/)
{
    if (!et) {
        return 0;
    }

    // earlier versions had flush of fill here, but it never executed and was removed

    return 0;
}


// fcolor is defined when gradients are being expanded, it is the color of one stripe or ring.
int PrintEmf::create_brush(SPStyle const *style, PU_COLORREF fcolor)
{
    float         rgb[3];
    char         *rec;
    U_LOGBRUSH    lb;
    uint32_t      brush, fmode;
    MFDrawMode    fill_mode;
    Inkscape::Pixbuf *pixbuf;
    uint32_t      brushStyle;
    int           hatchType;
    U_COLORREF    hatchColor;
    U_COLORREF    bkColor;
    uint32_t      width  = 0; // quiets a harmless compiler warning, initialization not otherwise required.
    uint32_t      height = 0;

    if (!et) {
        return 0;
    }

    // set a default fill in case we can't figure out a better way to do it
    fmode      = U_ALTERNATE;
    fill_mode  = DRAW_PAINT;
    brushStyle = U_BS_SOLID;
    hatchType  = U_HS_SOLIDCLR;
    bkColor    = U_RGB(0, 0, 0);
    if (fcolor) {
        hatchColor = *fcolor;
    } else {
        hatchColor = U_RGB(0, 0, 0);
    }

    if (!fcolor && style) {
        if (style->fill.isColor()) {
            fill_mode = DRAW_PAINT;
            float opacity = SP_SCALE24_TO_FLOAT(style->fill_opacity.value);
            if (opacity <= 0.0) {
                opacity = 0.0;    // basically the same as no fill
            }

            sp_color_get_rgb_floatv(&style->fill.value.color, rgb);
            hatchColor = U_RGB(255 * rgb[0], 255 * rgb[1], 255 * rgb[2]);

            fmode = style->fill_rule.computed == 0 ? U_WINDING : (style->fill_rule.computed == 2 ? U_ALTERNATE : U_ALTERNATE);
        } else if (SP_IS_PATTERN(SP_STYLE_FILL_SERVER(style))) { // must be paint-server
            SPPaintServer *paintserver = style->fill.value.href->getObject();
            SPPattern *pat = SP_PATTERN(paintserver);
            double dwidth  = pattern_width(pat);
            double dheight = pattern_height(pat);
            width  = dwidth;
            height = dheight;
            brush_classify(pat, 0, &pixbuf, &hatchType, &hatchColor, &bkColor);
            if (pixbuf) {
                fill_mode = DRAW_IMAGE;
            } else { // pattern
                fill_mode = DRAW_PATTERN;
                if (hatchType == -1) { // Not a standard hatch, so force it to something
                    hatchType  = U_HS_CROSS;
                    hatchColor = U_RGB(0xFF, 0xC3, 0xC3);
                }
            }
            if (FixPPTPatternAsHatch) {
                if (hatchType == -1) { // image or unclassified
                    fill_mode  = DRAW_PATTERN;
                    hatchType  = U_HS_DIAGCROSS;
                    hatchColor = U_RGB(0xFF, 0xC3, 0xC3);
                }
            }
            brushStyle = U_BS_HATCHED;
        } else if (SP_IS_GRADIENT(SP_STYLE_FILL_SERVER(style))) { // must be a gradient
            // currently we do not do anything with gradients, the code below just sets the color to the average of the stops
            SPPaintServer *paintserver = style->fill.value.href->getObject();
            SPLinearGradient *lg = NULL;
            SPRadialGradient *rg = NULL;

            if (SP_IS_LINEARGRADIENT(paintserver)) {
                lg = SP_LINEARGRADIENT(paintserver);
                SP_GRADIENT(lg)->ensureVector(); // when exporting from commandline, vector is not built
                fill_mode = DRAW_LINEAR_GRADIENT;
            } else if (SP_IS_RADIALGRADIENT(paintserver)) {
                rg = SP_RADIALGRADIENT(paintserver);
                SP_GRADIENT(rg)->ensureVector(); // when exporting from commandline, vector is not built
                fill_mode = DRAW_RADIAL_GRADIENT;
            } else {
                // default fill
            }

            if (rg) {
                if (FixPPTGrad2Polys) {
                    return hold_gradient(rg, fill_mode);
                } else {
                    hatchColor = avg_stop_color(rg);
                }
            } else if (lg) {
                if (FixPPTGrad2Polys) {
                    return hold_gradient(lg, fill_mode);
                } else {
                    hatchColor = avg_stop_color(lg);
                }
            }
        }
    } else { // if (!style)
        // default fill
    }

    lb   = logbrush_set(brushStyle, hatchColor, hatchType);

    switch (fill_mode) {
    case DRAW_LINEAR_GRADIENT: // fill with average color unless gradients are converted to slices
    case DRAW_RADIAL_GRADIENT: // ditto
    case DRAW_PAINT:
    case DRAW_PATTERN:
        // SVG text has no background attribute, so OPAQUE mode ALWAYS cancels after the next draw, otherwise it would mess up future text output.
        if (usebk) {
            rec = U_EMRSETBKCOLOR_set(bkColor);
            if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
                g_error("Fatal programming error in PrintEmf::create_brush at U_EMRSETBKCOLOR_set");
            }
            rec = U_EMRSETBKMODE_set(U_OPAQUE);
            if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
                g_error("Fatal programming error in PrintEmf::create_brush at U_EMRSETBKMODE_set");
            }
        }
        rec = createbrushindirect_set(&brush, eht, lb);
        if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
            g_error("Fatal programming error in PrintEmf::create_brush at createbrushindirect_set");
        }
        break;
    case DRAW_IMAGE:
        char                *px;
        char                *rgba_px;
        uint32_t             cbPx;
        uint32_t             colortype;
        PU_RGBQUAD           ct;
        int                  numCt;
        U_BITMAPINFOHEADER   Bmih;
        PU_BITMAPINFO        Bmi;
        rgba_px = (char *) pixbuf->pixels(); // Do NOT free this!!!
        colortype = U_BCBM_COLOR32;
        (void) RGBA_to_DIB(&px, &cbPx, &ct, &numCt,  rgba_px,  width, height, width * 4, colortype, 0, 1);
        // Not sure why the next swap is needed because the preceding does it, and the code is identical
        // to that in stretchdibits_set, which does not need this.
        swapRBinRGBA(px, width * height);
        Bmih = bitmapinfoheader_set(width, height, 1, colortype, U_BI_RGB, 0, PXPERMETER, PXPERMETER, numCt, 0);
        Bmi = bitmapinfo_set(Bmih, ct);
        rec = createdibpatternbrushpt_set(&brush, eht, U_DIB_RGB_COLORS, Bmi, cbPx, px);
        if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
            g_error("Fatal programming error in PrintEmf::create_brush at createdibpatternbrushpt_set");
        }
        free(px);
        free(Bmi); // ct will be NULL because of colortype
        break;
    }

    hbrush = brush;  // need this later for destroy_brush
    rec = selectobject_set(brush, eht);
    if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
        g_error("Fatal programming error in PrintEmf::create_brush at selectobject_set");
    }

    if (fmode != hpolyfillmode) {
        hpolyfillmode = fmode;
        rec = U_EMRSETPOLYFILLMODE_set(fmode);
        if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
            g_error("Fatal programming error in PrintEmf::create_brush at U_EMRSETPOLYdrawmode_set");
        }
    }

    return 0;
}


void PrintEmf::destroy_brush()
{
    char *rec;
    // before an object may be safely deleted it must no longer be selected
    // select in a stock object to deselect this one, the stock object should
    // never be used because we always select in a new one before drawing anythingrestore previous brush, necessary??? Would using a default stock object not work?
    rec = selectobject_set(U_NULL_BRUSH, eht);
    if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
        g_error("Fatal programming error in PrintEmf::destroy_brush at selectobject_set");
    }
    if (hbrush) {
        rec = deleteobject_set(&hbrush, eht);
        if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
            g_error("Fatal programming error in PrintEmf::destroy_brush");
        }
        hbrush = 0;
    }
}


int PrintEmf::create_pen(SPStyle const *style, const Geom::Affine &transform)
{
    U_EXTLOGPEN         *elp;
    U_NUM_STYLEENTRY     n_dash    = 0;
    U_STYLEENTRY        *dash      = NULL;
    char                *rec       = NULL;
    int                  linestyle = U_PS_SOLID;
    int                  linecap   = 0;
    int                  linejoin  = 0;
    uint32_t             pen;
    uint32_t             brushStyle;
    Inkscape::Pixbuf    *pixbuf;
    int                  hatchType;
    U_COLORREF           hatchColor;
    U_COLORREF           bkColor;
    uint32_t             width, height;
    char                *px = NULL;
    char                *rgba_px;
    uint32_t             cbPx = 0;
    uint32_t             colortype;
    PU_RGBQUAD           ct = NULL;
    int                  numCt = 0;
    U_BITMAPINFOHEADER   Bmih;
    PU_BITMAPINFO        Bmi = NULL;

    if (!et) {
        return 0;
    }

    // set a default stroke  in case we can't figure out a better way to do it
    brushStyle = U_BS_SOLID;
    hatchColor = U_RGB(0, 0, 0);
    hatchType  = U_HS_HORIZONTAL;
    bkColor    = U_RGB(0, 0, 0);

    if (style) {
        float rgb[3];

        if (SP_IS_PATTERN(SP_STYLE_STROKE_SERVER(style))) { // must be paint-server
            SPPaintServer *paintserver = style->stroke.value.href->getObject();
            SPPattern *pat = SP_PATTERN(paintserver);
            double dwidth  = pattern_width(pat);
            double dheight = pattern_height(pat);
            width  = dwidth;
            height = dheight;
            brush_classify(pat, 0, &pixbuf, &hatchType, &hatchColor, &bkColor);
            if (pixbuf) {
                brushStyle    = U_BS_DIBPATTERN;
                rgba_px = (char *) pixbuf->pixels(); // Do NOT free this!!!
                colortype = U_BCBM_COLOR32;
                (void) RGBA_to_DIB(&px, &cbPx, &ct, &numCt,  rgba_px,  width, height, width * 4, colortype, 0, 1);
                // Not sure why the next swap is needed because the preceding does it, and the code is identical
                // to that in stretchdibits_set, which does not need this.
                swapRBinRGBA(px, width * height);
                Bmih = bitmapinfoheader_set(width, height, 1, colortype, U_BI_RGB, 0, PXPERMETER, PXPERMETER, numCt, 0);
                Bmi = bitmapinfo_set(Bmih, ct);
            } else { // pattern
                brushStyle    = U_BS_HATCHED;
                if (usebk) { // OPAQUE mode ALWAYS cancels after the next draw, otherwise it would mess up future text output.
                    rec = U_EMRSETBKCOLOR_set(bkColor);
                    if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
                        g_error("Fatal programming error in PrintEmf::create_pen at U_EMRSETBKCOLOR_set");
                    }
                    rec = U_EMRSETBKMODE_set(U_OPAQUE);
                    if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
                        g_error("Fatal programming error in PrintEmf::create_pen at U_EMRSETBKMODE_set");
                    }
                }
                if (hatchType == -1) { // Not a standard hatch, so force it to something
                    hatchType  = U_HS_CROSS;
                    hatchColor = U_RGB(0xFF, 0xC3, 0xC3);
                }
            }
            if (FixPPTPatternAsHatch) {
                if (hatchType == -1) { // image or unclassified
                    brushStyle   = U_BS_HATCHED;
                    hatchType    = U_HS_DIAGCROSS;
                    hatchColor   = U_RGB(0xFF, 0xC3, 0xC3);
                }
            }
        } else if (SP_IS_GRADIENT(SP_STYLE_STROKE_SERVER(style))) { // must be a gradient
            // currently we do not do anything with gradients, the code below has no net effect.

            SPPaintServer *paintserver = style->stroke.value.href->getObject();
            if (SP_IS_LINEARGRADIENT(paintserver)) {
                SPLinearGradient *lg = SP_LINEARGRADIENT(paintserver);

                SP_GRADIENT(lg)->ensureVector(); // when exporting from commandline, vector is not built

                Geom::Point p1(lg->x1.computed, lg->y1.computed);
                Geom::Point p2(lg->x2.computed, lg->y2.computed);

                if (lg->gradientTransform_set) {
                    p1 = p1 * lg->gradientTransform;
                    p2 = p2 * lg->gradientTransform;
                }
                hatchColor = avg_stop_color(lg);
            } else if (SP_IS_RADIALGRADIENT(paintserver)) {
                SPRadialGradient *rg = SP_RADIALGRADIENT(paintserver);

                SP_GRADIENT(rg)->ensureVector(); // when exporting from commandline, vector is not built
                double r = rg->r.computed;

                Geom::Point c(rg->cx.computed, rg->cy.computed);
                Geom::Point xhandle_point(r, 0);
                Geom::Point yhandle_point(0, -r);
                yhandle_point += c;
                xhandle_point += c;
                if (rg->gradientTransform_set) {
                    c             = c             * rg->gradientTransform;
                    yhandle_point = yhandle_point * rg->gradientTransform;
                    xhandle_point = xhandle_point * rg->gradientTransform;
                }
                hatchColor = avg_stop_color(rg);
            } else {
                // default fill
            }
        } else if (style->stroke.isColor()) { // test last, always seems to be set, even for other types above
            sp_color_get_rgb_floatv(&style->stroke.value.color, rgb);
            brushStyle = U_BS_SOLID;
            hatchColor = U_RGB(255 * rgb[0], 255 * rgb[1], 255 * rgb[2]);
            hatchType  = U_HS_SOLIDCLR;
        } else {
            // default fill
        }



        using Geom::X;
        using Geom::Y;

        Geom::Point zero(0, 0);
        Geom::Point one(1, 1);
        Geom::Point p0(zero * transform);
        Geom::Point p1(one * transform);
        Geom::Point p(p1 - p0);

        double scale = sqrt((p[X] * p[X]) + (p[Y] * p[Y])) / sqrt(2);

        if (!style->stroke_width.computed) {
            return 0;   //if width is 0 do not (reset) the pen, it should already be NULL_PEN
        }
        uint32_t linewidth = MAX(1, (uint32_t) round(scale * style->stroke_width.computed * PX2WORLD));

        if (style->stroke_linecap.computed == 0) {
            linecap = U_PS_ENDCAP_FLAT;
        } else if (style->stroke_linecap.computed == 1) {
            linecap = U_PS_ENDCAP_ROUND;
        } else if (style->stroke_linecap.computed == 2) {
            linecap = U_PS_ENDCAP_SQUARE;
        }

        if (style->stroke_linejoin.computed == 0) {
            linejoin = U_PS_JOIN_MITER;
        } else if (style->stroke_linejoin.computed == 1) {
            linejoin = U_PS_JOIN_ROUND;
        } else if (style->stroke_linejoin.computed == 2) {
            linejoin = U_PS_JOIN_BEVEL;
        }

        if (style->stroke_dash.n_dash   &&
                style->stroke_dash.dash) {
            if (FixPPTDashLine) { // will break up line into many smaller lines.  Override gradient if that was set, cannot do both.
                brushStyle = U_BS_SOLID;
                hatchType  = U_HS_HORIZONTAL;
            } else {
                int i = 0;
                while ((linestyle != U_PS_USERSTYLE) && (i < style->stroke_dash.n_dash)) {
                    if (style->stroke_dash.dash[i] > 0.00000001) {
                        linestyle = U_PS_USERSTYLE;
                    }
                    i++;
                }

                if (linestyle == U_PS_USERSTYLE) {
                    n_dash = style->stroke_dash.n_dash;
                    dash = new uint32_t[n_dash];
                    for (i = 0; i < style->stroke_dash.n_dash; i++) {
                        dash[i] = (uint32_t)(style->stroke_dash.dash[i]);
                    }
                }
            }
        }

        elp = extlogpen_set(
                  U_PS_GEOMETRIC | linestyle | linecap | linejoin,
                  linewidth,
                  brushStyle,
                  hatchColor,
                  hatchType,
                  n_dash,
                  dash);

    } else { // if (!style)
        linejoin = 0;
        elp = extlogpen_set(
                  linestyle,
                  1,
                  U_BS_SOLID,
                  U_RGB(0, 0, 0),
                  U_HS_HORIZONTAL,
                  0,
                  NULL);
    }

    rec = extcreatepen_set(&pen, eht,  Bmi, cbPx, px, elp);
    if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
        g_error("Fatal programming error in PrintEmf::create_pen at extcreatepen_set");
    }
    free(elp);
    if (Bmi) {
        free(Bmi);
    }
    if (px) {
        free(px);    // ct will always be NULL
    }

    rec = selectobject_set(pen, eht);
    if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
        g_error("Fatal programming error in PrintEmf::create_pen at selectobject_set");
    }
    hpen = pen;  // need this later for destroy_pen

    if (linejoin == U_PS_JOIN_MITER) {
        float miterlimit = style->stroke_miterlimit.value;  // This is a ratio.

        if (miterlimit < 1) {
            miterlimit = 1;
        }

        rec = U_EMRSETMITERLIMIT_set((uint32_t) miterlimit);
        if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
            g_error("Fatal programming error in PrintEmf::create_pen at U_EMRSETMITERLIMIT_set");
        }
    }

    if (n_dash) {
        delete[] dash;
    }
    return 0;
}

// set the current pen to the stock object NULL_PEN and then delete the defined pen object, if there is one.
void PrintEmf::destroy_pen()
{
    char *rec = NULL;
    // before an object may be safely deleted it must no longer be selected
    // select in a stock object to deselect this one, the stock object should
    // never be used because we always select in a new one before drawing anythingrestore previous brush, necessary??? Would using a default stock object not work?
    rec = selectobject_set(U_NULL_PEN, eht);
    if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
        g_error("Fatal programming error in PrintEmf::destroy_pen at selectobject_set");
    }
    if (hpen) {
        rec = deleteobject_set(&hpen, eht);
        if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
            g_error("Fatal programming error in PrintEmf::destroy_pen");
        }
        hpen = 0;
    }
}


unsigned int PrintEmf::fill(
    Inkscape::Extension::Print * /*mod*/,
    Geom::PathVector const &pathv, Geom::Affine const & /*transform*/, SPStyle const *style,
    Geom::OptRect const &/*pbox*/, Geom::OptRect const &/*dbox*/, Geom::OptRect const &/*bbox*/)
{
    using Geom::X;
    using Geom::Y;

    Geom::Affine tf = m_tr_stack.top();

    use_fill   = true;
    use_stroke = false;

    fill_transform = tf;

    if (create_brush(style, NULL)) { // only happens if the style is a gradient
        /*
            Handle gradients.  Uses modified livarot as 2geom boolops is currently broken.
            Can handle gradients with multiple stops.

            The overlap is needed to avoid antialiasing artifacts when edges are not strictly aligned on pixel boundaries.
            There is an inevitable loss of accuracy saving through an EMF file because of the integer coordinate system.
            Keep the overlap quite large so that loss of accuracy does not remove an overlap.
        */
        destroy_pen();  //this sets the NULL_PEN, otherwise gradient slices may display with boundaries, see longer explanation below
        Geom::Path cutter;
        float      rgb[3];
        U_COLORREF wc, c1, c2;
        FillRule   frb = SPWR_to_LVFR((SPWindRule) style->fill_rule.computed);
        double     doff, doff_base, doff_range;
        double     divisions = 128.0;
        int        nstops;
        int        istop =     1;
        float      opa;                     // opacity at stop

        SPRadialGradient *tg = (SPRadialGradient *)(gv.grad);   // linear/radial are the same here
        nstops = tg->vector.stops.size();
        sp_color_get_rgb_floatv(&tg->vector.stops[0].color, rgb);
        opa    = tg->vector.stops[0].opacity;
        c1     = U_RGBA(255 * rgb[0], 255 * rgb[1], 255 * rgb[2], 255 * opa);
        sp_color_get_rgb_floatv(&tg->vector.stops[nstops - 1].color, rgb);
        opa    = tg->vector.stops[nstops - 1].opacity;
        c2     = U_RGBA(255 * rgb[0], 255 * rgb[1], 255 * rgb[2], 255 * opa);

        doff       = 0.0;
        doff_base  = 0.0;
        doff_range = tg->vector.stops[1].offset;              // next or last stop

        if (gv.mode == DRAW_RADIAL_GRADIENT) {
            Geom::Point xv = gv.p2 - gv.p1;           // X'  vector
            Geom::Point yv = gv.p3 - gv.p1;           // Y'  vector
            Geom::Point xuv = Geom::unit_vector(xv);  // X' unit vector
            double rx = hypot(xv[X], xv[Y]);
            double ry = hypot(yv[X], yv[Y]);
            double range    = fmax(rx, ry);           // length along the gradient
            double step     = range / divisions;      // adequate approximation for gradient
            double overlap  = step / 4.0;             // overlap slices slightly
            double start;
            double stop;
            Geom::PathVector pathvc, pathvr;

            /*  radial gradient might stop part way through the shape, fill with outer color from there to "infinity".
                Do this first so that outer colored ring will overlay it.
            */
            pathvc = center_elliptical_hole_as_SVG_PathV(gv.p1, rx * (1.0 - overlap / range), ry * (1.0 - overlap / range), asin(xuv[Y]));
            pathvr = sp_pathvector_boolop(pathvc, pathv, bool_op_inters, (FillRule) fill_oddEven, frb);
            wc = weight_opacity(c2);
            (void) create_brush(style, &wc);
            print_pathv(pathvr, fill_transform);

            sp_color_get_rgb_floatv(&tg->vector.stops[istop].color, rgb);
            opa = tg->vector.stops[istop].opacity;
            c2 = U_RGBA(255 * rgb[0], 255 * rgb[1], 255 * rgb[2], 255 * opa);

            for (start = 0.0; start < range; start += step, doff += 1. / divisions) {
                stop = start + step + overlap;
                if (stop > range) {
                    stop = range;
                }
                wc = weight_colors(c1, c2, (doff - doff_base) / (doff_range - doff_base));
                (void) create_brush(style, &wc);

                pathvc = center_elliptical_ring_as_SVG_PathV(gv.p1, rx * start / range, ry * start / range, rx * stop / range, ry * stop / range, asin(xuv[Y]));

                pathvr = sp_pathvector_boolop(pathvc, pathv, bool_op_inters, (FillRule) fill_nonZero, frb);
                print_pathv(pathvr, fill_transform);  // show the intersection

                if (doff >= doff_range - doff_base) {
                    istop++;
                    if (istop >= nstops) {
                        continue;    // could happen on a rounding error
                    }
                    doff_base  = doff_range;
                    doff_range = tg->vector.stops[istop].offset;  // next or last stop
                    c1 = c2;
                    sp_color_get_rgb_floatv(&tg->vector.stops[istop].color, rgb);
                    opa = tg->vector.stops[istop].opacity;
                    c2 = U_RGBA(255 * rgb[0], 255 * rgb[1], 255 * rgb[2], 255 * opa);
                }
            }
        } else if (gv.mode == DRAW_LINEAR_GRADIENT) {
            Geom::Point uv  = Geom::unit_vector(gv.p2 - gv.p1);  // unit vector
            Geom::Point puv = uv.cw();                           // perp. to unit vector
            double range    = Geom::distance(gv.p1, gv.p2);      // length along the gradient
            double step     = range / divisions;                 // adequate approximation for gradient
            double overlap  = step / 4.0;                        // overlap slices slightly
            double start;
            double stop;
            Geom::PathVector pathvc, pathvr;

            /* before lower end of gradient, overlap first slice position */
            wc = weight_opacity(c1);
            (void) create_brush(style, &wc);
            pathvc = rect_cutter(gv.p1, uv * (overlap), uv * (-50000.0), puv * 50000.0);
            pathvr = sp_pathvector_boolop(pathvc, pathv, bool_op_inters, (FillRule) fill_nonZero, frb);
            print_pathv(pathvr, fill_transform);

            /* after high end of gradient, overlap last slice poosition */
            wc = weight_opacity(c2);
            (void) create_brush(style, &wc);
            pathvc = rect_cutter(gv.p2, uv * (-overlap), uv * (50000.0), puv * 50000.0);
            pathvr = sp_pathvector_boolop(pathvc, pathv, bool_op_inters, (FillRule) fill_nonZero, frb);
            print_pathv(pathvr, fill_transform);

            sp_color_get_rgb_floatv(&tg->vector.stops[istop].color, rgb);
            opa = tg->vector.stops[istop].opacity;
            c2 = U_RGBA(255 * rgb[0], 255 * rgb[1], 255 * rgb[2], 255 * opa);

            for (start = 0.0; start < range; start += step, doff += 1. / divisions) {
                stop = start + step + overlap;
                if (stop > range) {
                    stop = range;
                }
                pathvc = rect_cutter(gv.p1, uv * start, uv * stop, puv * 50000.0);

                wc = weight_colors(c1, c2, (doff - doff_base) / (doff_range - doff_base));
                (void) create_brush(style, &wc);
                Geom::PathVector pathvr = sp_pathvector_boolop(pathvc, pathv, bool_op_inters, (FillRule) fill_nonZero, frb);
                print_pathv(pathvr, fill_transform);  // show the intersection

                if (doff >= doff_range - doff_base) {
                    istop++;
                    if (istop >= nstops) {
                        continue;    // could happen on a rounding error
                    }
                    doff_base  = doff_range;
                    doff_range = tg->vector.stops[istop].offset;  // next or last stop
                    c1 = c2;
                    sp_color_get_rgb_floatv(&tg->vector.stops[istop].color, rgb);
                    opa = tg->vector.stops[istop].opacity;
                    c2 = U_RGBA(255 * rgb[0], 255 * rgb[1], 255 * rgb[2], 255 * opa);
                }
            }
        } else {
            g_error("Fatal programming error in PrintEmf::fill, invalid gradient type detected");
        }
        use_fill = false;  // gradients handled, be sure stroke does not use stroke and fill
    } else {
        /*
            Inkscape was not calling create_pen for objects with no border.
            This was because it never called stroke() (next method).
            PPT, and presumably others, pick whatever they want for the border if it is not specified, so no border can
            become a visible border.
            To avoid this force the pen to NULL_PEN if we can determine that no pen will be needed after the fill.
        */
        if (style->stroke.noneSet || style->stroke_width.computed == 0.0) {
            destroy_pen();  //this sets the NULL_PEN
        }

        /*  postpone fill in case stroke also required AND all stroke paths closed
            Dashes converted to line segments will "open" a closed path.
        */
        bool all_closed = true;
        for (Geom::PathVector::const_iterator pit = pathv.begin(); pit != pathv.end(); ++pit) {
            for (Geom::Path::const_iterator cit = pit->begin(); cit != pit->end_open(); ++cit) {
                if (pit->end_default() != pit->end_closed()) {
                    all_closed = false;
                }
            }
        }
        if (
            (style->stroke.isNone() || style->stroke.noneSet || style->stroke_width.computed == 0.0) ||
            (style->stroke_dash.n_dash   &&  style->stroke_dash.dash  && FixPPTDashLine)             ||
            !all_closed
        ) {
            print_pathv(pathv, fill_transform);  // do any fills. side effect: clears fill_pathv
            use_fill = false;
        }
    }

    return 0;
}


unsigned int PrintEmf::stroke(
    Inkscape::Extension::Print * /*mod*/,
    Geom::PathVector const &pathv, const Geom::Affine &/*transform*/, const SPStyle *style,
    Geom::OptRect const &/*pbox*/, Geom::OptRect const &/*dbox*/, Geom::OptRect const &/*bbox*/)
{

    char *rec = NULL;
    Geom::Affine tf = m_tr_stack.top();

    use_stroke = true;
    //  use_fill was set in ::fill, if it is needed

    if (create_pen(style, tf)) {
        return 0;
    }

    if (style->stroke_dash.n_dash   &&  style->stroke_dash.dash  && FixPPTDashLine) {
        // convert the path, gets its complete length, and then make a new path with parameter length instead of t
        Geom::Piecewise<Geom::D2<Geom::SBasis> > tmp_pathpw;  // pathv-> sbasis
        Geom::Piecewise<Geom::D2<Geom::SBasis> > tmp_pathpw2; // sbasis using arc length parameter
        Geom::Piecewise<Geom::D2<Geom::SBasis> > tmp_pathpw3; // new (discontinuous) path, composed of dots/dashes
        Geom::Piecewise<Geom::D2<Geom::SBasis> > first_frag;  // first fragment, will be appended at end
        int n_dash = style->stroke_dash.n_dash;
        int i = 0; //dash index
        double tlength;                                       // length of tmp_pathpw
        double slength = 0.0;                                 // start of gragment
        double elength;                                       // end of gragment
        for (unsigned int i = 0; i < pathv.size(); i++) {
            tmp_pathpw.concat(pathv[i].toPwSb());
        }
        tlength = length(tmp_pathpw, 0.1);
        tmp_pathpw2 = arc_length_parametrization(tmp_pathpw);

        // go around the dash array repeatedly until the entire path is consumed (but not beyond).
        while (slength < tlength) {
            elength = slength + style->stroke_dash.dash[i++];
            if (elength > tlength) {
                elength = tlength;
            }
            Geom::Piecewise<Geom::D2<Geom::SBasis> > fragment(portion(tmp_pathpw2, slength, elength));
            if (slength) {
                tmp_pathpw3.concat(fragment);
            } else {
                first_frag = fragment;
            }
            slength = elength;
            slength += style->stroke_dash.dash[i++];  // the gap
            if (i >= n_dash) {
                i = 0;
            }
        }
        tmp_pathpw3.concat(first_frag); // may merge line around start point
        Geom::PathVector out_pathv = Geom::path_from_piecewise(tmp_pathpw3, 0.01);
        print_pathv(out_pathv, tf);
    } else {
        print_pathv(pathv, tf);
    }

    use_stroke = false;
    use_fill   = false;

    if (usebk) { // OPAQUE was set, revert to TRANSPARENT
        usebk = false;
        rec = U_EMRSETBKMODE_set(U_TRANSPARENT);
        if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
            g_error("Fatal programming error in PrintEmf::stroke at U_EMRSETBKMODE_set");
        }
    }

    return 0;
}


// Draws simple_shapes, those with closed EMR_* primitives, like polygons, rectangles and ellipses.
// These use whatever the current pen/brush are and need not be followed by a FILLPATH or STROKEPATH.
// For other paths it sets a few flags and returns.
bool PrintEmf::print_simple_shape(Geom::PathVector const &pathv, const Geom::Affine &transform)
{

    Geom::PathVector pv = pathv_to_linear_and_cubic_beziers(pathv * transform);

    int nodes  = 0;
    int moves  = 0;
    int lines  = 0;
    int curves = 0;
    char *rec  = NULL;

    for (Geom::PathVector::const_iterator pit = pv.begin(); pit != pv.end(); ++pit) {
        moves++;
        nodes++;

        for (Geom::Path::const_iterator cit = pit->begin(); cit != pit->end_open(); ++cit) {
            nodes++;

            if (is_straight_curve(*cit)) {
                lines++;
            } else if (&*cit) {
                curves++;
            }
        }
    }

    if (!nodes) {
        return false;
    }

    U_POINT *lpPoints = new U_POINT[moves + lines + curves * 3];
    int i = 0;

    /**
     * For all Subpaths in the <path>
     */
    for (Geom::PathVector::const_iterator pit = pv.begin(); pit != pv.end(); ++pit) {
        using Geom::X;
        using Geom::Y;

        Geom::Point p0 = pit->initialPoint();

        p0[X] = (p0[X] * PX2WORLD);
        p0[Y] = (p0[Y] * PX2WORLD);

        int32_t const x0 = (int32_t) round(p0[X]);
        int32_t const y0 = (int32_t) round(p0[Y]);

        lpPoints[i].x = x0;
        lpPoints[i].y = y0;
        i = i + 1;

        /**
         * For all segments in the subpath
         */
        for (Geom::Path::const_iterator cit = pit->begin(); cit != pit->end_open(); ++cit) {
            if (is_straight_curve(*cit)) {
                //Geom::Point p0 = cit->initialPoint();
                Geom::Point p1 = cit->finalPoint();

                //p0[X] = (p0[X] * PX2WORLD);
                p1[X] = (p1[X] * PX2WORLD);
                //p0[Y] = (p0[Y] * PX2WORLD);
                p1[Y] = (p1[Y] * PX2WORLD);

                //int32_t const x0 = (int32_t) round(p0[X]);
                //int32_t const y0 = (int32_t) round(p0[Y]);
                int32_t const x1 = (int32_t) round(p1[X]);
                int32_t const y1 = (int32_t) round(p1[Y]);

                lpPoints[i].x = x1;
                lpPoints[i].y = y1;
                i = i + 1;
            } else if (Geom::CubicBezier const *cubic = dynamic_cast<Geom::CubicBezier const *>(&*cit)) {
                std::vector<Geom::Point> points = cubic->points();
                //Geom::Point p0 = points[0];
                Geom::Point p1 = points[1];
                Geom::Point p2 = points[2];
                Geom::Point p3 = points[3];

                //p0[X] = (p0[X] * PX2WORLD);
                p1[X] = (p1[X] * PX2WORLD);
                p2[X] = (p2[X] * PX2WORLD);
                p3[X] = (p3[X] * PX2WORLD);
                //p0[Y] = (p0[Y] * PX2WORLD);
                p1[Y] = (p1[Y] * PX2WORLD);
                p2[Y] = (p2[Y] * PX2WORLD);
                p3[Y] = (p3[Y] * PX2WORLD);

                //int32_t const x0 = (int32_t) round(p0[X]);
                //int32_t const y0 = (int32_t) round(p0[Y]);
                int32_t const x1 = (int32_t) round(p1[X]);
                int32_t const y1 = (int32_t) round(p1[Y]);
                int32_t const x2 = (int32_t) round(p2[X]);
                int32_t const y2 = (int32_t) round(p2[Y]);
                int32_t const x3 = (int32_t) round(p3[X]);
                int32_t const y3 = (int32_t) round(p3[Y]);

                lpPoints[i].x = x1;
                lpPoints[i].y = y1;
                lpPoints[i + 1].x = x2;
                lpPoints[i + 1].y = y2;
                lpPoints[i + 2].x = x3;
                lpPoints[i + 2].y = y3;
                i = i + 3;
            }
        }
    }

    bool done = false;
    bool closed = (lpPoints[0].x == lpPoints[i - 1].x) && (lpPoints[0].y == lpPoints[i - 1].y);
    bool polygon = false;
    bool rectangle = false;
    bool ellipse = false;

    if (moves == 1 && moves + lines == nodes && closed) {
        polygon = true;
        //        if (nodes==5) {                             // disable due to LP Bug 407394
        //            if (lpPoints[0].x == lpPoints[3].x && lpPoints[1].x == lpPoints[2].x &&
        //                lpPoints[0].y == lpPoints[1].y && lpPoints[2].y == lpPoints[3].y)
        //            {
        //                rectangle = true;
        //            }
        //        }
    } else if (moves == 1 && nodes == 5 && moves + curves == nodes && closed) {
        //        if (lpPoints[0].x == lpPoints[1].x && lpPoints[1].x == lpPoints[11].x &&
        //            lpPoints[5].x == lpPoints[6].x && lpPoints[6].x == lpPoints[7].x &&
        //            lpPoints[2].x == lpPoints[10].x && lpPoints[3].x == lpPoints[9].x && lpPoints[4].x == lpPoints[8].x &&
        //            lpPoints[2].y == lpPoints[3].y && lpPoints[3].y == lpPoints[4].y &&
        //            lpPoints[8].y == lpPoints[9].y && lpPoints[9].y == lpPoints[10].y &&
        //            lpPoints[5].y == lpPoints[1].y && lpPoints[6].y == lpPoints[0].y && lpPoints[7].y == lpPoints[11].y)
        //        {                                           // disable due to LP Bug 407394
        //            ellipse = true;
        //        }
    }

    if (polygon || ellipse) {

        if (use_fill && !use_stroke) {  // only fill
            rec = selectobject_set(U_NULL_PEN, eht);
            if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
                g_error("Fatal programming error in PrintEmf::print_simple_shape at selectobject_set pen");
            }
        } else if (!use_fill && use_stroke) { // only stroke
            rec = selectobject_set(U_NULL_BRUSH, eht);
            if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
                g_error("Fatal programming error in PrintEmf::print_simple_shape at selectobject_set brush");
            }
        }

        if (polygon) {
            if (rectangle) {
                U_RECTL rcl = rectl_set((U_POINTL) {
                    lpPoints[0].x, lpPoints[0].y
                }, (U_POINTL) {
                    lpPoints[2].x, lpPoints[2].y
                });
                rec = U_EMRRECTANGLE_set(rcl);
            } else {
                rec = U_EMRPOLYGON_set(U_RCL_DEF, nodes, lpPoints);
            }
        } else if (ellipse) {
            U_RECTL rcl = rectl_set((U_POINTL) {
                lpPoints[6].x, lpPoints[3].y
            }, (U_POINTL) {
                lpPoints[0].x, lpPoints[9].y
            });
            rec = U_EMRELLIPSE_set(rcl);
        }
        if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
            g_error("Fatal programming error in PrintEmf::print_simple_shape at retangle/ellipse/polygon");
        }

        done = true;

        // replace the handle we moved above, assuming there was something set already
        if (use_fill && !use_stroke && hpen) { // only fill
            rec = selectobject_set(hpen, eht);
            if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
                g_error("Fatal programming error in PrintEmf::print_simple_shape at selectobject_set pen");
            }
        } else if (!use_fill && use_stroke && hbrush) { // only stroke
            rec = selectobject_set(hbrush, eht);
            if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
                g_error("Fatal programming error in PrintEmf::print_simple_shape at selectobject_set brush");
            }
        }
    }

    delete[] lpPoints;

    return done;
}

/** Some parts based on win32.cpp by Lauris Kaplinski <lauris@kaplinski.com>.  Was a part of Inkscape
    in the past (or will be in the future?)  Not in current trunk. (4/19/2012)

    Limitations of this code:
    1.  Transparency is lost on export.  (Apparently a limitation of the EMF format.)
    2.  Probably messes up if row stride != w*4
    3.  There is still a small memory leak somewhere, possibly in a pixbuf created in a routine
        that calls this one and passes px, but never removes the rest of the pixbuf.  The first time
        this is called it leaked 5M (in one test) and each subsequent call leaked around 200K more.
        If this routine is reduced to
          if(1)return(0);
        and called for a single 1280 x 1024 image then the program leaks 11M per call, or roughly the
        size of two bitmaps.
*/

unsigned int PrintEmf::image(
    Inkscape::Extension::Print * /* module */,  /** not used */
    unsigned char *rgba_px,   /** array of pixel values, Gdk::Pixbuf bitmap format */
    unsigned int w,      /** width of bitmap */
    unsigned int h,      /** height of bitmap */
    unsigned int rs,     /** row stride (normally w*4) */
    Geom::Affine const &/*tf_ignore*/,  /** WRONG affine transform, use the one from m_tr_stack */
    SPStyle const *style)  /** provides indirect link to image object */
{
    double x1, y1, dw, dh;
    char *rec = NULL;
    Geom::Affine tf = m_tr_stack.top();

    rec = U_EMRSETSTRETCHBLTMODE_set(U_COLORONCOLOR);
    if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
        g_error("Fatal programming error in PrintEmf::image at EMRHEADER");
    }

    x1 = atof(style->object->getAttribute("x"));
    y1 = atof(style->object->getAttribute("y"));
    dw = atof(style->object->getAttribute("width"));
    dh = atof(style->object->getAttribute("height"));
    Geom::Point pLL(x1, y1);
    Geom::Point pLL2 = pLL * tf;  //location of LL corner in Inkscape coordinates

    char                *px;
    uint32_t             cbPx;
    uint32_t             colortype;
    PU_RGBQUAD           ct;
    int                  numCt;
    U_BITMAPINFOHEADER   Bmih;
    PU_BITMAPINFO        Bmi;
    colortype = U_BCBM_COLOR32;
    (void) RGBA_to_DIB(&px, &cbPx, &ct, &numCt, (char *) rgba_px,  w, h, w * 4, colortype, 0, 1);
    Bmih = bitmapinfoheader_set(w, h, 1, colortype, U_BI_RGB, 0, PXPERMETER, PXPERMETER, numCt, 0);
    Bmi = bitmapinfo_set(Bmih, ct);

    U_POINTL Dest  = pointl_set(round(pLL2[Geom::X] * PX2WORLD), round(pLL2[Geom::Y] * PX2WORLD));
    U_POINTL cDest = pointl_set(round(dw * PX2WORLD), round(dh * PX2WORLD));
    U_POINTL Src   = pointl_set(0, 0);
    U_POINTL cSrc  = pointl_set(w, h);
    if (!FixImageRot) { /* Rotate images - some programs cannot read them in correctly if they are rotated */
        tf[4] = tf[5] = 0.0;  // get rid of the offset in the transform
        Geom::Point pLL2prime = pLL2 * tf;
        U_XFORM tmpTransform;
        tmpTransform.eM11 =  tf[0];
        tmpTransform.eM12 =  tf[1];
        tmpTransform.eM21 =  tf[2];
        tmpTransform.eM22 =  tf[3];
        tmpTransform.eDx  = (pLL2[Geom::X] - pLL2prime[Geom::X]) * PX2WORLD;   //map pLL2 (now in EMF coordinates) back onto itself after the rotation
        tmpTransform.eDy  = (pLL2[Geom::Y] - pLL2prime[Geom::Y]) * PX2WORLD;

        rec = U_EMRSAVEDC_set();
        if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
            g_error("Fatal programming error in PrintEmf::image at U_EMRSAVEDC_set");
        }

        rec = U_EMRMODIFYWORLDTRANSFORM_set(tmpTransform, U_MWT_LEFTMULTIPLY);
        if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
            g_error("Fatal programming error in PrintEmf::image at EMRMODIFYWORLDTRANSFORM");
        }
    }
    rec = U_EMRSTRETCHDIBITS_set(
              U_RCL_DEF,           //! Bounding rectangle in device units
              Dest,                //! Destination UL corner in logical units
              cDest,               //! Destination W & H in logical units
              Src,                 //! Source UL corner in logical units
              cSrc,                //! Source W & H in logical units
              U_DIB_RGB_COLORS,    //! DIBColors Enumeration
              U_SRCCOPY,           //! RasterOPeration Enumeration
              Bmi,                 //! (Optional) bitmapbuffer (U_BITMAPINFO section)
              h * rs,              //! size in bytes of px
              px                   //! (Optional) bitmapbuffer (U_BITMAPINFO section)
          );
    if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
        g_error("Fatal programming error in PrintEmf::image at U_EMRSTRETCHDIBITS_set");
    }
    free(px);
    free(Bmi);
    if (numCt) {
        free(ct);
    }

    if (!FixImageRot) {
        rec = U_EMRRESTOREDC_set(-1);
        if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
            g_error("Fatal programming error in PrintEmf::image at U_EMRRESTOREDC_set");
        }
    }

    return 0;
}

// may also be called with a simple_shape or an empty path, whereupon it just returns without doing anything
unsigned int PrintEmf::print_pathv(Geom::PathVector const &pathv, const Geom::Affine &transform)
{
    Geom::Affine tf = transform;
    char *rec = NULL;

    simple_shape = print_simple_shape(pathv, tf);
    if (simple_shape || pathv.empty()) {
        if (use_fill) {
            destroy_brush();    // these must be cleared even if nothing is drawn or hbrush,hpen fill up
        }
        if (use_stroke) {
            destroy_pen();
        }
        return TRUE;
    }

    /*  inkscape to EMF scaling is done below, but NOT the rotation/translation transform,
        that is handled by the EMF MODIFYWORLDTRANSFORM record
    */

    Geom::PathVector pv = pathv_to_linear_and_cubic_beziers(pathv * tf);

    rec = U_EMRBEGINPATH_set();
    if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
        g_error("Fatal programming error in PrintEmf::print_pathv at U_EMRBEGINPATH_set");
    }

    /**
     * For all Subpaths in the <path>
     */
    for (Geom::PathVector::const_iterator pit = pv.begin(); pit != pv.end(); ++pit) {
        using Geom::X;
        using Geom::Y;


        Geom::Point p0 = pit->initialPoint();

        p0[X] = (p0[X] * PX2WORLD);
        p0[Y] = (p0[Y] * PX2WORLD);

        U_POINTL ptl = pointl_set((int32_t) round(p0[X]), (int32_t) round(p0[Y]));
        rec = U_EMRMOVETOEX_set(ptl);
        if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
            g_error("Fatal programming error in PrintEmf::print_pathv at U_EMRMOVETOEX_set");
        }

        /**
         * For all segments in the subpath
         */
        for (Geom::Path::const_iterator cit = pit->begin(); cit != pit->end_open(); ++cit) {
            if (is_straight_curve(*cit)) {
                //Geom::Point p0 = cit->initialPoint();
                Geom::Point p1 = cit->finalPoint();

                //p0[X] = (p0[X] * PX2WORLD);
                p1[X] = (p1[X] * PX2WORLD);
                //p0[Y] = (p0[Y] * PX2WORLD);
                p1[Y] = (p1[Y] * PX2WORLD);

                //int32_t const x0 = (int32_t) round(p0[X]);
                //int32_t const y0 = (int32_t) round(p0[Y]);

                ptl = pointl_set((int32_t) round(p1[X]), (int32_t) round(p1[Y]));
                rec = U_EMRLINETO_set(ptl);
                if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
                    g_error("Fatal programming error in PrintEmf::print_pathv at U_EMRLINETO_set");
                }
            } else if (Geom::CubicBezier const *cubic = dynamic_cast<Geom::CubicBezier const *>(&*cit)) {
                std::vector<Geom::Point> points = cubic->points();
                //Geom::Point p0 = points[0];
                Geom::Point p1 = points[1];
                Geom::Point p2 = points[2];
                Geom::Point p3 = points[3];

                //p0[X] = (p0[X] * PX2WORLD);
                p1[X] = (p1[X] * PX2WORLD);
                p2[X] = (p2[X] * PX2WORLD);
                p3[X] = (p3[X] * PX2WORLD);
                //p0[Y] = (p0[Y] * PX2WORLD);
                p1[Y] = (p1[Y] * PX2WORLD);
                p2[Y] = (p2[Y] * PX2WORLD);
                p3[Y] = (p3[Y] * PX2WORLD);

                //int32_t const x0 = (int32_t) round(p0[X]);
                //int32_t const y0 = (int32_t) round(p0[Y]);
                int32_t const x1 = (int32_t) round(p1[X]);
                int32_t const y1 = (int32_t) round(p1[Y]);
                int32_t const x2 = (int32_t) round(p2[X]);
                int32_t const y2 = (int32_t) round(p2[Y]);
                int32_t const x3 = (int32_t) round(p3[X]);
                int32_t const y3 = (int32_t) round(p3[Y]);

                U_POINTL pt[3];
                pt[0].x = x1;
                pt[0].y = y1;
                pt[1].x = x2;
                pt[1].y = y2;
                pt[2].x = x3;
                pt[2].y = y3;

                rec = U_EMRPOLYBEZIERTO_set(U_RCL_DEF, 3, pt);
                if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
                    g_error("Fatal programming error in PrintEmf::print_pathv at U_EMRPOLYBEZIERTO_set");
                }
            } else {
                g_warning("logical error, because pathv_to_linear_and_cubic_beziers was used");
            }
        }

        if (pit->end_default() == pit->end_closed()) {  // there may be multiples of this on a single path
            rec = U_EMRCLOSEFIGURE_set();
            if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
                g_error("Fatal programming error in PrintEmf::print_pathv at U_EMRCLOSEFIGURE_set");
            }
        }

    }

    rec = U_EMRENDPATH_set();  // there may be only be one of these on a single path
    if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
        g_error("Fatal programming error in PrintEmf::print_pathv at U_EMRENDPATH_set");
    }

    // explicit FILL/STROKE commands are needed for each sub section of the path
    if (use_fill && !use_stroke) {
        rec = U_EMRFILLPATH_set(U_RCL_DEF);
        if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
            g_error("Fatal programming error in PrintEmf::fill at U_EMRFILLPATH_set");
        }
    } else if (use_fill && use_stroke) {
        rec  = U_EMRSTROKEANDFILLPATH_set(U_RCL_DEF);
        if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
            g_error("Fatal programming error in PrintEmf::stroke at U_EMRSTROKEANDFILLPATH_set");
        }
    } else if (!use_fill && use_stroke) {
        rec  = U_EMRSTROKEPATH_set(U_RCL_DEF);
        if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
            g_error("Fatal programming error in PrintEmf::stroke at U_EMRSTROKEPATH_set");
        }
    }

    // clean out brush and pen, but only after all parts of the draw complete
    if (use_fill) {
        destroy_brush();
    }
    if (use_stroke) {
        destroy_pen();
    }

    return TRUE;
}


unsigned int PrintEmf::text(Inkscape::Extension::Print * /*mod*/, char const *text, Geom::Point const &p,
                            SPStyle const *const style)
{
    if (!et) {
        return 0;
    }

    char *rec = NULL;
    int ccount, newfont;
    int fix90n = 0;
    uint32_t hfont = 0;
    Geom::Affine tf = m_tr_stack.top();
    double rot = -1800.0 * std::atan2(tf[1], tf[0]) / M_PI; // 0.1 degree rotation,  - sign for MM_TEXT
    double rotb = -std::atan2(tf[1], tf[0]);  // rotation for baseline offset for superscript/subscript, used below
    double dx, dy;
    double ky;

    // the dx array is smuggled in like: text<nul>w1 w2 w3 ...wn<nul><nul>, where the widths are floats 7 characters wide, including the space
    int ndx, rtl;
    uint32_t *adx;
    smuggle_adxkyrtl_out(text, &adx, &ky, &rtl, &ndx, PX2WORLD * std::min(tf.expansionX(), tf.expansionY())); // side effect: free() adx

    uint32_t textalignment;
    if (rtl > 0) {
        textalignment = U_TA_BASELINE | U_TA_LEFT;
    } else {
        textalignment = U_TA_BASELINE | U_TA_RIGHT | U_TA_RTLREADING;
    }
    if (textalignment != htextalignment) {
        htextalignment = textalignment;
        rec = U_EMRSETTEXTALIGN_set(textalignment);
        if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
            g_error("Fatal programming error in PrintEmf::text at U_EMRSETTEXTALIGN_set");
        }
    }

    char *text2 = strdup(text);  // because U_Utf8ToUtf16le calls iconv which does not like a const char *
    uint16_t *unicode_text = U_Utf8ToUtf16le(text2, 0, NULL);
    free(text2);
    //translates Unicode to NonUnicode, if possible.  If any translate, all will, and all to
    //the same font, because of code in Layout::print
    UnicodeToNon(unicode_text, &ccount, &newfont);

    //PPT gets funky with text within +-1 degree of a multiple of 90, but only for SOME fonts.Snap those to the central value
    //Some funky ones:  Arial, Times New Roman
    //Some not funky ones: Symbol and Verdana.
    //Without a huge table we cannot catch them all, so just the most common problem ones.
    FontfixParams params;

    if (FixPPTCharPos) {
        switch (newfont) {
        case CVTSYM:
            _lookup_ppt_fontfix("Convert To Symbol", params);
            break;
        case CVTZDG:
            _lookup_ppt_fontfix("Convert To Zapf Dingbats", params);
            break;
        case CVTWDG:
            _lookup_ppt_fontfix("Convert To Wingdings", params);
            break;
        default:  //also CVTNON
            _lookup_ppt_fontfix(style->text->font_family.value, params);
            break;
        }
        if (params.f2 != 0 || params.f3 != 0) {
            int irem = ((int) round(rot)) % 900 ;
            if (irem <= 9 && irem >= -9) {
                fix90n = 1; //assume vertical
                rot  = (double)(((int) round(rot)) - irem);
                rotb =  rot * M_PI / 1800.0;
                if (abs(rot) == 900.0) {
                    fix90n = 2;
                }
            }
        }
    }

    /*  Note that text font sizes are stored into the EMF as fairly small integers and that limits their precision.
        The EMF output files produced here have been designed so that the integer valued pt sizes
        land right on an integer value in the EMF file, so those are exact.  However, something like 18.1 pt will be
        somewhat off, so that when it is read back in it becomes 18.11 pt.  (For instance.)
    */
    int textheight = round(-style->font_size.computed * PX2WORLD * std::min(tf.expansionX(), tf.expansionY()));

    if (!hfont) {
        // Get font face name.  Use changed font name if unicode mapped to one
        // of the special fonts.
        uint16_t *wfacename;
        if (!newfont) {
            wfacename = U_Utf8ToUtf16le(style->text->font_family.value, 0, NULL);
        } else {
            wfacename = U_Utf8ToUtf16le(FontName(newfont), 0, NULL);
        }

        // Scale the text to the minimum stretch. (It tends to stay within bounding rectangles even if
        // it was streteched asymmetrically.)  Few applications support text from EMF which is scaled
        // differently by height/width, so leave lfWidth alone.

        U_LOGFONT lf = logfont_set(
                           textheight,
                           0,
                           round(rot),
                           round(rot),
                           _translate_weight(style->font_weight.computed),
                           (style->font_style.computed == SP_CSS_FONT_STYLE_ITALIC),
                           style->text_decoration_line.underline,
                           style->text_decoration_line.line_through,
                           U_DEFAULT_CHARSET,
                           U_OUT_DEFAULT_PRECIS,
                           U_CLIP_DEFAULT_PRECIS,
                           U_DEFAULT_QUALITY,
                           U_DEFAULT_PITCH | U_FF_DONTCARE,
                           wfacename);
        free(wfacename);

        rec  = extcreatefontindirectw_set(&hfont, eht, (char *) &lf, NULL);
        if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
            g_error("Fatal programming error in PrintEmf::text at extcreatefontindirectw_set");
        }
    }

    rec = selectobject_set(hfont, eht);
    if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
        g_error("Fatal programming error in PrintEmf::text at selectobject_set");
    }

    float rgb[3];
    sp_color_get_rgb_floatv(&style->fill.value.color, rgb);
    // only change the text color when it needs to be changed
    if (memcmp(htextcolor_rgb, rgb, 3 * sizeof(float))) {
        memcpy(htextcolor_rgb, rgb, 3 * sizeof(float));
        rec = U_EMRSETTEXTCOLOR_set(U_RGB(255 * rgb[0], 255 * rgb[1], 255 * rgb[2]));
        if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
            g_error("Fatal programming error in PrintEmf::text at U_EMRSETTEXTCOLOR_set");
        }
    }

    Geom::Point p2 = p * tf;

    //Handle super/subscripts and vertical kerning
    /*  Previously used this, but vertical kerning was not supported
        p2[Geom::X] -= style->baseline_shift.computed * std::sin( rotb );
        p2[Geom::Y] -= style->baseline_shift.computed * std::cos( rotb );
    */
    p2[Geom::X] += ky * std::sin(rotb);
    p2[Geom::Y] += ky * std::cos(rotb);

    //Conditionally handle compensation for PPT EMF import bug (affects PPT 2003-2010, at least)
    if (FixPPTCharPos) {
        if (fix90n == 1) { //vertical
            dx = 0.0;
            dy = params.f3 * style->font_size.computed * std::cos(rotb);
        } else if (fix90n == 2) { //horizontal
            dx = params.f2 * style->font_size.computed * std::sin(rotb);
            dy = 0.0;
        } else {
            dx = params.f1 * style->font_size.computed * std::sin(rotb);
            dy = params.f1 * style->font_size.computed * std::cos(rotb);
        }
        p2[Geom::X] += dx;
        p2[Geom::Y] += dy;
    }

    p2[Geom::X] = (p2[Geom::X] * PX2WORLD);
    p2[Geom::Y] = (p2[Geom::Y] * PX2WORLD);

    int32_t const xpos = (int32_t) round(p2[Geom::X]);
    int32_t const ypos = (int32_t) round(p2[Geom::Y]);


    // The number of characters in the string is a bit fuzzy.  ndx, the number of entries in adx is
    // the number of VISIBLE characters, since some may combine from the UTF (8 originally,
    // now 16) encoding.  Conversely strlen() or wchar16len() would give the absolute number of
    // encoding characters.  Unclear if emrtext wants the former or the latter but for now assume the former.

    //    This is currently being smuggled in from caller as part of text, works
    //    MUCH better than the fallback hack below
    //    uint32_t *adx = dx_set(textheight,  U_FW_NORMAL, slen);  // dx is needed, this makes one up
    char *rec2;
    if (rtl > 0) {
        rec2 = emrtext_set((U_POINTL) {
            xpos, ypos
        }, ndx, 2, unicode_text, U_ETO_NONE, U_RCL_DEF, adx);
    } else { // RTL text, U_TA_RTLREADING should be enough, but set this one too just in case
        rec2 = emrtext_set((U_POINTL) {
            xpos, ypos
        }, ndx, 2, unicode_text, U_ETO_RTLREADING, U_RCL_DEF, adx);
    }
    free(unicode_text);
    free(adx);
    rec = U_EMREXTTEXTOUTW_set(U_RCL_DEF, U_GM_COMPATIBLE, 1.0, 1.0, (PU_EMRTEXT)rec2);
    free(rec2);
    if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
        g_error("Fatal programming error in PrintEmf::text at U_EMREXTTEXTOUTW_set");
    }

    // Must deselect an object before deleting it.  Put the default font (back) in.
    rec = selectobject_set(U_DEVICE_DEFAULT_FONT, eht);
    if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
        g_error("Fatal programming error in PrintEmf::text at selectobject_set");
    }

    if (hfont) {
        rec = deleteobject_set(&hfont, eht);
        if (!rec || emf_append((PU_ENHMETARECORD)rec, et, U_REC_FREE)) {
            g_error("Fatal programming error in PrintEmf::text at deleteobject_set");
        }
    }

    return 0;
}

void PrintEmf::init(void)
{
    _load_ppt_fontfix_data();

    /* EMF print */
    Inkscape::Extension::build_from_mem(
        "<inkscape-extension xmlns=\"" INKSCAPE_EXTENSION_URI "\">\n"
        "<name>Enhanced Metafile Print</name>\n"
        "<id>org.inkscape.print.emf</id>\n"
        "<param name=\"destination\" type=\"string\"></param>\n"
        "<param name=\"textToPath\" type=\"boolean\">true</param>\n"
        "<param name=\"pageBoundingBox\" type=\"boolean\">true</param>\n"
        "<param name=\"FixPPTCharPos\" type=\"boolean\">false</param>\n"
        "<param name=\"FixPPTDashLine\" type=\"boolean\">false</param>\n"
        "<param name=\"FixPPTGrad2Polys\" type=\"boolean\">false</param>\n"
        "<param name=\"FixPPTPatternAsHatch\" type=\"boolean\">false</param>\n"
        "<param name=\"FixImageRot\" type=\"boolean\">false</param>\n"
        "<print/>\n"
        "</inkscape-extension>", new PrintEmf());

    return;
}

}  /* namespace Internal */
}  /* namespace Extension */
}  /* namespace Inkscape */


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
