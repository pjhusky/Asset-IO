#include "plyModel.h"
#include "stringUtils.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>
#include <cstdio>
#include <cmath>

#include <cassert>

namespace
{
    enum class eEncoding {
        BINARY_LITTLE_ENDIAN,
        BINARY_BIG_ENDIAN,
        ASCII,
        UNKNOWN,
    };

    static PlyModel::eDataType plyDataTypeStringToEnum( const std::string& strRep ) {
        //char uchar short ushort int   uint   float   double, 
        //int8 uint8 int16 uint16 int32 uint32 float32 float64
        if      ( strRep == "char"   || strRep == "int8" )    { return PlyModel::eDataType::i8; }
        else if ( strRep == "uchar"  || strRep == "uint8" )   { return PlyModel::eDataType::u8; }
        else if ( strRep == "short"  || strRep == "int16" )   { return PlyModel::eDataType::i16; }
        else if ( strRep == "ushort" || strRep == "uint16" )  { return PlyModel::eDataType::u16; }
        else if ( strRep == "int"    || strRep == "int32" )   { return PlyModel::eDataType::i32; }
        else if ( strRep == "uint"   || strRep == "uint32" )  { return PlyModel::eDataType::u32; }
        else if ( strRep == "float"  || strRep == "float32" ) { return PlyModel::eDataType::f32; }
        else if ( strRep == "double" || strRep == "float64" ) { return PlyModel::eDataType::f64; }
        return PlyModel::eDataType::UNKNOWN;
    }

    static std::string plyDataTypeEnumToString( const PlyModel::eDataType& dtEnum ) {
        // if      ( dtEnum == PlyModel::eDataType::i8  )     { return "int8"; }
        // else if ( dtEnum == PlyModel::eDataType::u8  )     { return "uint8"; }
        // else if ( dtEnum == PlyModel::eDataType::i16  )    { return "int16"; }
        // else if ( dtEnum == PlyModel::eDataType::u16  )    { return "uint16"; }
        // else if ( dtEnum == PlyModel::eDataType::i32  )    { return "int32"; }
        // else if ( dtEnum == PlyModel::eDataType::u32  )    { return "uint32"; }
        // else if ( dtEnum == PlyModel::eDataType::f32  )    { return "float32"; }
        // else if ( dtEnum == PlyModel::eDataType::f64  )    { return "float64"; }
        if      ( dtEnum == PlyModel::eDataType::i8  )     { return "char"; }
        else if ( dtEnum == PlyModel::eDataType::u8  )     { return "uchar"; }
        else if ( dtEnum == PlyModel::eDataType::i16  )    { return "short"; }
        else if ( dtEnum == PlyModel::eDataType::u16  )    { return "ushort"; }
        else if ( dtEnum == PlyModel::eDataType::i32  )    { return "int"; }
        else if ( dtEnum == PlyModel::eDataType::u32  )    { return "uint"; }
        else if ( dtEnum == PlyModel::eDataType::f32  )    { return "float"; }
        else if ( dtEnum == PlyModel::eDataType::f64  )    { return "double"; }
        return "UNKNOWN";
    }

    // https://stackoverflow.com/questions/1513209/is-there-a-way-to-use-fopen-s-with-gcc-or-at-least-create-a-define-about-it
#if defined( __unix ) || defined( __APPLE__ ) 
    #define fopen_s(pFile,filename,mode) ((*(pFile))=fopen((filename),(mode)))==NULL
#endif

} // namespace

