#include "ObjModel.h"

#include <fstream>
#include <sstream>
#include <regex>
#include <array>
#include <map>

#include <assert.h>

#ifndef _USE_MATH_DEFINES
    #define _USE_MATH_DEFINES
#endif
#include <math.h>

#define TRIM_LINE     0

using namespace FileLoader;

namespace {

    enum class eStatus {
        OK = 0,
        ERROR,
    };

    struct StringUtils
    {
        template <typename val_T>
        static val_T convStrTo(const std::string& str)
        {
            std::stringstream numStream{str};
            val_T             val;
            numStream >> val;
            return val;
        }

    #if ( TRIM_LINE != 0 )
        static std::string whitespaceCharacters() {
            return " \n\r\t\f\v";
        }

        static std::string ltrim( const std::string& s ) {
            size_t start = s.find_first_not_of( whitespaceCharacters() );
            return (start == std::string::npos) ? "" : s.substr( start );
        }

        static std::string rtrim( const std::string& s ) {
            size_t end = s.find_last_not_of( whitespaceCharacters() );
            return (end == std::string::npos) ? "" : s.substr( 0, end + 1 );
        }

        static std::string trim( const std::string& s ) {
            return rtrim( ltrim( s ) );
        }
    #endif

    };

    static void insertFaceIdxString( std::vector< uint32_t >& faceIdxTriple, const std::string& idxString ) {
        faceIdxTriple.push_back( StringUtils::convStrTo<uint32_t>( idxString ) - 1 ); // -1 since indices start at 1 in OBJ files
    }

    static void fixupDataForRendering( std::vector<VertexData>& out_vertexBuffer, 
                                       std::vector<uint32_t>& out_indexBuffer,
                                       const std::vector<float3>& vertexPos,
                                       const std::vector<float3>& vertexNorm,
                                       const std::vector<float2>& texCoord,
                                       const std::vector< std::vector<uint32_t> >& faceIndexTriples_v_vt_vn ) {
    #if 0 // brute force vertex buffer with duplicates    
        for ( size_t vertIdx = 0; vertIdx < faceIndexTriples_v_vt_vn.size(); vertIdx++ ) {
            const auto& currTriIdxTriple = faceIndexTriples_v_vt_vn[ vertIdx ];

            const uint32_t idx_v  = currTriIdxTriple[0];
            const uint32_t idx_vt = currTriIdxTriple[1];
            const uint32_t idx_vn = currTriIdxTriple[2];
            out_vertexBuffer.push_back( 
                VertexData{
                    vertexPos[idx_v],
                    vertexNorm[idx_vn],
                    texCoord[idx_vt],
                } );
            out_indexBuffer.push_back( static_cast< uint32_t >( vertIdx ) );
        }
    #else // only keep unique vertices
        size_t currIdxCount = 0;
        std::map< std::array< uint32_t, 3 >, size_t > uniqueTriIndices;
        for ( size_t triIdx = 0; triIdx < faceIndexTriples_v_vt_vn.size(); triIdx++ ) {
            const auto& currTriIdxTriple = faceIndexTriples_v_vt_vn[ triIdx ];
            const auto currEntry = std::array< uint32_t, 3 >{ currTriIdxTriple[0], currTriIdxTriple[1], currTriIdxTriple[2] };
            const auto findResult = uniqueTriIndices.find( currEntry );
            size_t currIdx = 0;
            if ( findResult == uniqueTriIndices.end() ) { // entry does not exist yet
                uniqueTriIndices.insert( std::make_pair( currEntry, currIdxCount ) );

                const uint32_t idx_v  = currTriIdxTriple[0];
                const uint32_t idx_vt = currTriIdxTriple[1];
                const uint32_t idx_vn = currTriIdxTriple[2];
                out_vertexBuffer.push_back( 
                    VertexData{
                        vertexPos[idx_v],
                        vertexNorm[idx_vn],
                        texCoord[idx_vt],
                    } );

                currIdx = currIdxCount;
                currIdxCount++;
            } else {
                currIdx = findResult->second;
            }

            out_indexBuffer.push_back( static_cast< uint32_t >( currIdx ) );
        }
        printf( "unique entries: %zu, brute force entries: %zu\n", out_vertexBuffer.size(), out_indexBuffer.size() );
    #endif
    }

