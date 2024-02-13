#include "Graphics.h"
#include "Common.h"
#include "Font.h"
#include "Image.h"
// #include "DDImage.h"
#include "misc/Debug.h"
#include "misc/Rect.h"
#include "misc/SexyMatrix.h"
#include <array>
#include <math.h>

using namespace Sexy;

DummyImage GraphicsState::mStaticImage;
const Point *Graphics::mPFPoints;

//////////////////////////////////////////////////////////////////////////

void GraphicsState::CopyStateFrom(const GraphicsState *theState) {
    mDestImage = theState->mDestImage;
    mTransX = theState->mTransX;
    mTransY = theState->mTransY;
    mClipRect = theState->mClipRect;
    mFont = theState->mFont;
    mColor = theState->mColor;
    mDrawMode = theState->mDrawMode;
    mColorizeImages = theState->mColorizeImages;
    mFastStretch = theState->mFastStretch;
    mWriteColoredString = theState->mWriteColoredString;
    mLinearBlend = theState->mLinearBlend;
    mScaleX = theState->mScaleX;
    mScaleY = theState->mScaleY;
    mScaleOrigX = theState->mScaleOrigX;
    mScaleOrigY = theState->mScaleOrigY;
    //	mIs3D = theState->mIs3D;
}

//////////////////////////////////////////////////////////////////////////

Graphics::Graphics(const Graphics &theGraphics) { CopyStateFrom(&theGraphics); }

Graphics::Graphics(Image *theDestImage) {
    mTransX = 0;
    mTransY = 0;
    mScaleX = 1;
    mScaleY = 1;
    mScaleOrigX = 0;
    mScaleOrigY = 0;
    mFont = nullptr;
    mDestImage = theDestImage;
    mDrawMode = DRAWMODE_NORMAL;
    mColorizeImages = false;
    mFastStretch = false;
    mWriteColoredString = true;
    mLinearBlend = false;

    if (mDestImage == nullptr) mDestImage = &mStaticImage;

    mClipRect = Rect(0, 0, mDestImage->GetWidth(), mDestImage->GetHeight());
}

Graphics::~Graphics() {}

void Graphics::PushState() {
    mStateStack.push_back(GraphicsState());
    mStateStack.back().CopyStateFrom(this);
}

void Graphics::PopState() {
    DBG_ASSERTE(mStateStack.size() > 0);
    if (mStateStack.size() > 0) {
        CopyStateFrom(&mStateStack.back());
        mStateStack.pop_back();
    }
}

Graphics *Graphics::Create() const { return new Graphics(*this); }

Sexy::_Font *Graphics::GetFont() { return mFont; }

void Graphics::SetFont(Sexy::_Font *theFont) { mFont = theFont; }

void Graphics::SetColor(const Color &theColor) { mColor = theColor; }

const Color &Graphics::GetColor() { return mColor; }

void Graphics::SetDrawMode(int theDrawMode) { mDrawMode = theDrawMode; }

int Graphics::GetDrawMode() { return mDrawMode; }

void Graphics::SetColorizeImages(bool colorizeImages) { mColorizeImages = colorizeImages; }

bool Graphics::GetColorizeImages() { return mColorizeImages; }

void Graphics::SetFastStretch(bool fastStretch) { mFastStretch = fastStretch; }

bool Graphics::GetFastStretch() { return mFastStretch; }

void Graphics::SetLinearBlend(bool linear) { mLinearBlend = linear; }

bool Graphics::GetLinearBlend() { return mLinearBlend; }

void Graphics::ClearRect(int theX, int theY, int theWidth, int theHeight) {
    const Rect aDestRect = Rect(theX + mTransX, theY + mTransY, theWidth, theHeight).Intersection(mClipRect);
    mDestImage->ClearRect(aDestRect);
}

void Graphics::ClearRect(const Rect &theRect) { ClearRect(theRect.mX, theRect.mY, theRect.mWidth, theRect.mHeight); }

void Graphics::FillRect(int theX, int theY, int theWidth, int theHeight) {
    if (mColor.mAlpha == 0) return;

    const Rect aDestRect = Rect(theX + mTransX, theY + mTransY, theWidth, theHeight).Intersection(mClipRect);
    mDestImage->FillRect(aDestRect, mColor, mDrawMode);
}

void Graphics::FillRect(const Rect &theRect) { FillRect(theRect.mX, theRect.mY, theRect.mWidth, theRect.mHeight); }

void Graphics::DrawRect(int theX, int theY, int theWidth, int theHeight) {
    if (mColor.mAlpha == 0) return;

    const auto aDestRect = Rect(theX + mTransX, theY + mTransY, theWidth, theHeight);
    const auto aFullDestRect = Rect(theX + mTransX, theY + mTransY, theWidth + 1, theHeight + 1);
    const Rect aFullClippedRect = aFullDestRect.Intersection(mClipRect);

    if (aFullDestRect == aFullClippedRect) {
        mDestImage->DrawRect(aDestRect, mColor, mDrawMode);
    } else {
        FillRect(theX, theY, theWidth + 1, 1);
        FillRect(theX, theY + theHeight, theWidth + 1, 1);
        FillRect(theX, theY + 1, 1, theHeight - 1);
        FillRect(theX + theWidth, theY + 1, 1, theHeight - 1);

        /*if (aClippedRect.mX == aDestRect.mX)
            mDestImage->FillRect(Rect(aClippedRect.mX, aClippedRect.mY, 1, aClippedRect.mHeight), mColor, mDrawMode);
        if (aClippedRect.mY == aDestRect.mY)
            mDestImage->FillRect(Rect(aClippedRect.mX, aClippedRect.mY, aClippedRect.mWidth, 1), mColor, mDrawMode);
        if (aClippedRect.mX + aClippedRect.mWidth == aDestRect.mX + aDestRect.mWidth)
            mDestImage->FillRect(Rect(aClippedRect.mX + aClippedRect.mWidth, aClippedRect.mY, 1, aClippedRect.mHeight),
        mColor, mDrawMode); if (aClippedRect.mY + aClippedRect.mHeight == aDestRect.mY + aDestRect.mHeight)
            mDestImage->FillRect(Rect(aClippedRect.mX, aClippedRect.mY + aClippedRect.mHeight, aClippedRect.mWidth, 1),
        mColor, mDrawMode);*/
    }
}

void Graphics::DrawRect(const Rect &theRect) { DrawRect(theRect.mX, theRect.mY, theRect.mWidth, theRect.mHeight); }

int Graphics::PFCompareInd(const void *u, const void *v) {
    return mPFPoints[*((int *)u)].mY <= mPFPoints[*((int *)v)].mY ? -1 : 1;
}

int Graphics::PFCompareActive(const void *u, const void *v) { return ((Edge *)u)->mX <= ((Edge *)v)->mX ? -1 : 1; }

