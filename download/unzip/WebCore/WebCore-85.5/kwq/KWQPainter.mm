/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "KWQPainter.h"
#import "KWQWidget.h"
#import "KWQFontMetrics.h"
#import "KWQPixmap.h"
#import "KWQPtrStack.h"
#import "KWQPointArray.h"
#import "KWQPaintDevice.h"
#import "KWQPrinter.h"

#import "KWQAssertions.h"

#import "WebCoreImageRenderer.h"
#import "WebCoreTextRenderer.h"
#import "WebCoreTextRendererFactory.h"

struct QPState {
    QPState() : paintingDisabled(false) { }
    QFont font;
    QPen pen;
    QBrush brush;
    QRegion clip;
    bool paintingDisabled;
};

struct QPainterPrivate {
    QPainterPrivate() : textRenderer(0) { }
    ~QPainterPrivate() { [textRenderer release]; }
    QPState state;
    QPtrStack<QPState> stack;
    id <WebCoreTextRenderer> textRenderer;
    QFont textRendererFont;
};

QPainter::QPainter() : data(new QPainterPrivate), _isForPrinting(false), _usesInactiveTextBackgroundColor(false)
{
}

QPainter::QPainter(bool forPrinting) : data(new QPainterPrivate), _isForPrinting(forPrinting), _usesInactiveTextBackgroundColor(false)
{
}

QPainter::~QPainter()
{
    delete data;
}

QPaintDevice *QPainter::device() const
{
    static QPrinter printer;
    static QPaintDevice screen;
    return _isForPrinting ? &printer : &screen;
}

const QFont &QPainter::font() const
{
    return data->state.font;
}

void QPainter::setFont(const QFont &aFont)
{
    data->state.font = aFont;
}

QFontMetrics QPainter::fontMetrics() const
{
    return data->state.font;
}

const QPen &QPainter::pen() const
{
    return data->state.pen;
}

void QPainter::setPen(const QPen &pen)
{
    data->state.pen = pen;
}

void QPainter::setPen(PenStyle style)
{
    data->state.pen.setStyle(style);
    data->state.pen.setColor(Qt::black);
    data->state.pen.setWidth(0);
}

void QPainter::setPen(QRgb rgb)
{
    data->state.pen.setStyle(SolidLine);
    data->state.pen.setColor(rgb);
    data->state.pen.setWidth(0);
}

void QPainter::setBrush(const QBrush &brush)
{
    data->state.brush = brush;
}

void QPainter::setBrush(BrushStyle style)
{
    data->state.brush.setStyle(style);
    data->state.brush.setColor(Qt::black);
}

void QPainter::setBrush(QRgb rgb)
{
    data->state.brush.setStyle(SolidPattern);
    data->state.brush.setColor(rgb);
}

const QBrush &QPainter::brush() const
{
    return data->state.brush;
}

QRect QPainter::xForm(const QRect &aRect) const
{
    // No difference between device and model coords, so the identity transform is ok.
    return aRect;
}

void QPainter::save()
{
    data->stack.push(new QPState(data->state));

    [NSGraphicsContext saveGraphicsState]; 
}

void QPainter::restore()
{
    if (data->stack.isEmpty()) {
        ERROR("ERROR void QPainter::restore() stack is empty");
	return;
    }
    QPState *ps = data->stack.pop();
    data->state = *ps;
    delete ps;

    [NSGraphicsContext restoreGraphicsState];
}

// Draws a filled rectangle with a stroked border.
void QPainter::drawRect(int x, int y, int w, int h)
{
    if (data->state.paintingDisabled)
        return;
        
    if (data->state.brush.style() != NoBrush) {
        _setColorFromBrush();
        
        NSRectFillUsingOperation (NSMakeRect(x,y,w,h), NSCompositeSourceOver);
    }
    if (data->state.pen.style() != NoPen) {
        _setColorFromPen();
        NSFrameRect(NSMakeRect(x, y, w, h));
    }
}

void QPainter::_setColorFromBrush()
{
    [data->state.brush.color().getNSColor() set];
}

void QPainter::_setColorFromPen()
{
    [data->state.pen.color().getNSColor() set];
}

