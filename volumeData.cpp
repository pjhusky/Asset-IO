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

    //constexpr float sobel_z[3][9] = {
    constexpr float sobel_y[3][9] = {
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

    //constexpr float sobel_y[3][9] = {
    constexpr float sobel_z[3][9] = {
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


void VolumeData::sobelGradients() {
#if 1
    // https://github.com/snapfinger/sobel-operator/blob/master/v3dedge.c
    float sobelX[3][3][3], sobelY[3][3][3], sobelZ[3][3][3];
    constexpr float hx[3] = { 1.0f, 2.0f, 1.0f };
    constexpr float hy[3] = { 1.0f, 2.0f, 1.0f };
    constexpr float hz[3] = { 1.0f, 2.0f, 1.0f };
    //float hpx[3]={ 1.0f, 0.0f, -1.0f }, hpy[3]={ 1.0f, 0.0f, -1.0f },hpz[3]={ 1.0f, 0.0f, -1.0f };
    constexpr float hpx[3] = { -1.0f, 0.0f, 1.0f };
    constexpr float hpy[3] = { -1.0f, 0.0f, 1.0f };
    constexpr float hpz[3] = { -1.0f, 0.0f, 1.0f };

    //VXparse(&argc, &argv, par); /* parse the command line */
    //V3fread( &im, IVAL);        /* read 3D image */
    //if ( im.type != VX_PBYTE || im.chan != 1) { /* check  format  */
    //    fprintf (stderr, "image not byte type or single channel\kernelY");
    //    exit (1);
    //}   

    //V3fembed(&tm, &im, 1,1,1,1,1,1); /* temp image copy with border */
    //if(VFLAG){
    //    fprintf(stderr,"bbx is %f %f %f %f %f %f\kernelY", im.bbx[0],
    //        im.bbx[1],im.bbx[2],im.bbx[3],im.bbx[4],im.bbx[5]);
    //}

    for (int kernelX = 0; kernelX <= 2; kernelX++) {//build the kernel
        for (int kernelY = 0; kernelY <= 2; kernelY++) {
            for (int kernelZ = 0; kernelZ <= 2; kernelZ++) {
                sobelX[kernelX][kernelY][kernelZ] = hpx[kernelX] * hy[kernelY] * hz[kernelZ];
                sobelY[kernelX][kernelY][kernelZ] = hx[kernelX] * hpy[kernelY] * hz[kernelZ];
                sobelZ[kernelX][kernelY][kernelZ] = hx[kernelX] * hy[kernelY] * hpz[kernelZ];
            }
        }
    }

#pragma omp parallel for schedule(dynamic, 1) // OpenMP 
    for (int32_t z = 0; z < mDim[2]; z++) { //convolve 
        for (int32_t y = 0; y < mDim[1]; y++) {
            for (int32_t x = 0; x < mDim[0]; x++) {
                float sumx = 0.0f;
                float sumy = 0.0f;
                float sumz = 0.0f;
                for( int kernelX = -1; kernelX <= 1; kernelX++ ) {
                    for( int kernelY = -1; kernelY <= 1; kernelY++ ) {
                        for( int kernelZ = -1; kernelZ <= 1; kernelZ++ ) {
                            const auto addr = calcAddrClamped( x + kernelX, y + kernelY, z + kernelZ );
                            const auto density = mDensities[addr];
                            //sumx+=sobelX[kernelX+1][kernelY+1][kernelZ+1]*tm.u[z-kernelX][y-kernelY][x-kernelZ];
                            //sumy+=sobelY[kernelX+1][kernelY+1][kernelZ+1]*tm.u[z-kernelX][y-kernelY][x-kernelZ];
                            //sumz+=sobelZ[kernelX+1][kernelY+1][kernelZ+1]*tm.u[z-kernelX][y-kernelY][x-kernelZ
                            sumx += sobelX[kernelX + 1][kernelY + 1][kernelZ + 1] * density;
                            sumy += sobelY[kernelX + 1][kernelY + 1][kernelZ + 1] * density;
                            sumz += sobelZ[kernelX + 1][kernelY + 1][kernelZ + 1] * density;
                        } 
                    }
                }		
                //sumx/=16.0f; 
                //sumy/=16.0f;
                //sumz/=16.0f;
                sumx/=32.0f; 
                sumy/=32.0f;
                sumz/=32.0f;

                //temp=abs(sumx)+abs(sumy)+abs(sumz);//approximation of the gradient magnitude
                //temp=sqrt(sumx*sumx+sumy*sumy+sumz*sumz); //or use this more presise computation instead of line above
                //im.u[z][y][x]=temp>50?255:0; //threshold at 50

                //const uint32_t addr_center = (z * mDim[1] + y) * mDim[0] + x;
                const uint32_t addr_center = calcAddr( x, y, z );
                mNormals[addr_center][0] = sumx;
                mNormals[addr_center][1] = sumy;
                mNormals[addr_center][2] = sumz;

            }
        }
    }   
#else
    for (int32_t z = 0; z < mDim[2]; z++) { // error C3016: 'z': index variable in OpenMP 'for' statement must have signed integral type
        for (int32_t y = 0; y < mDim[1]; y++) {
            for (int32_t x = 0; x < mDim[0]; x++) {

                // convolve
                float sum_x = 0.0f;
                float kernel_sum_x = 0.0f;
                for (int32_t off_x = -1; off_x <= +1; off_x++) {
                    const int32_t conv_x = xClamp( x + off_x );
                    const int32_t conv_y = y;
                    const int32_t conv_z = z;
                    uint32_t kernelIdx = 0;
                    int32_t cx = 0;
                    
                    for (int32_t cy = -1; cy <= 1; cy++) {
                        //for (int32_t cz = -1; cz <= 1; cz++) {
                        for (int32_t cz = +1; cz >= -1; cz--) {
                            const uint32_t conv_addr = calcAddrClamped( conv_x + cx, conv_y + cy, conv_z + cz );
                            sum_x += mDensities[conv_addr] * sobel_x[off_x + 1][kernelIdx];
                            kernel_sum_x += fabsf( sobel_x[off_x + 1][kernelIdx] );
                            //kernel_sum_x += sobel_x[off_x + 1][kernelIdx];
                            kernelIdx++;
                        }
                    }
                }
                if (kernel_sum_x != 0.0) { sum_x /= kernel_sum_x; }
                

                float sum_y = 0.0f;
                float kernel_sum_y = 0.0f;
                for (int32_t off_y = -1; off_y <= +1; off_y++) {
                    const int32_t conv_x = x;
                    const int32_t conv_y = yClamp( y + off_y );
                    const int32_t conv_z = z;
                    uint32_t kernelIdx = 0;
                    int32_t cy = 0;                    
                    for (int32_t cz = +1; cz >= -1; cz--) {
                        for (int32_t cx = -1; cx <= 1; cx++) {
                            const uint32_t conv_addr = calcAddrClamped( conv_x + cx, conv_y + cy, conv_z + cz );
                            sum_y += mDensities[conv_addr] * sobel_y[off_y + 1][kernelIdx];
                            kernel_sum_y += fabsf( sobel_y[off_y + 1][kernelIdx] );
                            //kernel_sum_y += sobel_y[off_y + 1][kernelIdx];
                            kernelIdx++;
                        }
                    }
                }
                if (kernel_sum_y != 0.0) { sum_y /= kernel_sum_y; }

                float sum_z = 0.0f;
                float kernel_sum_z = 0.0f;                
                for (int32_t off_z = -1; off_z <= +1; off_z++) {
                    const int32_t conv_x = x;
                    const int32_t conv_y = y;
                    const int32_t conv_z = zClamp( z + off_z );
                    uint32_t kernelIdx = 0;
                    int32_t cz = 0;
                    //for (int32_t cy = -1; cy <= 1; cy++) {
                    //    for (int32_t cx = -1; cx <= 1; cx++) {
                    for (int32_t cx = -1; cx <= 1; cx++) {
                        for (int32_t cy = +1; cy >= 1; cy--) {
                            const uint32_t conv_addr = calcAddrClamped( conv_x + cx, conv_y + cy, conv_z + cz );
                            sum_z += mDensities[conv_addr] * sobel_z[off_z + 1][kernelIdx];
                            kernel_sum_z += fabsf( sobel_z[off_z + 1][kernelIdx] );
                            //kernel_sum_z += sobel_z[off_z + 1][kernelIdx];
                            kernelIdx++;
                        }
                    }
                }
                if (kernel_sum_z != 0.0) { sum_z /= kernel_sum_z; }

                //const float recipLen = 1.0f / sqrtf( sum_x * sum_x + sum_y * sum_y + sum_z * sum_z );
                //const float recipLen = 1.0f / ( fabsf( sum_x ) + fabsf( sum_y ) + fabsf( sum_z ) );
                const float recipLen = 1.0f;

                const uint32_t addr_center = calcAddr( x, y, z );
                //mNormals[addr_center][0] =  sum_x * recipLen;
                //mNormals[addr_center][1] = -sum_z * recipLen;
                //mNormals[addr_center][2] =  sum_y * recipLen;

                //  1  1  1
                //  1  1 -1
                //  1 -1  1
                //  1 -1 -1
                // -1  1  1
                // -1  1 -1
                // -1 -1  1
                // -1 -1 -1
                mNormals[addr_center][0] = sum_x * recipLen;
                mNormals[addr_center][1] = sum_y * recipLen;
                mNormals[addr_center][2] = sum_z * recipLen;
            }
        }
    }
#endif
}

void VolumeData::centralDifferencesGradients() {
#pragma omp parallel for schedule(dynamic, 1) // OpenMP
    for (int32_t z = 0; z < mDim[2]; z++) { // error C3016: 'z': index variable in OpenMP 'for' statement must have signed integral type
        for (int32_t y = 0; y < mDim[1]; y++) {
            for (int32_t x = 0; x < mDim[0]; x++) {

                //const int32_t mx = std::max( 0, x - 1 );
                //const int32_t px = std::min( mDim[0] - 1, x + 1 );

                //const int32_t my = std::max( 0, y - 1 );
                //const int32_t py = std::min( mDim[1] - 1, y + 1 );

                //const int32_t mz = std::max( 0, z - 1 );
                //const int32_t pz = std::min( mDim[2] - 1, z + 1 );

                //const uint32_t addr_mx = (z * mDim[1] + y) * mDim[0] + mx;
                //const uint32_t addr_px = (z * mDim[1] + y) * mDim[0] + px;

                //const uint32_t addr_my = (z * mDim[1] + my) * mDim[0] + x;
                //const uint32_t addr_py = (z * mDim[1] + py) * mDim[0] + x;

                //const uint32_t addr_mz = (mz * mDim[1] + y) * mDim[0] + x;
                //const uint32_t addr_pz = (pz * mDim[1] + y) * mDim[0] + x;

                //const uint32_t addr_center = (z * mDim[1] + y) * mDim[0] + x;

                // central differences
                //mNormals[addr_center][0] = (mDensities[addr_px] - mDensities[addr_mx]) * 0.5f;
                //mNormals[addr_center][1] = (mDensities[addr_py] - mDensities[addr_my]) * 0.5f;
                //mNormals[addr_center][2] = (mDensities[addr_pz] - mDensities[addr_mz]) * 0.5f;

                const uint32_t addr_center = calcAddr( x, y, z );
                mNormals[addr_center][0] = (mDensities[ calcAddrClamped( x + 1, y    , z     ) ] - mDensities[ calcAddrClamped( x - 1, y    , z     ) ]) * 0.5f;
                mNormals[addr_center][1] = (mDensities[ calcAddrClamped( x    , y + 1, z     ) ] - mDensities[ calcAddrClamped( x    , y - 1, z     ) ]) * 0.5f;
                mNormals[addr_center][2] = (mDensities[ calcAddrClamped( x    , y    , z + 1 ) ] - mDensities[ calcAddrClamped( x    , y    , z - 1 ) ]) * 0.5f;
            }
        }
    }
}


void VolumeData::calculateNormals( const gradientMode_t mode ) {

    if (mode == gradientMode_t::SOBEL_3D) {
        sobelGradients();
    } else if (mode == gradientMode_t::CENTRAL_DIFFERENCES) {
        centralDifferencesGradients();
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
