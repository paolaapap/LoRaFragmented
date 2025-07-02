
#ifndef __FLORA_LORAFRAGMENTEDAPP_H_
#define __FLORA_LORAFRAGMENTEDAPP_H_

#include <omnetpp.h>
#include "FragEncoder.h"
#include "FragDecoder.h"
#include <vector>
#include <cstdint>

#define FRAGMENTATION_DATA_FRAGMENT_CMD 0x08
#define FRAGMENTATION_PORT 201

#define START_TRANSMISSION_MSG_KIND 100

namespace flora {

std::vector<std::vector<uint8_t>> generateCodedFragments(
    const std::vector<uint8_t>& originalFileBuffer,
    uint16_t numFragments,
    uint8_t fragmentSize,
    uint16_t numCodedFragmentsToGenerate
);


#define FRAG_SESSION_ONGOING            -1
#define FRAG_SESSION_FINISHED_OK         0
#define FRAG_SESSION_FINISHED_ERROR      1

enum FragSessionStatus {
    FRAG_SESSION_NOT_STARTED = -2,
    FRAG_SESSION_ONGOING_APP = -1,
    FRAG_SESSION_FINISHED_OK_APP = 0,
    FRAG_SESSION_FINISHED_ERROR_APP = 1
};

struct FragDecoderAppStatus {
    uint16_t FragNbRx;
    uint16_t FragNbLost;
    bool MatrixError;
};

struct FragSessionParameters {
    uint16_t fragNb;
    uint8_t fragSize;
    uint8_t m;
    uint8_t redundancy;
    uint8_t padding;
    uint8_t fragIndex;
    uint32_t dataSize;
};

class LoRaFragmentedApp : public omnetpp::cSimpleModule
{
  protected:
    FragSessionParameters sessionParams;

    std::vector<uint8_t> decoderMemoryBuffer;
    uint32_t decoderMemorySize;

    std::vector<uint8_t> rxFragmentBuffer;

    FragSessionStatus txSessionStatus;
    FragSessionStatus rxSessionStatus;

    uint32_t packetsSent;
    uint32_t packetsReceived;
    uint32_t packetsPassedToDecoder;
    uint32_t successfulDecodings;
    uint32_t failedDecodings;

    omnetpp::simsignal_t packetsSentSignal;
    omnetpp::simsignal_t fragmentsReceivedSignal;
    omnetpp::simsignal_t fragmentsLostSignal;
    omnetpp::simsignal_t matrixErrorSignal;
    omnetpp::simsignal_t decodingSuccessSignal;
    omnetpp::simsignal_t overheadRatioSignal;

    omnetpp::cGate* outGate;

    omnetpp::cMessage* startTxMsg;

    static void fragDecoderWrite(uint32_t addr, uint8_t *buffer, uint32_t size);
    static void fragDecoderRead(uint32_t addr, uint8_t *buffer, uint32_t size);
    static void fragDecoderErase();

    virtual void initialize() override;
    virtual void handleMessage(omnetpp::cMessage* msg) override;
    virtual void finish() override;

    void startTransmission(const std::vector<uint8_t>& originalData);
    void processLoRaPacket(omnetpp::cPacket* pkt);
    void resetRxSession();

  public:
    LoRaFragmentedApp();
    virtual ~LoRaFragmentedApp();

    FragDecoderAppStatus getDecoderStatus() const;
    FragSessionStatus getTxSessionStatus() const { return txSessionStatus; }
    FragSessionStatus getRxSessionStatus() const { return rxSessionStatus; }
    uint32_t getPacketsSent() const { return packetsSent; }
    uint32_t getPacketsReceived() const { return packetsReceived; }
    uint32_t getPacketsPassedToDecoder() const { return packetsPassedToDecoder; }
    uint32_t getSuccessfulDecodings() const { return successfulDecodings; }
    uint32_t getFailedDecodings() const { return failedDecodings; }
};

}

#endif