// This is only used to draw borders.
void QPainter::drawLine(int x1, int y1, int x2, int y2)
{
    if (data->state.paintingDisabled)
        return;
        
    PenStyle penStyle = data->state.pen.style();
    if (penStyle == NoPen)
        return;
    float width = data->state.pen.width();
    if (width < 1)
        width = 1;

    NSPoint p1 = NSMakePoint(x1, y1);
    NSPoint p2 = NSMakePoint(x2, y2);
    
    // For odd widths, we add in 0.5 to the appropriate x/y so that the float arithmetic
    // works out.  For example, with a border width of 3, KHTML will pass us (y1+y2)/2, e.g.,
    // (50+53)/2 = 103/2 = 51 when we want 51.5.  It is always true that an even width gave
    // us a perfect position, but an odd width gave us a position that is off by exactly 0.5.
    if (penStyle == DotLine || penStyle == DashLine) {
        if (x1 == x2) {
            p1.y += width;
            p2.y -= width;
        }
        else {
            p1.x += width;
            p2.x -= width;
        }
    }
    
    if (((int)width)%2) {
        if (x1 == x2) {
            // We're a vertical line.  Adjust our x.
            p1.x += 0.5;
            p2.x += 0.5;
        }
        else {
            // We're a horizontal line. Adjust our y.
            p1.y += 0.5;
            p2.y += 0.5;
        }
    }
    
    NSBezierPath *path = [[NSBezierPath alloc] init];
    [path setLineWidth:width];

    int patWidth = 0;
    switch (penStyle) {
    case NoPen:
    case SolidLine:
        break;
    case DotLine:
        patWidth = (int)width;
        break;
    case DashLine:
        patWidth = 3*(int)width;
        break;
    }

    _setColorFromPen();
    NSGraphicsContext *graphicsContext = [NSGraphicsContext currentContext];
    BOOL flag = [graphicsContext shouldAntialias];
    [graphicsContext setShouldAntialias: NO];
    
    if (patWidth) {
        // Do a rect fill of our endpoints.  This ensures we always have the
        // appearance of being a border.  We then draw the actual dotted/dashed line.
        if (x1 == x2) {
            NSRectFill(NSMakeRect(p1.x-width/2, p1.y-width, width, width));
            NSRectFill(NSMakeRect(p2.x-width/2, p2.y, width, width));
        }
        else {
            NSRectFill(NSMakeRect(p1.x-width, p1.y-width/2, width, width));
            NSRectFill(NSMakeRect(p2.x, p2.y-width/2, width, width));
        }
        
        // Example: 80 pixels with a width of 30 pixels.
        // Remainder is 20.  The maximum pixels of line we could paint
        // will be 50 pixels.
        int distance = ((x1 == x2) ? (y2 - y1) : (x2 - x1)) - 2*(int)width;
        int remainder = distance%patWidth;
        int coverage = distance-remainder;
        int numSegments = coverage/patWidth;

        float patternOffset = 0;
        // Special case 1px dotted borders for speed.
        if (patWidth == 1)
            patternOffset = 1.0;
        else {
            bool evenNumberOfSegments = numSegments%2 == 0;
            if (remainder)
                evenNumberOfSegments = !evenNumberOfSegments;
            if (evenNumberOfSegments) {
                if (remainder) {
                    patternOffset += patWidth - remainder;
                    patternOffset += remainder/2;
                }
                else
                    patternOffset = patWidth/2;
            }
            else if (!evenNumberOfSegments) {
                if (remainder)
                    patternOffset = (patWidth - remainder)/2;
            }
        }
        
        const float dottedLine[2] = { patWidth, patWidth };
        [path setLineDash:dottedLine count:2 phase:patternOffset];
    }
    
    [path moveToPoint:p1];
    [path lineToPoint:p2];

    [path stroke];
    
    [path release];

    [graphicsContext setShouldAntialias: flag];
}


// This method is only used to draw the little circles used in lists.
void QPainter::drawEllipse(int x, int y, int w, int h)
{
    if (data->state.paintingDisabled)
        return;
        
    NSBezierPath *path = [NSBezierPath bezierPathWithOvalInRect: NSMakeRect (x, y, w, h)];
    
    if (data->state.brush.style() != NoBrush) {
        _setColorFromBrush();
        [path fill];
    }
    if (data->state.pen.style() != NoPen) {
        _setColorFromPen();
        [path stroke];
    }
}