Status_t PlyModel::load( const std::string& url )
{
#if 1
    std::vector< char > charactersReadFromFile;
    charactersReadFromFile.reserve( 500 );
    FILE* pFile = nullptr;
    //auto fopenErr = 
    fopen_s( &pFile, url.c_str(), "r" );
    if ( pFile == nullptr ) { return Status_t::ERROR(); }
    const std::string endHeaderLiteral{ "end_header" };
    const size_t endHeaderNumChars = endHeaderLiteral.size();

    for ( ; ; ) {
        constexpr size_t elementNumBytes = 1;
        constexpr size_t numElementsToRead = 1;
        char currChar;
        size_t charsRead = fread( &currChar, elementNumBytes, numElementsToRead, pFile );
        assert( charsRead == numElementsToRead );

        charactersReadFromFile.push_back( currChar );

        const size_t currNumChars = charactersReadFromFile.size();
        if ( currNumChars > endHeaderNumChars ) {
            std::string currSuffix{ charactersReadFromFile.begin() + currNumChars - endHeaderNumChars, charactersReadFromFile.end() };
            if ( currSuffix == endHeaderLiteral ) { break; }
        }
    }
    
    std::string contentString{ charactersReadFromFile.begin(), charactersReadFromFile.end() };
    //std::cout << "ply file header:\n------------\n" << contentString << "\n---------------\n" << std::endl;
    
#elif 1
    std::ifstream fileStream( url );
    std::string contentString(
        ( std::istreambuf_iterator< char >( fileStream ) ),
        std::istreambuf_iterator< char >() );
#else
    std::string contentStringHack = 
    R"(ply
format binary_little_endian 1.0
comment PLY by OPTOCAT
element vertex 506639
property float x
property float y
property float z
property uchar red
property uchar green
property uchar blue
element face 1006879
property list uchar int vertex_indices
end_header)";
    std::string contentString{ contentStringHack };
