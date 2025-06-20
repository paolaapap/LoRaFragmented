
package flora.LoraNode;

import inet.common.packet.Packet; 
import inet.applications.contract.IApp; 

module LoRaFragmentedApp like IApp
{
    parameters:
        int numFragments = default(32); //w nel codice MATLAB
        int fragmentSize = default(50); 
        int numCodedFragmentsToSend = default(64); //2*w nel codice MATLAB
        int fragmentationPackageVersion = default(2); //1 per V1.0.0, 2 per V2.0.0
        bool isSender = default(false); //indica se questo modulo è un mittente
        bool isReceiver = default(false); //indica se questo modulo è un ricevitore
        double sendInterval = default(1s); //intervallo tra gli invii (solo per mittente)
        string destinationAddress = default(""); //indirizzo di destinazione (se unicast)

    gates:
        output out; //Verso il livello MAC (per inviare)
        input in;   //Dal livello MAC (per ricevere)
}