void Graphics::PFDelete(int i) // remove edge i from active list
{
    int j;

    for (j = 0; j < mPFNumActiveEdges && mPFActiveEdgeList[j].i != i; j++)
        ;
    if (j >= mPFNumActiveEdges) return; /* edge not in active list; happens at aMinY*/

    mPFNumActiveEdges--;
    memcpy(&mPFActiveEdgeList[j], &mPFActiveEdgeList[j + 1], (mPFNumActiveEdges - j) * sizeof mPFActiveEdgeList[0]);
}

void Graphics::PFInsert(int i, int y) // append edge i to end of active list
{
    double dx;
    const Point *p, *q;

    const int j = i < mPFNumVertices - 1 ? i + 1 : 0;
    if (mPFPoints[i].mY < mPFPoints[j].mY) {
        p = &mPFPoints[i];
        q = &mPFPoints[j];
    } else {
        p = &mPFPoints[j];
        q = &mPFPoints[i];
    }
    /* initialize x position at intersection of edge with scanline y */
    mPFActiveEdgeList[mPFNumActiveEdges].mDX = dx = (q->mX - p->mX) / static_cast<double>(q->mY - p->mY);
    mPFActiveEdgeList[mPFNumActiveEdges].mX = dx * (y + 0.5 - p->mY - mTransY) + p->mX + mTransX;
    mPFActiveEdgeList[mPFNumActiveEdges].i = i;
    mPFActiveEdgeList[mPFNumActiveEdges].b = p->mY - 1.0 / dx * p->mX;
    mPFNumActiveEdges++;
}

void Graphics::PolyFill(const Point *theVertexList, int theNumVertices, bool convex) {
    if (convex &&
        mDestImage->PolyFill3D(theVertexList, theNumVertices, &mClipRect, mColor, mDrawMode, mTransX, mTransY))
        return;

    Span aSpans[MAX_TEMP_SPANS];
    int aSpanPos = 0;

    int k, j;
    /* list of vertex indices, sorted by mPFPoints[ind[j]].y */

    const int aMinX = mClipRect.mX;
    const int aMaxX = mClipRect.mX + mClipRect.mWidth - 1;
    const int aMinY = mClipRect.mY;
    const int aMaxY = mClipRect.mY + mClipRect.mHeight - 1;

    mPFNumVertices = theNumVertices;
    mPFPoints = theVertexList;

    if (mPFNumVertices <= 0) return;

    int *ind = new int[mPFNumVertices];
    mPFActiveEdgeList = new Edge[mPFNumVertices];

    /* create y-sorted array of indices ind[k] into vertex list */
    for (k = 0; k < mPFNumVertices; k++)
        ind[k] = k;
    qsort(ind, mPFNumVertices, sizeof ind[0], PFCompareInd); /* sort ind by mPFPoints[ind[k]].y */

    mPFNumActiveEdges = 0; /* start with empty active list */
    k = 0;                 /* ind[k] is next vertex to process */
    const int y0 = std::max(aMinY, static_cast<int>(ceil(mPFPoints[ind[0]].mY - 0.5 + mTransY))); /* ymin of polygon */
    const int y1 = std::min(aMaxY, static_cast<int>(floor(mPFPoints[ind[mPFNumVertices - 1]].mY - 0.5 + mTransY)));
    /* ymax of polygon */

    for (int y = y0; y <= y1; y++) {
        // step through scanlines
        // scanline y is at y+.5 in continuous coordinates

        // check vertices between previous scanline and current one, if any
        for (; (k < mPFNumVertices) && (mPFPoints[ind[k]].mY + mTransY <= y + 0.5); k++) {
            // to simplify, if mPFPoints.mY=y+.5, pretend it's above
            // invariant: y-.5 < mPFPoints[i].mY <= y+.5
            const int i = ind[k];
            // insert or delete edges before and after vertex i (i-1 to i,
            // and i to i+1) from active list if they cross scanline y

            j = i > 0 ? i - 1 : mPFNumVertices - 1;   // vertex previous to i
            if (mPFPoints[j].mY + mTransY <= y - 0.5) // old edge, remove from active list
                PFDelete(j);
            else if (mPFPoints[j].mY + mTransY > y + 0.5) // new edge, add to active list
                PFInsert(j, y);

            j = i < mPFNumVertices - 1 ? i + 1 : 0;   // vertex next after i
            if (mPFPoints[j].mY + mTransY <= y - 0.5) // old edge, remove from active list
                PFDelete(i);
            else if (mPFPoints[j].mY + mTransY > y + 0.5) // new edge, add to active list
                PFInsert(i, y);
        }

        // sort active edge list by active[j].mX
        qsort(mPFActiveEdgeList, mPFNumActiveEdges, sizeof mPFActiveEdgeList[0], PFCompareActive);

        // draw horizontal segments for scanline y
        for (j = 0; j < mPFNumActiveEdges; j += 2) {
            // draw horizontal segments
            // span 'tween j & j+1 is inside, span tween j+1 & j+2 is outside
            int xl = static_cast<int>(ceil(mPFActiveEdgeList[j].mX - 0.5)); // left end of span
            if (xl < aMinX) xl = aMinX;
            int xr = static_cast<int>(floor(mPFActiveEdgeList[j + 1].mX - 0.5)); // right end of span
            if (xr > aMaxX) xr = aMaxX;

            if ((xl <= xr) && (aSpanPos < MAX_TEMP_SPANS)) {
                Span *aSpan = &aSpans[aSpanPos++];
                aSpan->mY = y;
                aSpan->mX = xl;
                aSpan->mWidth = xr - xl + 1;
            }

            mPFActiveEdgeList[j].mX += mPFActiveEdgeList[j].mDX; // increment edge coords
            mPFActiveEdgeList[j + 1].mX += mPFActiveEdgeList[j + 1].mDX;
        }
    }

    mDestImage->FillScanLines(aSpans, aSpanPos, mColor, mDrawMode);

    delete[] ind;
    delete mPFActiveEdgeList;
}