#endif

    const std::string whiteSpaceAndNewlinePattern = R"([\s\n\r])";
    const std::string anyCharacterPattern = R"([\s\S])";
    
    const std::string formatDirectivePattern = R"((?:format)\s+(\w+)\s+(\d+\.\d+))";
    const std::string commentDirectivePattern = R"((?:comment)([^\n]*\n))";
    
    {
        const std::string headerStartPattern = R"((?:ply))";
        const std::string headerEndPattern = R"((?:end_header))";

        std::string headerExtractPattern =
            whiteSpaceAndNewlinePattern + "*" +
            headerStartPattern +
            whiteSpaceAndNewlinePattern + "*" +
            "(" + anyCharacterPattern + "*)" +
            headerEndPattern;

        std::regex  headerExtractRegex{ headerExtractPattern };
        std::smatch regexMatches;
        bool didRegexMatch = std::regex_search( 
            contentString, 
            regexMatches, 
            headerExtractRegex );
        //std::cout << "the matches are: " << std::endl << std::endl;
        //for ( const auto& match : regexMatches )
        //{
        //    static size_t i = 0;
        //    std::cout << "match " << i++ << " = " << match << std::endl << std::endl;
        //}

        assert( regexMatches.size() == 2 );

        size_t headerLen = regexMatches[ 0 ].length(); // offset for data start

        std::string headerContentString = regexMatches[ 1 ];
    
        std::string headerContentCommentsStripped = std::regex_replace( 
            headerContentString, 
            std::regex{ commentDirectivePattern }, 
            "\n" );
    
        eEncoding dataEncoding = eEncoding::UNKNOWN;
        std::string headerContentFormatStripped;
        std::string version;
        {
            std::regex  formatDirectiveRegex{ anyCharacterPattern + "*(" + formatDirectivePattern + ")" + anyCharacterPattern + "*" };
            std::smatch regexFormatMatches;
            bool didRegexMatch = std::regex_match( 
                headerContentCommentsStripped, 
                regexFormatMatches, 
                formatDirectiveRegex );
            assert( regexFormatMatches.size() == 4 ); // whole format directive, encoding, and version
            if ( regexFormatMatches[ 2 ] == "binary_little_endian" ) 
            {
                dataEncoding = eEncoding::BINARY_LITTLE_ENDIAN;
                //std::cout << "encoding: binary little endian\n";
            } 
            else if ( regexFormatMatches[ 2 ] == "binary_big_endian" )
            {
                dataEncoding = eEncoding::BINARY_BIG_ENDIAN;
                //std::cout << "encoding: binary big endian\n";
                assert( false ); // not implemented yet
            }
            else if ( regexFormatMatches[ 2 ] == "ascii" )
            {
                dataEncoding = eEncoding::ASCII;
                //std::cout << "encoding: ascii\n";
                assert( false ); // not implemented yet
            }

            version = regexFormatMatches[ 3 ];
            //std::cout << "encoding: version = " << version << std::endl;

            headerContentFormatStripped = std::regex_replace( 
                headerContentCommentsStripped, 
                std::regex{ std::string{R"((?:)"} + regexFormatMatches[ 1 ].str() + ")" },
                "\n" );
        }

        const std::string& headerElementInfos = headerContentFormatStripped;

        const std::string elementHeaderPattern = R"((?:element)\s+((?:vertex)|(?:face))\s+(\d+))";
        const std::string elementHeaderNoCapturePattern = R"((?:element)\s+(?:(?:vertex)|(?:face))\s+(?:\d+))";

        const std::string typesPattern1of2 = R"((?:char)|(?:uchar)|(?:short)|(?:ushort)|(?:int)|(?:uint)|(?:float)|(?:double))";
        const std::string typesPattern2of2 = R"((?:int8)|(?:uint8)|(?:int16)|(?:uint16)|(?:int32)|(?:uint32)|(?:float32)|(?:float64))";
        const std::string typesPattern          = "(" + typesPattern1of2 + "|" + typesPattern2of2 + ")";
        const std::string typesNoCapturePattern = "(?:" + typesPattern1of2 + "|" + typesPattern2of2 + ")";

        const std::string propertyPattern          = R"((?:property)\s+)" + typesPattern + R"(\s+(\w+))";
        const std::string propertyNoCapturePattern = R"((?:property)\s+)" + typesNoCapturePattern + R"(\s+(?:\w+))";

        const std::string propertyListPattern          = std::string{ R"((?:property)\s+(?:list)\s+)" } + R"((?:)" + typesPattern + R"(\s+)" + R"()+(\w+))";
        const std::string propertyListNoCapturePattern = std::string{ R"((?:property)\s+(?:list)\s+)" } + R"((?:)" + typesNoCapturePattern + R"(\s+)" + R"()+(?:\w+))";
        const std::string propertyListNoTypeCapturePattern = std::string{ R"((?:property)\s+(?:list)\s+)" } + R"(((?:)" + typesNoCapturePattern + R"(\s+)" + R"()+)(\w+))";

        const std::string elementsNoCapturePattern =
            elementHeaderNoCapturePattern +
            R"(((?:)" + whiteSpaceAndNewlinePattern + "|" + propertyNoCapturePattern + "|" + propertyListNoCapturePattern + R"()+))";

        // unfortunately, the compact expression with capture groups will keep overwriting matches as they are encountered,
        // thus leaving us only with the very last match => need to switch from declarative to procedural processing in a loop
        // match the element blocks one-by-one in the outer block
        std::regex elementBlockRegex{ elementsNoCapturePattern };
        for ( auto elementsIter = std::sregex_iterator( headerElementInfos.begin(), headerElementInfos.end(), elementBlockRegex );
                   elementsIter != std::sregex_iterator();
                 ++elementsIter )
        {
            std::smatch elementBlock = *elementsIter;
            std::string elementBlockStr = elementBlock.str();
            //std::cout << "*) elementBlock:\n-----------\n" << elementBlockStr << "-----------\n";

            std::regex elementHeaderRegex{ elementHeaderPattern };
            std::smatch elementHeaderSubMatches;
            std::regex_search( elementBlockStr, elementHeaderSubMatches, elementHeaderRegex );
            //for ( const auto& elementHeaderSubMatch : elementHeaderSubMatches )
            for ( auto elementHeaderSubMatchIter = elementHeaderSubMatches.begin() + 1; 
                       elementHeaderSubMatchIter != elementHeaderSubMatches.end();
                     ++elementHeaderSubMatchIter )
            {
                //std::cout << "\t##) element header sub match = " << elementHeaderSubMatchIter->str() << std::endl;
            }

            const std::string elementBlockName          = ( elementHeaderSubMatches.begin() + 1 )->str();
            const std::string elementBlockNumEntriesStr = ( elementHeaderSubMatches.begin() + 2 )->str();
            //std::cout << "elementBlockName = " << elementBlockName << ", elementBlockNumEntriesStr = " << elementBlockNumEntriesStr << std::endl;
            elementBlock_t elementBlockEntry{ stringUtils::convStrTo< size_t >( elementBlockNumEntriesStr ) };

            const std::string propertyBlock = elementBlock[ 1 ].str();
            //std::cout << "*#) elementBlock ContentOnly:\n" << propertyBlock << std::endl;            

            // match the properties below this element block one-by-one
            std::regex propertyRegex{ propertyPattern };
            //NOTE: need a loop here, since we can't use capture groups to capture many 'type' matches (only last one is saved)
            for ( auto propertiesIter = std::sregex_iterator( propertyBlock.begin(), propertyBlock.end(), propertyRegex );
                propertiesIter != std::sregex_iterator();
                ++propertiesIter )
            {
                //std::cout << "***) property: " << propertiesIter->str() << std::endl;

                std::smatch propertyMatch = *propertiesIter;
                // for ( auto propertyMatchIter = propertyMatch.begin() + 1;
                //     propertyMatchIter != propertyMatch.end();
                //     ++propertyMatchIter )
                // {
                //     std::cout << "****)  submatch = " << propertyMatchIter->str() << std::endl;
                // }

                // convert dataType string rep to enum
                eDataType propertyDataType = plyDataTypeStringToEnum( ( propertyMatch.begin() + 1 )->str() );
                const std::string propertyName = ( propertyMatch.begin() + 2 )->str();
                propertyDesc_t propertyDesc;
                propertyDesc.isList = false;
                propertyDesc.name = propertyName;
                propertyDesc.dataType = propertyDataType;
                elementBlockEntry.propertyDescriptions.push_back( propertyDesc );
            }

            // somehow merge this "special case" into the "normal", i.e., non-list, property handling in the loop above 
            std::regex propertyListRegex{ propertyListNoTypeCapturePattern };
            for ( auto propertiesIter = std::sregex_iterator( propertyBlock.begin(), propertyBlock.end(), propertyListRegex );
                propertiesIter != std::sregex_iterator();
                ++propertiesIter )
            {
                std::smatch propertyMatch = *propertiesIter;
                std::string propertyListTypesAndNames = propertyMatch[ 0 ].str();
                //std::cout << "list case, whole match = " << propertyListTypesAndNames << std::endl;

                std::string propertyId = propertyMatch[ 2 ].str();
                //std::cout << "list case, id = " << propertyId << std::endl;

                const std::string listOfTypesString = propertyMatch[ 1 ].str();
                //std::cout << "list listOfTypesString = " << listOfTypesString << std::endl;

                std::regex propertyListTypesRegex{ typesPattern };

                propertyDesc_t propertyDesc;
                propertyDesc.name = propertyId;
                propertyDesc.isList = true;

                size_t iterCount = 0;
                for ( auto propertiesIter = std::sregex_iterator( listOfTypesString.begin(), listOfTypesString.end(), propertyListTypesRegex );
                    propertiesIter != std::sregex_iterator();
                    ++propertiesIter )
                {
                    std::smatch listTypeMatch = *propertiesIter;
                    const std::string listTypeStrRep = listTypeMatch.str();
                    //std::cout << "listTypeMatch = " << listTypeStrRep << std::endl;

                    // convert dataType string rep to enum
                    eDataType propertyDataType = plyDataTypeStringToEnum( listTypeStrRep );
                    if ( iterCount == 0 ) {
                        //std::cout << "list count data type" << std::endl;
                        propertyDesc.listElementsCountDataType = propertyDataType;
                    } else {
                        //std::cout << "list elements data type" << std::endl;
                        propertyDesc.dataType = propertyDataType;
                    }
                    iterCount++;
                }
                assert( iterCount == 2 );
                elementBlockEntry.propertyDescriptions.push_back( propertyDesc );
            }
            mElementBlockDescriptions.push_back( elementBlockHeader_t{ elementBlockName, elementBlockEntry } );
        }
    }

    // determine size of data entries (per element-block)
    std::vector< size_t > elementEntrySizes;
    elementEntrySizes.reserve( mElementBlockDescriptions.size() );

    for ( const auto& elementBlockDescription : mElementBlockDescriptions ) {
        size_t accumNumBytes = 0;
        const elementBlock_t& properties = elementBlockDescription.elementBlockData;
        for ( const propertyDesc_t& property : properties.propertyDescriptions ) {
            if ( property.isList ) { continue; } // can't know size up-front, since format is "numElements" "element_1 element_2 ... element_N"
            accumNumBytes += dataTypeNumBytes[ static_cast< int32_t >( property.dataType ) ];
        }
        
        elementEntrySizes.push_back( accumNumBytes );
    }

    // now read actual data from file according to data layout given in the header
    //FILE* pBinaryFile = fopen( url.c_str(), "rb" );
    FILE* pBinaryFile; 
    //fopenErr = 
    fopen_s( &pBinaryFile, url.c_str(), "rb" );

    //int32_t seekResult = fseeko( pBinaryFile, charactersReadFromFile.size() + 1, SEEK_SET ); // +1 for newline
    int32_t seekResult = fseek( pBinaryFile, charactersReadFromFile.size() + 1, SEEK_SET ); // +1 for newline

    size_t blockIdx = -1;
    for ( auto& elementBlockDescription : mElementBlockDescriptions ) {
        blockIdx++;

        std::vector< std::vector< uint8_t > > dataPerProperty;
        dataPerProperty.resize( elementBlockDescription.elementBlockData.propertyDescriptions.size() );
        for( size_t i = 0; i < elementBlockDescription.elementBlockData.propertyDescriptions.size(); i++ ) {
            auto& propDesc = elementBlockDescription.elementBlockData.propertyDescriptions[ i ];
            dataPerProperty[ i ].reserve( 
                elementBlockDescription.elementBlockData.numProperties * 
                dataTypeNumBytes[ static_cast< int32_t >( propDesc.dataType ) ] * 
                ( elementBlockDescription.elementBlockData.propertyDescriptions[ i ].isList ? 3 : 1 ) ); // estimate for triangles
        }

        for ( size_t currProperty = 0; currProperty < elementBlockDescription.elementBlockData.numProperties; currProperty++ ) {

            size_t currPropertyDescIdx = -1;

            for ( auto& propDesc : elementBlockDescription.elementBlockData.propertyDescriptions ) {
                currPropertyDescIdx++;
                //std::cout << "currPropertyDescIdx = " << currPropertyDescIdx << std::endl << std::flush;
            
                if ( propDesc.isList ) {
                    //std::cout << "  list, currPropertyDescIdx = " << currPropertyDescIdx << std::endl << std::flush;
                    int32_t propListLen = 0; // big enough to take any interger data type used for storing list length in
                    const size_t numBytesOfListLen = dataTypeNumBytes[ static_cast< int32_t >( propDesc.listElementsCountDataType ) ];

                    //std::cout << "  numBytesOfListLen = " << numBytesOfListLen << std::endl << std::flush;

                    fread( &propListLen, numBytesOfListLen, 1, pBinaryFile );
                    
                    //std::cout << "  propListLen = " << propListLen << std::endl << std::flush;
                    assert( propListLen == 3 ); // only triangular faces supported

                    propDesc.numListElements = propListLen;
                    for ( size_t propListIdx = 0; propListIdx < propListLen; propListIdx++ ) {
                        const size_t numBytesOfCurrPropertyElement = dataTypeNumBytes[ static_cast< int32_t >( propDesc.dataType ) ];

                        for ( size_t currByteIdx = 0; currByteIdx < numBytesOfCurrPropertyElement; currByteIdx++ ) {
                            dataPerProperty[ currPropertyDescIdx ].push_back( 0 );
                        }

                        fread(  dataPerProperty[ currPropertyDescIdx ].data() + dataPerProperty[ currPropertyDescIdx ].size() - numBytesOfCurrPropertyElement,
                                numBytesOfCurrPropertyElement, 1, pBinaryFile );
                    }

                } else {
                    //std::cout << "  not a list property" << std::endl << std::flush;
                    const size_t numBytesOfCurrPropertyElement = dataTypeNumBytes[ static_cast< int32_t >( propDesc.dataType ) ];

                    for ( size_t currByteIdx = 0; currByteIdx < numBytesOfCurrPropertyElement; currByteIdx++ ) {
                        dataPerProperty[ currPropertyDescIdx ].push_back( 0 );
                    }
                    
                    fread(  dataPerProperty[ currPropertyDescIdx ].data() + numBytesOfCurrPropertyElement * currProperty, 
                            numBytesOfCurrPropertyElement, 1, pBinaryFile );
                }
            }
        }

        size_t currPropertyDescIdx = -1;
        for ( auto& propDesc : elementBlockDescription.elementBlockData.propertyDescriptions ) {
            currPropertyDescIdx++;
            const size_t numBytesOfCurrPropertyElement = dataTypeNumBytes[ static_cast< int32_t >( propDesc.dataType ) ];
            const size_t numBytesTotal = dataPerProperty[ currPropertyDescIdx ].size();// * numBytesOfCurrPropertyElement;
            // propDesc.data = std::shared_ptr< uint8_t[] >( 
            //     new uint8_t[ numBytesTotal ] );
            propDesc.data.resize( numBytesTotal );
            memcpy( propDesc.data.data(), 
                    dataPerProperty[ currPropertyDescIdx ].data(), 
                    numBytesTotal );

        #if 0 // DEBUG PRINT
            if ( propDesc.isList ) {
                if ( propDesc.dataType == eDataType::i32 ) {
                    int32_t* pListData = reinterpret_cast< int32_t* >( dataPerProperty[ currPropertyDescIdx ].data() );
                    for ( size_t i = 0; i < dataPerProperty[ currPropertyDescIdx ].size() / ( numBytesOfCurrPropertyElement ); i += 3 ) {
                        std::cout << "pListData " << i << " = " << pListData[ i ] << " , " << pListData[ i + 1 ] << " , " << pListData[ i + 2 ] << std::endl;
                    }
                    // std::cout << "pListData = " << pListData[ 0 ] << " , " << pListData[ 1 ] << " , " << pListData[ 2 ] << std::endl;
                    // std::cout << "pListData = " << pListData[ 3 ] << " , " << pListData[ 4 ] << " , " << pListData[ 5 ] << std::endl;
                    std::cout << std::endl << std::flush;
                } else if ( propDesc.dataType == eDataType::u32 ) {
                    uint32_t* pListData = reinterpret_cast< uint32_t* >( dataPerProperty[ currPropertyDescIdx ].data() );
                    std::cout << "pListData = " << pListData[ 0 ] << " , " << pListData[ 1 ] << " , " << pListData[ 2 ] << std::endl;
                    std::cout << "pListData = " << pListData[ 3 ] << " , " << pListData[ 4 ] << " , " << pListData[ 5 ] << std::endl;
                    std::cout << std::endl << std::flush;
                }
            }
        #endif
                    
        }
    }

