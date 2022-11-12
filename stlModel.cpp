#include "stlModel.h"

#include <iostream>
#include <stl_reader/stl_reader.h>

void StlModel::clear() {
    mNumStlIndices = 0;
    mCoords.clear();
    mNormals.clear();
    mTriangleVertexIndices.clear();
    mSolids.clear();

    mGfxCoords.clear();
    mGfxNormals.clear();
    mGfxTriangleVertexIndices.clear();

    mCenterAndRadius = linAlg::vec4_t{ 0.0f, 0.0f, 0.0f, 0.0f };
    mWasRadiusCalculated = false;
}

Status_t StlModel::load(const std::string& url)
{
    try {
        stl_reader::ReadStlFile(url.c_str(), mCoords, mNormals, mTriangleVertexIndices, mSolids);
    }
    catch (std::exception& e) {
        std::cout << e.what() << std::endl;
    }

    mCenterAndRadius = linAlg::vec4_t{ 0.0f, 0.0f, 0.0f, 0.0f };
    mWasRadiusCalculated = false;

    mGfxCoords.reserve(mTriangleVertexIndices.size());
    mGfxNormals.reserve(mTriangleVertexIndices.size());
    mGfxTriangleVertexIndices.reserve(mTriangleVertexIndices.size());
    uint32_t gfxCurrIdx = 0;
    const size_t numTris = mTriangleVertexIndices.size() / 3;
    for (size_t itri = 0; itri < numTris; ++itri) {
        //std::cout << "coordinates of triangle " << itri << ": ";
        for (size_t icorner = 0; icorner < 3; ++icorner) {
            const float *const c = &mCoords[3 * mTriangleVertexIndices[3 * itri + icorner]];
            //std::cout << "(" << c[0] << ", " << c[1] << ", " << c[2] << ") ";
            //mCenterAndRadius[0] += c[0];
            //mCenterAndRadius[1] += c[1];
            //mCenterAndRadius[2] += c[2];

            mGfxCoords.push_back(c[0]);
            mGfxCoords.push_back(c[1]);
            mGfxCoords.push_back(c[2]);

            float* n = &mNormals[3 * itri];
            mGfxNormals.push_back(n[0]);
            mGfxNormals.push_back(n[1]);
            mGfxNormals.push_back(n[2]);

            mGfxTriangleVertexIndices.push_back(gfxCurrIdx + 0);
            mGfxTriangleVertexIndices.push_back(gfxCurrIdx + 1);
            mGfxTriangleVertexIndices.push_back(gfxCurrIdx + 2);
            gfxCurrIdx += 3;
        }
        //std::cout << std::endl;

        //float* n = &mNormals[3 * itri];
        //std::cout << "normal of triangle " << itri << ": " << "(" << n[0] << ", " << n[1] << ", " << n[2] << ")\n";
    }

    mNumStlIndices = mGfxTriangleVertexIndices.size();

    getBoundingSphere(mCenterAndRadius);
    
    return Status_t::OK();
}

Status_t StlModel::save(const std::string& url, const std::string& comment)
{
    return Status_t::ERROR("StlModel::save() is not implemented yet");
}

const void StlModel::getBoundingSphere(linAlg::vec4_t& centerAndRadius) const
{
    if (mWasRadiusCalculated) {
        centerAndRadius = mCenterAndRadius;
        return;
    }

    const size_t numTris = mTriangleVertexIndices.size() / 3;
    for (size_t itri = 0; itri < numTris; ++itri) {
        //std::cout << "coordinates of triangle " << itri << ": ";
        for (size_t icorner = 0; icorner < 3; ++icorner) {
            const float* const c = &mCoords[3 * mTriangleVertexIndices[3 * itri + icorner]];
            //std::cout << "(" << c[0] << ", " << c[1] << ", " << c[2] << ") ";
            mCenterAndRadius[0] += c[0];
            mCenterAndRadius[1] += c[1];
            mCenterAndRadius[2] += c[2];
        }
    }

    const float fNumCoords = static_cast<float>(mTriangleVertexIndices.size());
    mCenterAndRadius[0] /= fNumCoords;
    mCenterAndRadius[1] /= fNumCoords;
    mCenterAndRadius[2] /= fNumCoords;
    const linAlg::vec3_t sphereCenter{ mCenterAndRadius[0], mCenterAndRadius[1], mCenterAndRadius[2] };
    float maxLen = 0.0f;

    for (size_t itri = 0; itri < numTris; ++itri) {
        for (size_t icorner = 0; icorner < 3; ++icorner) {
            const float *const c = &mCoords[3 * mTriangleVertexIndices[3 * itri + icorner]];
            linAlg::vec3_t coord{ c[0], c[1], c[2] };
            linAlg::vec3_t vecToCenter;
            linAlg::sub< linAlg::vec3_t >(vecToCenter, coord, sphereCenter);
            maxLen = linAlg::maximum(maxLen, linAlg::dot(vecToCenter, vecToCenter));
        }
    }
    mCenterAndRadius[3] = sqrtf(maxLen);
    mWasRadiusCalculated = true;

    centerAndRadius = mCenterAndRadius;

}