    template < typename val_T >
    static val_T len2( const std::array<val_T, 3>& vec ) {
        //return dot( vec, vec );
        return vec[0] * vec[0] + vec[1] * vec[1] + vec[2] * vec[2];
    }

    static ObjModel::boundingSphere_t calcBoundingSphere( std::vector<float3>& vertexPositions ) { 
        
        float3 center = vertexPositions[0];
        for ( size_t i = 1, last_i = vertexPositions.size(); i < last_i; i++ ) {
            const auto& vertexPos = vertexPositions[i];
            center.x += vertexPos.x;
            center.y += vertexPos.y;
            center.z += vertexPos.z;
            center.x *= 0.5f;
            center.y *= 0.5f;
            center.z *= 0.5f;
        }

        float maxDist = 0.0f;
        for ( const auto& vertexPos : vertexPositions ) {
            maxDist = std::max( maxDist, len2( std::array<float, 3>{center.x - vertexPos.x, center.y - vertexPos.y, center.z - vertexPos.z} ) );
        }

        return ObjModel::boundingSphere_t{ center.x, center.y, center.z, sqrtf( maxDist ) };
    }

    static eStatus loadObj( const std::string& objFileUrl, 
                            std::vector<VertexData>& out_vertexBuffer, 
                            std::vector<uint32_t>& out_indexBuffer,
                            ObjModel::boundingSphere_t& out_boundingSphere ) {
        // INDICES START AT 1 NOT AT 0
        // #  ... comment
        // v  ... vertex pos
        // vn ... vertex normals (ignore)
        // vt ... vertex tex coord
        // f  ... face index triples - vertIdx/texCoordIdx/vertNormalIdx

        std::ifstream fileStream( objFileUrl );
        std::string fileContentString( 
            ( std::istreambuf_iterator<char>( fileStream ) ),
            std::istreambuf_iterator<char>() );

        //const auto number_rxPattern = std::string{ R"(-?\d*.*\d+)" };
        const auto number_rxPattern = std::string{ R"([-+]?[0-9]*\.?[0-9]+)" };
        const auto whitespaceButNotNewline_rxPattern = std::string{ R"([^\S\r\n])" };

        // vertex position parsing
        const auto v_rxPattern = std::string{ R"(^)" }
            + whitespaceButNotNewline_rxPattern + "*"
            + "v"
            + whitespaceButNotNewline_rxPattern + "+"
            + "(" + number_rxPattern + ")"
            + whitespaceButNotNewline_rxPattern + "+"
            + "(" + number_rxPattern + ")"
            + whitespaceButNotNewline_rxPattern + "+"
            + "(" + number_rxPattern + ")"
            + R"(\s*$)";
        printf( "v_rxPattern %s\n", v_rxPattern.c_str() );
        const auto v_rx = std::regex{ v_rxPattern };

        // vertex normal parsing
        const auto vn_rxPattern = std::string{ R"(^)" }
            + whitespaceButNotNewline_rxPattern + "*"
            + "vn"
            + whitespaceButNotNewline_rxPattern + "+"
            + "(" + number_rxPattern + ")"
            + whitespaceButNotNewline_rxPattern + "+"
            + "(" + number_rxPattern + ")"
            + whitespaceButNotNewline_rxPattern + "+"
            + "(" + number_rxPattern + ")"
            + R"(\s*$)";
        printf( "vn_rxPattern %s\n", vn_rxPattern.c_str() );
        const auto vn_rx = std::regex{ vn_rxPattern };

        // vertex texture coordinate parsing
        const auto vt_rxPattern = std::string{ R"(^)" }
            + whitespaceButNotNewline_rxPattern + "*"
            + "vt"
            + whitespaceButNotNewline_rxPattern + "+"
            + "(" + number_rxPattern + ")"
            + whitespaceButNotNewline_rxPattern + "+"
            + "(" + number_rxPattern + ")"
            + R"(\s*$)";
        printf( "vt_rxPattern %s\n", vt_rxPattern.c_str() );
        const auto vt_rx = std::regex{ vt_rxPattern };

        // faces parsing outer level
        const auto f_rxPattern = std::string{ R"(^)" }
            + whitespaceButNotNewline_rxPattern + "*"
            + "f"
            + whitespaceButNotNewline_rxPattern + "+"
            + R"(((?:[0-9\/]+)" + whitespaceButNotNewline_rxPattern + R"(+)+)"
            + R"([0-9\/]+))"
            + R"(\s*$)";
        printf( "f_rxPattern %s\n", f_rxPattern.c_str() );
        const auto f_rx = std::regex{ f_rxPattern };

        const auto faceTripleIdx_rxPattern = R"(([0-9\/]+))";
        const auto faceTripleIdx_rx = std::regex{ faceTripleIdx_rxPattern };

        std::vector<float3> vertexPos;
        std::vector<float3> vertexNorm;
        std::vector<float2> texCoord;
        std::vector< std::vector<uint32_t> > faceIndexTriples_v_vt_vn;

        std::stringstream fileContentStringStream{ fileContentString };
        std::string line;
        while ( std::getline( fileContentStringStream, line, '\n' ) ) {
            // some quick rejection tests
            if ( line.empty() ) { continue; }
            if ( line.length() <= 5 ) { continue; }
            const uint8_t char0 = line.at( 0 );
            if ( char0 == '#' ) { continue; } // skip comments
            if ( char0 == ' ' ) { continue; } // assume new line
            if ( char0 == 'm' ) { continue; } // assume material statement - ignore for now!
            if ( char0 == 'o' ) { continue; } // "object directive" - ignore for now!
            
            const uint8_t char1 = line.at( 1 );

            std::smatch       tokenMatch;
            if ( ( char0 == 'v' && char1 == ' ' ) && std::regex_match( line, tokenMatch, v_rx ) ) { // match vertex pos
                vertexPos.push_back( 
                    float3{ 
                        StringUtils::convStrTo<float>(tokenMatch[1].str()), 
                        StringUtils::convStrTo<float>(tokenMatch[2].str()),
                        StringUtils::convStrTo<float>(tokenMatch[3].str()) 
                    } );
            } else if ( ( char0 == 'v' && char1 == 'n' ) && std::regex_match( line, tokenMatch, vn_rx ) ) { // match vertex normal
                vertexNorm.push_back( 
                    float3{ 
                        StringUtils::convStrTo<float>(tokenMatch[1].str()), 
                        StringUtils::convStrTo<float>(tokenMatch[2].str()),
                        StringUtils::convStrTo<float>(tokenMatch[3].str()) 
                    } );
            } else if ( ( char0 == 'v' && char1 == 't' ) && std::regex_match( line, tokenMatch, vt_rx ) ) { // match texture coordinate
                texCoord.push_back( 
                    float2{ 
                        StringUtils::convStrTo<float>(tokenMatch[1].str()), 
                        StringUtils::convStrTo<float>(tokenMatch[2].str()) 
                    } );
            } else if ( ( char0 == 'f' && char1 == ' ' ) && std::regex_match( line, tokenMatch, f_rx ) ) {
                printf( "match face indices!\n" );
                std::smatch faceTripleIdxMatch;
                std::string faceSequenceString = tokenMatch[1].str();
                while( std::regex_search( faceSequenceString, faceTripleIdxMatch, faceTripleIdx_rx ) ) {
                    const auto currentFaceTripleIdxMatchStr = faceTripleIdxMatch.str();
                    printf( "one face triple found %s\n", currentFaceTripleIdxMatchStr.c_str() );
                    faceSequenceString = faceTripleIdxMatch.suffix();

                    const std::string delimiter = "/";

                    std::vector< uint32_t > faceIdxTriple;
                    faceIdxTriple.reserve( 3 );

                    // split the triple string at the separator '/' into the individual indices
                    size_t last = 0; 
                    size_t next = 0; 
                    while ((next = currentFaceTripleIdxMatchStr.find(delimiter, last)) != std::string::npos) {   
                        insertFaceIdxString( faceIdxTriple, currentFaceTripleIdxMatchStr.substr(last, next-last) );
                        last = next + 1; 
                    } 
                    insertFaceIdxString( faceIdxTriple, currentFaceTripleIdxMatchStr.substr(last) );

                    assert( faceIdxTriple.size() == 3 ); // make sure that we have an index for all 3 of pos,tc,normal
                    faceIndexTriples_v_vt_vn.push_back( faceIdxTriple );
                }
                assert( faceIndexTriples_v_vt_vn.size() % 3 == 0 ); // make sure we're dealing with triangles only
            }
        } // while

        // at this point the file is parsed, and vertex- and face data is stored in separate arrays (strings have been converted to numbers) 

        // now loop t/hrough the face definitions and assemble the vertex- and index buffer
        fixupDataForRendering( out_vertexBuffer, out_indexBuffer, vertexPos, vertexNorm, texCoord, faceIndexTriples_v_vt_vn );
        
        out_boundingSphere = calcBoundingSphere( vertexPos );

        return eStatus::OK;
    }

} // namespace