void Graphics::PolyFillAA(const Point *theVertexList, int theNumVertices, bool convex) {
    if (convex &&
        mDestImage->PolyFill3D(theVertexList, theNumVertices, &mClipRect, mColor, mDrawMode, mTransX, mTransY))
        return;

    int i;

    Span aSpans[MAX_TEMP_SPANS];
    int aSpanPos = 0;

    static unsigned char aCoverageBuffer[256 * 256];
    int aCoverWidth = 256, aCoverHeight = 256;
    int aCoverLeft, aCoverRight, aCoverTop, aCoverBottom;

    for (i = 0; i < theNumVertices; ++i) {
        const Point *aPt = &theVertexList[i];
        if (i == 0) {
            aCoverLeft = aCoverRight = aPt->mX;
            aCoverTop = aCoverBottom = aPt->mY;
        } else {
            aCoverLeft = std::min(aCoverLeft, aPt->mX);
            aCoverRight = std::max(aCoverRight, aPt->mX);
            aCoverTop = std::min(aCoverTop, aPt->mY);
            aCoverBottom = std::max(aCoverBottom, aPt->mY);
        }
    }
    unsigned char *coverPtr = aCoverageBuffer;
    if ((aCoverRight - aCoverLeft + 1) > aCoverWidth || (aCoverBottom - aCoverTop + 1) > aCoverHeight) {
        aCoverWidth = aCoverRight - aCoverLeft + 1;
        aCoverHeight = aCoverBottom - aCoverTop + 1;
        coverPtr = new unsigned char[aCoverWidth * aCoverHeight];
    }
    memset(coverPtr, 0, aCoverWidth * aCoverHeight);

    int k, j;
    /* list of vertex indices, sorted by mPFPoints[ind[j]].y */

    const int aMinX = mClipRect.mX;
    const int aMaxX = mClipRect.mX + mClipRect.mWidth - 1;
    const int aMinY = mClipRect.mY;
    const int aMaxY = mClipRect.mY + mClipRect.mHeight - 1;

    mPFNumVertices = theNumVertices;
    mPFPoints = theVertexList;

    if (mPFNumVertices <= 0) return;

    int *ind = new int[mPFNumVertices];
    mPFActiveEdgeList = new Edge[mPFNumVertices];

    /* create y-sorted array of indices ind[k] into vertex list */
    for (k = 0; k < mPFNumVertices; k++)
        ind[k] = k;
    qsort(ind, mPFNumVertices, sizeof ind[0], PFCompareInd); /* sort ind by mPFPoints[ind[k]].y */

    mPFNumActiveEdges = 0; /* start with empty active list */
    k = 0;                 /* ind[k] is next vertex to process */
    const int y0 = std::max(aMinY, static_cast<int>(ceil(mPFPoints[ind[0]].mY - 0.5 + mTransY))); /* ymin of polygon */
    const int y1 = std::min(aMaxY, static_cast<int>(floor(mPFPoints[ind[mPFNumVertices - 1]].mY - 0.5 + mTransY)));
    /* ymax of polygon */

    for (int y = y0; y <= y1; y++) {
        // step through scanlines
        // scanline y is at y+.5 in continuous coordinates

        // check vertices between previous scanline and current one, if any
        for (; (k < mPFNumVertices) && (mPFPoints[ind[k]].mY + mTransY <= y + 0.5); k++) {
            // to simplify, if mPFPoints.mY=y+.5, pretend it's above
            // invariant: y-.5 < mPFPoints[i].mY <= y+.5
            i = ind[k];
            // insert or delete edges before and after vertex i (i-1 to i,
            // and i to i+1) from active list if they cross scanline y

            j = i > 0 ? i - 1 : mPFNumVertices - 1;   // vertex previous to i
            if (mPFPoints[j].mY + mTransY <= y - 0.5) // old edge, remove from active list
                PFDelete(j);
            else if (mPFPoints[j].mY + mTransY > y + 0.5) // new edge, add to active list
                PFInsert(j, y);

            j = i < mPFNumVertices - 1 ? i + 1 : 0;   // vertex next after i
            if (mPFPoints[j].mY + mTransY <= y - 0.5) // old edge, remove from active list
                PFDelete(i);
            else if (mPFPoints[j].mY + mTransY > y + 0.5) // new edge, add to active list
                PFInsert(i, y);
        }

        // sort active edge list by active[j].mX
        qsort(mPFActiveEdgeList, mPFNumActiveEdges, sizeof mPFActiveEdgeList[0], PFCompareActive);

        // draw horizontal segments for scanline y
        for (j = 0; j < mPFNumActiveEdges; j += 2) {
            // draw horizontal segments
            // span 'tween j & j+1 is inside, span tween j+1 & j+2 is outside
            int xl = static_cast<int>(ceil(mPFActiveEdgeList[j].mX - 0.5)); // left end of span
            int lErr = static_cast<int>((fabs((mPFActiveEdgeList[j].mX - 0.5) - xl)) * 255);
            if (xl < aMinX) {
                xl = aMinX;
                lErr = 255;
            }
            int xr = static_cast<int>(floor(mPFActiveEdgeList[j + 1].mX - 0.5)); // right end of span
            int rErr = static_cast<int>((fabs((mPFActiveEdgeList[j + 1].mX - 0.5) - xr)) * 255);
            if (xr > aMaxX) {
                xr = aMaxX;
                rErr = 255;
            }

            if ((xl <= xr) && (aSpanPos < MAX_TEMP_SPANS)) {
                Span *aSpan = &aSpans[aSpanPos++];
                aSpan->mY = y;
                aSpan->mX = xl;
                aSpan->mWidth = xr - xl + 1;

                unsigned char *coverRow = coverPtr + (y - aCoverTop) * aCoverWidth;
                if (xr == xl) {
                    coverRow[xl - aCoverLeft] = std::min(255, coverRow[xl - aCoverLeft] + ((lErr * rErr) >> 8));
                } else {
                    if (fabs(mPFActiveEdgeList[j].mDX) > 1.0f) // mostly horizontal on the left edge
                    {
                        double m = 1.0 / mPFActiveEdgeList[j].mDX, b = mPFActiveEdgeList[j].b,
                               c = fabs(mPFActiveEdgeList[j].mDX);
                        do {
                            const double _y = m * xl + b;
                            lErr = std::min(255, static_cast<int>(fabs((_y)-y - .5) * 255));
                            coverRow[xl - aCoverLeft] = std::min(255, coverRow[xl - aCoverLeft] + lErr);
                            xl++;
                            c -= 1.0;
                        } while (xl <= xr && c > 0);
                    } else {
                        coverRow[xl - aCoverLeft] = std::min(255, coverRow[xl - aCoverLeft] + lErr);
                        xl++;
                    }

                    if (fabs(mPFActiveEdgeList[j + 1].mDX) > 1.0f) // mostly horizontal on the right edge
                    {
                        double m = 1.0 / mPFActiveEdgeList[j + 1].mDX, b = mPFActiveEdgeList[j + 1].b,
                               c = fabs(mPFActiveEdgeList[j + 1].mDX);
                        do {
                            const double _y = m * xr + b;
                            rErr = std::min(255, static_cast<int>(fabs((_y)-y - .5) * 255));
                            coverRow[xr - aCoverLeft] = std::min(255, coverRow[xr - aCoverLeft] + rErr);
                            xr--;
                            c -= 1.0;
                        } while (xr >= xl && c > 0);
                    } else {
                        coverRow[xr - aCoverLeft] = std::min(255, coverRow[xr - aCoverLeft] + rErr);
                        xr--;
                    }

                    if (xl <= xr) memset(&coverRow[xl - aCoverLeft], 255, xr - xl + 1);
                }
            }

            mPFActiveEdgeList[j].mX += mPFActiveEdgeList[j].mDX; // increment edge coords
            mPFActiveEdgeList[j + 1].mX += mPFActiveEdgeList[j + 1].mDX;
        }
    }

    mDestImage->FillScanLinesWithCoverage(
        aSpans, aSpanPos, mColor, mDrawMode, coverPtr, aCoverLeft, aCoverTop, aCoverWidth, aCoverHeight
    );

    if (coverPtr != aCoverageBuffer) delete[] coverPtr;
    delete[] ind;
    delete[] mPFActiveEdgeList;
}

