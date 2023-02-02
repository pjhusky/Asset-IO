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
        using vec3_t = std::array<float, 3>;
        using vec4_t = std::array<float, 4>;

        eRetVal load( const std::string& fileUrl );
        
        void calculateNormals();
        void calculateHistogramBuckets();
        
        void getBoundingSphere( vec4_t& boundingSphere );
        u16vec3_t getDim() const { return mDim; }

        std::vector< uint16_t >& getDensities() { return mDensities; }
        const std::vector< uint16_t >& getDensities() const { return mDensities; }

        std::vector< vec3_t >& getNormals() { return mNormals; }
        const std::vector< vec3_t >& getNormals() const { return mNormals; }

        const u16vec2_t& getMinMaxDensity() const { return mMinMaxDensity; }

        static constexpr uint32_t   mNumHistogramBuckets = 1024;
        static constexpr uint32_t   mHistogramDensitiesPerBucket = 4096 / mNumHistogramBuckets;
        const std::array< uint32_t, mNumHistogramBuckets >& getHistoBuckets() const { return mHistogramBuckets; }

    private:
        u16vec3_t                   mDim;
        std::vector< uint16_t >     mDensities;
        std::vector< vec3_t >       mNormals;
        std::array< uint16_t, 2 >   mMinMaxDensity;
        
        std::array< uint32_t, mNumHistogramBuckets > mHistogramBuckets;
    };
}
#endif // _VOLUMEDATA_H_EA89F308_240F_4AE0_97B5_AFE55000B453
