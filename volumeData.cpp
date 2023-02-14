#include "volumeData.h"

#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <limits.h>
#include <omp.h>

#ifndef _USE_MATH_DEFINES
    #define _USE_MATH_DEFINES
#endif
#include <math.h>

#include <algorithm>
//#include <iostream>
#include <limits>

#include <atomic>

using namespace FileLoader;

namespace {
    constexpr float sobel_x[3][9] = {
        {   -1.0f, -3.0f, -1.0f,
            -3.0f, -6.0f, -3.0f,
            -1.0f, -3.0f, -1.0f     },
        {    0.0f,  0.0f,  0.0f,
             0.0f,  0.0f,  0.0f,
             0.0f,  0.0f,  0.0f     },
        {    1.0f,  3.0f,  1.0f,
             3.0f,  6.0f,  3.0f,
             1.0f,  3.0f,  1.0f     },
    };

    constexpr float sobel_z[3][9] = {
        {    1.0f,  3.0f,  1.0f,
             0.0f,  0.0f,  0.0f,
            -1.0f, -3.0f, -1.0f     },
        {    3.0f,  6.0f,  3.0f,
             0.0f,  0.0f,  0.0f,
            -3.0f, -6.0f, -3.0f     },
        {    1.0f,  3.0f,  1.0f,
             0.0f,  0.0f,  0.0f,
            -1.0f, -3.0f, -1.0f     },
    };

    constexpr float sobel_y[3][9] = {
        {   -1.0f,  0.0f,  1.0f,
            -3.0f,  0.0f,  3.0f,
            -1.0f,  0.0f,  1.0f     },
        {   -3.0f,  0.0f,  3.0f,
            -6.0f,  0.0f,  6.0f,
            -3.0f,  0.0f,  3.0f     },
        {   -1.0f,  0.0f,  1.0f,
            -3.0f,  0.0f,  3.0f,
            -1.0f,  0.0f,  1.0f     },
    };

}

eRetVal VolumeData::load( const std::string& fileUrl, const VolumeData::gradientMode_t mode ) {
    //-- set number of threads
    omp_set_num_threads( 8 );
    #pragma omp parallel
    #pragma omp master
    { fprintf( stderr, "using %d threads\n", omp_get_num_threads() ); }

    printf( "reading file '%s'\n", fileUrl.c_str() );

    FILE* pFile = fopen( fileUrl.c_str(), "rb" );
    if (pFile == nullptr) { 
        return eRetVal::ERROR; //Status_t::ERROR( "failed to open VolumeData file" ); 
    }

    size_t elementsRead = 0;
    elementsRead = fread( mDim.data(), sizeof( uint16_t ), 3, pFile );
    assert( elementsRead == 3 );

    printf( "dimensions: %u x %u x %u \n", (uint32_t)mDim[0], (uint32_t)mDim[1], (uint32_t)mDim[2] );

    const uint32_t numVoxels = mDim[0] * mDim[1] * mDim[2];
    mDensities.resize( numVoxels );
    elementsRead = fread( mDensities.data(), sizeof( uint16_t ), numVoxels, pFile );
    assert( elementsRead == numVoxels );

    // from https://www.cg.tuwien.ac.at/research/vis/datasets/
    // The data range is [0,4095].
    mMinMaxDensity[0] = std::numeric_limits<uint16_t>::max();
    mMinMaxDensity[1] = std::numeric_limits<uint16_t>::min();

    const int32_t numDensityEntries = static_cast<int32_t>( mDensities.size() );
    #pragma omp parallel for schedule(dynamic, 1)		// OpenMP 
    //for (const auto& density : mDensities) { // on VS this doesn't work with OpenMP
    for ( int32_t densityIdx = 0; densityIdx < numDensityEntries; densityIdx++ ) {
        const auto& density = mDensities[densityIdx];
        if (density > 0) { // skip density 0 as minimum
            mMinMaxDensity[0] = std::min( mMinMaxDensity[0], density ); 
        }
        mMinMaxDensity[1] = std::max( mMinMaxDensity[1], density );
    }
    if (mMinMaxDensity[0] == mMinMaxDensity[1]) { mMinMaxDensity[0] = 0; }
    assert( mMinMaxDensity[1] <= std::numeric_limits<uint16_t>::max() );
    if (mMinMaxDensity[0] >= mMinMaxDensity[1]) { mMinMaxDensity[1] += 1; }
    
    //mMinMaxDensity[0] = 0;
    //mMinMaxDensity[1] = 4095;

#if 1 // TODO!!!
    mNormals.resize( numVoxels );
    mGradientMode = mode;
    calculateNormals( mGradientMode );
#endif

    return eRetVal::OK; //Status_t::OK();
}


