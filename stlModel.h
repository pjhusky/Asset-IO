#ifndef _STLMODEL_H_B1919F73_C820_4A53_9062_39A9B97E0CD3
#define _STLMODEL_H_B1919F73_C820_4A53_9062_39A9B97E0CD3

#include "eRetVal_FileLoader.h"

#include <string>
#include <vector>
#include <array>

namespace FileLoader {
    struct StlModel {
        StlModel()
            : mWasRadiusCalculated(false)
        {}

        void clear();
        eRetVal load(const std::string& url);
        eRetVal save(const std::string& url, const std::string& comment);

        const void getBoundingSphere(std::array<float, 4>& centerAndRadius) const;

        const std::vector<float>& coords() const        { return mCoords; }
        const std::vector<float>& normals() const       { return mNormals; }
        const std::vector<uint32_t>& indices() const { return mTriangleVertexIndices; }

    private:
        std::vector<float>                                  mCoords;
        std::vector<float>                                  mNormals;
        std::vector<uint32_t>                               mTriangleVertexIndices;
        std::vector<uint32_t>                               mSolids;

        mutable std::array<float, 4>                        mCenterAndRadius;
        mutable bool                                        mWasRadiusCalculated;
    };
}
#endif // _STLMODEL_H_B1919F73_C820_4A53_9062_39A9B97E0CD3