#if 0
    { // DEBUG - print x, y, z values of each vertex (AOS style, i.e., all attribs of a vertex at once, e.g., 'x y z s t r g b'
        blockIdx = -1;
        for ( auto& elementBlockDescription : mElementBlockDescriptions ) {
            blockIdx++;

            for ( size_t propIdx = 0; propIdx < elementBlockDescription.elementBlockData.numProperties; propIdx++ ) {

                for ( auto& propDesc : elementBlockDescription.elementBlockData.propertyDescriptions ) {
                    size_t numElementsInProperty = ( propDesc.isList ) ? propDesc.numListElements : 1;
                    std::cout << " " << propDesc.name << ":";
                    for ( size_t elemIdx = 0; elemIdx < numElementsInProperty; elemIdx++ ) {
                        if ( propDesc.dataType == eDataType::u8 ) {
                            uint8_t* pData = reinterpret_cast< uint8_t* >( propDesc.data.get() );
                            std::cout << " " << static_cast< uint32_t >( pData[ propIdx * numElementsInProperty + elemIdx ] );
                        } else if ( propDesc.dataType == eDataType::i32 ) {
                            int32_t* pData = reinterpret_cast< int32_t* >( propDesc.data.get() );
                            std::cout << " " << pData[ propIdx * numElementsInProperty + elemIdx ];
                        } else if ( propDesc.dataType == eDataType::u32 ) {
                            uint32_t* pData = reinterpret_cast< uint32_t* >( propDesc.data.get() );
                            std::cout << " " << pData[ propIdx * numElementsInProperty + elemIdx ];
                        } else if ( propDesc.dataType == eDataType::f32 ) {
                            float* pData = reinterpret_cast< float* >( propDesc.data.get() );
                            std::cout << " " << pData[ propIdx * numElementsInProperty + elemIdx ];
                        } else {
                            std::cout << " | TODO: datatype not printed currently | ";
                        }
                    }
                }
                std::cout << std::endl;
            }
        }
    }