void VolumeData::calculateNormals( const gradientMode_t mode ) {
#pragma omp parallel for schedule(dynamic, 1)		// OpenMP 
    for (int32_t z = 0; z < mDim[2]; z++) { // error C3016: 'z': index variable in OpenMP 'for' statement must have signed integral type
        for (int32_t y = 0; y < mDim[1]; y++) {
            for (int32_t x = 0; x < mDim[0]; x++) {

                if (mode == gradientMode_t::SOBEL_3D) { // 3D Sobel
                    // convolve
                    float sum_x = 0.0f;
                    float kernel_sum_x = 0.0f;
                    for (int32_t off_x = -1; off_x <= +1; off_x++) {
                        const int32_t conv_x = xClamp( x + off_x );
                        const int32_t conv_y = y;
                        const int32_t conv_z = z;
                        uint32_t kernelIdx = 0;
                        //float kernel_sum_x = 0.0f;
                        //float tmp_sum_x = 0.0f;
                        int32_t cx = 0;

                        for (int32_t cz = -1; cz <= 1; cz++) {
                            for (int32_t cy = -1; cy <= 1; cy++) {
                                const uint32_t conv_addr = calcAddrClamped( conv_x + cx, conv_y + cy, conv_z + cz );
                                sum_x += mDensities[conv_addr] * sobel_x[off_x + 1][kernelIdx];
                                kernel_sum_x += fabs( sobel_x[off_x + 1][kernelIdx] );
                                kernelIdx++;
                            }
                        }
                        //sum_x += tmp_sum_x / std::max<float>(fabs( kernel_sum_x ), 1.0f);
                    }
                    //sum_x /= fabs( std::max(kernel_sum_x, 1.0f) );
                    //sum_x /= 3.0f;
                    sum_x /= kernel_sum_x;

                    float sum_y = 0.0f;
                    float kernel_sum_y = 0.0f;
                    for (int32_t off_y = -1; off_y <= +1; off_y++) {
                        const int32_t conv_x = x;
                        const int32_t conv_y = yClamp( y + off_y );
                        const int32_t conv_z = z;
                        uint32_t kernelIdx = 0;
                        //float kernel_sum_y = 0.0f;
                        //float tmp_sum_y = 0.0f;
                        int32_t cy = 0;
                        for (int32_t cz = -1; cz <= 1; cz++) {
                            for (int32_t cx = -1; cx <= 1; cx++) {
                                const uint32_t conv_addr = calcAddrClamped( conv_x + cx, conv_y + cy, conv_z + cz );
                                sum_y += mDensities[conv_addr] * sobel_y[off_y + 1][kernelIdx];
                                kernel_sum_y += fabs( sobel_y[off_y + 1][kernelIdx] );
                                kernelIdx++;
                            }
                        }
                        //sum_y += tmp_sum_y / std::max<float>( fabs( kernel_sum_y ), 1.0f );
                    }
                    //sum_y /= std::max<float>( fabs( kernel_sum_y ), 1.0f);
                    //sum_y /= 3.0f;
                    sum_y /= kernel_sum_y;

                    float sum_z = 0.0f;
                    float kernel_sum_z = 0.0f;
                    for (int32_t off_z = -1; off_z <= +1; off_z++) {
                        const int32_t conv_x = x;
                        const int32_t conv_y = y;
                        const int32_t conv_z = zClamp( z + off_z );
                        uint32_t kernelIdx = 0;
                        //float kernel_sum_z = 0.0f;
                        //float tmp_sum_z = 0.0f;
                        int32_t cz = 0;

                        for (int32_t cy = -1; cy <= 1; cy++) {
                            for (int32_t cx = -1; cx <= 1; cx++) {
                                const uint32_t conv_addr = calcAddrClamped( conv_x + cx, conv_y + cy, conv_z + cz );
                                sum_z += mDensities[conv_addr] * sobel_z[off_z + 1][kernelIdx];
                                kernel_sum_z += fabs( sobel_z[off_z + 1][kernelIdx] );
                                kernelIdx++;
                            }
                        }

                        //sum_z += tmp_sum_z / std::max<float>(fabs(kernel_sum_z), 1.0f);
                    }
                    //sum_z /= std::max<float>(fabs(kernel_sum_z), 1.0f);
                    //sum_z /= 3.0f;
                    sum_z /= kernel_sum_z;

                    const uint32_t addr_center = (z * mDim[1] + y) * mDim[0] + x;
                    mNormals[addr_center][0] = sum_x;
                    mNormals[addr_center][1] = -sum_z;
                    mNormals[addr_center][2] = sum_y;

                } else if (mode == gradientMode_t::CENTRAL_DIFFERENCES) {
                    // TODO!!!
                    //mNormals[z * mDim[1] * mDim[0] + y * mDim[0] + x] = vec3_t{ 0.0f, 1.0f, 0.0f };

                    const int32_t mx = std::max( 0, x - 1 );
                    const int32_t px = std::min( mDim[0] - 1, x + 1 );

                    const int32_t my = std::max( 0, y - 1 );
                    const int32_t py = std::min( mDim[1] - 1, y + 1 );

                    const int32_t mz = std::max( 0, z - 1 );
                    const int32_t pz = std::min( mDim[2] - 1, z + 1 );

                    const uint32_t addr_mx = (z * mDim[1] + y) * mDim[0] + mx;
                    const uint32_t addr_px = (z * mDim[1] + y) * mDim[0] + px;

                    const uint32_t addr_my = (z * mDim[1] + my) * mDim[0] + x;
                    const uint32_t addr_py = (z * mDim[1] + py) * mDim[0] + x;

                    const uint32_t addr_mz = (mz * mDim[1] + y) * mDim[0] + x;
                    const uint32_t addr_pz = (pz * mDim[1] + y) * mDim[0] + x;

                    const uint32_t addr_center = (z * mDim[1] + y) * mDim[0] + x;

                    // central differences
                    mNormals[addr_center][0] = (mDensities[addr_px] - mDensities[addr_mx]) * 0.5f;
                    mNormals[addr_center][1] = (mDensities[addr_py] - mDensities[addr_my]) * 0.5f;
                    mNormals[addr_center][2] = (mDensities[addr_pz] - mDensities[addr_mz]) * 0.5f;
                }
            
            }
        }
    }
    mGradientMode = mode;
}