bool Graphics::DrawLineClipHelper(double *theStartX, double *theStartY, double *theEndX, double *theEndY) {
    double aStartX = *theStartX;
    double aStartY = *theStartY;
    double aEndX = *theEndX;
    double aEndY = *theEndY;

    // Clip X
    if (aStartX > aEndX) {
        std::swap(aStartX, aEndX);
        std::swap(aStartY, aEndY);
    }

    if (aStartX < mClipRect.mX) {
        if (aEndX < mClipRect.mX) return false;

        const double aSlope = (aEndY - aStartY) / (aEndX - aStartX);
        aStartY += (mClipRect.mX - aStartX) * aSlope;
        aStartX = mClipRect.mX;
    }

    if (aEndX >= mClipRect.mX + mClipRect.mWidth) {
        if (aStartX >= mClipRect.mX + mClipRect.mWidth) return false;

        const double aSlope = (aEndY - aStartY) / (aEndX - aStartX);
        aEndY += (mClipRect.mX + mClipRect.mWidth - 1 - aEndX) * aSlope;
        aEndX = mClipRect.mX + mClipRect.mWidth - 1;
    }

    // Clip Y
    if (aStartY > aEndY) {
        std::swap(aStartX, aEndX);
        std::swap(aStartY, aEndY);
    }

    if (aStartY < mClipRect.mY) {
        if (aEndY < mClipRect.mY) return false;

        const double aSlope = (aEndX - aStartX) / (aEndY - aStartY);
        aStartX += (mClipRect.mY - aStartY) * aSlope;

        aStartY = mClipRect.mY;
    }

    if (aEndY >= mClipRect.mY + mClipRect.mHeight) {
        if (aStartY >= mClipRect.mY + mClipRect.mHeight) return false;

        const double aSlope = (aEndX - aStartX) / (aEndY - aStartY);
        aEndX += (mClipRect.mY + mClipRect.mHeight - 1 - aEndY) * aSlope;
        aEndY = mClipRect.mY + mClipRect.mHeight - 1;
    }

    *theStartX = aStartX;
    *theStartY = aStartY;
    *theEndX = aEndX;
    *theEndY = aEndY;

    return true;
}

void Graphics::DrawLine(int theStartX, int theStartY, int theEndX, int theEndY) {
    double aStartX = theStartX + mTransX;
    double aStartY = theStartY + mTransY;
    double aEndX = theEndX + mTransX;
    double aEndY = theEndY + mTransY;

    if (!DrawLineClipHelper(&aStartX, &aStartY, &aEndX, &aEndY)) return;

    mDestImage->DrawLine(aStartX, aStartY, aEndX, aEndY, mColor, mDrawMode);
}

void Graphics::DrawLineAA(int theStartX, int theStartY, int theEndX, int theEndY) {
    double aStartX = theStartX + mTransX;
    double aStartY = theStartY + mTransY;
    double aEndX = theEndX + mTransX;
    double aEndY = theEndY + mTransY;

    if (!DrawLineClipHelper(&aStartX, &aStartY, &aEndX, &aEndY)) return;

    mDestImage->DrawLineAA(aStartX, aStartY, aEndX, aEndY, mColor, mDrawMode);
}

void Graphics::DrawString(const SexyString &theString, int theX, int theY) {
    if (mFont != nullptr) mFont->DrawString(this, theX, theY, theString, mColor, mClipRect);
}

void Graphics::DrawImage(Sexy::Image *theImage, int theX, int theY) {
    if (mScaleX != 1 || mScaleY != 1) {
        DrawImage(theImage, theX, theY, Rect(0, 0, theImage->mWidth, theImage->mHeight));
        return;
    }

    theX += mTransX;
    theY += mTransY;

    const Rect aDestRect = Rect(theX, theY, theImage->GetWidth(), theImage->GetHeight()).Intersection(mClipRect);
    const Rect aSrcRect(aDestRect.mX - theX, aDestRect.mY - theY, aDestRect.mWidth, aDestRect.mHeight);

    if ((aSrcRect.mWidth > 0) && (aSrcRect.mHeight > 0))
        mDestImage->Blt(
            theImage, aDestRect.mX, aDestRect.mY, aSrcRect, mColorizeImages ? mColor : Color::White, mDrawMode
        );
}

void Graphics::DrawImage(Image *theImage, int theX, int theY, const Rect &theSrcRect) {
    DBG_ASSERTE(theSrcRect.mX + theSrcRect.mWidth <= theImage->GetWidth());
    DBG_ASSERTE(theSrcRect.mY + theSrcRect.mHeight <= theImage->GetHeight());

    if ((theSrcRect.mX + theSrcRect.mWidth > theImage->GetWidth()) ||
        (theSrcRect.mY + theSrcRect.mHeight > theImage->GetHeight()))
        return;

    theX += mTransX;
    theY += mTransY;

    if (mScaleX != 1 || mScaleY != 1) {
        const Rect aDestRect(
            mScaleOrigX + floor((theX - mScaleOrigX) * mScaleX), mScaleOrigY + floor((theY - mScaleOrigY) * mScaleY),
            ceil(theSrcRect.mWidth * mScaleX), ceil(theSrcRect.mHeight * mScaleY)
        );
        mDestImage->StretchBlt(
            theImage, aDestRect, theSrcRect, mClipRect, mColorizeImages ? mColor : Color::White, mDrawMode, mFastStretch
        );
        return;
    }

    const Rect aDestRect = Rect(theX, theY, theSrcRect.mWidth, theSrcRect.mHeight).Intersection(mClipRect);
    const Rect aSrcRect(
        theSrcRect.mX + aDestRect.mX - theX, theSrcRect.mY + aDestRect.mY - theY, aDestRect.mWidth, aDestRect.mHeight
    );

    if ((aSrcRect.mWidth > 0) && (aSrcRect.mHeight > 0))
        mDestImage->Blt(
            theImage, aDestRect.mX, aDestRect.mY, aSrcRect, mColorizeImages ? mColor : Color::White, mDrawMode
        );
}

void Graphics::DrawImageMirror(Image *theImage, int theX, int theY, bool mirror) {
    DrawImageMirror(theImage, theX, theY, Rect(0, 0, theImage->mWidth, theImage->mHeight), mirror);
}

