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

eRetVal VolumeData::load( const std::string& fileUrl ) {
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
    calculateNormals();
#endif

    return eRetVal::OK; //Status_t::OK();
}

void VolumeData::calculateNormals() {
#pragma omp parallel for schedule(dynamic, 1)		// OpenMP 
    for (int32_t z = 0; z < mDim[2]; z++) { // error C3016: 'z': index variable in OpenMP 'for' statement must have signed integral type
        for (int32_t y = 0; y < mDim[1]; y++) {
            for (int32_t x = 0; x < mDim[0]; x++) {
                // TODO!!!
                //mNormals[z * mDim[1] * mDim[0] + y * mDim[0] + x] = vec3_t{ 0.0f, 1.0f, 0.0f };

                const int32_t mx = std::max( 0, x - 1 );
                const int32_t px = std::min( mDim[0] - 1, x + 1 );

                const int32_t my = std::max( 0, y - 1 );
                const int32_t py = std::min( mDim[1] - 1, y + 1 );

                const int32_t mz = std::max( 0, z - 1 );
                const int32_t pz = std::min( mDim[2] - 1, z + 1 );

                const uint32_t addr_mx = (  z * mDim[1] + y  ) * mDim[0] + mx;
                const uint32_t addr_px = (  z * mDim[1] + y  ) * mDim[0] + px;

                const uint32_t addr_my = (  z * mDim[1] + my ) * mDim[0] + x;
                const uint32_t addr_py = (  z * mDim[1] + py ) * mDim[0] + x;

                const uint32_t addr_mz = ( mz * mDim[1] + y )  * mDim[0] + x;
                const uint32_t addr_pz = ( pz * mDim[1] + y )  * mDim[0] + x;

                const uint32_t addr_center = z * mDim[1] * mDim[0] + y * mDim[0] + x;

                mNormals[ addr_center ][0] = (mDensities[addr_px] - mDensities[addr_mx]) * 0.5f;
                mNormals[ addr_center ][1] = (mDensities[addr_py] - mDensities[addr_my]) * 0.5f;
                mNormals[ addr_center ][2] = (mDensities[addr_pz] - mDensities[addr_mz]) * 0.5f;

                //mDensities[z * mDim[1] * mDim[0] + y * mDim[1] + x] = 15000;
                //mDensities[z * mDim[1] * mDim[0] + y * mDim[0] + x] = 1.0f;
            }
        }
    }
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
