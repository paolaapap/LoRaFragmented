#include "FragDecoder.h"
#include "FragDecoderUtils.h"
#include <string.h>
#include <algorithm>

#define memset1 memset

namespace fragmentedApp {

typedef struct FragDecoder_t{
    FragDecoderCallbacks_t *Callbacks;
    uint16_t FragNb;                         // Numero totale di frammenti originali (M).
    uint8_t FragSize;                        // Dimensione di ogni frammento in byte.
    uint32_t M2BLine;
    uint8_t MatrixM2B[( ( FRAG_MAX_REDUNDANCY >> 3 ) + 1 ) * FRAG_MAX_REDUNDANCY];
    uint16_t FragNbMissingIndex[FRAG_MAX_NB];
    uint8_t S[( ( FRAG_MAX_REDUNDANCY >> 3 ) + 1 )];
    FragDecoderStatus_t Status;
} FragDecoder_t;


static FragDecoder_t FragDecoder;
static uint8_t matrixRow[( FRAG_MAX_NB >> 3 ) + 1];
static uint8_t matrixDataTemp[FRAG_MAX_SIZE];
static uint8_t dataTempVector[( ( FRAG_MAX_REDUNDANCY >> 3 ) + 1 )];
static uint8_t dataTempVector2[( ( FRAG_MAX_REDUNDANCY >> 3 ) + 1 )];


static void SetRow( uint8_t *src, uint16_t row, uint16_t size ){
    if( ( FragDecoder.Callbacks != NULL ) && ( FragDecoder.Callbacks->FragDecoderWrite != NULL ) ){
        FragDecoder.Callbacks->FragDecoderWrite( (uint32_t)row * size, src, (uint32_t)size );
    }
}


static void GetRow( uint8_t *dst, uint16_t row, uint16_t size ){
    if( ( FragDecoder.Callbacks != NULL ) && ( FragDecoder.Callbacks->FragDecoderRead != NULL ) ){
        FragDecoder.Callbacks->FragDecoderRead( (uint32_t)row * size, dst, (uint32_t)size );
    }
}


static void FragFindMissingFrags( uint16_t counter ){
    int32_t i;
    for( i = FragDecoder.Status.FragNbLastRx; i < ( counter - 1 ); i++ ){
        if( i < FragDecoder.FragNb ){
            FragDecoder.Status.FragNbLost++;
            FragDecoder.FragNbMissingIndex[i] = FragDecoder.Status.FragNbLost;
        }
    }

    if( i < FragDecoder.FragNb ){
        FragDecoder.Status.FragNbLastRx = counter;
    }
    else{
        FragDecoder.Status.FragNbLastRx = FragDecoder.FragNb + 1;
    }
}


static uint16_t FragFindMissingIndex( uint16_t x ){
    for( uint16_t i = 0; i < FragDecoder.FragNb; i++ ){
        if( FragDecoder.FragNbMissingIndex[i] == ( x + 1 ) ){
            return i;
        }
    }
    return 0;
}


static void FragExtractLineFromBinaryMatrix( uint8_t *bitArray, uint16_t rowIndex, uint16_t bitsInRow ){
    uint32_t findByte = 0;
    uint32_t findBitInByte = 0;
    uint32_t offsetBits = 0;

    for (uint16_t k = 0; k < rowIndex; ++k) {
        offsetBits += (bitsInRow - k);
    }

    findByte = offsetBits >> 3;
    findBitInByte = offsetBits % 8;
    std::fill(bitArray, bitArray + rowIndex, 0);

    for( uint16_t i = rowIndex; i < bitsInRow; i++ ){
        uint8_t bit = ( FragDecoder.MatrixM2B[findByte] >> ( 7 - findBitInByte ) ) & 0x01;
        fragmentedApp::SetParity( i, bitArray, bit );
        findBitInByte++;
        if( findBitInByte == 8 ){
            findBitInByte = 0;
            findByte++;
        }
    }
}


static void FragPushLineToBinaryMatrix( uint8_t *bitArray, uint16_t rowIndex, uint16_t bitsInRow ){
    uint32_t findByte = 0;
    uint32_t findBitInByte = 0;
    uint32_t offsetBits = 0;
    for (uint16_t k = 0; k < rowIndex; ++k) {
        offsetBits += (bitsInRow - k);
    }

    findByte = offsetBits >> 3;
    findBitInByte = offsetBits % 8;

    for( uint16_t i = rowIndex; i < bitsInRow; i++ ){
        uint8_t bit = fragmentedApp::GetParity(i, bitArray);
        uint8_t mask = (1 << (7 - findBitInByte));
        if (bit == 0) {
            FragDecoder.MatrixM2B[findByte] &= (0xFF - mask);
        } else {
            FragDecoder.MatrixM2B[findByte] |= mask;
        }

        findBitInByte++;
        if( findBitInByte == 8 ){
            findBitInByte = 0;
            findByte++;
        }
    }
}



void FragDecoderInit( uint16_t fragNb, uint8_t fragSize, FragDecoderCallbacks_t *callbacks ){
    FragDecoder.Callbacks = callbacks;
    FragDecoder.FragNb = fragNb;
    FragDecoder.FragSize = fragSize;
    FragDecoder.Status.FragNbLastRx = 0;
    FragDecoder.Status.FragNbLost = 0;
    FragDecoder.M2BLine = 0;

    for( uint16_t i = 0; i < FRAG_MAX_NB; i++ ){
        FragDecoder.FragNbMissingIndex[i] = 1;
    }

    for( uint32_t i = 0; i < ( ( FRAG_MAX_REDUNDANCY >> 3 ) + 1 ); i++ ){
        FragDecoder.S[i] = 0;
    }

    for( uint32_t i = 0; i < ( ( ( FRAG_MAX_REDUNDANCY >> 3 ) + 1 ) * FRAG_MAX_REDUNDANCY ); i++ ){
        FragDecoder.MatrixM2B[i] = 0xFF;
    }


    if( FragDecoder.Callbacks->FragDecoderErase != NULL ){
        FragDecoder.Callbacks->FragDecoderErase();
    }

    FragDecoder.Status.FragNbLost = 0;
    FragDecoder.Status.FragNbLastRx = 0;
}

int32_t FragDecoderProcess( uint16_t fragCounter, uint8_t *rawData ){
    uint16_t firstOneInRow = 0;
    int32_t first = 0;
    int32_t noInfo = 0;

    memset1( matrixRow, 0, ( FRAG_MAX_NB >> 3 ) + 1 );
    memset1( matrixDataTemp, 0, FRAG_MAX_SIZE );
    memset1( dataTempVector, 0, ( FRAG_MAX_REDUNDANCY >> 3 ) + 1 );
    memset1( dataTempVector2, 0, ( FRAG_MAX_REDUNDANCY >> 3 ) + 1 );

    FragDecoder.Status.FragNbRx = fragCounter;

    if( fragCounter < FragDecoder.Status.FragNbLastRx ){
        return FRAG_SESSION_ONGOING;
    }

    if( fragCounter <= FragDecoder.FragNb ){
        SetRow( rawData, fragCounter - 1, FragDecoder.FragSize );
        FragDecoder.FragNbMissingIndex[fragCounter - 1] = 0;
        FragFindMissingFrags( fragCounter );

        if( ( fragCounter == FragDecoder.FragNb ) && ( FragDecoder.Status.FragNbLost == 0U ) ){
            return FRAG_SESSION_FINISHED;
        }
    }
    else{
        if( FragDecoder.Status.FragNbLost > FRAG_MAX_REDUNDANCY ){
            FragDecoder.Status.MatrixError = 1;
            return FRAG_SESSION_FINISHED;
        }

        FragFindMissingFrags( fragCounter );

        if( FragDecoder.Status.FragNbLost == 0 ){
            return FRAG_SESSION_ONGOING;
        }

        fragmentedApp::FragGetParityMatrixRow( fragCounter - FragDecoder.FragNb, FragDecoder.FragNb, matrixRow );


        for( int32_t i = 0; i < FragDecoder.FragNb; i++ ){
            if( fragmentedApp::GetParity( i, matrixRow ) == 1 ){
                if( FragDecoder.FragNbMissingIndex[i] == 0 ) {
                    fragmentedApp::SetParity( i, matrixRow, 0 );
                    GetRow( matrixDataTemp, i, FragDecoder.FragSize );
                    fragmentedApp::XorDataLine( rawData, matrixDataTemp, FragDecoder.FragSize );
                }
                else {
                    fragmentedApp::SetParity( FragDecoder.FragNbMissingIndex[i] - 1, dataTempVector, 1 );
                    if( first == 0 ){
                        first = 1;
                    }
                }
            }
        }

        if( first > 0 ){
            firstOneInRow = fragmentedApp::BitArrayFindFirstOne( dataTempVector, FragDecoder.Status.FragNbLost );
            int32_t li;
            while( fragmentedApp::GetParity( firstOneInRow, FragDecoder.S ) == 1 ){
                FragExtractLineFromBinaryMatrix( dataTempVector2, firstOneInRow, FragDecoder.Status.FragNbLost );
                fragmentedApp::XorParityLine( dataTempVector, dataTempVector2, FragDecoder.Status.FragNbLost );
                li = FragFindMissingIndex( firstOneInRow );
                GetRow( matrixDataTemp, li, FragDecoder.FragSize );
                fragmentedApp::XorDataLine( rawData, matrixDataTemp, FragDecoder.FragSize );

                if( fragmentedApp::BitArrayIsAllZeros( dataTempVector, FragDecoder.Status.FragNbLost ) ){
                    noInfo = 1;
                    break;
                }
                firstOneInRow = fragmentedApp::BitArrayFindFirstOne( dataTempVector, FragDecoder.Status.FragNbLost );
            }

            if( noInfo == 0 ) {
                FragPushLineToBinaryMatrix( dataTempVector, firstOneInRow, FragDecoder.Status.FragNbLost );
                li = FragFindMissingIndex( firstOneInRow );
                SetRow( rawData, li, FragDecoder.FragSize );
                fragmentedApp::SetParity( firstOneInRow, FragDecoder.S, 1 );
                FragDecoder.M2BLine++;
            }

            if( FragDecoder.M2BLine == FragDecoder.Status.FragNbLost ){
                if( FragDecoder.Status.FragNbLost > 1 ){
                    int32_t i;
                    int32_t j;

                    for( i = ( FragDecoder.Status.FragNbLost - 2 ); i >= 0 ; i-- ){
                        li = FragFindMissingIndex( i );
                        GetRow( matrixDataTemp, li, FragDecoder.FragSize );
                        for( j = ( FragDecoder.Status.FragNbLost - 1 ); j > i; j-- ){
                            FragExtractLineFromBinaryMatrix( dataTempVector2, i, FragDecoder.Status.FragNbLost );
                            FragExtractLineFromBinaryMatrix( dataTempVector, j, FragDecoder.Status.FragNbLost );
                            if( fragmentedApp::GetParity( j, dataTempVector2 ) == 1 ){
                                int32_t lj = FragFindMissingIndex( j );
                                GetRow( rawData, lj, FragDecoder.FragSize );
                                fragmentedApp::XorDataLine( matrixDataTemp, rawData, FragDecoder.FragSize );
                            }
                        }
                        SetRow( matrixDataTemp, li, FragDecoder.FragSize );
                    }
                    return FRAG_SESSION_FINISHED;
                }
                else {
                    return FRAG_SESSION_FINISHED;
                }
            }
        }
    }
    return FRAG_SESSION_ONGOING;
}

FragDecoderStatus_t FragDecoderGetStatus( void ){
    return FragDecoder.Status;
}
}