void Graphics::DrawImageMirror(Image *theImage, int theX, int theY, const Rect &theSrcRect, bool mirror) {
    if (!mirror) {
        DrawImage(theImage, theX, theY, theSrcRect);
        return;
    }

    theX += mTransX;
    theY += mTransY;

    DBG_ASSERTE(theSrcRect.mX + theSrcRect.mWidth <= theImage->GetWidth());
    DBG_ASSERTE(theSrcRect.mY + theSrcRect.mHeight <= theImage->GetHeight());

    if ((theSrcRect.mX + theSrcRect.mWidth > theImage->GetWidth()) ||
        (theSrcRect.mY + theSrcRect.mHeight > theImage->GetHeight()))
        return;

    const Rect aDestRect = Rect(theX, theY, theSrcRect.mWidth, theSrcRect.mHeight).Intersection(mClipRect);

    const int aTotalClip = theSrcRect.mWidth - aDestRect.mWidth;
    const int aLeftClip = aDestRect.mX - theX;
    const int aRightClip = aTotalClip - aLeftClip;

    const Rect aSrcRect(
        theSrcRect.mX + aRightClip, theSrcRect.mY + aDestRect.mY - theY, aDestRect.mWidth, aDestRect.mHeight
    );

    if ((aSrcRect.mWidth > 0) && (aSrcRect.mHeight > 0))
        mDestImage->BltMirror(
            theImage, aDestRect.mX, aDestRect.mY, aSrcRect, mColorizeImages ? mColor : Color::White, mDrawMode
        );
}

void Graphics::DrawImageMirror(Image *theImage, const Rect &theDestRect, const Rect &theSrcRect, bool mirror) {
    if (!mirror) {
        DrawImage(theImage, theDestRect, theSrcRect);
        return;
    }

    const auto aDestRect =
        Rect(theDestRect.mX + mTransX, theDestRect.mY + mTransY, theDestRect.mWidth, theDestRect.mHeight);

    mDestImage->StretchBltMirror(
        theImage, aDestRect, theSrcRect, mClipRect, mColorizeImages ? mColor : Color::White, mDrawMode, mFastStretch
    );
}

void Graphics::DrawImage(Image *theImage, int theX, int theY, int theStretchedWidth, int theStretchedHeight) {
    const auto aDestRect = Rect(theX + mTransX, theY + mTransY, theStretchedWidth, theStretchedHeight);
    const auto aSrcRect = Rect(0, 0, theImage->mWidth, theImage->mHeight);

    mDestImage->StretchBlt(
        theImage, aDestRect, aSrcRect, mClipRect, mColorizeImages ? mColor : Color::White, mDrawMode, mFastStretch
    );
}

void Graphics::DrawImage(Image *theImage, const Rect &theDestRect, const Rect &theSrcRect) {
    const auto aDestRect =
        Rect(theDestRect.mX + mTransX, theDestRect.mY + mTransY, theDestRect.mWidth, theDestRect.mHeight);

    mDestImage->StretchBlt(
        theImage, aDestRect, theSrcRect, mClipRect, mColorizeImages ? mColor : Color::White, mDrawMode, mFastStretch
    );
}

void Graphics::DrawImageF(Image *theImage, float theX, float theY) {
    theX += mTransX;
    theY += mTransY;

    const Rect aSrcRect(0, 0, theImage->mWidth, theImage->mHeight);
    mDestImage->BltF(theImage, theX, theY, aSrcRect, mClipRect, mColorizeImages ? mColor : Color::White, mDrawMode);
}

void Graphics::DrawImageF(Image *theImage, float theX, float theY, const Rect &theSrcRect) {
    DBG_ASSERTE(theSrcRect.mX + theSrcRect.mWidth <= theImage->GetWidth());
    DBG_ASSERTE(theSrcRect.mY + theSrcRect.mHeight <= theImage->GetHeight());

    theX += mTransX;
    theY += mTransY;

    mDestImage->BltF(theImage, theX, theY, theSrcRect, mClipRect, mColorizeImages ? mColor : Color::White, mDrawMode);
}

void Graphics::DrawImageRotated(Image *theImage, int theX, int theY, double theRot, const Rect *theSrcRect) {
    if (theSrcRect == nullptr) {
        const int aRotCenterX = theImage->GetWidth() / 2;
        const int aRotCenterY = theImage->GetHeight() / 2;

        DrawImageRotatedF(theImage, theX, theY, theRot, aRotCenterX, aRotCenterY, theSrcRect);
    } else {
        const int aRotCenterX = theSrcRect->mWidth / 2;
        const int aRotCenterY = theSrcRect->mHeight / 2;

        DrawImageRotatedF(theImage, theX, theY, theRot, aRotCenterX, aRotCenterY, theSrcRect);
    }
}

void Graphics::DrawImageRotatedF(Image *theImage, float theX, float theY, double theRot, const Rect *theSrcRect) {
    if (theSrcRect == nullptr) {
        const float aRotCenterX = theImage->GetWidth() / 2.0f;
        const float aRotCenterY = theImage->GetHeight() / 2.0f;

        DrawImageRotatedF(theImage, theX, theY, theRot, aRotCenterX, aRotCenterY, theSrcRect);
    } else {
        const float aRotCenterX = theSrcRect->mWidth / 2.0f;
        const float aRotCenterY = theSrcRect->mHeight / 2.0f;

        DrawImageRotatedF(theImage, theX, theY, theRot, aRotCenterX, aRotCenterY, theSrcRect);
    }
}

void Graphics::DrawImageRotated(
    Image *theImage, int theX, int theY, double theRot, int theRotCenterX, int theRotCenterY, const Rect *theSrcRect
) {
    DrawImageRotatedF(theImage, theX, theY, theRot, theRotCenterX, theRotCenterY, theSrcRect);
}

void Graphics::DrawImageRotatedF(
    Image *theImage, float theX, float theY, double theRot, float theRotCenterX, float theRotCenterY,
    const Rect *theSrcRect
) {
    theX += mTransX;
    theY += mTransY;

    if (theSrcRect == nullptr) {
        const Rect aSrcRect(0, 0, theImage->mWidth, theImage->mHeight);
        mDestImage->BltRotated(
            theImage, theX, theY, aSrcRect, mClipRect, mColorizeImages ? mColor : Color::White, mDrawMode, theRot,
            theRotCenterX, theRotCenterY
        );
    } else
        mDestImage->BltRotated(
            theImage, theX, theY, *theSrcRect, mClipRect, mColorizeImages ? mColor : Color::White, mDrawMode, theRot,
            theRotCenterX, theRotCenterY
        );
}

void Graphics::DrawImageMatrix(Image *theImage, const SexyMatrix3 &theMatrix, float x, float y) {
    const Rect aSrcRect(0, 0, theImage->mWidth, theImage->mHeight);
    mDestImage->BltMatrix(
        theImage, x + mTransX, y + mTransY, theMatrix, mClipRect, mColorizeImages ? mColor : Color::White, mDrawMode,
        aSrcRect, mLinearBlend
    );
}

void Graphics::DrawImageMatrix(
    Image *theImage, const SexyMatrix3 &theMatrix, const Rect &theSrcRect, float x, float y
) {
    mDestImage->BltMatrix(
        theImage, x + mTransX, y + mTransY, theMatrix, mClipRect, mColorizeImages ? mColor : Color::White, mDrawMode,
        theSrcRect, mLinearBlend
    );
}