// Only supports arc on circles.  That's all khtml needs.
void QPainter::drawArc (int x, int y, int w, int h, int a, int alen)
{
    if (data->state.paintingDisabled)
        return;
        
    if (data->state.pen.style() != NoPen) {
        if (w != h) {
            ERROR("only supports drawing arcs on a circle");
        }
        
        NSBezierPath *path = [[NSBezierPath alloc] init];
        float fa = (float)a / 16;
        float falen =  fa + (float)alen / 16;
        [path appendBezierPathWithArcWithCenter:NSMakePoint(x + w / 2, y + h / 2) 
                                         radius:(float)w / 2 
                                     startAngle:-fa
                                       endAngle:-falen
                                      clockwise:YES];
    
        _setColorFromPen();
        [path stroke];
        [path release];
    }
}

void QPainter::drawLineSegments(const QPointArray &points, int index, int nlines)
{
    if (data->state.paintingDisabled)
        return;
    
    _drawPoints (points, 0, index, nlines, false);
}

void QPainter::drawPolyline(const QPointArray &points, int index, int npoints)
{
    _drawPoints (points, 0, index, npoints, false);
}

void QPainter::drawPolygon(const QPointArray &points, bool winding, int index, 
    int npoints)
{
    _drawPoints (points, winding, index, npoints, true);
}

void QPainter::drawConvexPolygon(const QPointArray &points)
{
    _drawPoints (points, false, 0, -1, true);
}

void QPainter::_drawPoints (const QPointArray &_points, bool winding, int index, int _npoints, bool fill)
{
    if (data->state.paintingDisabled)
        return;
        
    int npoints = _npoints != -1 ? _npoints : _points.size()-index;
    NSPoint points[npoints];
    for (int i = 0; i < npoints; i++) {
        points[i].x = _points[index+i].x();
        points[i].y = _points[index+i].y();
    }
            
    NSBezierPath *path = [[NSBezierPath alloc] init];
    [path appendBezierPathWithPoints:&points[0] count:npoints];
    [path closePath]; // Qt always closes the path.  Determined empirically.
    
    if (fill && data->state.brush.style() != NoBrush) {
        [path setWindingRule:winding ? NSNonZeroWindingRule : NSEvenOddWindingRule];
        _setColorFromBrush();
        [path fill];
    }

    if (data->state.pen.style() != NoPen) {
        _setColorFromPen();
        [path stroke];
    }
    
    [path release];
}


void QPainter::drawPixmap(const QPoint &p, const QPixmap &pix)
{        
    drawPixmap(p.x(), p.y(), pix);
}


void QPainter::drawPixmap(const QPoint &p, const QPixmap &pix, const QRect &r)
{
    drawPixmap(p.x(), p.y(), pix, r.x(), r.y(), r.width(), r.height());
}

void QPainter::drawPixmap( int x, int y, const QPixmap &pixmap,
                           int sx, int sy, int sw, int sh )
{
    if (data->state.paintingDisabled)
        return;
        
    if (sw == -1)
        sw = pixmap.width();
    if (sh == -1)
        sh = pixmap.height();
    
    [pixmap.imageRenderer beginAnimationInRect:NSMakeRect(x, y, sw, sh)
                                      fromRect:NSMakeRect(sx, sy, sw, sh)];
}

void QPainter::drawTiledPixmap( int x, int y, int w, int h,
				const QPixmap &pixmap, int sx, int sy )
{
    if (data->state.paintingDisabled)
        return;
    
    [pixmap.imageRenderer tileInRect:NSMakeRect(x, y, w, h) fromPoint:NSMakePoint(sx, sy)];
}

void QPainter::_updateRenderer(NSString **families)
{
    if (data->textRenderer == 0 || data->state.font != data->textRendererFont) {
        data->textRendererFont = data->state.font;
        id <WebCoreTextRenderer> oldRenderer = data->textRenderer;
        data->textRenderer = [[[WebCoreTextRendererFactory sharedFactory]
            rendererWithFont:data->textRendererFont.getNSFont()
            usingPrinterFont:data->textRendererFont.isPrinterFont()] retain];
        [oldRenderer release];
    }
}
    
