/*
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "SVGImageCache.h"

#if ENABLE(SVG)
#include "FrameView.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "Page.h"
#include "RenderSVGRoot.h"
#include "SVGImage.h"

namespace WebCore {

SVGImageCache::SVGImageCache(SVGImage* svgImage)
    : m_svgImage(svgImage)
    , m_redrawTimer(this, &SVGImageCache::redrawTimerFired)
{
    ASSERT(m_svgImage);
}

SVGImageCache::~SVGImageCache()
{
    m_sizeAndScalesMap.clear();

    ImageDataMap::iterator end = m_imageDataMap.end();
    for (ImageDataMap::iterator it = m_imageDataMap.begin(); it != end; ++it)
        delete it->second.buffer;

    m_imageDataMap.clear();
}

void SVGImageCache::removeRendererFromCache(const RenderObject* renderer)
{
    ASSERT(renderer);
    m_sizeAndScalesMap.remove(renderer);

    ImageDataMap::iterator it = m_imageDataMap.find(renderer);
    if (it == m_imageDataMap.end())
        return;

    delete it->second.buffer;
    m_imageDataMap.remove(it);
}

void SVGImageCache::setRequestedSizeAndScales(const RenderObject* renderer, const SizeAndScales& sizeAndScales)
{
    ASSERT(renderer);
    ASSERT(!sizeAndScales.size.isEmpty());
    m_sizeAndScalesMap.set(renderer, sizeAndScales);
}

SVGImageCache::SizeAndScales SVGImageCache::requestedSizeAndScales(const RenderObject* renderer) const
{
    ASSERT(renderer);
    SizeAndScalesMap::const_iterator it = m_sizeAndScalesMap.find(renderer);
    if (it == m_sizeAndScalesMap.end())
        return SizeAndScales();
    return it->second;
}

void SVGImageCache::imageContentChanged()
{
    ImageDataMap::iterator end = m_imageDataMap.end();
    for (ImageDataMap::iterator it = m_imageDataMap.begin(); it != end; ++it)
        it->second.imageNeedsUpdate = true;

    // If we're in the middle of layout, start redrawing dirty
    // images on a timer; otherwise it's safe to draw immediately.
    FrameView* frameView = m_svgImage->frameView();
    if (frameView && (frameView->needsLayout() || frameView->isInLayout())) {
        if (!m_redrawTimer.isActive())
            m_redrawTimer.startOneShot(0);
    } else
       redraw();
}

void SVGImageCache::redraw()
{
    ImageDataMap::iterator end = m_imageDataMap.end();
    for (ImageDataMap::iterator it = m_imageDataMap.begin(); it != end; ++it) {
        ImageData& data = it->second;
        if (!data.imageNeedsUpdate)
            continue;
        // If the content changed we redraw using our existing ImageBuffer.
        ASSERT(data.buffer);
        ASSERT(data.image);
        m_svgImage->drawSVGToImageBuffer(data.buffer, data.sizeAndScales.size, data.sizeAndScales.zoom, data.sizeAndScales.scale, SVGImage::ClearImageBuffer);
        data.image = data.buffer->copyImage(CopyBackingStore);
        data.imageNeedsUpdate = false;
    }
    ASSERT(m_svgImage->imageObserver());
    m_svgImage->imageObserver()->animationAdvanced(m_svgImage);
}

void SVGImageCache::redrawTimerFired(Timer<SVGImageCache>*)
{
    // We have no guarantee that the frame does not require layout when the timer fired.
    // So be sure to check again in case it is still not safe to run redraw.
    FrameView* frameView = m_svgImage->frameView();
    if (frameView && (frameView->needsLayout() || frameView->isInLayout())) {
        if (!m_redrawTimer.isActive())
            m_redrawTimer.startOneShot(0);
    } else
       redraw();
}

Image* SVGImageCache::lookupOrCreateBitmapImageForRenderer(const RenderObject* renderer)
{
    ASSERT(renderer);

    // The cache needs to know the size of the renderer before querying an image for it.
    SizeAndScalesMap::iterator sizeIt = m_sizeAndScalesMap.find(renderer);
    if (sizeIt == m_sizeAndScalesMap.end())
        return Image::nullImage();

    IntSize size = sizeIt->second.size;
    float zoom = sizeIt->second.zoom;
    float scale = sizeIt->second.scale;
    ASSERT(!size.isEmpty());

    // FIXME (85335): This needs to take CSS transform scale into account as well.
    Page* page = renderer->document()->page();
    if (!scale) {
        scale = page->deviceScaleFactor() * renderer->document()->frame()->documentScale();
    }

    // Lookup image for renderer in cache and eventually update it.
    ImageDataMap::iterator it = m_imageDataMap.find(renderer);
    if (it != m_imageDataMap.end()) {
        ImageData& data = it->second;

        // Common case: image size & zoom remained the same.
        if (data.sizeAndScales.size == size && data.sizeAndScales.zoom == zoom && data.sizeAndScales.scale == scale)
            return data.image.get();

        // If the image size for the renderer changed, we have to delete the buffer, remove the item from the cache and recreate it.
        delete data.buffer;
        m_imageDataMap.remove(it);
    }

    FloatSize scaledSize(size);
    scaledSize.scale(scale);

    // Create and cache new image and image buffer at requested size.
    OwnPtr<ImageBuffer> newBuffer = ImageBuffer::create(expandedIntSize(scaledSize), 1);
    if (!newBuffer)
        return Image::nullImage();

    m_svgImage->drawSVGToImageBuffer(newBuffer.get(), size, zoom, scale, SVGImage::DontClearImageBuffer);

    RefPtr<Image> newImage = newBuffer->copyImage(CopyBackingStore);
    Image* newImagePtr = newImage.get();
    ASSERT(newImagePtr);

    m_imageDataMap.add(renderer, ImageData(newBuffer.leakPtr(), newImage.release(), SizeAndScales(size, zoom, scale)));
    return newImagePtr;
}

} // namespace WebCore

#endif // ENABLE(SVG)