void Graphics::DrawImageTransformHelper(
    Image *theImage, const Transform &theTransform, const Rect &theSrcRect, float x, float y, bool useFloat
) {
    if (theTransform.mComplex || useFloat) {
        DrawImageMatrix(theImage, theTransform.GetMatrix(), theSrcRect, x, y);
        return;
    }

    // Translate into appropriate graphics call
    const float w2 = theSrcRect.mWidth / 2.0f;
    const float h2 = theSrcRect.mHeight / 2.0f;

    if (theTransform.mHaveRot) {
        const float rx = w2 - theTransform.mTransX1;
        const float ry = h2 - theTransform.mTransY1;

        x = x + theTransform.mTransX2 - rx + 0.5f;
        y = y + theTransform.mTransY2 - ry + 0.5f;

        if (useFloat) DrawImageRotatedF(theImage, x, y, theTransform.mRot, rx, ry, &theSrcRect);
        else DrawImageRotated(theImage, x, y, theTransform.mRot, rx, ry, &theSrcRect);
    } else if (theTransform.mHaveScale) {
        bool mirror = false;
        if (theTransform.mScaleX == -1) {
            if (theTransform.mScaleY == 1) {
                x = x + theTransform.mTransX1 + theTransform.mTransX2 - w2 + 0.5f;
                y = y + theTransform.mTransY1 + theTransform.mTransY2 - h2 + 0.5f;
                DrawImageMirror(theImage, x, y, theSrcRect);
                return;
            }
            mirror = true;
        }

        const float sw = w2 * theTransform.mScaleX;
        const float sh = h2 * theTransform.mScaleY;

        x = x + theTransform.mTransX2 - sw;
        y = y + theTransform.mTransY2 - sh;

        const Rect aDestRect(x, y, sw * 2, sh * 2);
        DrawImageMirror(theImage, aDestRect, theSrcRect, mirror);
    } else {
        x = x + theTransform.mTransX1 + theTransform.mTransX2 - w2 + 0.5f;
        y = y + theTransform.mTransY1 + theTransform.mTransY2 - h2 + 0.5f;

        if (useFloat) DrawImageF(theImage, x, y, theSrcRect);
        else DrawImage(theImage, x, y, theSrcRect);
    }
}

void Graphics::DrawImageTransform(Image *theImage, const Transform &theTransform, float x, float y) {
    DrawImageTransformHelper(theImage, theTransform, Rect(0, 0, theImage->mWidth, theImage->mHeight), x, y, false);
}

void Graphics::DrawImageTransformF(Image *theImage, const Transform &theTransform, float x, float y) {
    DrawImageTransformHelper(theImage, theTransform, Rect(0, 0, theImage->mWidth, theImage->mHeight), x, y, true);
}

void Graphics::DrawImageTransform(
    Image *theImage, const Transform &theTransform, const Rect &theSrcRect, float x, float y
) {
    DrawImageTransformHelper(theImage, theTransform, theSrcRect, x, y, false);
}

void Graphics::DrawImageTransformF(
    Image *theImage, const Transform &theTransform, const Rect &theSrcRect, float x, float y
) {
    DrawImageTransformHelper(theImage, theTransform, theSrcRect, x, y, true);
}

void Graphics::DrawTriangleTex(Image *theTexture, const TriVertex &v1, const TriVertex &v2, const TriVertex &v3) {
    // TriVertex v[1][3] = {{v1,v2,v3}};
    const std::array v{v1, v2, v3};
    mDestImage->BltTrianglesTex(
        theTexture, &v, 1, mClipRect, mColorizeImages ? mColor : Color::White, mDrawMode, mTransX, mTransY, mLinearBlend
    );
}

void Graphics::DrawTrianglesTex(Image *theTexture, const std::array<TriVertex, 3> *theVertices, int theNumTriangles) {
    mDestImage->BltTrianglesTex(
        theTexture, theVertices, theNumTriangles, mClipRect, mColorizeImages ? mColor : Color::White, mDrawMode,
        mTransX, mTransY, mLinearBlend
    );
}

void Graphics::ClearClipRect() { mClipRect = Rect(0, 0, mDestImage->GetWidth(), mDestImage->GetHeight()); }

void Graphics::SetClipRect(int theX, int theY, int theWidth, int theHeight) {
    mClipRect = Rect(0, 0, mDestImage->GetWidth(), mDestImage->GetHeight())
                    .Intersection(Rect(theX + mTransX, theY + mTransY, theWidth, theHeight));
}

void Graphics::SetClipRect(const Rect &theRect) {
    SetClipRect(theRect.mX, theRect.mY, theRect.mWidth, theRect.mHeight);
}

void Graphics::ClipRect(int theX, int theY, int theWidth, int theHeight) {
    mClipRect = mClipRect.Intersection(Rect(theX + mTransX, theY + mTransY, theWidth, theHeight));
}

void Graphics::ClipRect(const Rect &theRect) { ClipRect(theRect.mX, theRect.mY, theRect.mWidth, theRect.mHeight); }

void Graphics::Translate(int theTransX, int theTransY) {
    mTransX += theTransX;
    mTransY += theTransY;
}

void Graphics::TranslateF(float theTransX, float theTransY) {
    mTransX += theTransX;
    mTransY += theTransY;
}

void Graphics::SetScale(float theScaleX, float theScaleY, float theOrigX, float theOrigY) {
    mScaleX = theScaleX;
    mScaleY = theScaleY;
    mScaleOrigX = theOrigX + mTransX;
    mScaleOrigY = theOrigY + mTransY;
}

int Graphics::StringWidth(const SexyString &theString) { return mFont->StringWidth(theString); }

void Graphics::DrawImageBox(const Rect &theDest, Image *theComponentImage) {
    DrawImageBox(Rect(0, 0, theComponentImage->mWidth, theComponentImage->mHeight), theDest, theComponentImage);
}

