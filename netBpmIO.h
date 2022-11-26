#ifndef _NETBPMIO_H_914c094e_61f3_4b57_ba38_80cba8f79b82
#define _NETBPMIO_H_914c094e_61f3_4b57_ba38_80cba8f79b82

#include "stringUtils.h"
#include "eRetVal_FileLoader.h"

#include <cstdint>
#include <iostream> 
#include <fstream>
#include <vector>
#include <map>
#include <memory>
#include <regex>

#include <cassert>

namespace FileLoader {
    namespace netBpmIO {
        enum class eEncoding {
            BINARY_LITTLE_ENDIAN,
            BINARY_BIG_ENDIAN,
            ASCII,
            UNKNOWN,
        };

        enum class ePfmColorMode {
            gray,
            rgb,
        };

        // https://stackoverflow.com/questions/1513209/is-there-a-way-to-use-fopen-s-with-gcc-or-at-least-create-a-define-about-it
#if defined( __unix ) || defined( __APPLE__ ) 
#define fopen_s(pFile,filename,mode) ((*(pFile))=fopen((filename),(mode)))==NULL
#endif

        template< typename val_T >
        static eRetVal readPpm(
            const std::string& filePath,
            std::vector< val_T >& pData,
            int32_t& numSrcChannels,
            int32_t& dimX,
            int32_t& dimY ) {

            std::ifstream fileStream( filePath );
            std::string contentString(
                (std::istreambuf_iterator< char >( fileStream )),
                std::istreambuf_iterator< char >() );

            // strip comments
            const std::string commentPattern = R"((?:#.*))";
            const std::regex commentRegex{ commentPattern };
            const std::string contentStringNoComments = std::regex_replace( contentString, commentRegex, "" );
            //printf( " orig: '%s'\n replaced: '%s'\n", contentString.c_str(), contentStringNoComments.c_str() ); fflush( stdout );

            // get mode (binary / ascii)
            const std::string modeTokenPattern = "((?:P3)|(?:P6))";
            const std::regex modeTokenRegex{ modeTokenPattern };
            std::smatch tokenMatch;
            assert( std::regex_search( contentStringNoComments, tokenMatch, modeTokenRegex ) ); // must be either 'P3' or 'P6'

            const std::string& dataMode = tokenMatch[0].str();
            const bool isBinary = (dataMode == "P6");

            const std::string& headerImgSizeAndMaxVal = tokenMatch.suffix();

            const char* const pHeaderImgSizeAndMaxVal = headerImgSizeAndMaxVal.c_str();

            const std::string imgSizeAndMaxValPattern = R"([\s^.]*(\d+)[\s^.]+(\d+)[\s^.]+(\d+)[\s^.]+)"; // 3 integers separated by spaces and terminated by space or newline
            const std::regex imgSizeAndMaxValRegex{ imgSizeAndMaxValPattern };
            std::smatch imgSizeAndMaxValMatches;
            assert( std::regex_search( headerImgSizeAndMaxVal, imgSizeAndMaxValMatches, imgSizeAndMaxValRegex ) );

            // for ( const auto& searchResult : imgSizeAndMaxValMatches ) {
            //     printf( "match: %s\n", searchResult.str().c_str() );
            // }
            // fflush( stdout );

            assert( imgSizeAndMaxValMatches.size() == 4 ); // match 0 is the whole string
            dimX = stringUtils::convStrTo< int32_t >( imgSizeAndMaxValMatches[1] );
            dimY = stringUtils::convStrTo< int32_t >( imgSizeAndMaxValMatches[2] );
            numSrcChannels = 3;
            int32_t maxVal = stringUtils::convStrTo< int32_t >( imgSizeAndMaxValMatches[3] );

            const size_t imgNumEntries = dimX * dimY * numSrcChannels;
            const size_t imgByteSize = imgNumEntries * sizeof( val_T );

            if (isBinary) {
                FILE* pFile = nullptr;
                //auto fopenErr = 
                fopen_s( &pFile, filePath.c_str(), "rb" );
                if (pFile == nullptr) { return eRetVal::ERROR; }

                fseek( pFile, -imgByteSize, SEEK_END );
                pData.resize( imgNumEntries );
                //fread( pData.data(), imgNumEntries, sizeof( val_T ), pFile );
                for (int32_t y = dimY - 1; y >= 0; y--) {
                    fread( pData.data() + y * dimX * numSrcChannels, dimX * numSrcChannels, sizeof( val_T ), pFile );
                }
                fclose( pFile );
            }
            else {
                printf( "starting PPM ascii read\n" );
#if 1
                const std::regex digitRegex{ R"((\d+)\s+)" };
                std::smatch digitMatch;
                std::string contentString = imgSizeAndMaxValMatches.suffix();
                pData.reserve( imgNumEntries );
                int32_t loopIdx = 0;
                int32_t maxLoopIdx = dimX * dimY * numSrcChannels;

                while (std::regex_search( contentString, digitMatch, digitRegex )) {
                    assert( digitMatch.size() == 2 ); // first match is the whole thing
                    pData.push_back( stringUtils::convStrTo< val_T >( digitMatch[1].str() ) );
                    contentString = digitMatch.suffix().str();
                    //printf( "%d of %d\n", loopIdx, maxLoopIdx );
                    loopIdx++;
                }
#elif 1
                const std::regex& rgbTripleRegex = imgSizeAndMaxValRegex;
                std::string contentString = imgSizeAndMaxValMatches.suffix();
                //std::smatch rgbTripleMatch;
                std::smatch& rgbTripleMatch = imgSizeAndMaxValMatches;
                pData.reserve( imgNumEntries );
                while (std::regex_search( contentString, rgbTripleMatch, rgbTripleRegex )) {
                    //while ( std::regex_search( rgbTripleMatch.suffix().str(), rgbTripleMatch, rgbTripleRegex ) ) {
                    assert( rgbTripleMatch.size() == 4 ); // first match is the whole thing
                    for (size_t c = 1; c <= 3; c++) {
                        //printf( "%s ", rgbTripleMatch[ c ].str().c_str() );
                        pData.push_back( stringUtils::convStrTo< val_T >( rgbTripleMatch[c].str() ) );
                    }
                    //printf( "\n" );
                    contentString = rgbTripleMatch.suffix().str();
                }
#else
                const std::regex& rgbTripleRegex = imgSizeAndMaxValRegex;
                std::string contentString = imgSizeAndMaxValMatches.suffix();
                //std::smatch rgbTripleMatch;
                std::smatch& rgbTripleMatch = imgSizeAndMaxValMatches;
                pData.resize( imgNumEntries );
                for (int32_t y = dimY - 1; y >= 0; y--) {
                    for (int32_t x = 0; x < dimX; x++) {
                        //for ( int32_t c = 0; c < numSrcChannels; c++ ) {
                        assert( std::regex_search( contentString, rgbTripleMatch, rgbTripleRegex ) && rgbTripleMatch.size() == 4 );
                        for (size_t c = 1; c <= 3; c++) {
                            pData[(y * dimX + x) * numSrcChannels + c] = stringUtils::convStrTo< val_T >( rgbTripleMatch[c] );
                        }
                        contentString = rgbTripleMatch.suffix();
                    }
                }
#endif
                printf( "ended PPM ascii read\n" );
            }

            return eRetVal::OK;
        }

