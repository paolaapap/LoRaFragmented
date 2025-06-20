
#define LORA_FRAGMENTED_APP_H

#include <vector>
#include <string>
#include <functional>
#include "FragEncoderUtils.h"
#include "FragDecoder.h"

#include "inet/common/packet/Packet.h"

using namespace omnetpp;
using namespace inet;

namespace loraNode {

class LoRaFragmentedApp : public cSimpleModule{
protected:

    int numFragments;
    int fragmentSize;
    int numCodedFragmentsToSend;
    uint8_t fragmentationPackageVersion;
    bool isSender;
    bool isReceiver;
    simsignal_t packetSentSignal;
    simsignal_t packetReceivedSignal;
    simsignal_t fileDecodedSignal;

    //Variabili per il Mittente
    std::vector<uint8_t> originalFileBuffer;
    std::vector<std::vector<uint8_t>> uncodedFragments;
    std::vector<std::vector<uint8_t>> codedFragments;
    int nextFragmentToSend;
    cMessage *sendTimer;
    simtime_t sendInterval;
    L3Address destinationAddress;

    //Variabili per il Ricevitore
    std::vector<uint8_t> decodedFileBuffer;
    fragmentedApp::FragDecoderCallbacks_t decoderCallbacks;
    int receivedFragmentsCount;
    int decodedFragmentsCount;



    void generateCodedFragments();
    void sendFragment(const std::vector<uint8_t>& fragmentData, int fragmentIndex);

    //callback del decodificatore
    void decoderWriteCallback(uint32_t addr, uint8_t *buffer, uint32_t size);
    void decoderReadCallback(uint32_t addr, uint8_t *buffer, uint32_t size);
    void decoderEraseCallback();


    virtual void initialize(int stage) override;
    virtual int numInitStages() const override { return 2; } // Necessario per UDP/INET
    virtual void handleMessage(cMessage *msg) override;
    virtual void refreshDisplay() const override;
    virtual void finish() override;


    virtual void setCallback(IUdpApplication* callback) override {}
    virtual bool isToApplication(Packet* msg) override { return false; }
    virtual void handleUdpMessage(Packet* msg) override;
    virtual void setSocket(UdpSocket* socket) override {}
    virtual UdpSocket* getSocket() override { return nullptr; }
};

}
