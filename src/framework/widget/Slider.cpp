#include "Slider.h"
#include "SexyAppBase.h"
#include "SliderListener.h"
#include "WidgetManager.h"
#include "graphics/Graphics.h"
#include "graphics/Image.h"

using namespace Sexy;

Slider::Slider(Image *theTrackImage, Image *theThumbImage, int theId, SliderListener *theListener)
    : mListener(theListener), mVal(0.0), mId(theId), mTrackImage(theTrackImage), mThumbImage(theThumbImage) {
    mDragging = false;
    mHorizontal = true;
    mRelX = mRelY = 0;
}

void Slider::SetValue(double theValue) {
    mVal = theValue;
    if (mVal < 0.0) mVal = 0.0;
    else if (mVal > 1.0) mVal = 1.0;

    MarkDirtyFull();
}

bool Slider::HasTransparencies() { return true; }

void Slider::Draw(Graphics *g) {
    if (mTrackImage != NULL) {
        int cw = mHorizontal ? mTrackImage->GetWidth() / 3 : mTrackImage->GetWidth();
        int ch = mHorizontal ? mTrackImage->GetHeight() : mTrackImage->GetHeight() / 3;

        if (mHorizontal) {
            int ty = (mHeight - ch) / 2;

            g->DrawImage(mTrackImage, 0, ty, Rect(0, 0, cw, ch));

            Graphics aClipG(*g);
            aClipG.ClipRect(cw, ty, mWidth - cw * 2, ch);
            for (int i = 0; i < (mWidth - cw * 2 + cw - 1) / cw; i++)
                aClipG.DrawImage(mTrackImage, cw + i * cw, ty, Rect(cw, 0, cw, ch));

            g->DrawImage(mTrackImage, mWidth - cw, ty, Rect(cw * 2, 0, cw, ch));
        } else {
            g->DrawImage(mTrackImage, 0, 0, Rect(0, 0, cw, ch));
            Graphics aClipG(*g);
            aClipG.ClipRect(0, ch, cw, mHeight - ch * 2);
            for (int i = 0; i < (mHeight - ch * 2 + ch - 1) / ch; i++)
                aClipG.DrawImage(mTrackImage, 0, ch + i * ch, Rect(0, ch, cw, ch));

            g->DrawImage(mTrackImage, 0, mHeight - ch, Rect(0, ch * 2, cw, ch));
        }
    }

    if (mHorizontal && (mThumbImage != NULL))
        g->DrawImage(
            mThumbImage, static_cast<int>(mVal * (mWidth - mThumbImage->GetWidth())),
            (mHeight - mThumbImage->GetHeight()) / 2
        );
    else if (!mHorizontal && (mThumbImage != NULL))
        g->DrawImage(
            mThumbImage, (mWidth - mThumbImage->GetWidth()) / 2,
            static_cast<int>(mVal * (mHeight - mThumbImage->GetHeight()))
        );

    // g->SetColor(Color(255, 255, 0));
    // g->FillRect(0, 0, mWidth, mHeight);
}

void Slider::MouseDown(int x, int y, int theClickCount) {
    (void)theClickCount;
    if (mHorizontal) {
        int aThumbX = static_cast<int>(mVal * (mWidth - mThumbImage->GetWidth()));

        if ((x >= aThumbX) && (x < aThumbX + mThumbImage->GetWidth())) {
            mWidgetManager->mApp->SetCursor(CURSOR_DRAGGING);
            mDragging = true;
            mRelX = x - aThumbX;
        } else {
            // clicked on the bar, set position to mouse click
            double pos = static_cast<double>(x) / mWidth;
            SetValue(pos);
        }
    } else {
        int aThumbY = static_cast<int>(mVal * (mHeight - mThumbImage->GetHeight()));

        if ((y >= aThumbY) && (y < aThumbY + mThumbImage->GetHeight())) {
            mWidgetManager->mApp->SetCursor(CURSOR_DRAGGING);
            mDragging = true;
            mRelY = y - aThumbY;
        } else {
            // clicked on the bar, set position to mouse click
            double pos = static_cast<double>(y) / mHeight;
            SetValue(pos);
        }
    }
}

void Slider::MouseMove(int x, int y) {
    if (mHorizontal) {
        int aThumbX = static_cast<int>(mVal * (mWidth - mThumbImage->GetWidth()));

        if ((x >= aThumbX) && (x < aThumbX + mThumbImage->GetWidth())) mWidgetManager->mApp->SetCursor(CURSOR_DRAGGING);
        else mWidgetManager->mApp->SetCursor(CURSOR_POINTER);
    } else {
        int aThumbY = static_cast<int>(mVal * (mHeight - mThumbImage->GetHeight()));

        if ((y >= aThumbY) && (y < aThumbY + mThumbImage->GetHeight()))
            mWidgetManager->mApp->SetCursor(CURSOR_DRAGGING);
        else mWidgetManager->mApp->SetCursor(CURSOR_POINTER);
    }
}

void Slider::MouseDrag(int x, int y) {
    if (mDragging) {
        double anOldVal = mVal;

        if (mHorizontal) mVal = (x - mRelX) / static_cast<double>(mWidth - mThumbImage->GetWidth());
        else mVal = (y - mRelY) / static_cast<double>(mHeight - mThumbImage->GetHeight());

        if (mVal < 0.0) mVal = 0.0;
        if (mVal > 1.0) mVal = 1.0;

        if (mVal != anOldVal) {
            mListener->SliderVal(mId, mVal);
            MarkDirtyFull();
        }
    }
}

void Slider::MouseUp(int x, int y) {
    (void)x;
    (void)y;
    mDragging = false;
    mWidgetManager->mApp->SetCursor(CURSOR_POINTER);
    mListener->SliderVal(mId, mVal);
}

void Slider::MouseLeave() {
    if (!mDragging) mWidgetManager->mApp->SetCursor(CURSOR_POINTER);
}