void QPainter::drawText(int x, int y, int, int, int alignmentFlags, const QString &qstring)
{
    if (data->state.paintingDisabled)
        return;
        
    // Avoid allocations, use stack array to pass font families.  Normally these
    // css fallback lists are small <= 3.
    CREATE_FAMILY_ARRAY(data->state.font, families);

    _updateRenderer(families);
    
    const UniChar* str = (const UniChar*)qstring.unicode();
    if (alignmentFlags & Qt::AlignRight)
        x -= ROUND_TO_INT([data->textRenderer floatWidthForCharacters:(const UniChar *)str stringLength:qstring.length() fromCharacterPosition:0 numberOfCharacters:qstring.length() withPadding: 0 applyRounding:YES attemptFontSubstitution: YES widths: 0 letterSpacing: 0 wordSpacing: 0 smallCaps: false fontFamilies: families]);
     
    [data->textRenderer drawCharacters:str stringLength:qstring.length()
        fromCharacterPosition:0 
        toCharacterPosition:qstring.length() 
        atPoint:NSMakePoint(x, y)
        withPadding: 0
        withTextColor:data->state.pen.color().getNSColor() 
        backgroundColor:nil
        rightToLeft: false
        letterSpacing: 0
        wordSpacing: 0
        smallCaps: false
        fontFamilies: families];
}

void QPainter::drawText(int x, int y, const QChar *str, int len, int from, int to, int toAdd, const QColor &backgroundColor, QPainter::TextDirection d, int letterSpacing, int wordSpacing, bool smallCaps)
{
    if (data->state.paintingDisabled || len <= 0)
        return;

    // Avoid allocations, use stack array to pass font families.  Normally these
    // css fallback lists are small <= 3.
    CREATE_FAMILY_ARRAY(data->state.font, families);
    
    _updateRenderer(families);

    [data->textRenderer
    	drawCharacters:(const UniChar *)str stringLength:len
        fromCharacterPosition:from 
        toCharacterPosition:to 
        atPoint:NSMakePoint(x, y)
        withPadding: toAdd
        withTextColor:data->state.pen.color().getNSColor() 
        backgroundColor:backgroundColor.isValid() ? backgroundColor.getNSColor() : nil
        rightToLeft: d == RTL ? true : false
        letterSpacing: letterSpacing
        wordSpacing: wordSpacing
        smallCaps: smallCaps
        fontFamilies: families];
}

void QPainter::drawLineForText(int x, int y, int yOffset, int width)
{
    if (data->state.paintingDisabled)
        return;

    [data->textRenderer
        drawLineForCharacters: NSMakePoint(x, y)
               yOffset:(float)yOffset
             withWidth: width
             withColor:data->state.pen.color().getNSColor()];
}

QColor QPainter::selectedTextBackgroundColor() const
{
    NSColor *color = _usesInactiveTextBackgroundColor ? [NSColor secondarySelectedControlColor] : [NSColor selectedTextBackgroundColor];
    color = [color colorUsingColorSpaceName:NSCalibratedRGBColorSpace];
    return QColor((int)(255 * [color redComponent]), (int)(255 * [color greenComponent]), (int)(255 * [color blueComponent]));
}

void QPainter::fillRect(int x, int y, int w, int h, const QBrush &brush)
{
    if (data->state.paintingDisabled)
        return;
    
    if (brush.style() == SolidPattern) {
        [brush.color().getNSColor() set];
        NSRectFill(NSMakeRect(x, y, w, h));
    }
}

void QPainter::addClip(const QRect &rect)
{
    [NSBezierPath clipRect:rect];
}

Qt::RasterOp QPainter::rasterOp() const
{
    return CopyROP;
}

void QPainter::setRasterOp(RasterOp)
{
}

void QPainter::setPaintingDisabled(bool f)
{
    data->state.paintingDisabled = f;
}

bool QPainter::paintingDisabled() const
{
    return data->state.paintingDisabled;
}
