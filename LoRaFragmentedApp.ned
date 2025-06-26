package flora.LoraNode;

import inet.applications.contract.IApp;

simple LoRaFragmentedApp like IApp
{
    parameters:
        int M @prompt("Number of original data fragments (M)");
        int R @prompt("Number of redundancy fragments (R)");
        int fragmentSize @prompt("Size of each fragment in bytes");
        int dataSize @prompt("Total size of original data in bytes (M * fragmentSize)");
        int sessionIndex @prompt("Fragmentation session index");
        
        @display("i=block/app"); 

    gates:
        input in;
        output out;
}