        template< typename val_T >
        static eRetVal writePpm(
            const std::string& filePath,
            const val_T* const pData,
            const int32_t numSrcChannels,
            const int32_t dimX,
            const int32_t dimY,
            const bool storeBinary,
            const std::string& comment ) {
            FILE* pFile = fopen( filePath.c_str(), "w" );
            const std::string modeString = (storeBinary == false) ? "P3" : "P6";
            fprintf( pFile, "%s\n%d %d %d\n", modeString.c_str(), dimX, dimY, 255 );
            if (!storeBinary) { fprintf( pFile, "#%s\n", comment.c_str() ); } // depending on length of string, messes up binary file (colors inverted, image shifted

            if (storeBinary) {
                fclose( pFile );
                //pFile = fopen( filePath.c_str(), "ab" );
                pFile = fopen( filePath.c_str(), "ab" );
                //fseek( pFile, 0, SEEK_END );
                //fwrite( pData, sizeof( uint8_t) * numSrcChannels, dimX * dimY, pFile ); // turned upside down
                for (int32_t y = dimY - 1; y >= 0; y--) { // write line-by-line, flipping the y coord
                    fwrite( pData + y * dimX * numSrcChannels, sizeof( uint8_t ) * numSrcChannels, dimX, pFile );
                }
            }
            else {
                for (int32_t y = dimY - 1; y >= 0; y--) {
                    for (int32_t x = 0; x < dimX; x++) {
                        int32_t i = (x + y * dimX) * numSrcChannels;
                        for (int32_t c = 0; c < numSrcChannels; c++) {
                            fprintf( pFile, "%d ", static_cast<int32_t>(pData[i + c]) );
                        }
                    }
                }
            }
            fclose( pFile );
            return eRetVal::OK;
        }

