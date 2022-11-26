#ifndef _PLYMODEL_H_914c094e_61f3_4b57_ba38_80cba8f79b82
#define _PLYMODEL_H_914c094e_61f3_4b57_ba38_80cba8f79b82

#include "eRetVal_FileLoader.h"

#include <cstdint>

#include <string>
#include <vector>
#include <array>
#include <map>
#include <memory>

namespace FileLoader {
    struct PlyModel {
        PlyModel() 
            : mWasRadiusCalculated( false )
        {}

        eRetVal load( const std::string& url );
        eRetVal save( const std::string& url, const std::string& comment );

        enum class eDataType {
            i8 = 0,
            u8, 
            i16,
            u16, 
            i32, 
            u32,
            f32,
            f64,
            UNKNOWN,
        };

        // https://web.archive.org/web/20161204152348/http://www.dcs.ed.ac.uk/teaching/cs4/www/graphics/Web/ply.html
        static constexpr size_t dataTypeNumBytes[] = {
            1, //i8,
            1, //u8, 
            2, //i16,
            2, //u16, 
            4, //i32, 
            4, //u32,
            4, //f32,
            8, //f64,
            0 //UNKNOWN,
        };

        struct propertyDesc_t {
            std::string                     name;
            eDataType                       dataType;
            eDataType                       listElementsCountDataType;
            std::vector< uint8_t >          data;
            size_t                          numListElements;
            bool                            isList;
        };

        struct elementBlock_t {
            size_t                          numProperties;
            std::vector< propertyDesc_t >   propertyDescriptions;
        };

        struct elementBlockHeader_t {
            std::string     elementBlockName; 
            elementBlock_t  elementBlockData;
        };

        const propertyDesc_t *const getPropertyByName( const std::string& propertyName, size_t& numElements ) const;
        const propertyDesc_t *const getPropertyByName( const std::string& blockName, const std::string& propertyName, size_t& numElements ) const;

        eRetVal addProperty( 
            const std::string& blockName, 
            const propertyDesc_t& propertyDesc );

        const void getBoundingSphere( std::array<float, 4>& centerAndRadius ) const;

    private: 
        std::vector< elementBlockHeader_t >                 mElementBlockDescriptions;
        mutable std::array<float, 4>                        mCenterAndRadius;
        mutable bool                                        mWasRadiusCalculated;
    };
}
#endif // _PLYMODEL_H_914c094e_61f3_4b57_ba38_80cba8f79b82
