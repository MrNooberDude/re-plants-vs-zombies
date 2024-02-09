#include "SexyMatrix.h"

#include <math.h>

using namespace Sexy;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
SexyMatrix3::SexyMatrix3() {}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SexyMatrix3::ZeroMatrix() { mat = glm::zero<glm::mat3>(); }

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SexyMatrix3::LoadIdentity() { mat = glm::identity<glm::mat3>(); }

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
SexyMatrix3 SexyMatrix3::operator*(const SexyMatrix3 &theMat) const {
    SexyMatrix3 aResult(theMat.mat * mat);
    return aResult;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
SexyVector2 SexyMatrix3::operator*(const SexyVector2 &theVec) const {
    return {m00 * theVec.x + m01 * theVec.y + m02, m10 * theVec.x + m11 * theVec.y + m12};
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
SexyVector3 SexyMatrix3::operator*(const SexyVector3 &theVec) const { return {theVec.vec * mat}; }

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
const SexyMatrix3 &SexyMatrix3::operator*=(const SexyMatrix3 &theMat) { return operator=(operator*(theMat)); }

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
SexyTransform2D::SexyTransform2D() { LoadIdentity(); }

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
SexyTransform2D::SexyTransform2D(bool loadIdentity) {
    if (loadIdentity) LoadIdentity();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
SexyTransform2D::SexyTransform2D(const SexyMatrix3 &theMatrix) : SexyMatrix3(theMatrix) {}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
const SexyTransform2D &SexyTransform2D::operator=(const SexyMatrix3 &theMat) {
    SexyMatrix3::operator=(theMat);
    return *this;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SexyTransform2D::Translate(float tx, float ty) {
    SexyMatrix3 aMat;
    aMat.LoadIdentity();
    aMat.m02 = tx;
    aMat.m12 = ty;
    aMat.m22 = 1;

    *this = aMat * (*this);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SexyTransform2D::RotateRad(float rot) {
    SexyMatrix3 aMat;
    aMat.LoadIdentity();

    const float sinRot = -sinf(rot);
    const float cosRot = cosf(rot);

    aMat.m00 = cosRot;
    aMat.m01 = -sinRot;
    aMat.m10 = sinRot;
    aMat.m11 = cosRot;

    *this = aMat * (*this);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SexyTransform2D::RotateDeg(float rot) { RotateRad(3.1415926535897932384626433832795028841971f * rot / 180.0f); }

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SexyTransform2D::Scale(float sx, float sy) {
    SexyMatrix3 aMat;
    aMat.LoadIdentity();
    aMat.m00 = sx;
    aMat.m11 = sy;

    *this = aMat * (*this);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
Transform::Transform() : mMatrix(false) { Reset(); }

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Transform::Reset() {
    mNeedCalcMatrix = true;
    mComplex = false;
    mTransX1 = mTransY1 = 0;
    mTransX2 = mTransY2 = 0;
    mScaleX = mScaleY = 1;
    mRot = 0;
    mHaveRot = false;
    mHaveScale = false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Transform::Translate(float tx, float ty) {
    if (!mComplex) {
        mNeedCalcMatrix = true;
        if (mHaveRot || mHaveScale) {
            mTransX2 += tx;
            mTransY2 += ty;
        } else {
            mTransX1 += tx;
            mTransY1 += ty;
        }
    } else mMatrix.Translate(tx, ty);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Transform::RotateRad(float rot) {
    if (!mComplex) {
        if (mHaveScale) {
            MakeComplex();
            mMatrix.RotateRad(rot);
        } else {
            mNeedCalcMatrix = true;
            mHaveRot = true;
            mRot += rot;
        }
    } else mMatrix.RotateRad(rot);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Transform::RotateDeg(float rot) {
    Transform::RotateRad(3.1415926535897932384626433832795028841971f * rot / 180.0f);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Transform::Scale(float sx, float sy) {
    if (!mComplex) {
        if (mHaveRot || mTransX1 != 0 || mTransY1 != 0 || (sx < 0 && mScaleX * sx != -1) || sy < 0) {
            MakeComplex();
            mMatrix.Scale(sx, sy);
        } else {
            mNeedCalcMatrix = true;
            mHaveScale = true;
            mScaleX *= sx;
            mScaleY *= sy;
        }
    } else mMatrix.Scale(sx, sy);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Transform::MakeComplex() {
    if (!mComplex) {
        mComplex = true;
        CalcMatrix();
    }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Transform::CalcMatrix() const {
    if (mNeedCalcMatrix) {
        mNeedCalcMatrix = false;

        mMatrix.LoadIdentity();
        mMatrix.m02 = mTransX1;
        mMatrix.m12 = mTransY1;
        mMatrix.m22 = 1;

        if (mHaveScale) {
            mMatrix.m00 = mScaleX;
            mMatrix.m11 = mScaleY;
        } else if (mHaveRot) mMatrix.RotateRad(mRot);

        if (mTransX2 != 0 || mTransY2 != 0) mMatrix.Translate(mTransX2, mTransY2);
    }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
const SexyTransform2D &Transform::GetMatrix() const {
    CalcMatrix();
    return mMatrix;
}
