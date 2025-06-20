#define FRAG_DECODER_H
#include <stdint.h>
#include <stdbool.h>


#define FRAG_MAX_REDUNDANCY         20 // Massima ridondanza
#define FRAG_MAX_NB                 256 // Massimo numero di frammenti (M)
#define FRAG_MAX_SIZE               53 // Massima dimensione di un frammento (bytes) 51 + 2(Index&N)

// Codici di stato
#define FRAG_SESSION_ONGOING        -1
#define FRAG_SESSION_FINISHED       0

namespace fragmentedApp {

typedef struct{
    //callback per scrivere i dati decodificati
    void ( *FragDecoderWrite )( uint32_t addr, uint8_t *buffer, uint32_t size );
    //callback per leggere i dati memorizzati
    void ( *FragDecoderRead )( uint32_t addr, uint8_t *buffer, uint32_t size );
    //callback per cancellare i dati memorizzati
    void ( *FragDecoderErase )( void );
} FragDecoderCallbacks_t;

//struct per contenere lo stato attuale del decoder
typedef struct{
    uint16_t FragNbRx;
    uint16_t FragNbLost;
    uint16_t FragNbLastRx;
    uint8_t MatrixError; //flag che indica un errore nella matrice
} FragDecoderStatus_t;

//inizializza il decoder di frammentazione
void FragDecoderInit( uint16_t fragNb, uint8_t fragSize, FragDecoderCallbacks_t *callbacks, uint8_t fragPVer );

//elabora un frammento ricevuto
int32_t FragDecoderProcess( uint16_t fragCounter, uint8_t *rawData );

//recupera lo stato attuale del decoder
FragDecoderStatus_t FragDecoderGetStatus( void );

}

