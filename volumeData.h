#ifndef _VOLUMEDATA_H_EA89F308_240F_4AE0_97B5_AFE55000B453
#define _VOLUMEDATA_H_EA89F308_240F_4AE0_97B5_AFE55000B453

#include "statusType.h"
#include "math/linAlg.h"

// https://stackoverflow.com/questions/7597025/difference-between-stdint-h-and-inttypes-h
#include <stdint.h>

#include <string>
#include <vector>

struct VolumeData {
    
    Status_t load( const std::string& fileUrl );
    //Status_t clear();
    void calculateNormals();
    
    void getBoundingSphere( linAlg::vec4_t& boundingSphere );
    linAlg::u16vec3_t getDim() const { return mDim; }

    std::vector< uint16_t >& getDensities() { return mDensities; }
    const std::vector< uint16_t >& getDensities() const { return mDensities; }

    std::vector< linAlg::vec3_t >& getNormals() { return mNormals; }
    const std::vector< linAlg::vec3_t >& getNormals() const { return mNormals; }

    const std::array< uint16_t, 2 >& getMinMaxDensity() const { return mMinMaxDensity; }

private:
    linAlg::u16vec3_t             mDim;
    std::vector< uint16_t >       mDensities;
    //std::vector< int16_t >       mDensities;
    std::vector< linAlg::vec3_t > mNormals; // calculate with wide kernel (~Sobel)
    std::array< uint16_t, 2 >     mMinMaxDensity;
};

#endif // _VOLUMEDATA_H_EA89F308_240F_4AE0_97B5_AFE55000B453
