#include "volumeData.h"

//#include <inttypes.h>
#include <stdio.h>
#include <assert.h>

#include <omp.h>

Status_t VolumeData::load( const std::string& fileUrl ) {
    //-- set number of threads
    omp_set_num_threads( 8 );
    #pragma omp parallel
    #pragma omp master
    { fprintf( stderr, "using %d threads\n", omp_get_num_threads() ); }

    printf( "reading file '%s'\n", fileUrl.c_str() );

    FILE* pFile = fopen( fileUrl.c_str(), "rb" );
    if (pFile == nullptr) { 
        return Status_t::ERROR( "failed to open VolumeData file" ); 
    }

    size_t elementsRead = 0;
    elementsRead = fread( mDim.data(), sizeof( uint16_t ), 3, pFile );

    printf( "dimensions: %u x %u x %u \n", (uint32_t)mDim[0], (uint32_t)mDim[1], (uint32_t)mDim[2] );

    const uint32_t numVoxels = mDim[0] * mDim[1] * mDim[2];
    mDensities.resize( numVoxels );
    fread( mDensities.data(), sizeof( uint16_t ), numVoxels, pFile );

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
            mMinMaxDensity[0] = linAlg::minimum( mMinMaxDensity[0], density ); 
        }
        mMinMaxDensity[1] = linAlg::maximum( mMinMaxDensity[1], density );
    }
    if (mMinMaxDensity[0] == mMinMaxDensity[1]) { mMinMaxDensity[0] = 0; }
    assert( mMinMaxDensity[1] <= std::numeric_limits<uint16_t>::max() );
    if (mMinMaxDensity[0] >= mMinMaxDensity[1]) { mMinMaxDensity[1] += 1; }
    
    //mMinMaxDensity[0] = 0;
    //mMinMaxDensity[1] = 4095;

#if 0 // TODO!!!
    mNormals.resize( numVoxels );
    calculateNormals();
#endif

    return Status_t::OK();
}

void VolumeData::calculateNormals() {
#pragma omp parallel for schedule(dynamic, 1)		// OpenMP 
    for (int32_t z = 0; z < mDim[2]; z++) { // error C3016: 'z': index variable in OpenMP 'for' statement must have signed integral type
        for (uint32_t y = 0; y < mDim[1]; y++) {
            for (uint32_t x = 0; x < mDim[0]; x++) {
                // TODO!!!
                mNormals[z * mDim[1] * mDim[0] + y * mDim[0] + x] = linAlg::vec3_t{ 0.0f, 1.0f, 0.0f };
                //mDensities[z * mDim[1] * mDim[0] + y * mDim[1] + x] = 15000;
                //mDensities[z * mDim[1] * mDim[0] + y * mDim[0] + x] = 1.0f;
            }
        }
    }
}



void VolumeData::getBoundingSphere( linAlg::vec4_t& boundingSphere ) {
    const float hx = mDim[0] * 0.5f;
    const float hy = mDim[1] * 0.5f;
    const float hz = mDim[2] * 0.5f;

    const float hr = sqrtf( hx * hx + hy * hy + hz * hz );

    boundingSphere[0] = hx;
    boundingSphere[1] = hy;
    boundingSphere[2] = hz;
    boundingSphere[3] = hr;
}