void Graphics::DrawImageBox(const Rect &theSrc, const Rect &theDest, Image *theComponentImage) {
    if (theSrc.mWidth <= 0 || theSrc.mHeight <= 0) return;

    int cw = theSrc.mWidth / 3;
    int ch = theSrc.mHeight / 3;
    int cx = theSrc.mX;
    int cy = theSrc.mY;
    int cmw = theSrc.mWidth - cw * 2;
    int cmh = theSrc.mHeight - ch * 2;

    // Draw 4 corners
    DrawImage(theComponentImage, theDest.mX, theDest.mY, Rect(cx, cy, cw, ch));
    DrawImage(theComponentImage, theDest.mX + theDest.mWidth - cw, theDest.mY, Rect(cx + cw + cmw, cy, cw, ch));
    DrawImage(theComponentImage, theDest.mX, theDest.mY + theDest.mHeight - ch, Rect(cx, cy + ch + cmh, cw, ch));
    DrawImage(
        theComponentImage, theDest.mX + theDest.mWidth - cw, theDest.mY + theDest.mHeight - ch,
        Rect(cx + cw + cmw, cy + ch + cmh, cw, ch)
    );

    // Draw top and bottom
    Graphics aVertClip(*this);
    aVertClip.ClipRect(theDest.mX + cw, theDest.mY, theDest.mWidth - cw * 2, theDest.mHeight);
    int aCol, aRow;
    for (aCol = 0; aCol < (theDest.mWidth - cw * 2 + cmw - 1) / cmw; aCol++) {
        aVertClip.DrawImage(theComponentImage, theDest.mX + cw + aCol * cmw, theDest.mY, Rect(cx + cw, cy, cmw, ch));
        aVertClip.DrawImage(
            theComponentImage, theDest.mX + cw + aCol * cmw, theDest.mY + theDest.mHeight - ch,
            Rect(cx + cw, cy + ch + cmh, cmw, ch)
        );
    }

    // Draw sides
    Graphics aHorzClip(*this);
    aHorzClip.ClipRect(theDest.mX, theDest.mY + ch, theDest.mWidth, theDest.mHeight - ch * 2);
    for (aRow = 0; aRow < (theDest.mHeight - ch * 2 + cmh - 1) / cmh; aRow++) {
        aHorzClip.DrawImage(theComponentImage, theDest.mX, theDest.mY + ch + aRow * cmh, Rect(cx, cy + ch, cw, cmh));
        aHorzClip.DrawImage(
            theComponentImage, theDest.mX + theDest.mWidth - cw, theDest.mY + ch + aRow * cmh,
            Rect(cx + cw + cmw, cy + ch, cw, cmh)
        );
    }

    // Draw middle
    Graphics aMidClip(*this);
    aMidClip.ClipRect(theDest.mX + cw, theDest.mY + ch, theDest.mWidth - cw * 2, theDest.mHeight - ch * 2);
    for (aCol = 0; aCol < (theDest.mWidth - cw * 2 + cmw - 1) / cmw; aCol++)
        for (aRow = 0; aRow < (theDest.mHeight - ch * 2 + cmh - 1) / cmh; aRow++)
            aMidClip.DrawImage(
                theComponentImage, theDest.mX + cw + aCol * cmw, theDest.mY + ch + aRow * cmh,
                Rect(cx + cw, cy + ch, cmw, cmh)
            );
}

void Graphics::DrawImageCel(Image *theImageStrip, int theX, int theY, int theCel) {
    DrawImageCel(theImageStrip, theX, theY, theCel % theImageStrip->mNumCols, theCel / theImageStrip->mNumCols);
}

void Graphics::DrawImageCel(Image *theImageStrip, const Rect &theDestRect, int theCel) {
    DrawImageCel(theImageStrip, theDestRect, theCel % theImageStrip->mNumCols, theCel / theImageStrip->mNumCols);
}

void Graphics::DrawImageCel(Image *theImageStrip, int theX, int theY, int theCelCol, int theCelRow) {
    if (theCelRow < 0 || theCelCol < 0 || theCelRow >= theImageStrip->mNumRows || theCelCol >= theImageStrip->mNumCols)
        return;

    const int aCelWidth = theImageStrip->mWidth / theImageStrip->mNumCols;
    const int aCelHeight = theImageStrip->mHeight / theImageStrip->mNumRows;
    const Rect aSrcRect(aCelWidth * theCelCol, aCelHeight * theCelRow, aCelWidth, aCelHeight);

    DrawImage(theImageStrip, theX, theY, aSrcRect);
}

void Graphics::DrawImageAnim(Image *theImageAnim, int theX, int theY, int theTime) {
    DrawImageCel(theImageAnim, theX, theY, theImageAnim->GetAnimCel(theTime));
}

void Graphics::DrawImageCel(Image *theImageStrip, const Rect &theDestRect, int theCelCol, int theCelRow) {
    if (theCelRow < 0 || theCelCol < 0 || theCelRow >= theImageStrip->mNumRows || theCelCol >= theImageStrip->mNumCols)
        return;

    const int aCelWidth = theImageStrip->mWidth / theImageStrip->mNumCols;
    const int aCelHeight = theImageStrip->mHeight / theImageStrip->mNumRows;
    const Rect aSrcRect(aCelWidth * theCelCol, aCelHeight * theCelRow, aCelWidth, aCelHeight);

    DrawImage(theImageStrip, theDestRect, aSrcRect);
}

int Graphics::WriteString(
    const SexyString &theString, int theX, int theY, int theWidth, int theJustification, bool drawString, int theOffset,
    int theLength, int theOldColor
) {
    // _Font* aFont = GetFont(); // unused
    if (theOldColor == -1) theOldColor = mColor.ToInt();

    if (drawString) {
        switch (theJustification) {
        case 0:
            theX += (theWidth -
                     WriteString(theString, theX, theY, theWidth, -1, false, theOffset, theLength, theOldColor)) /
                    2;
            break;
        case 1:
            theX +=
                theWidth - WriteString(theString, theX, theY, theWidth, -1, false, theOffset, theLength, theOldColor);
            break;
        }
    }

    if (theLength < 0 || theOffset + theLength > static_cast<int>(theString.length()))
        theLength = static_cast<int>(theString.length());
    else theLength = theOffset + theLength;

    SexyString aString;
    int aXOffset = 0;

    for (int i = theOffset; i < theLength; i++) {
        if ((theString[i] == '^') && mWriteColoredString) {
            if (i + 1 < theLength && theString[i + 1] == '^') // literal '^'
            {
                aString += _S('^');
                i++;
            } else if (i > theLength - 8) // badly formatted color specification
                break;
            else // change color instruction
            {
                unsigned long aColor = 0;
                if (theString[i + 1] == _S('o')) {
                    if (strncmp(theString.c_str() + i + 1, _S("oldclr"), 6) == 0) aColor = theOldColor;
                } else {
                    for (int aDigitNum = 0; aDigitNum < 6; aDigitNum++) {
                        const SexyChar aChar = theString[i + aDigitNum + 1];
                        int aVal = 0;

                        if ((aChar >= _S('0')) && (aChar <= _S('9'))) aVal = aChar - _S('0');
                        else if ((aChar >= _S('A')) && (aChar <= _S('F'))) aVal = (aChar - _S('A')) + 10;
                        else if ((aChar >= _S('a')) && (aChar <= _S('f'))) aVal = (aChar - _S('a')) + 10;

                        aColor += (aVal << ((5 - aDigitNum) * 4));
                    }
                }

                if (drawString) {
                    DrawString(aString, theX + aXOffset, theY);
                    SetColor(Color((aColor >> 16) & 0xFF, (aColor >> 8) & 0xFF, (aColor) & 0xFF, GetColor().mAlpha));
                }

                i += 7;

                aXOffset += GetFont()->StringWidth(aString);

                aString = _S("");
            }
        } else aString += theString[i];
    }

    if (drawString) {
        DrawString(aString, theX + aXOffset, theY);
    }

    aXOffset += GetFont()->StringWidth(aString);

    return aXOffset;
}

static int WriteWordWrappedHelper(
    Graphics *g, const SexyString &theString, int theX, int theY, int theWidth, int theJustification, bool drawString,
    int theOffset, int theLength, int theOldColor, int theMaxChars
) {
    if (theOffset + theLength > theMaxChars) {
        theLength = theMaxChars - theOffset;
        if (theLength <= 0) return -1;
    }

    return g->WriteString(
        theString, theX, theY, theWidth, theJustification, drawString, theOffset, theLength, theOldColor
    );
}

