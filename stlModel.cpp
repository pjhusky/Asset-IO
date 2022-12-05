#include "stlModel.h"

#include <iostream>
#include "stl_reader/stl_reader.h"

#ifndef _USE_MATH_DEFINES
    #define _USE_MATH_DEFINES
#endif
#include <math.h>

using namespace FileLoader;

namespace {
    template<typename val_T, uint32_t num_T>
    static val_T dot( const std::array<val_T, num_T>& v1, const std::array<val_T, num_T>& v2 ) {
        val_T result = val_T( 0 );
        for (uint32_t i = 0; i < num_T; i++) {
            result += v1[i] * v2[i];
        }
        return result;
    }
}

void StlModel::clear() {
    mCoords.clear();
    mNormals.clear();
    mIndices.clear();
    mSolids.clear();

    mCenterAndRadius = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f };
    mWasRadiusCalculated = false;
}

eRetVal StlModel::load(const std::string& url)
{
    std::vector<float> faceNormals;
    try {
        stl_reader::ReadStlFile(url.c_str(), mCoords, faceNormals, mIndices, mSolids);
    }
    catch (std::exception& e) {
        std::cout << e.what() << std::endl;
    }

    mCenterAndRadius = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f };
    mWasRadiusCalculated = false;

    // distribute face normals to vertices, assume triangles only
    mNormals.resize( faceNormals.size() * 3 );
    size_t idx = 0;
    for (const auto& faceNormalCoord : faceNormals) {
        mNormals[ ( idx + 0 ) * 3 ] = faceNormalCoord;
        mNormals[ ( idx + 1 ) * 3 ] = faceNormalCoord;
        mNormals[ ( idx + 2 ) * 3 ] = faceNormalCoord;
        idx++;
    }
    
    getBoundingSphere(mCenterAndRadius);
    
    return eRetVal::OK;//();
}

eRetVal StlModel::save(const std::string& url, const std::string& comment)
{
    return eRetVal::ERROR; //("StlModel::save() is not implemented yet");
}

const void StlModel::getBoundingSphere(std::array<float, 4>& centerAndRadius) const
{
    if (mWasRadiusCalculated) {
        centerAndRadius = mCenterAndRadius;
        return;
    }

    const size_t numTris = mIndices.size() / 3;
    for (size_t itri = 0; itri < numTris; ++itri) {
        //std::cout << "coordinates of triangle " << itri << ": ";
        for (size_t icorner = 0; icorner < 3; ++icorner) {
            const float* const c = &mCoords[3 * mIndices[3 * itri + icorner]];
            //std::cout << "(" << c[0] << ", " << c[1] << ", " << c[2] << ") ";
            mCenterAndRadius[0] += c[0];
            mCenterAndRadius[1] += c[1];
            mCenterAndRadius[2] += c[2];
        }
    }

    const float fNumCoords = static_cast<float>(mIndices.size());
    mCenterAndRadius[0] /= fNumCoords;
    mCenterAndRadius[1] /= fNumCoords;
    mCenterAndRadius[2] /= fNumCoords;
    const std::array<float, 3> sphereCenter{ mCenterAndRadius[0], mCenterAndRadius[1], mCenterAndRadius[2] };
    float maxLen = 0.0f;

    for (size_t itri = 0; itri < numTris; ++itri) {
        for (size_t icorner = 0; icorner < 3; ++icorner) {
            const float *const c = &mCoords[3 * mIndices[3 * itri + icorner]];
            std::array<float, uint32_t{3u}> coord{ c[0], c[1], c[2] };

            std::array<float, uint32_t{3u}> vecToCenter{ 
                coord[0] - sphereCenter[0],
                coord[1] - sphereCenter[1],
                coord[2] - sphereCenter[2],
            };

            maxLen = std::max( maxLen, dot<float, uint32_t{3u}>( vecToCenter, vecToCenter ) );
        }
    }
    mCenterAndRadius[3] = sqrtf(maxLen);
    mWasRadiusCalculated = true;

    centerAndRadius = mCenterAndRadius;
}
