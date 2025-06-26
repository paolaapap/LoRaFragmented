package flora.LoraNode;

import inet.applications.contract.IApp;

simple LoRaFragmentedApp like IApp
{
    parameters:
        int M;
        int R;
        int fragmentSize;
        int dataSize;
        int sessionIndex;
        @display("i=block/app"); 

    gates:
        input in;
        output out;
}