#ifndef _VOLUMEDATA_H_EA89F308_240F_4AE0_97B5_AFE55000B453
#define _VOLUMEDATA_H_EA89F308_240F_4AE0_97B5_AFE55000B453

#include "eRetVal_FileLoader.h"

// https://stackoverflow.com/questions/7597025/difference-between-stdint-h-and-inttypes-h
#include <stdint.h>

#include <string>
#include <vector>
#include <array>

namespace FileLoader{
    struct VolumeData {
        
        using u16vec2_t = std::array<uint16_t, 2>;
        using u16vec3_t = std::array<uint16_t, 3>;
        using u32vec2_t = std::array<uint32_t, 2>;
        using u32vec3_t = std::array<uint32_t, 3>;
        using vec3_t = std::array<float, 3>;
        using vec4_t = std::array<float, 4>;

        enum class gradientMode_t {
            CENTRAL_DIFFERENCES = 0,
            SOBEL_3D            = 1,
        };

        eRetVal load( const std::string& fileUrl, const gradientMode_t mode );
        
        void calculateNormals( const gradientMode_t mode );
        gradientMode_t getGradientMode() const { return mGradientMode; }
        
        void calculateHistogramBuckets();
        
        void getBoundingSphere( vec4_t& boundingSphere );
        inline u16vec3_t getDim() const { return mDim; }

        inline std::vector< uint16_t >& getDensities() { return mDensities; }
        inline const std::vector< uint16_t >& getDensities() const { return mDensities; }

        inline std::vector< vec3_t >& getNormals() { return mNormals; }
        inline const std::vector< vec3_t >& getNormals() const { return mNormals; }

        inline const u16vec2_t& getMinMaxDensity() const { return mMinMaxDensity; }

        inline int32_t xClamp( const int32_t x ) { return std::min( std::max( x, 0 ), mDim[0] - 1 ); }
        inline int32_t yClamp( const int32_t y ) { return std::min( std::max( y, 0 ), mDim[1] - 1 ); }
        inline int32_t zClamp( const int32_t z ) { return std::min( std::max( z, 0 ), mDim[2] - 1 ); }
        inline int32_t dimClamp( const int32_t coord, const int32_t dimIdx ) { return std::min( std::max( coord, 0 ), mDim[dimIdx] - 1 ); }

        inline uint32_t calcAddr( const int32_t x, const int32_t y, const int32_t z ) { return (z * mDim[1] + y)* mDim[0] + x; }
        inline uint32_t calcAddrClamped( const int32_t x, const int32_t y, const int32_t z ) { return (zClamp(z) * mDim[1] + yClamp(y))* mDim[0] + xClamp(x); }

        static constexpr uint32_t   mNumHistogramBuckets = 1024;
        static constexpr uint32_t   mHistogramDensitiesPerBucket = 4096 / mNumHistogramBuckets;
        const std::array< uint32_t, mNumHistogramBuckets >& getHistoBuckets() const { return mHistogramBuckets; }

    private:
        void sobelGradients();
        void centralDifferencesGradients();

        u16vec3_t                   mDim;
        std::vector< uint16_t >     mDensities;
        std::vector< vec3_t >       mNormals;
        std::array< uint16_t, 2 >   mMinMaxDensity;
        gradientMode_t              mGradientMode = gradientMode_t::SOBEL_3D;
        
        std::array< uint32_t, mNumHistogramBuckets > mHistogramBuckets;
    };
}
#endif // _VOLUMEDATA_H_EA89F308_240F_4AE0_97B5_AFE55000B453