#endif

    return Status_t::OK();
}

const PlyModel::propertyDesc_t *const PlyModel::getPropertyByName( const std::string& propertyName, size_t& numElements ) const {
    for ( const auto& elementBlockDesc : mElementBlockDescriptions ) {
        for ( const auto& propDesc : elementBlockDesc.elementBlockData.propertyDescriptions ) {
            if ( propDesc.name == propertyName ) {
                numElements = elementBlockDesc.elementBlockData.numProperties;
                return &propDesc;
            }
        }
    }
    return nullptr;
}

const PlyModel::propertyDesc_t *const PlyModel::getPropertyByName( 
    const std::string& blockName,
    const std::string& propertyName, 
    size_t& numElements ) const {
    for ( const auto& elementBlockDesc : mElementBlockDescriptions ) {
        if ( elementBlockDesc.elementBlockName != blockName ) { continue; }
        for ( const auto& propDesc : elementBlockDesc.elementBlockData.propertyDescriptions ) {
            if ( propDesc.name == propertyName ) {
                numElements = elementBlockDesc.elementBlockData.numProperties;
                return &propDesc;
            }
        }
    }
    return nullptr;
}

Status_t PlyModel::addProperty( 
    const std::string& blockName, 
    const PlyModel::propertyDesc_t& propertyDesc ) {
    for ( auto& elementBlockDesc : mElementBlockDescriptions ) {
        if ( elementBlockDesc.elementBlockName != blockName ) { continue; }

        elementBlockDesc.elementBlockData.propertyDescriptions.push_back( propertyDesc );
    }
    return Status_t::OK();
}