namespace FileLoader
{
    void ObjModel::loadModel(const std::string& geometryPath, const std::string& texturePath)
    {
        // TODO: Given the geometryPath
        // load the VertexData (position, normal, texture coordinates)
        // from the .obj file
        // The assignment sheet contains more information about OBJ files.
        const auto loadObjStatus = loadObj( geometryPath, m_vertexBuffer, m_indexBuffer, m_boundingSphere );
        (void) loadObjStatus;

    #if 1 // This dummy implementation currently only hardcodes a single quad.
        //m_vertexBuffer = {
        //    {float3(-0.5f, 0.5f, 0.0f), float3(0.0f, 0.0f, -1.0f), float2(0.0f, 0.0f)}, // top left
        //    {float3(0.5f, 0.5f, 0.0f), float3(0.0f, 0.0f, -1.0f), float2(1.0f, 0.0f)}, // top right
        //    {float3(-0.5f, -0.5f, 0.0f), float3(0.0f, 0.0f, -1.0f), float2(0.0f, 1.0f)}, // bottom left
        //    {float3(0.5f, -0.5f, 0.0f), float3(0.0f, 0.0f, -1.0f), float2(1.0f, 1.0f)}}; // bottom right
        m_vertexBuffer = {
            {float3(-0.5f, 0.0f,  0.5f), float3(0.0f, 0.0f, -1.0f), float2(0.0f, 0.0f)}, // top left
            {float3(0.5f,  0.0f,  0.5f), float3(0.0f, 0.0f, -1.0f), float2(1.0f, 0.0f)}, // top right
            {float3(-0.5f, 0.0f, -0.5f), float3(0.0f, 0.0f, -1.0f), float2(0.0f, 1.0f)}, // bottom left
            {float3(0.5f,  0.0f, -0.5f), float3(0.0f, 0.0f, -1.0f), float2(1.0f, 1.0f)}}; // bottom right
        m_indexBuffer = {
            0, 1, 3, 0, 3, 2};

        m_boundingSphere = float4{ 0.0f, 0.0f, 0.0f, 0.707f };
    #endif

    }

}
