#include "LoRaFragmentedApp.h"
#include "FragEncoder.h"
#include "FragDecoder.h"

namespace flora {

using namespace fragmentedApp;

static std::vector<uint8_t>* globalDecoderMemoryBuffer = nullptr;
static uint32_t globalDecoderMemorySize = 0;

Define_Module(LoRaFragmentedApp);

LoRaFragmentedApp::LoRaFragmentedApp() :
    txSessionStatus(FRAG_SESSION_NOT_STARTED),
    rxSessionStatus(FRAG_SESSION_NOT_STARTED),
    packetsSent(0),
    packetsReceived(0),
    packetsPassedToDecoder(0),
    successfulDecodings(0),
    failedDecodings(0),
    outGate(nullptr),
    startTxMsg(nullptr),
    nextFragTimer(nullptr), 
    currentFragToSend(0),   
    decoderMemorySize(0)
{
}

LoRaFragmentedApp::~LoRaFragmentedApp()
{
    cancelAndDelete(startTxMsg);
    cancelAndDelete(nextFragTimer); 
    globalDecoderMemoryBuffer = nullptr;
    globalDecoderMemorySize = 0;
}


void LoRaFragmentedApp::fragDecoderWrite(uint32_t addr, uint8_t* buffer, uint32_t size) {
    if (globalDecoderMemoryBuffer && addr + size <= globalDecoderMemorySize) {
        std::copy(buffer,buffer + size,globalDecoderMemoryBuffer->begin() + addr);
    }
}

void LoRaFragmentedApp::fragDecoderRead(uint32_t addr, uint8_t* buffer, uint32_t size) {
    if (globalDecoderMemoryBuffer && addr + size <= globalDecoderMemorySize) {
        std::copy(globalDecoderMemoryBuffer->begin() + addr,
                  globalDecoderMemoryBuffer->begin() + addr + size,
                  buffer);
    }
}

void LoRaFragmentedApp::fragDecoderErase() {
    if (globalDecoderMemoryBuffer) {
        std::fill(globalDecoderMemoryBuffer->begin(), globalDecoderMemoryBuffer->end(), 0);
    }
}

void LoRaFragmentedApp::initialize()
{
    packetsSentSignal = registerSignal("packetsSent");
    fragmentsReceivedSignal = registerSignal("fragmentsReceived");
    fragmentsLostSignal = registerSignal("fragmentsLost");
    matrixErrorSignal = registerSignal("matrixError");
    decodingSuccessSignal = registerSignal("decodingSuccess");
    overheadRatioSignal = registerSignal("overheadRatio");

    sessionParams.m = par("M");
    sessionParams.redundancy = par("R");
    sessionParams.fragSize = par("fragmentSize");
    sessionParams.dataSize = par("dataSize");
    sessionParams.fragIndex = par("sessionIndex");

    sessionParams.fragNb = sessionParams.m + sessionParams.redundancy;

    uint32_t originalDataBytesForEncoding = sessionParams.m * sessionParams.fragSize;
    if (sessionParams.dataSize > originalDataBytesForEncoding) {
        sessionParams.padding = 0; 
    } else {
        sessionParams.padding = originalDataBytesForEncoding - sessionParams.dataSize;
    }

    decoderMemorySize = sessionParams.fragNb * sessionParams.fragSize;
    decoderMemoryBuffer.resize(decoderMemorySize);

    globalDecoderMemoryBuffer = &decoderMemoryBuffer;
    globalDecoderMemorySize = decoderMemorySize;

    FragDecoderCallbacks_t decoderCallbacks = {&LoRaFragmentedApp::fragDecoderWrite,
                                                &LoRaFragmentedApp::fragDecoderRead,
                                                &LoRaFragmentedApp::fragDecoderErase};
    fragmentedApp::FragDecoderInit(sessionParams.fragNb, sessionParams.fragSize, &decoderCallbacks);

    rxFragmentBuffer.resize(sessionParams.fragSize);

    outGate = gate("out");

    nextFragTimer = new omnetpp::cMessage("nextFragTimer");
    currentFragToSend = 0;

    startTxMsg = new omnetpp::cMessage("startTxMsg", START_TRANSMISSION_MSG_KIND);
    scheduleAt(0.0, startTxMsg);
}

void LoRaFragmentedApp::handleMessage(omnetpp::cMessage* msg)
{
    if (msg == startTxMsg) {
        std::vector<uint8_t> originalData(sessionParams.dataSize);
        for (uint32_t i = 0; i < sessionParams.dataSize; ++i) {
            originalData[i] = (uint8_t)(i % 256);
        }
        startTransmission(originalData);
        delete msg;
        startTxMsg = nullptr;

    } 
    else if (msg == nextFragTimer) {
        if (currentFragToSend < sessionParams.fragNb && currentFragToSend < fragmentsToSend.size()) {
            
            // 1. Recupera il frammento
            const std::vector<uint8_t>& currentFragment = fragmentsToSend[currentFragToSend];

            // 2. Crea Header
            uint16_t headerValue = (static_cast<uint16_t>(sessionParams.fragIndex & 0x03) << 14) | (currentFragToSend & 0x3FFF);

            // 3. Crea Packet OMNeT
            omnetpp::cPacket* packet = new omnetpp::cPacket("fragPacket");
            
            // 4. Prepara Payload
            std::vector<uint8_t> rawPayloadData(currentFragment.size() + 3);
            rawPayloadData[0] = FRAGMENTATION_DATA_FRAGMENT_CMD;
            rawPayloadData[1] = (headerValue >> 8) & 0xFF;
            rawPayloadData[2] = headerValue & 0xFF;
            std::copy(currentFragment.begin(), currentFragment.end(), rawPayloadData.begin() + 3);

            packet->setByteLength(rawPayloadData.size());
            uint8_t* payloadCopy = new uint8_t[rawPayloadData.size()];
            std::copy(rawPayloadData.begin(), rawPayloadData.end(), payloadCopy);
            packet->setContextPointer(payloadCopy);
            packet->setByteLength(rawPayloadData.size());

            // 5. Invia
            send(packet, outGate);
            packetsSent++;
            emit(packetsSentSignal, (double)packetsSent);

            EV << "Sent fragment index: " << currentFragToSend << endl;

            currentFragToSend++;

            // 6. Pianifica il prossimo invio
            scheduleAt(simTime() + 5.0, nextFragTimer);

        } else {
            EV << "Transmission session finished." << endl;
            txSessionStatus = FRAG_SESSION_FINISHED_OK_APP;
        }
    } 
    else if (msg->isPacket()) {
        omnetpp::cPacket* pkt = omnetpp::check_and_cast<omnetpp::cPacket*>(msg);
        processLoRaPacket(pkt);
        delete pkt;
    } else {
        EV_WARN << "Received unknown message kind: " << msg->getKind() << "\n";
        delete msg;
    }
}

void LoRaFragmentedApp::finish() {

    recordScalar("finalPacketsSent", (double)packetsSent);
    recordScalar("finalSuccessfulDecodings", (double)successfulDecodings);
    recordScalar("finalFailedDecodings", (double)failedDecodings);

    FragDecoderAppStatus finalStatus = getDecoderStatus();
    recordScalar("finalFragNbRx", (double)finalStatus.FragNbRx);
    recordScalar("finalFragNbLost", (double)finalStatus.FragNbLost);
    recordScalar("finalMatrixError", (double)finalStatus.MatrixError);

    double overheadRatioValue = 0.0;

    if (successfulDecodings > 0 && rxSessionStatus == FRAG_SESSION_FINISHED_OK_APP) {
        overheadRatioValue = (double)finalStatus.FragNbRx / sessionParams.m;
    }

    recordScalar("overheadRatio", overheadRatioValue);
}

void LoRaFragmentedApp::startTransmission(const std::vector<uint8_t>& originalData)
{
    if (outGate == nullptr) {
        EV_ERROR << "Output gate not connected. Cannot start transmission.\n";
        txSessionStatus = FRAG_SESSION_FINISHED_ERROR_APP;
        return;
    }

    txSessionStatus = FRAG_SESSION_ONGOING_APP;
    packetsSent = 0;
    emit(packetsSentSignal, 0.0);

    std::vector<uint8_t> paddedOriginalData = originalData;
    size_t expectedEncoderInputSize = static_cast<size_t>(sessionParams.m) * sessionParams.fragSize;
    if (paddedOriginalData.size() < expectedEncoderInputSize) {
        paddedOriginalData.resize(expectedEncoderInputSize, 0);
    } else if (paddedOriginalData.size() > expectedEncoderInputSize) {
        paddedOriginalData.resize(expectedEncoderInputSize);
    }

    fragmentsToSend = fragmentedApp::generateCodedFragments(paddedOriginalData,
                                                            sessionParams.m,
                                                            sessionParams.fragSize,
                                                            sessionParams.fragNb);

    if (fragmentsToSend.empty() || fragmentsToSend.size() != sessionParams.fragNb) {
        EV_ERROR << "Failed to generate coded fragments.\n";
        txSessionStatus = FRAG_SESSION_FINISHED_ERROR_APP;
        return;
    }

    currentFragToSend = 0;
    scheduleAt(simTime(), nextFragTimer);
}

void LoRaFragmentedApp::processLoRaPacket(omnetpp::cPacket* pkt)
{
    packetsReceived++;

    uint8_t* bytesPayload = static_cast<uint8_t*>(pkt->getContextPointer());

    if (bytesPayload == nullptr) {
        EV_ERROR << "Received packet with no payload. Ignoring.\n";
        return;
    }

    std::vector<uint8_t> packetData(bytesPayload, bytesPayload + pkt->getByteLength());

    if (packetData.size() < 3) {
        EV_WARN << "Received too small packet. Ignoring.\n";
        return;
    }

    uint8_t command = packetData[0];
    if (command != FRAGMENTATION_DATA_FRAGMENT_CMD) {
        EV_WARN << "Received control command. Ignoring for now.\n";
        return;
    }

    uint16_t headerValue = (static_cast<uint16_t>(packetData[1]) << 8) | packetData[2];
    uint8_t receivedFragIndex = (headerValue >> 14) & 0x03;
    uint16_t receivedFragCounter = headerValue & 0x3FFF;

    if (receivedFragIndex != sessionParams.fragIndex) {
        EV_WARN << "Fragment for wrong session index. Ignoring.\n";
        return;
    }

    size_t payloadSize = packetData.size() - 3;
    if (payloadSize != sessionParams.fragSize) {
        EV_WARN << "Received fragment with wrong payload size. Ignoring.\n";
        return;
    }
    std::copy(packetData.begin() + 3, packetData.end(), rxFragmentBuffer.begin());

    packetsPassedToDecoder++;

    if (rxSessionStatus == FRAG_SESSION_NOT_STARTED) {
        rxSessionStatus = FRAG_SESSION_ONGOING_APP;
    }

    int32_t decodeStatus = fragmentedApp::FragDecoderProcess(receivedFragCounter, rxFragmentBuffer.data());

    emit(fragmentsReceivedSignal, (double)FragDecoderGetStatus().FragNbRx);
    emit(fragmentsLostSignal, (double)FragDecoderGetStatus().FragNbLost);

    if (decodeStatus == FRAG_SESSION_FINISHED_OK) {
        rxSessionStatus = FRAG_SESSION_FINISHED_OK_APP;
        successfulDecodings++;
        emit(decodingSuccessSignal, 1.0);

        emit(overheadRatioSignal, (double)FragDecoderGetStatus().FragNbRx / sessionParams.m);

        resetRxSession();
        FragDecoderCallbacks_t decoderCallbacks = {&LoRaFragmentedApp::fragDecoderWrite, &LoRaFragmentedApp::fragDecoderRead, &LoRaFragmentedApp::fragDecoderErase};
        fragmentedApp::FragDecoderInit(sessionParams.fragNb, sessionParams.fragSize, &decoderCallbacks);

    } else if (decodeStatus == FRAG_SESSION_FINISHED_ERROR) {
        rxSessionStatus = FRAG_SESSION_FINISHED_ERROR_APP;
        failedDecodings++;
        emit(decodingSuccessSignal, 0.0);
        emit(matrixErrorSignal, (double)FragDecoderGetStatus().MatrixError);

        resetRxSession();
        FragDecoderCallbacks_t decoderCallbacks = {&LoRaFragmentedApp::fragDecoderWrite, &LoRaFragmentedApp::fragDecoderRead, &LoRaFragmentedApp::fragDecoderErase};
        fragmentedApp::FragDecoderInit(sessionParams.fragNb, sessionParams.fragSize, &decoderCallbacks);
    }

    delete[] bytesPayload;
}

FragDecoderAppStatus LoRaFragmentedApp::getDecoderStatus() const {
    fragmentedApp::FragDecoderStatus_t internalStatus = fragmentedApp::FragDecoderGetStatus();
    FragDecoderAppStatus appStatus;
    appStatus.FragNbRx = internalStatus.FragNbRx;
    appStatus.FragNbLost = internalStatus.FragNbLost;
    appStatus.MatrixError = (internalStatus.MatrixError == 1);
    return appStatus;
}

void LoRaFragmentedApp::resetRxSession() {
    rxSessionStatus = FRAG_SESSION_NOT_STARTED;
    packetsPassedToDecoder = 0;
}

} //namespace
