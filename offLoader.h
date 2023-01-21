#ifndef _OFF_LOADER_H_
#define _OFF_LOADER_H_

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdint>
#include <vector>
#include <array>
#include <regex>

#include <assert.h>

namespace {
    template< typename val_T >
    static val_T strToNum( const std::string& stringRep ) {
        std::stringstream stringRepStream{ stringRep };
        val_T converted;
        stringRepStream >> converted;
        return converted;
    }

    template< typename val_T >
    static val_T maximumM( const val_T& a, const val_T& b ) {
        return ( ( a >= b ) ? a : b );
    }
} // namespace

struct Mesh {

    size_t numVerts() const { return mVertexPositions.size(); }
    size_t numFaces() const { return mTriangleFaceIndices.size(); }

    std::array< float, 3 > *const pVertexPositions() { return mVertexPositions.data(); }
    const std::array< float, 3 > *const pVertexPositions() const { return mVertexPositions.data(); }

    const std::array< int32_t, 3 > *const pTriangleFaceIndices() const { return mTriangleFaceIndices.data(); }
    const std::array< float, 4 >& boundingSphere() const { return mBoundingSphere; }

    int32_t loadOff( const std::string filePath ) {

        // NOTE: check regexes online at https://regex101.com/ 

        const std::string     commentRegexPattern = R"(#.*\n)"; //R"(#[\s\S]*\n)";
        const std::regex      commentRegex{ commentRegexPattern };

        std::ifstream fileIfstream( filePath );

        std::string fileContent( 
            ( std::istreambuf_iterator< char >( fileIfstream ) ), 
            ( std::istreambuf_iterator< char >() ) );

/*
        { // DEBUG: print all comments that were found
            std::smatch commentMatch;
            std::string searchStr = fileContent;
            while ( std::regex_search( searchStr, commentMatch, commentRegex ) ) {
                std::cout << "commentMatch = " << commentMatch.str() << std::endl;
                searchStr = commentMatch.suffix();
            }
        }
*/

        const std::string fileContentStrippedComments = std::regex_replace( fileContent, commentRegex, "\n" );

    #if 0
        std::cout << "\n ---------------------------------- \n";
        //std::cout << "fileContent = \n" << fileContent << std::endl;
        //std::cout << "commentRegexPattern = '" << commentRegexPattern << "'" << std::endl;
        //!!! std::cout << "fileContentStrippedComments = \n" << fileContentStrippedComments << std::endl;
        std::cout << "fileContentStrippedComments[0-200]substr = \n" << fileContentStrippedComments.substr( 0, 200 ) << std::endl;
        std::cout << "\n ---------------------------------- \n";
    #endif

        const std::string statsRegexPattern = R"(\s*(\d+)\s+(\d+)\s+(\d+)\s*)"; // numVerts, numFaces, numEdges [ignore numEdges]
        const std::string headerAndDataRegexPattern = 
            std::string{ R"([\s\n]*(?:OFF)?[\s\n]*)" } + // consume first (optional) 'OFF'
            statsRegexPattern + // number of vertices, faces, and edges
            R"(([\s\S]*))"; // data section (will be processed separately)

        size_t numFaces = 0;

        const std::regex headerAndDataRegex{ headerAndDataRegexPattern };
        std::smatch headerVsDataMatch;
        if ( std::regex_search( fileContentStrippedComments, headerVsDataMatch, headerAndDataRegex ) ) {
            //std::cout << "headerVsDataMatch = " << headerVsDataMatch.str() << std::endl;
            size_t i = 0;
            for ( const auto& match: headerVsDataMatch ) {
                //if ( i > 0 && i < headerVsDataMatch.size() - 1 ) { std::cout << "headerVsDataMatch " << i << " = " << match.str() << std::endl; }
                if ( i == 1 ) { // 1st sub match
                    mVertexPositions.resize( strToNum< size_t >( match.str() ) );
                    //std::cout << "\tnumVertices = " << mVertexPositions.size() << std::endl;
                } else if ( i == 2 ) { // 2nd sub match
                    // note: if faces are given as polygons with more than 3 indices, we have to split them into triangles!
                    numFaces = strToNum< size_t >( match.str() );
                    //std::cout << "\tnumFaces = " << numFaces << std::endl;
                    mTriangleFaceIndices.reserve( numFaces );
                }
                i++;
            }
        } else { std::cout << "no match" << std::endl; }

        // std::cout << "\n ---------------------------------- \n";
        
        // regarding the line anchors ^and $
        // https://stackoverflow.com/questions/6127317/anchors-in-regex
        //    $ matches the position before the line break, and ^ matches the start of a line
        //    NOTE: $ looks the char before the line break, which may be a digit actually
        const std::string roughFloatRegexPattern = R"(([\+\-]?(?:\d*\.?\d+|\d+\.?\d*)))";
        const std::string vertexRegexPattern = 
            std::string{ R"(^\s*)" } +
            roughFloatRegexPattern +
            R"(\s+)" + 
            roughFloatRegexPattern +
            R"(\s+)" + 
            roughFloatRegexPattern +
            R"(\s*$)";

        const std::regex  vertexRegex{ vertexRegexPattern };

        std::string dataLine; 
        const std::string& dataString = headerVsDataMatch[ headerVsDataMatch.size() - 1 ];
        std::istringstream dataStream{ dataString };
        size_t numVerticesRead = 0;
        while ( std::getline( dataStream, dataLine ) ) {
            //std::cout << "current line = '" << dataLine << "'\n"; // looks OK

            std::smatch vertexDataMatch;
            if ( std::regex_match( dataLine, vertexDataMatch, vertexRegex ) ) {
                const float x = strToNum< float >( vertexDataMatch[ 1 ] );
                const float y = strToNum< float >( vertexDataMatch[ 2 ] );
                const float z = strToNum< float >( vertexDataMatch[ 3 ] );
                mVertexPositions[ numVerticesRead ] = std::array< float, 3 >{ x, y, z };
                numVerticesRead++;
            }
            if ( numVerticesRead == mVertexPositions.size() ) { 
                // std::cout << "numVerticesRead = " << numVerticesRead << std::endl;
                break; 
            }
        }

        // faces
        const std::regex intRegex{ R"(\s*(\d+)\s*)" };
        const std::regex faceRegex{ R"(\s*(\d+)((?:\s+\d+)+)\s*)" };
        size_t numFacesRead = 0;
        while ( std::getline( dataStream, dataLine ) ) {
            std::smatch faceDataMatch;
            if ( std::regex_match( dataLine, faceDataMatch, faceRegex ) ) {

                std::smatch indexMatch;
                std::string matchString = faceDataMatch[ 2 ];

                std::cout << "matchString = " << matchString << std::endl << std::flush;

                int numVertsInFace = strToNum< int >( faceDataMatch[ 1 ] );

                if ( numVertsInFace != 3 ) {
                    fprintf( stderr, " !!! We are expecting triangle meshes, but instead of 3 vertices, this face consists of %d vertices !!!\n", numVertsInFace );
                    assert( false );
                }

                std::array< int32_t, 3 > triangleFaceIndices;
                size_t vertIdxNum = 0;
                while ( std::regex_search( matchString, indexMatch, intRegex ) ) {
                    //printf( "%d ", strToNum< int >( indexMatch[ 1 ] ) );
                    triangleFaceIndices[ vertIdxNum ] = strToNum< int32_t >( indexMatch[ 1 ] );

                    matchString = indexMatch.suffix();

                    if (  vertIdxNum >= 2 ) { break; }
                    vertIdxNum++;
                }
                mTriangleFaceIndices.push_back( triangleFaceIndices );
                //printf( "\n" );

                std::array< float, 4 > triangleColors;
                size_t vertColorIdxNum = 0;
                while ( std::regex_search( matchString, indexMatch, intRegex ) ) {
                    //printf( "%d ", strToNum< int >( indexMatch[ 1 ] ) );
                    triangleFaceIndices[ vertColorIdxNum ] = strToNum< int32_t >( indexMatch[ 1 ] );

                    matchString = indexMatch.suffix();

                    if (  vertColorIdxNum >= 3 ) { break; }
                    vertColorIdxNum++;
                }
                if ( vertColorIdxNum > 0 ) { mTriangleFaceColors.push_back( triangleColors ); }

                numFacesRead++;
            }
            if ( numFacesRead == numFaces ) { 
                // std::cout << "numFacesRead = " << numFacesRead << std::endl;
                break; 
            }
        }

        assert( numFacesRead == numFaces );

        calculateBoundingSphere();

        return 0;
    }