const void PlyModel::getBoundingSphere( linAlg::vec4_t& centerAndRadius ) const {

    if ( mWasRadiusCalculated ) {
        centerAndRadius = mCenterAndRadius;
        return;
    }

    size_t numElementsX;
    const propertyDesc_t *const pPropertyX = getPropertyByName( "x", numElementsX );
    size_t numElementsY;
    const propertyDesc_t *const pPropertyY = getPropertyByName( "y", numElementsY );
    size_t numElementsZ;
    const propertyDesc_t *const pPropertyZ = getPropertyByName( "z", numElementsZ );

    assert( numElementsX == numElementsY && numElementsY == numElementsZ );
    assert( pPropertyX->dataType == PlyModel::eDataType::f32 && pPropertyX->dataType == pPropertyY->dataType && pPropertyX->dataType == pPropertyZ->dataType );

    const float *const pX = reinterpret_cast< const float *const >( pPropertyX->data.data() );
    const float *const pY = reinterpret_cast< const float *const >( pPropertyY->data.data() );
    const float *const pZ = reinterpret_cast< const float *const >( pPropertyZ->data.data() );

    float minX = pX[ 0 ];
    float maxX = pX[ 0 ];

    float minY = pY[ 0 ];
    float maxY = pY[ 0 ];

    float minZ = pZ[ 0 ];
    float maxZ = pZ[ 0 ];

    for ( size_t i = 1; i < numElementsX; i++ ) {
        minX = linAlg::minimum( minX, pX[ i ] );
        maxX = linAlg::maximum( maxX, pX[ i ] );

        minY = linAlg::minimum( minY, pY[ i ] );
        maxY = linAlg::maximum( maxY, pY[ i ] );

        minZ = linAlg::minimum( minZ, pZ[ i ] );
        maxZ = linAlg::maximum( maxZ, pZ[ i ] );
    }

    const float centerX = ( maxX + minX ) * 0.5f;
    const float centerY = ( maxY + minY ) * 0.5f;
    const float centerZ = ( maxZ + minZ ) * 0.5f;

    float maxDistSquared = 0.0f;
    for ( size_t i = 0; i < numElementsX; i++ ) {
        const float x = pX[ i ];
        const float y = pY[ i ];
        const float z = pZ[ i ];

        const float dX = centerX - x;
        const float dY = centerY - y;
        const float dZ = centerZ - z;

        const float distSquared = dX * dX + dY * dY + dZ * dZ;
        maxDistSquared = linAlg::maximum( maxDistSquared, distSquared );
    }

    centerAndRadius[ 0 ] = centerX;
    centerAndRadius[ 1 ] = centerY;
    centerAndRadius[ 2 ] = centerZ;
    centerAndRadius[ 3 ] = sqrtf( maxDistSquared );

    mWasRadiusCalculated = true;
    mCenterAndRadius = centerAndRadius;
}