int Graphics::WriteWordWrapped(
    const Rect &theRect, const SexyString &theLine, int theLineSpacing, int theJustification, int *theMaxWidth,
    int theMaxChars, int *theLastWidth
) {
    const Color anOrigColor = GetColor();
    int anOrigColorInt = anOrigColor.ToInt();
    if ((anOrigColorInt & 0xFF000000) == 0xFF000000) anOrigColorInt &= ~0xFF000000;

    if (theMaxChars < 0) theMaxChars = static_cast<int>(theLine.length());

    _Font *aFont = GetFont();

    int aYOffset = aFont->GetAscent() - aFont->GetAscentPadding();

    if (theLineSpacing == -1) theLineSpacing = aFont->GetLineSpacing();

    SexyString aCurString;
    ulong aCurPos = 0;
    int aLineStartPos = 0;
    int aCurWidth = 0;
    SexyChar aCurChar = 0;
    SexyChar aPrevChar = 0;
    int aSpacePos = -1;
    int aMaxWidth = 0;
    int anIndentX = 0;

    if (theLastWidth != nullptr) {
        anIndentX = *theLastWidth;
        aCurWidth = anIndentX;
    }

    while (aCurPos < theLine.length()) {
        aCurChar = theLine[aCurPos];
        if (aCurChar == _S('^') && mWriteColoredString) // Handle special color modifier
        {
            if (aCurPos + 1 < theLine.length()) {
                if (theLine[aCurPos + 1] == _S('^')) aCurPos++; // literal '^' -> just skip the extra '^'
                else {
                    aCurPos += 8;
                    continue; // skip the color specifier when calculating the width
                }
            }
        } else if (aCurChar == _S(' ')) aSpacePos = aCurPos;
        else if (aCurChar == _S('\n')) {
            aCurWidth = theRect.mWidth + 1; // force word wrap
            aSpacePos = aCurPos;
            aCurPos++; // skip enter on next go round
        }

        aCurWidth += aFont->CharWidthKern(aCurChar, aPrevChar);
        aPrevChar = aCurChar;

        if (aCurWidth > theRect.mWidth) // need to wrap
        {
            int aWrittenWidth;
            if (aSpacePos != -1) {
                // aWrittenWidth = WriteWordWrappedHelper(this, theLine, theRect.mX, theRect.mY + aYOffset,
                // theRect.mWidth, 	theJustification, true, aLineStartPos, aSpacePos-aLineStartPos, anOrigColorInt,
                //theMaxChars);

                const int aPhysPos = theRect.mY + aYOffset + mTransY;
                if ((aPhysPos >= mClipRect.mY) && (aPhysPos < mClipRect.mY + mClipRect.mHeight + theLineSpacing)) {
                    WriteWordWrappedHelper(
                        this, theLine, theRect.mX + anIndentX, theRect.mY + aYOffset, theRect.mWidth, theJustification,
                        true, aLineStartPos, aSpacePos - aLineStartPos, anOrigColorInt, theMaxChars
                    );

                    /*WriteString(theLine, theRect.mX + anIndentX, theRect.mY + aYOffset, theRect.mWidth,
                    theJustification, true, aLineStartPos, aSpacePos-aLineStartPos);*/
                }

                aWrittenWidth = aCurWidth + anIndentX;

                if (aWrittenWidth < 0) break;

                aCurPos = aSpacePos + 1;
                if (aCurChar != _S('\n')) {
                    while (aCurPos < theLine.length() && theLine[aCurPos] == _S(' '))
                        aCurPos++;
                }
                aLineStartPos = aCurPos;
            } else {
                if (static_cast<int>(aCurPos) < aLineStartPos + 1)
                    aCurPos++; // ensure at least one character gets written

                aWrittenWidth = WriteWordWrappedHelper(
                    this, theLine, theRect.mX + anIndentX, theRect.mY + aYOffset, theRect.mWidth, theJustification,
                    true, aLineStartPos, aCurPos - aLineStartPos, anOrigColorInt, theMaxChars
                );

                if (aWrittenWidth < 0) break;

                if (theMaxWidth != nullptr && aWrittenWidth > *theMaxWidth) *theMaxWidth = aWrittenWidth;
                if (theLastWidth != nullptr) *theLastWidth = aWrittenWidth;
            }

            if (aWrittenWidth > aMaxWidth) aMaxWidth = aWrittenWidth;

            aLineStartPos = aCurPos;
            aSpacePos = -1;
            aCurWidth = 0;
            aPrevChar = 0;
            anIndentX = 0;
            aYOffset += theLineSpacing;
        } else aCurPos++;
    }

    if (aLineStartPos < static_cast<int>(theLine.length())) // write the last piece
    {
        const int aWrittenWidth = WriteWordWrappedHelper(
            this, theLine, theRect.mX + anIndentX, theRect.mY + aYOffset, theRect.mWidth, theJustification, true,
            aLineStartPos, theLine.length() - aLineStartPos, anOrigColorInt, theMaxChars
        );

        if (aWrittenWidth >= 0) {
            if (aWrittenWidth > aMaxWidth) aMaxWidth = aWrittenWidth;

            if (theMaxWidth != nullptr && aWrittenWidth > *theMaxWidth) *theMaxWidth = aWrittenWidth;
            if (theLastWidth != nullptr) *theLastWidth = aWrittenWidth;

            aYOffset += theLineSpacing;
        }
    } else if (aCurChar == '\n') {
        aYOffset += theLineSpacing;
        if (theLastWidth != nullptr) *theLastWidth = 0;
    }

    SetColor(anOrigColor);

    if (theMaxWidth != nullptr) *theMaxWidth = aMaxWidth;

    return aYOffset + aFont->GetDescent() - theLineSpacing;
}

int Graphics::DrawStringColor(const SexyString &theLine, int theX, int theY, int theOldColor) {
    return WriteString(theLine, theX, theY, -1, -1, true, 0, -1, theOldColor);
}

int Graphics::DrawStringWordWrapped(
    const SexyString &theLine, int theX, int theY, int theWrapWidth, int theLineSpacing, int theJustification,
    int *theMaxWidth
) {
    const int aYOffset = mFont->GetAscent() - mFont->GetAscentPadding();

    const Rect aRect(theX, theY - aYOffset, theWrapWidth, 0);
    return WriteWordWrapped(aRect, theLine, theLineSpacing, theJustification, theMaxWidth);
}

int Graphics::GetWordWrappedHeight(int theWidth, const SexyString &theLine, int theLineSpacing, int *theMaxWidth) {
    Graphics aTestG;
    aTestG.SetFont(mFont);
    const int aHeight = aTestG.WriteWordWrapped(Rect(0, 0, theWidth, 0), theLine, theLineSpacing, -1, theMaxWidth);

    return aHeight;
}