void FileLoader::VolumeData::calculateHistogramBuckets() {
    const uint32_t numVoxels = mDim[0] * mDim[1] * mDim[2];
    std::array< std::atomic<uint32_t>, mNumHistogramBuckets > mHistogramBucketsAtomic;

#pragma omp parallel for schedule(dynamic, 1)		// OpenMP 
    for ( int64_t i = 0; i < mNumHistogramBuckets; i++ ) {
        mHistogramBucketsAtomic[ i ].store(0u);
    }

#pragma omp parallel for schedule(dynamic, 1)		// OpenMP 
    for ( int64_t i = 0; i < numVoxels; i++ ) {
        const auto density = mDensities[ i ];
        const auto bucketIdx = density / mHistogramDensitiesPerBucket;
        mHistogramBucketsAtomic[ bucketIdx ]++;
    }

#pragma omp parallel for schedule(dynamic, 1)		// OpenMP 
    for ( int64_t i = 0; i < mNumHistogramBuckets; i++ ) {
        mHistogramBuckets[ i ] = mHistogramBucketsAtomic[ i ];
    }
}

void VolumeData::getBoundingSphere( vec4_t& boundingSphere ) {
    const float hx = mDim[0] * 0.5f;
    const float hy = mDim[1] * 0.5f;
    const float hz = mDim[2] * 0.5f;

    const float hr = sqrtf( hx * hx + hy * hy + hz * hz );

    boundingSphere[0] = hx;
    boundingSphere[1] = hy;
    boundingSphere[2] = hz;
    boundingSphere[3] = hr;
}