        static eRetVal readPfm(
            const std::string& filePath,
            std::vector< float >& data,
            int32_t& numSrcChannels,
            int32_t& dimX,
            int32_t& dimY,
            float& scale,
            ePfmColorMode& colorMode ) {

            printf( " - in readPfm\n" );

            // ### ASCII header ###
            std::ifstream fileStream( filePath );
            const std::string contentString(
                (std::istreambuf_iterator< char >( fileStream )),
                std::istreambuf_iterator< char >() );

            // const std::string contentString = 
            // R"(PF # comment 1
            // 600 400
            // -1.0 # comment 2 
            // ... binary file content ...

            const std::string commentPattern = R"((?:#\S*[\s^.]))";

            const std::string whitespaceOrNotNewlinePattern = R"([\s.])";
            const std::string atLeastOneWhitespaceOrOneNewline = R"((?:\s+|\n))";
            const std::string intPattern = R"(\d+)";
            const std::string floatingPointPattern = R"([-+]?(?:[0-9]*))(?:.[0-9]*)?)";
            std::string headerPattern = std::string{ R"(\s*)" } + R"(((?:PF)|(?:Pf)))" +
                commentPattern + "?" +
                whitespaceOrNotNewlinePattern + "*" + atLeastOneWhitespaceOrOneNewline +
                R"(\s*)" + "(" + intPattern + R"()\s+()" + intPattern + ")" + atLeastOneWhitespaceOrOneNewline +
                R"(\s*)" + "(" + "(?:" + intPattern + "|" + floatingPointPattern + ")";

            const std::regex headerRegex{ headerPattern };
            std::smatch regexMatches;
            bool didRegexMatch = std::regex_search(
                contentString,
                regexMatches,
                headerRegex );
            printf( "didRegexMatch = %s\n", didRegexMatch ? "yes" : "no" );
            assert( regexMatches.size() == 5 );
            if (regexMatches.size() != 5) { return eRetVal::ERROR; }

            if (regexMatches[1] == "PF") { colorMode = ePfmColorMode::rgb; numSrcChannels = 3; }
            else if (regexMatches[1] == "Pf") { colorMode = ePfmColorMode::gray; numSrcChannels = 1; }

            dimX = stringUtils::convStrTo< int32_t >( regexMatches[2] );
            dimY = stringUtils::convStrTo< int32_t >( regexMatches[3] );
            scale = stringUtils::convStrTo< float >( regexMatches[4] );

            data.resize( numSrcChannels * dimX * dimY );

            fileStream.close();

            FILE* pFile = fopen( filePath.c_str(), "rb" );
            if (fseek( pFile, -sizeof( float ) * data.size(), SEEK_END ) != 0) { return eRetVal::ERROR; }
            fread( data.data(), sizeof( float ), data.size(), pFile );
            fclose( pFile );

            printf( " - readPfm before swap\n" );

            for (int32_t y = 0; y < dimY; y++) {
                for (int32_t x = 0; x < dimX / 2; x++) {
                    for (int32_t c = 0; c < numSrcChannels; c++) {
                        std::swap( data[(x + y * dimX) * numSrcChannels + c], data[(dimX - 1 - x + y * dimX) * numSrcChannels + c] );
                    }
                }
            }

            printf( " - readPfm done\n" );
            return (didRegexMatch ? eRetVal::OK : eRetVal::ERROR);
        }

        static eRetVal writePfm(
            const std::string& filePath,
            const float* const pData,
            const int32_t numSrcChannels, // will be 4 for an RGBA image, but will still be written to a 1-channel grayscale, or 3-channel rgb image, depending on 'colorMode'
            const int32_t dimX,
            const int32_t dimY,
            const float scale,
            const ePfmColorMode colorMode, // only RGB right now
            const std::string& comment = "" ) {

            // ### ASCII header ###
            FILE* fileFP = fopen( filePath.c_str(), "w" ); // write image to file
            //fprintf(fileFP, "PF # spp = %d\n", spp);
            fprintf( fileFP, "%s\n", (colorMode == ePfmColorMode::rgb) ? "PF" : "Pf" );
            fprintf( fileFP, "%d %d\n", dimX, dimY );
            //fprintf(fileFP, "-1 # rendering time: %f s\n", duration);
            fprintf( fileFP, "-1.0\n" ); // negative means little endian, absolute value denotes scale of values
            //fprintf( fileFP, "%f\n", -abs( scale ) ); // make sure it's little endian
            fclose( fileFP );

            // ### binary data ###
            fileFP = fopen( filePath.c_str(), "ab" );
            fseek( fileFP, 0, SEEK_END );

            const int32_t numDstChannels = (colorMode == ePfmColorMode::rgb) ? 3 : 1;

            if (numDstChannels == 1) { assert( numSrcChannels == numDstChannels ); }

            for (int y = 0; y < dimY; y++) {
                for (int x = 0; x < dimX; x++) {
                    size_t i = (dimX - 1 - x) + y * dimX;

                    if (numDstChannels == 3) {
                        float scaledValues[] = { pData[numSrcChannels * i], pData[numSrcChannels * i + 1], pData[numSrcChannels * i + 2] };
                        scaledValues[0] *= abs( scale ); scaledValues[1] *= abs( scale ); scaledValues[2] *= abs( scale );
                        fwrite( &scaledValues, sizeof( float ), numDstChannels, fileFP );
                    }
                    else {
                        float scaledValues[] = { pData[numSrcChannels * i] };
                        scaledValues[0] *= abs( scale );
                        fwrite( &scaledValues, sizeof( float ), numDstChannels, fileFP );
                    }
                }
            }
            fclose( fileFP );
            return eRetVal::OK;
        }
    } // netBpmIO
} // fileLoader

#endif // _NETBPMIO_H_914c094e_61f3_4b57_ba38_80cba8f79b82