Status_t PlyModel::save( const std::string& url, const std::string& comment ) {

    FILE* pFile = nullptr;
    //std::string filepath = url + ".okay.ply";
    std::string filepath = url;
    fopen_s( &pFile, filepath.c_str(), "w" );

    int64_t headerByteLen;

    // writing the header
    { 
		int64_t numCharsWritten = 0;
        numCharsWritten += fprintf( pFile, "ply\n" );
        numCharsWritten += fprintf( pFile, "format binary_little_endian 1.0\n" );
        numCharsWritten += fprintf( pFile, "comment %s\n", comment.c_str() );
        //numCharsWritten += fprintf( pFile, "comment not-so-awesome homebrew ply file saving\n" );
        //numCharsWritten += fprintf( pFile, "comment VCGLIB generated\n" ); // for easier comparison with original data file

        for ( const auto& elementBlockDescription : mElementBlockDescriptions ) {
            const auto& blockData = elementBlockDescription.elementBlockData;

            numCharsWritten += fprintf( pFile, "element %s %zu\n", elementBlockDescription.elementBlockName.c_str(), blockData.numProperties );

            for ( const auto& propertyDesc: blockData.propertyDescriptions ) {
                numCharsWritten += fprintf( pFile, "property %s%s%s %s\n", 
                propertyDesc.isList ? "list " : "", 
                propertyDesc.isList ? ( plyDataTypeEnumToString( propertyDesc.listElementsCountDataType ) + " " ).c_str() : "",
                plyDataTypeEnumToString( propertyDesc.dataType ).c_str(), 
                propertyDesc.name.c_str() );
            }
        }
        numCharsWritten += fprintf( pFile, "end_header\n" );
        //fprintf( pFile, "end_header" ); // no newline for binary encoding
        fflush( pFile );
        //headerByteLen = ftell( pFile );
        //assert( headerByteLen == numCharsWritten );
        fclose( pFile );

		headerByteLen = numCharsWritten;
    }
    
    // now for the actual data (binary litte-endian mode)
    fopen_s( &pFile, filepath.c_str(), "ab" );
    int fseekRet = fseek( pFile, headerByteLen, SEEK_SET );
    for ( const auto& elementBlockDescription : mElementBlockDescriptions ) {
        const auto& blockData = elementBlockDescription.elementBlockData;
        for ( size_t vertAttrIdx = 0; vertAttrIdx < blockData.numProperties; vertAttrIdx++ ) {
            for ( const auto& propertyDesc: blockData.propertyDescriptions ) {
                const size_t attribByteSize = dataTypeNumBytes[ static_cast< int32_t >( propertyDesc.dataType ) ];
                if ( !propertyDesc.isList ) {
                    fwrite( propertyDesc.data.data() + vertAttrIdx * attribByteSize, attribByteSize, 1, pFile );
                } else {
                    const size_t listCountByteSize = dataTypeNumBytes[ static_cast< int32_t >( propertyDesc.listElementsCountDataType ) ];
                    // list entries
                    fwrite( &propertyDesc.numListElements, listCountByteSize, 1, pFile );
                    fwrite( propertyDesc.data.data() + vertAttrIdx * attribByteSize * propertyDesc.numListElements, attribByteSize, propertyDesc.numListElements, pFile );
                }
            }
        }
    }
    fflush( pFile );
    fclose( pFile );

    return Status_t::OK();
}
