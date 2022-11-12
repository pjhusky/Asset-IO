#ifndef _STLMODEL_H_B1919F73_C820_4A53_9062_39A9B97E0CD3
#define _STLMODEL_H_B1919F73_C820_4A53_9062_39A9B97E0CD3

#include "statusType.h"
#include "math/linAlg.h"

#include <string>
#include <vector>

struct StlModel {
    StlModel()
        : mWasRadiusCalculated(false)
    {}

    void clear();
    Status_t load(const std::string& url);
    Status_t save(const std::string& url, const std::string& comment);

    const void getBoundingSphere(linAlg::vec4_t& centerAndRadius) const;

    const std::vector<float>& coords() const        { return mCoords; }
    const std::vector<float>& normals() const       { return mNormals; }
    const std::vector<uint32_t>& triangleVertexIndices() const { return mTriangleVertexIndices; }

    size_t numStlIndices() const { return mNumStlIndices; }

    const std::vector<float>& gfxCoords() const     { return mGfxCoords; }
    const std::vector<float>& gfxNormals() const    { return mGfxNormals; }
    const std::vector<uint32_t>& gfxTriangleVertexIndices() const { return mGfxTriangleVertexIndices; }

private:
    size_t                                              mNumStlIndices = 0;
    std::vector<float>                                  mCoords;
    std::vector<float>                                  mNormals;
    std::vector<uint32_t>                               mTriangleVertexIndices;
    std::vector<uint32_t>                               mSolids;

    std::vector<float>                                  mGfxCoords;
    std::vector<float>                                  mGfxNormals;
    std::vector<uint32_t>                               mGfxTriangleVertexIndices;

    mutable linAlg::vec4_t                              mCenterAndRadius;
    mutable bool                                        mWasRadiusCalculated;
};


#endif // _STLMODEL_H_B1919F73_C820_4A53_9062_39A9B97E0CD3
