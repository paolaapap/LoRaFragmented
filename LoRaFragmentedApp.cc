#include "LoRaFragmentedApp.h"
#include "inet/networklayer/common/L3AddressResolver.h" // Per L3AddressResolver
#include <cmath>

namespace loraNode {

Define_Module(LoRaFragmentedApp);

//callback del decodificatore
void LoRaFragmentedApp::decoderWriteCallback(uint32_t addr, uint8_t *buffer, uint32_t size) {
    if (addr + size > decodedFileBuffer.size()) {
        decodedFileBuffer.resize(addr + size);
    }
    std::copy(buffer, buffer + size, decodedFileBuffer.begin() + addr);
}

void LoRaFragmentedApp::decoderReadCallback(uint32_t addr, uint8_t *buffer, uint32_t size) {
    if (addr + size > decodedFileBuffer.size()) {
        std::fill(buffer, buffer + size, 0);
        EV_WARN << "Attempted to read beyond decoded file buffer. Address: " << addr << ", Size: " << size << ", Buffer size: " << decodedFileBuffer.size() << "\n";
    } else {
        std::copy(decodedFileBuffer.begin() + addr, decodedFileBuffer.begin() + addr + size, buffer);
    }
}

void LoRaFragmentedApp::decoderEraseCallback() {
    std::fill(decodedFileBuffer.begin(), decodedFileBuffer.end(), 0);
    EV_INFO << "Decoded file buffer erased.\n";
}


void LoRaFragmentedApp::initialize(int stage) {
    if (stage == INITSTAGE_LOCAL) {
        numFragments = par("numFragments");
        fragmentSize = par("fragmentSize");
        numCodedFragmentsToSend = par("numCodedFragmentsToSend");
        fragmentationPackageVersion = par("fragmentationPackageVersion");
        isSender = par("isSender");
        isReceiver = par("isReceiver");
        sendInterval = par("sendInterval").doubleValue();

        packetSentSignal = registerSignal("packetSent");
        packetReceivedSignal = registerSignal("packetReceived");
        fileDecodedSignal = registerSignal("fileDecoded");

        if (isSender) {
            originalFileBuffer.resize(numFragments * fragmentSize);
            for (int i = 0; i < originalFileBuffer.size(); ++i) {
                originalFileBuffer[i] = i % 256;
            }
            EV_INFO << "SENDER: Simulating file of size " << originalFileBuffer.size() << " bytes.\n";

            generateCodedFragments();

            nextFragmentToSend = 0;
            sendTimer = new cMessage("sendFragmentTimer");

            std::string destAddrStr = par("destinationAddress").stringValue();
            if (!destAddrStr.empty()) {
                destinationAddress = L3AddressResolver().resolve(destAddrStr.c_str());
            }

            scheduleAt(simTime() + 0.1, sendTimer);
        }

        if (isReceiver) {
            decodedFileBuffer.resize(numFragments * fragmentSize);
            std::fill(decodedFileBuffer.begin(), decodedFileBuffer.end(), 0);

            //callback del decodificatore
            decoderCallbacks.FragDecoderWrite = std::bind(&LoRaFragmentedApp::decoderWriteCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
            decoderCallbacks.FragDecoderRead = std::bind(&LoRaFragmentedApp::decoderReadCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
            decoderCallbacks.FragDecoderErase = std::bind(&LoRaFragmentedApp::decoderEraseCallback, this);

            fragmentedApp::FragDecoderInit(numFragments, fragmentSize, &decoderCallbacks, fragmentationPackageVersion);
            receivedFragmentsCount = 0;
            decodedFragmentsCount = 0;
            EV_INFO << "RECEIVER: Initialized decoder for " << numFragments << " fragments of size " << fragmentSize << " bytes.\n";
        }
    }
}

void LoRaFragmentedApp::generateCodedFragments() {
    //Step1. Divido il file in frammenti non codificati
    uncodedFragments.resize(numFragments, std::vector<uint8_t>(fragmentSize));
    for (int k = 0; k < numFragments; ++k) {
        std::copy(originalFileBuffer.begin() + k * fragmentSize,
                  originalFileBuffer.begin() + (k + 1) * fragmentSize,
                  uncodedFragments[k].begin());
    }

    // Step2. Genero i frammenti codificati
    codedFragments.resize(numCodedFragmentsToSend, std::vector<uint8_t>(fragmentSize));
    for (int y = 1; y <= numCodedFragmentsToSend; ++y) {
        std::vector<uint8_t> s(fragmentSize, 0);
        std::vector<uint8_t> A = fragmentedApp::matrix_line(y, numFragments, fragmentationPackageVersion); // Riga y della matrice di parità

        for (int x = 0; x < numFragments; ++x) {
            if (A[x] == 1) {
                for (int i = 0; i < fragmentSize; ++i) {
                    s[i] = s[i] ^ uncodedFragments[x][i];
                }
            }
        }
        codedFragments[y - 1] = s;
    }
    EV_INFO << "SENDER: Generated " << codedFragments.size() << " coded fragments.\n";
}

void LoRaFragmentedApp::handleMessage(cMessage *msg) {
    if (isSender && msg == sendTimer) {
        if (nextFragmentToSend < numCodedFragmentsToSend) {
            sendFragment(codedFragments[nextFragmentToSend], nextFragmentToSend);
            nextFragmentToSend++;
            scheduleAt(simTime() + sendInterval, sendTimer);
        } else {
            EV_INFO << "SENDER: All " << numCodedFragmentsToSend << " coded fragments sent.\n";
            cancelAndDelete(sendTimer);
        }
    } else if (isReceiver && msg->isPacket()) {
        Packet *packet = dynamic_cast<Packet *>(msg);
        if (!packet) {
            EV_WARN << "Received non-packet message in handleMessage. Deleting.\n";
            delete msg;
            return;
        }

        auto appMsg = dynamicPtrCast<const LoRaWANAppMsg>(packet->peekData());
        if (!appMsg) {
             EV_WARN << "Received non-LoRaWANAppMsg. Deleting.\n";
             delete packet;
             return;
        }

        auto byteArrayChunk = dynamicPtrCast<const ByteArrayChunk>(appMsg->getPayload());
        if (!byteArrayChunk) {
            EV_WARN << "LoRaWANAppMsg does not contain ByteArrayChunk payload. Deleting.\n";
            delete packet;
            return;
        }

        const auto& payloadData = byteArrayChunk->getBytes();

        if (payloadData.size() < 2) {
            EV_WARN << "RECEIVER: Received packet with too small payload for fragment index.\n";
            delete packet;
            return;
        }

        uint16_t fragmentIndex = (payloadData[0] << 8) | payloadData[1];

        if (payloadData.size() - 2 != fragmentSize) {
             EV_WARN << "RECEIVER: Fragment " << fragmentIndex << " has incorrect size. Expected " << fragmentSize + 2
                     << ", got " << payloadData.size() << ". Dropping.\n";
             delete packet;
             return;
        }

        std::vector<uint8_t> rawFragmentData(payloadData.begin() + 2, payloadData.end());

        int32_t decoderResult = fragmentedApp::FragDecoderProcess(fragmentIndex + 1, rawFragmentData.data());
        receivedFragmentsCount++;
        emit(packetReceivedSignal, 1);

        fragmentedApp::FragDecoderStatus_t status = fragmentedApp::FragDecoderGetStatus();

        EV_INFO << "RECEIVER: Fragment " << fragmentIndex << " received. Decoded count: " << status.FragNbRx
                << ", Lost count: " << status.FragNbLost << ", MatrixError: " << (int)status.MatrixError << "\n";

        if (status.MatrixError != 0) {
            EV_ERROR << "RECEIVER: Fragmentation decoding error: Matrix error!\n";

        } else if (decoderResult == FRAG_SESSION_FINISHED) {
            EV_INFO << "RECEIVER: File successfully decoded! Total size: " << decodedFileBuffer.size() << " bytes.\n";
            bool integrityOk = true;
            for (int i = 0; i < originalFileBuffer.size(); ++i) {
                if (decodedFileBuffer[i] != (uint8_t)(i % 256)) {
                    integrityOk = false;
                    EV_ERROR << "RECEIVER: Data integrity check FAILED at byte " << i << ".\n";
                    break;
                }
            }
            if (integrityOk) {
                EV_INFO << "RECEIVER: Data integrity check PASSED.\n";
                emit(fileDecodedSignal, 1);
            }

        }

        delete packet;
    } else {

        delete msg;
    }
}

void LoRaFragmentedApp::sendFragment(const std::vector<uint8_t>& fragmentData, int fragmentIndex) {
    std::vector<uint8_t> payloadData;

    payloadData.push_back((fragmentIndex >> 8) & 0xFF);
    payloadData.push_back(fragmentIndex & 0xFF);

    payloadData.insert(payloadData.end(), fragmentData.begin(), fragmentData.end());

    auto appPayload = makeWith=make<ByteArrayChunk>();
    appPayload->setBytes(payloadData);

    LoRaWANAppMsg *lorawanAppMsg = new LoRaWANAppMsg("LoRaFragment");
    lorawanAppMsg->setPayload(appPayload);

    Packet *packet = new Packet("LoRaFragmentPacket");
    packet->addTag<CreationTimeTag>()->setCreationTime(simTime());
    packet->addTag<PacketIdTag>()->setPacketId(fragmentIndex);
    packet->insertAtBack(lorawanAppMsg); // Inserisci il messaggio FLORA

    send(packet, "out");
    emit(packetSentSignal, 1);
}

void LoRaFragmentedApp::handleUdpMessage(Packet* msg) {

    delete msg;
}

void LoRaFragmentedApp::refreshDisplay() const {
    if (isSender) {
        char buf[64];
        sprintf(buf, "Sent: %d/%d", nextFragmentToSend, numCodedFragmentsToSend);
        get;
    }
    if (isReceiver) {
        char buf[64];
        sprintf(buf, "Rcvd: %d, Lost: %d, Decoded: %s",
                receivedFragmentsCount,
                fragmentedApp::FragDecoderGetStatus().FragNbLost,
                (fragmentedApp::FragDecoderGetStatus().FragNbLost == 0 && receivedFragmentsCount >= numFragments) ? "YES" : "NO");
        get;
    }
}

void LoRaFragmentedApp::finish() {
    if (isSender) {
        cancelAndDelete(sendTimer);
    }
}

}
