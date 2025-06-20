
package flora.LoraNode;

import inet.mobility.static.StationaryMobility;
import inet.networklayer.common.InterfaceTable;
import inet.applications.contract.IApp;
import flora.LoRa.LoRaNic;
import LoraNode.LoRaFragmentedApp;


module LoRaNode
{
    parameters:
        int numApps = default(1);
        string deploymentType = default("");
        double maxGatewayDistance = default(320.0);
        double gatewayX = default(320.0);
        double gatewayY = default(320.0);
        string appType = default("loraNode.LoRaFragmentedApp");
        @networkNode();
        *.interfaceTableModule = default(absPath(".interfaceTable"));
        @display("bgb=297,313;i=device/accesspoint;is=vs");
    submodules:
        interfaceTable: InterfaceTable {
            @display("p=30,26");
        }
        mobility: StationaryMobility {
            @display("p=24,88");
        }
        LoRaNic: LoRaNic {
            @display("p=137,239");
        }
        app[numApps]: <> like IApp {
            parameters:
                @display("p=375,76,row,150");
        }
    connections allowunconnected:
        for i=0..numApps-1 {		
        	app[i].socketOut --> LoRaNic.upperLayerIn;
        	LoRaNic.upperLayerOut --> app[i].socketIn;
        }
}