    float calculateArea() {
        double area = 0.0;
        for ( const auto& triangleFaceIndices : mTriangleFaceIndices ) {
            const auto& vertex0 = mVertexPositions[ triangleFaceIndices[ 0 ] ];
            const auto& vertex1 = mVertexPositions[ triangleFaceIndices[ 1 ] ];
            const auto& vertex2 = mVertexPositions[ triangleFaceIndices[ 2 ] ];      
            const auto e0 = std::array< double, 3 >{ vertex1[ 0 ] - vertex0[ 0 ], vertex1[ 1 ] - vertex0[ 1 ], vertex1[ 2 ] - vertex0[ 2 ] };
            const auto e1 = std::array< double, 3 >{ vertex2[ 0 ] - vertex0[ 0 ], vertex2[ 1 ] - vertex0[ 1 ], vertex2[ 2 ] - vertex0[ 2 ] };

            // length of cross prod * 0.5 == triangle area
            const auto normal = std::array< double, 3 >{ 
                e0[ 1 ] * e1[ 2 ] - e0[ 2 ] * e1[ 1 ], 
                e0[ 2 ] * e1[ 0 ] - e0[ 0 ] * e1[ 2 ], 
                e0[ 0 ] * e1[ 1 ] - e0[ 1 ] * e1[ 0 ] };
            const auto length = sqrt( normal[ 0 ] * normal[ 0 ] + normal[ 1 ] * normal[ 1 ] + normal[ 2 ] * normal[ 2 ] );
            area += length;
        }
        return static_cast<float>(area * 0.5f);
    }

#if 1
    void calculateBoundingSphere() {
        std::array< double, 3 > centerPos{ 0.0, 0.0, 0.0 };
        // calculate center
        for ( const auto& vertexPos : mVertexPositions ) {
            centerPos[ 0 ] += vertexPos[ 0 ];
            centerPos[ 1 ] += vertexPos[ 1 ];
            centerPos[ 2 ] += vertexPos[ 2 ];
        }
        const double numVerts = static_cast<double>(mVertexPositions.size());
        centerPos[ 0 ] /= numVerts;
        centerPos[ 1 ] /= numVerts;
        centerPos[ 2 ] /= numVerts;
        
        double radiusSquared = 0.0;
        for ( const auto& vertexPos : mVertexPositions ) {
            const double distX = vertexPos[ 0 ] - centerPos[ 0 ];
            const double distY = vertexPos[ 1 ] - centerPos[ 1 ];
            const double distZ = vertexPos[ 2 ] - centerPos[ 2 ];
            const double currRadiusSquared = distX * distX + distY * distY + distZ * distZ;
            radiusSquared = maximumM( radiusSquared, currRadiusSquared );
        }
        const double boundingRadius = sqrt( radiusSquared );
        mBoundingSphere = std::array< float, 4 >{ 
            static_cast< float >( centerPos[ 0 ] ), 
            static_cast< float >( centerPos[ 1 ] ), 
            static_cast< float >( centerPos[ 2 ] ), 
            static_cast< float >( boundingRadius ) };
    }
#else // bounding sphere from bounding box => very conservative!
    void calculateBoundingSphere() {
        float xMin = mVertexPositions[ 0 ][ 0 ];
        float xMax = mVertexPositions[ 0 ][ 0 ];
        float yMin = mVertexPositions[ 0 ][ 1 ];
        float yMax = mVertexPositions[ 0 ][ 1 ];
        float zMin = mVertexPositions[ 0 ][ 2 ];
        float zMax = mVertexPositions[ 0 ][ 2 ];
        for ( const auto& vertexPos : mVertexPositions ) {
            const float currX = vertexPos[ 0 ];
            const float currY = vertexPos[ 1 ];
            const float currZ = vertexPos[ 2 ];
            if ( currX < xMin ) { xMin = currX; }
            if ( currX > xMax ) { xMax = currX; }
            if ( currY < yMin ) { yMin = currY; }
            if ( currY > yMax ) { yMax = currY; }
            if ( currZ < zMin ) { zMin = currZ; }
            if ( currZ > zMax ) { zMax = currZ; }
        }
        const float centerX = ( xMin + xMax ) * 0.5f;
        const float centerY = ( yMin + yMax ) * 0.5f;
        const float centerZ = ( zMin + zMax ) * 0.5f;
        const float halfLenX = centerX - xMin;
        const float halfLenY = centerY - yMin;
        const float halfLenZ = centerZ - zMin;
        // CORRECT 
        const float radius = sqrt( halfLenX * halfLenX + halfLenY * halfLenY + halfLenZ * halfLenZ ); // CORRECT
        //const float radius = maximumM( maximumM( halfLenX, halfLenY ), halfLenZ ); // DEBUG !!!
        mBoundingSphere = std::array< float, 4 >{ centerX, centerY, centerZ, radius };
    }
#endif

    private:

    std::vector< std::array< float, 3 > >   mVertexPositions;
    std::vector< std::array< int32_t, 3 > > mTriangleFaceIndices; // NOTE: may need to split quads etc. into tris
    std::vector< std::array< float, 4 > >   mTriangleFaceColors; //optional
    std::array< float, 4 >                  mBoundingSphere;
};

#endif //_OFF_LOADER_H_
