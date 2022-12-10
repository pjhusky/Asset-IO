#include "stlModel.h"

#include <iostream>
#include "stl_reader/stl_reader.h"

#ifndef _USE_MATH_DEFINES
    #define _USE_MATH_DEFINES
#endif
#include <math.h>

#include <assert.h>

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
    
    assert( faceNormals.size() % 3 == 0 && "assume there is always a coord-triple xyz in each face normal" );

    // distribute face normals to vertices, assume triangles only
    mNormals.resize( faceNormals.size() * 3 );
    size_t idx = 0;
    for (const auto& faceNormalCoord : faceNormals) {
        mNormals[ idx * 3 + 0 ] = faceNormalCoord;
        mNormals[ idx * 3 + 1 ] = faceNormalCoord;
        mNormals[ idx * 3 + 2 ] = faceNormalCoord;
        idx++;
    }

    std::fill( mNormals.begin(), mNormals.end(), 0.0f );
    for (size_t faceIdx = 0, lastIdx = mIndices.size() / 3; faceIdx < lastIdx; faceIdx++) {
        const uint32_t v0_idx = mIndices[faceIdx * 3 + 0];
        const uint32_t v1_idx = mIndices[faceIdx * 3 + 1];
        const uint32_t v2_idx = mIndices[faceIdx * 3 + 2];

        //const float v0_x = mCoords[ v0_idx * 3 + 0 ];
        //const float v0_y = mCoords[ v0_idx * 3 + 1 ];
        //const float v0_z = mCoords[ v0_idx * 3 + 2 ];

        //const float v1_x = mCoords[ v1_idx * 3 + 0 ];
        //const float v1_y = mCoords[ v1_idx * 3 + 1 ];
        //const float v1_z = mCoords[ v1_idx * 3 + 2 ];

        //const float v2_x = mCoords[ v2_idx * 3 + 0 ];
        //const float v2_y = mCoords[ v2_idx * 3 + 1 ];
        //const float v2_z = mCoords[ v2_idx * 3 + 2 ];

        const float faceNormal_x = faceNormals[ faceIdx * 3 + 0 ];
        const float faceNormal_y = faceNormals[ faceIdx * 3 + 1 ];
        const float faceNormal_z = faceNormals[ faceIdx * 3 + 2 ];

        mNormals[v0_idx * 3 + 0] = 0.5f * ( mNormals[v0_idx * 3 + 0] + faceNormal_x );
        mNormals[v0_idx * 3 + 1] = 0.5f * ( mNormals[v0_idx * 3 + 1] + faceNormal_y );
        mNormals[v0_idx * 3 + 2] = 0.5f * ( mNormals[v0_idx * 3 + 2] + faceNormal_z );

        mNormals[v1_idx * 3 + 0] = 0.5f * ( mNormals[v1_idx * 3 + 0] + faceNormal_x );
        mNormals[v1_idx * 3 + 1] = 0.5f * ( mNormals[v1_idx * 3 + 1] + faceNormal_y );
        mNormals[v1_idx * 3 + 2] = 0.5f * ( mNormals[v1_idx * 3 + 2] + faceNormal_z );

        mNormals[v2_idx * 3 + 0] = 0.5f * ( mNormals[v2_idx * 3 + 0] + faceNormal_x );
        mNormals[v2_idx * 3 + 1] = 0.5f * ( mNormals[v2_idx * 3 + 1] + faceNormal_y );
        mNormals[v2_idx * 3 + 2] = 0.5f * ( mNormals[v2_idx * 3 + 2] + faceNormal_z );
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
