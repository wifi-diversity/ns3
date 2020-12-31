/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2017 Jose Saldana, University of Zaragoza (jsaldana@unizar.es)
 *
 * This work has been partially financed by the EU H2020 Wi-5 project (G.A. no: 644262).
 *
 * If you use this code, please cite the next research article:
 *
 * Jose Saldana, Jose Ruiz-Mas, Jose Almodovar, "Frame Aggregation in Central Controlled
 * 802.11 WLANs: the Latency vs. Throughput Trade-off," IEEE Communications Letters,
 * vol.21, no. 2, pp. 2500-2530, Nov. 2017. ISSN 1089-7798.
 * http://dx.doi.org/10.1109/LCOMM.2017.2741940
 *
 * http://ieeexplore.ieee.org/document/8013762/
 * Author's self-archive version: http://diec.unizar.es/~jsaldana/personal/amsterdam_2017_in_proc.pdf
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Some parts are inspired on https://www.nsnam.org/doxygen/wifi-aggregation_8cc.html, by Sebastien Deronne
 * Other parts are inspired on https://www.nsnam.org/doxygen/wifi-wired-bridging_8cc_source.html
 * The flow monitor part is inspired on https://www.nsnam.org/doxygen/wifi-hidden-terminal_8cc_source.html
 * The association record is inspired on https://github.com/MOSAIC-UA/802.11ah-ns3/blob/master/ns-3/scratch/s1g-mac-test.cc
 * The hub is inspired on https://www.nsnam.org/doxygen/csma-bridge_8cc_source.html
 *
 * v259
 * Developed and tested for ns-3.30.1, https://www.nsnam.org/releases/ns-3-30/
 */


// PENDING
/*
1) make the 'unset assoc' change the channel to that of the closest AP

Two possibilities:
- WORKING: the STA deassociates by itself. I have to use Yanswifi.
- Not working yet: That it needs the new AP to send beacons to it. In this second option,
  the command 'addoperationalchannel' should be used on each STA. 
  see https://www.nsnam.org/doxygen/classns3_1_1_spectrum_wifi_phy.html#a948c6d197accf2028529a2842ec68816

  the function AddOperationalChannel has been removed in ns-3.30.1


2) To separate this file into a number of them, using .h files.
*/


//
// The network scenario includes
// - a number of STA wifi nodes, with different mobility patterns
// - a number of AP wifi nodes. They are distributed in rows and columns in an area. An AP may have two WiFi cards
// - a number of servers: each of them communicates with one STA (it is the origin or the destination of the packets)
//
// On each AP node, there is
// - a csma device
// - a wifi device (or two)
// - a bridge that binds both interfaces the whole thing into one network
// 
// IP addresses:
// - the STAs have addresses 10.0.0.0 (mask 255.255.0.0)
// - the servers behind the router (only in topology 2) have addresses 10.1.0.0 (mask 255.255.0.0) 
//
// There are three topologies:
//
// topology = 0
//
//              (*)
//            +--|-+                      10.0.0.0
//    (*)     |STA1|           csma     +-----------+  csma  +--------+ 
//  +--|-+    +----+    +---------------|   hub     |--------| single | All the server applications
//  |STA0|              |               +-----------+        | server | are in this node
//  +----+              |                         |          +--------+    
//                      |                     csma|                        
//        +-------------|--+         +------------|--+                 
//        | +----+  +----+ |         | +----+ +----+ |                 
//   ((*))--|WIFI|  |CSMA| |    ((*))--|WIFI| |CSMA| |                 
//        | +----+  +----+ |         | +----+ +----+ |                 
//        |   |       |    |         |   |      |    |                 
//        |  +----------+  |         |  +---------+  |                 
//        |  |  BRIDGE  |  |         |  |  BRIDGE |  |                  
//        |  +----------+  |         |  +---------+  |
//        +----------------+         +---------------+                  
//               AP 0                       AP 1                          
//
//
//
// topology = 1
//
//              (*)
//            +--|-+                      10.0.0.0                       
//    (*)     |STA1|           csma     +-----------+  csma         
//  +--|-+    +----+    +---------------|   hub     |----------------------------------------+
//  |STA0|              |               +-----------+----------------------+                 |
//  +----+              |                         |                        |                 |
//                      |                     csma|                        |                 |
//        +-------------|--+         +------------|--+                 +--------+      +--------+
//        | +----+  +----+ |         | +----+ +----+ |                 | +----+ |      | +----+ |
//   ((*))--|WIFI|  |CSMA| |    ((*))--|WIFI| |CSMA| |                 | |CSMA| |      | |CSMA| |  ...
//        | +----+  +----+ |         | +----+ +----+ |                 | +----+ |      | +----+ |
//        |   |       |    |         |   |      |    |                 |        |      |        |
//        |  +----------+  |         |  +---------+  |                 +--------+      +--------+
//        |  |  BRIDGE  |  |         |  |  BRIDGE |  |                  server 0        server 1
//        |  +----------+  |         |  +---------+  |
//        +----------------+         +---------------+                  talks with      talks with
//               AP 0                       AP 1                          STA 0           STA 1
//
//
//
// topology = 2 (DEFAULT)
//                  
//              (*)                                 10.0.0.0 |        | 10.1.0.0
//            +--|-+                                      <--+        +-->
//            |STA1|
//    (*)     +----+           csma     +-----------+  csma  +--------+       point to point
//  +--|-+              +---------------|   hub     |--------| router |----------------------+
//  |STA0|              |             0 +-----------+ 2      |        |----+                 |
//  +----+              |                         |1         +--------+    |                 |
//                      |                     csma|                        |                 |
//        +-------------|--+         +------------|--+                 +--------+      +--------+
//        | +----+  +----+ |         | +----+ +----+ |                 | +----+ |      | +----+ |
//   ((*))--|WIFI|  |CSMA| |    ((*))--|WIFI| |CSMA| |                 | |CSMA| |      | |CSMA| |  ...
//        | +----+  +----+ |         | +----+ +----+ |                 | +----+ |      | +----+ |
//        |   |       |    |         |   |      |    |                 |        |      |        |
//        |  +----------+  |         |  +---------+  |                 +--------+      +--------+
//        |  |  BRIDGE  |  |         |  |  BRIDGE |  |                  server 0        server 1
//        |  +----------+  |         |  +---------+  |
//        +----------------+         +---------------+                  talks with      talks with
//               AP 0                       AP 1                          STA 0           STA 1
//
//

// When the default aggregation parameters are enabled, the
// maximum A-MPDU size is the one defined by the standard, and the throughput is maximal.
// When aggregation is limited, the throughput is lower
//
// Packets in this simulation can be marked with a QosTag so they
// will be considered belonging to  different queues.
// By default, all the packets belong to the BestEffort Access Class (AC_BE).
//
// The user can select many parameters when calling the program. Examples:
// ns-3.27$ ./waf --run "scratch/wifi-central-controlled-aggregation --PrintHelp"
// ns-3.27$ ./waf --run "scratch/wifi-central-controlled-aggregation --number_of_APs=1 --nodeMobility=1 --nodeSpeed=0.1 --simulationTime=10 --distance_between_APs=20"
//
// if you want different results in different runs, use a different seed each time you call the program
// (see https://www.nsnam.org/docs/manual/html/random-variables.html). One example:
//
// ns-3.26$ NS_GLOBAL_VALUE="RngRun=3" ./waf --run "scratch/wifi-central-controlled-aggregation --simulationTime=2 --nodeMobility=3 --verboseLevel=2 --number_of_APs=10 --number_of_APs_per_row=1"
// you can call it with different values of RngRun to obtain different realizations
//
// for being able to see the logs, do ns-3.26$ export NS_LOG=UdpEchoClientApplication=level_all
// or /ns-3-dev$ export 'NS_LOG=ArpCache=level_all'
// or /ns-3-dev$ export 'NS_LOG=ArpCache=level_error' for showing only errors
// see https://www.nsnam.org/docs/release/3.7/tutorial/tutorial_21.html
// see https://www.nsnam.org/docs/tutorial/html/tweaking.html#usinglogging

// Output files
//  You can establish a 'name' and a 'surname' for the output files, using the parameters:
//    --outputFileName=name --outputFileSurname=seed-1
//
//  You can run a battery of tests with the same name and different surname, and you
//  will finally obtain a single file name_average.txt with all the averaged results.
//
//  Example: if you use --outputFileName=name --outputFileSurname=seed-1
//  you will obtain the next output files:
//
//    - name_average.txt                            it integrates all the tests with the same name, even if they have a different surname
//                                                  the file is not deleted, so each test with the same name is added at the bottom
//    - name_seed-1_flows.txt                       information of all the flows of this run
//    - name_seed-1_flow_1_delay_histogram.txt      delay histogram of flow #1
//    - name_seed-1_flow_1_jitter_histogram.txt
//    - name_seed-1_flow_1_packetsize_histogram.txt
//    - name_seed-1_KPIs.txt                        text file reporting periodically the KPIs (generated if aggregationDynamicAlgorithm==1)
//    - name_seed-1_positions.txt                   text file reporting periodically the positions of the STAs
//    - name_seed-1_AMPDUvalues.txt                 text file reporting periodically the AMPDU values (generated if aggregationDynamicAlgorithm==1)
//    - name_seed-1_flowmonitor.xml
//    - name_seed-1_AP-0.2.pcap                     pcap file of the device 2 of AP #0
//    - name_seed-1_server-2-1.pcap                 pcap file of the device 1 of server #2
//    - name_seed-1_STA-8-1.pcap                    pcap file of the device 1 of STA #8
//    - name_seed-1_hub.pcap                        pcap file of the hub connecting all the APs


#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/bridge-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/nstime.h"
#include "ns3/spectrum-module.h"    // For the spectrum channel
#include <ns3/friis-spectrum-propagation-loss.h>
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/random-variable-stream.h"
#include "ns3/ht-configuration.h"
#include "ns3/he-configuration.h"
#include <sstream>
#include <iomanip>

//#include "ns3/arp-cache.h"  // If you want to do things with the ARPs
//#include "ns3/arp-header.h"

using namespace ns3;

#define VERBOSE_FOR_DEBUG 0

#define NUM_CHANNELS_24GHZ 3
#define NUM_CHANNELS_5GHZ_20MHZ 34
#define NUM_CHANNELS_5GHZ_40MHZ 12
#define NUM_CHANNELS_5GHZ_80MHZ 6
#define NUM_CHANNELS_5GHZ_160MHZ 2

// Maximum AMPDU size of 802.11n
#define MAXSIZE80211n 65535

// Maximum AMPDU size of 802.11ac
//https://groups.google.com/forum/#!topic/ns-3-users/T_21O5mGlgM
//802.11ac allows the maximum A-MPDU length to range from 8 KB to 1 MB.
// The maximum transmission length is defined by time, and is a little less than 5.5 microseconds. At the highest data rates for 802.11ac, 
//an aggregate frame can hold almost four and a half megabytes of data. Rather than represent such a large number of bytes in the PLCP header,
//which is transmitted at the lowest possible data rate, 802.11ac shifts the length indication to the MPDU delimiters that are transmitted 
//as part of the high-data-rate payload
// http://www.rfwireless-world.com/Tutorials/802-11ac-MAC-layer.html
// Max. length of A-MPDU = 2^13+Exp -1 bytes 
// Exp can range from 0 to 7, this yields A-MPDU to be of length:
// - 2^13 - 1 = 8191 (8KB)
// - 2^20 - 1 = 1,048,575 (about 1MB)
#define MAXSIZE80211ac 65535  // FIXME. for 802.11ac max ampdu size should be 4692480
                              // You can set it in src/wifi/model/regular-wifi-mac.cc
                              // https://www.nsnam.org/doxygen/classns3_1_1_sta_wifi_mac.html

#define STEPADJUSTAMPDUDEFAULT 1000  // Default value. We will update AMPDU up and down using this step size

#define AGGRESSIVENESS 10     // Factor to decrease AMPDU down

#define INITIALPORT_VOIP_UPLOAD     10000      // The value of the port for the first VoIP upload communication
#define INITIALPORT_VOIP_DOWNLOAD   20000      // The value of the port for the first VoIP download communication
#define INITIALPORT_TCP_UPLOAD      55000      // The value of the port for the first TCP upload communication
// I don't use 40000 because TCP ACKs use ports starting from 49153
#define INITIALPORT_TCP_DOWNLOAD    30000      // The value of the port for the first TCP download communication
#define INITIALPORT_VIDEO_DOWNLOAD  60000      // The value of the port for the first video download communication

#define MTU 1500    // The value of the MTU of the packets

#define INITIALTIMEINTERVAL 1.0 // time before the applications start (seconds). The same amount of time is added at the end

#define HANDOFFMETHOD 0     // 1 - ns3 is in charge of the channel switch of the STA for performing handoffs 
                            // 0 - the handoff method is implemented in this script

// Define a log component
NS_LOG_COMPONENT_DEFINE ("SimpleMpduAggregation");

std::string getWirelessBandOfChannel(uint8_t channel) {
  // see https://en.wikipedia.org/wiki/List_of_WLAN_channels#2.4_GHz_(802.11b/g/n/ax)
  if (channel <= 14 ) {
    return "2.4 GHz";
  }
  else {
    return "5 GHz";
  }
}

std::string getWirelessBandOfStandard(enum ns3::WifiPhyStandard standard) {
  // see https://en.wikipedia.org/wiki/List_of_WLAN_channels#2.4_GHz_(802.11b/g/n/ax)
  if (standard == WIFI_PHY_STANDARD_80211a ) {
    return "5 GHz"; // 5GHz
  }
  else if (standard == WIFI_PHY_STANDARD_80211b ) {
    return "2.4 GHz";
  }
  else if (standard == WIFI_PHY_STANDARD_80211g ) {
    return "2.4 GHz";
  }
  else if (standard == WIFI_PHY_STANDARD_80211n_2_4GHZ  ) {
    return "2.4 GHz";
  }
  else if (standard == WIFI_PHY_STANDARD_80211n_5GHZ  ) {
    return "5 GHz";
  }
  else if (standard == WIFI_PHY_STANDARD_80211ac ) {
    return "5 GHz";
  }
  // if I am here, it means I have asked for an unsupported standard (unsupported by this program so far)
  NS_ASSERT(false);
}

ns3::WifiPhyStandard convertVersionToStandard (std::string my_version80211) {
  if (my_version80211 == "11n5") {
    return WIFI_PHY_STANDARD_80211n_5GHZ;
  }
  else if (my_version80211 == "11ac") {
    return WIFI_PHY_STANDARD_80211ac;
  }
  else if (my_version80211 == "11n2.4") {
    return WIFI_PHY_STANDARD_80211n_2_4GHZ;
  }
  else if (my_version80211 == "11g") {
    return WIFI_PHY_STANDARD_80211g;
  }
  else if (my_version80211 == "11a") {
    return WIFI_PHY_STANDARD_80211a;
  }
  // if I am here, it means I have asked for an unsupported version (unsupported by this program so far)
  NS_ASSERT(false);  
}

// returns
// 0  both bands are supported
// 2  2.4GHz band is supported
// 5  5GHz band is supported
std::string bandsSupportedByTheAPs(int numberAPsSamePlace, std::string version80211primary, std::string version80211secondary) {
  if (numberAPsSamePlace == 1) {
    // single WiFi card
    return getWirelessBandOfStandard(convertVersionToStandard(version80211primary));
  }
  else if (numberAPsSamePlace == 2) {
    // two WiFi cards
    if (getWirelessBandOfStandard(convertVersionToStandard(version80211primary)) == "5 GHz" ) {
      if (getWirelessBandOfStandard(convertVersionToStandard(version80211secondary)) == "2.4 GHz" ) {
        return "both"; // both bands are supported by the AP
      }
      else if (getWirelessBandOfStandard(convertVersionToStandard(version80211secondary)) == "5 GHz" ) {
        std::cout << "ERROR: both interfaces of the AP are in the same band - "
                  << "FINISHING SIMULATION"
                  << std::endl;
        NS_ASSERT(false); // both interfaces of the AP are in the same band
        return "";
      }
    }
    else if (getWirelessBandOfStandard(convertVersionToStandard(version80211primary)) == "2.4 GHz" ) {
      if (getWirelessBandOfStandard(convertVersionToStandard(version80211secondary)) == "5 GHz" ) {
        return "both"; // both bands are supported by the AP
      }
      else if (getWirelessBandOfStandard(convertVersionToStandard(version80211secondary)) == "2.4 GHz" ) {
        std::cout << "ERROR: both interfaces of the AP are in the same band - "
                  << "FINISHING SIMULATION"
                  << std::endl;
        NS_ASSERT(false); // both interfaces of the AP are in the same band
        return "";
      }
    }
  }
  else {
    // only 2 wireless cards are supported so far
    NS_ASSERT(false);
    return "";
  }
  return "";
}

// returns
// 0  both bands are supported
// 2  2.4GHz band is supported
// 5  5GHz band is supported
std::string bandsSupportedByTheSTAs(int numberSTAsSamePlace, std::string version80211primary, std::string version80211secondary) {
  if (numberSTAsSamePlace == 1) {
    // single WiFi card
    return getWirelessBandOfStandard(convertVersionToStandard(version80211primary));
  }
  else if (numberSTAsSamePlace == 2) {
    // two WiFi cards
    if (getWirelessBandOfStandard(convertVersionToStandard(version80211primary)) == "5 GHz" ) {
      if (getWirelessBandOfStandard(convertVersionToStandard(version80211secondary)) == "2.4 GHz" ) {
        return "both"; // both bands are supported by the STA
      }
      else if (getWirelessBandOfStandard(convertVersionToStandard(version80211secondary)) == "5 GHz" ) {
        std::cout << "ERROR: both interfaces of the STA are in the same band - "
                  << "FINISHING SIMULATION"
                  << std::endl;
        NS_ASSERT(false); // both interfaces of the STA are in the same band
        return "";
      }
    }
    else if (getWirelessBandOfStandard(convertVersionToStandard(version80211primary)) == "2.4 GHz" ) {
      if (getWirelessBandOfStandard(convertVersionToStandard(version80211secondary)) == "5 GHz" ) {
        return "both"; // both bands are supported by the STA
      }
      else if (getWirelessBandOfStandard(convertVersionToStandard(version80211secondary)) == "2.4 GHz" ) {
        std::cout << "ERROR: both interfaces of the STA are in the same band - "
                  << "FINISHING SIMULATION"
                  << std::endl;
        NS_ASSERT(false); // both interfaces of the STA are in the same band
        return "";
      }
    }
  }
  else {
    // only 2 bands are supported
    NS_ASSERT(false);
    return "";
  }
  return "";
}

/*  this works, but it is not needed
static void SetPhySleepMode (Ptr <WifiPhy> myPhy) {
  myPhy->SetSleepMode();
  //if (myverbose > 1)
    std::cout << Simulator::Now().GetSeconds()
              << "\t[SetPhySleepMode]\tSTA set to SLEEP mode" << std::endl;  
}
*/

// Enable the network device of a set of STAs (included in the NetDeviceContainer passed as a parameter)
uint8_t ChannelNetworkDevice ( NetDeviceContainer deviceslink, uint32_t mywifiModel, uint32_t myverbose ) {

  uint8_t channel;

  for (uint32_t i = 0; i < deviceslink.GetN (); i++) {

    if (myverbose > 1)
      std::cout << Simulator::Now().GetSeconds()
                << "\t[ChannelNetworkDevice]\tGetting channel of network device on STA with MAC " << deviceslink.Get (i)->GetAddress () 
                << std::endl;

    Ptr<WifiNetDevice> wifidevice = DynamicCast<WifiNetDevice> (deviceslink.Get(i));

    if (wifidevice == 0) {
      std::cout << "[ChannelNetworkDevice]\tWARNING: wifidevice IS NULL" << '\n';
      NS_ASSERT(false);
    }

    if (mywifiModel == 0) {
      // YansWifiPhy
      Ptr<WifiPhy> phy0 = wifidevice->GetPhy();

      channel = phy0->GetChannelNumber();
    }

    else {
      // spectrumWiFiPhy
      Ptr<SpectrumWifiPhy> phy0 = wifidevice->GetPhy()->GetObject<SpectrumWifiPhy>(); 

      channel = phy0->GetChannelNumber();
    }

    if (myverbose > 1)
      std::cout << Simulator::Now().GetSeconds()
                << "\t[ChannelNetworkDevice]\tSTA with MAC " << deviceslink.Get (i)->GetAddress ()
                << " is currently in channel " << (uint16_t)channel
                << std::endl;
  }

  return channel;
}

// Enable the network device of a set of STAs (included in the NetDeviceContainer passed as a parameter)
void EnableNetworkDevice ( NetDeviceContainer deviceslink, uint32_t mywifiModel, uint32_t myverbose ) {

  uint8_t channel;

  for (uint32_t i = 0; i < deviceslink.GetN (); i++) {

    if (myverbose > 1)
      std::cout << Simulator::Now().GetSeconds()
                << "\t[EnableNetworkDevice]\tEnabling network device on STA with MAC " << deviceslink.Get (i)->GetAddress () 
                << std::endl;

    Ptr<WifiNetDevice> wifidevice = DynamicCast<WifiNetDevice> (deviceslink.Get(i));

    if (wifidevice == 0) {
      std::cout << "[EnableNetworkDevice]\tWARNING: wifidevice IS NULL" << '\n';
      NS_ASSERT(false);
    }

    if (mywifiModel == 0) {
      // YansWifiPhy
      Ptr<WifiPhy> phy0 = wifidevice->GetPhy();

      phy0->ResumeFromOff();

      channel = phy0->GetChannelNumber();
    }

    else {
      // spectrumWiFiPhy
      Ptr<SpectrumWifiPhy> phy0 = wifidevice->GetPhy()->GetObject<SpectrumWifiPhy>(); 

      /*
      // schedule the switch OFF and ON of this phy - it works, but it is not needed
      Simulator::Schedule (Seconds(0.9), &SetPhySleepMode, phy0);
      Simulator::Schedule (Seconds(1.0), &SetPhyOff, phy0);
      Simulator::Schedule (Seconds(2.0), &SetPhyOn, phy0);
      //
      */
      phy0->ResumeFromOff();

      channel = phy0->GetChannelNumber();
    }

    if (myverbose > 1)
      std::cout << Simulator::Now().GetSeconds()
                << "\t[EnableNetworkDevice]\tSTA with MAC " << deviceslink.Get (i)->GetAddress () 
                << "  set to ON mode" 
                << ". Currently in channel " << (uint16_t)channel
                << std::endl;
  }
}

// Disable the network device of a set of STAs (included in the NetDeviceContainer passed as a parameter)
void DisableNetworkDevice ( NetDeviceContainer deviceslink, uint32_t mywifiModel, uint32_t myverbose ) {

  for (uint32_t i = 0; i < deviceslink.GetN (); i++) {

    Ptr<WifiNetDevice> wifidevice = DynamicCast<WifiNetDevice> (deviceslink.Get(i));

    if (wifidevice == 0) {
      std::cout << "[DisableNetworkDevice]\tWARNING: wifidevice IS NULL" << '\n';
      NS_ASSERT(false);
    }

    if (mywifiModel == 0) {
      // YansWifiPhy
      Ptr<WifiPhy> phy0 = wifidevice->GetPhy();

      if (myverbose > 1)
        std::cout << Simulator::Now().GetSeconds()
                  << "\t[DisableNetworkDevice]\tDisabling network device on STA with MAC " << deviceslink.Get (i)->GetAddress ()
                  << ". Currently in channel " << (uint16_t)phy0->GetChannelNumber ()
                  << std::endl;

      // see https://www.nsnam.org/doxygen/classns3_1_1_wifi_phy.html#ac365794e06cc92ae1262cbe72b72213d
      phy0->SetOffMode();
    }

    else {
      // spectrumWiFiPhy
      Ptr<SpectrumWifiPhy> phy0 = wifidevice->GetPhy()->GetObject<SpectrumWifiPhy>();

      if (myverbose > 1)
        std::cout << Simulator::Now().GetSeconds()
                  << "\t[DisableNetworkDevice]\tDisabling network device on STA with MAC " << deviceslink.Get (i)->GetAddress ()
                  << ". Currently in channel " << (uint16_t)phy0->GetChannelNumber ()
                  << std::endl;

      if (phy0->IsStateCcaBusy()) {
        std::cout << Simulator::Now().GetSeconds()
                  << "\t[DisableNetworkDevice]\tNetwork device on STA with MAC " << deviceslink.Get (i)->GetAddress ()
                  << " is in state CCA busy "
                  << std::endl;    

        phy0->ResetCca(false, 0, 0);
      }

      phy0->SetOffMode();

    }

    if (myverbose > 1)
      std::cout << Simulator::Now().GetSeconds()
                << "\t[DisableNetworkDevice]\tSTA with MAC " << deviceslink.Get (i)->GetAddress () 
                << ". Network device set to OFF mode  "
                << std::endl;  
  }
}

// Enable the network device of a set of STAs (included in the NetDeviceContainer passed as a parameter)
void ResumeNetworkDevice ( NetDeviceContainer deviceslink, uint32_t mywifiModel, uint32_t myverbose ) {

  for (uint32_t i = 0; i < deviceslink.GetN (); i++) {

    if (myverbose > 1)
      std::cout << Simulator::Now().GetSeconds()
                << "\t[ResumeNetworkDevice]\tResuming network device on STA with MAC " << deviceslink.Get (i)->GetAddress () 
                << std::endl;

    Ptr<WifiNetDevice> wifidevice = DynamicCast<WifiNetDevice> (deviceslink.Get(i));

    if (wifidevice == 0) {
      std::cout << "[ResumeNetworkDevice]\tWARNING: wifidevice IS NULL" << '\n';
      NS_ASSERT(false);
    }

    uint8_t channel;

    if (mywifiModel == 0) {
      // YansWifiPhy
      Ptr<WifiPhy> phy0 = wifidevice->GetPhy();

      phy0->ResumeFromSleep();

      channel = phy0->GetChannelNumber();
    }

    else {
      // spectrumWiFiPhy
      Ptr<SpectrumWifiPhy> phy0 = wifidevice->GetPhy()->GetObject<SpectrumWifiPhy>(); 

      /*
      // schedule the switch OFF and ON of this phy - it works, but it is not needed
      Simulator::Schedule (Seconds(0.9), &SetPhySleepMode, phy0);
      Simulator::Schedule (Seconds(1.0), &SetPhyOff, phy0);
      Simulator::Schedule (Seconds(2.0), &SetPhyOn, phy0);
      //
      */
      phy0->ResumeFromSleep();

      channel = phy0->GetChannelNumber();
    }

    if (myverbose > 1)
      std::cout << Simulator::Now().GetSeconds()
                << "\t[ResumeNetworkDevice]\tSTA with MAC " << deviceslink.Get (i)->GetAddress () 
                << "  resumed from sleep" 
                << ". Currently in channel " << (uint16_t)channel
                << std::endl;
  }
}

// Disable the network device of a set of STAs (included in the NetDeviceContainer passed as a parameter)
void SleepNetworkDevice ( NetDeviceContainer deviceslink, uint32_t mywifiModel, uint32_t myverbose ) {

  for (uint32_t i = 0; i < deviceslink.GetN (); i++) {

    Ptr<WifiNetDevice> wifidevice = DynamicCast<WifiNetDevice> (deviceslink.Get(i));

    if (wifidevice == 0) {
      std::cout << "[SleepNetworkDevice]\tWARNING: wifidevice IS NULL" << '\n';
      NS_ASSERT(false);
    }

    if (mywifiModel == 0) {
      // YansWifiPhy
      Ptr<WifiPhy> phy0 = wifidevice->GetPhy();

      if (myverbose > 1)
        std::cout << Simulator::Now().GetSeconds()
                  << "\t[SleepNetworkDevice]\tNetwork device on STA with MAC " << deviceslink.Get (i)->GetAddress ()
                  << "set to sleep mode. Currently in channel " << (uint16_t)phy0->GetChannelNumber ()
                  << std::endl;

      // see https://www.nsnam.org/doxygen/classns3_1_1_wifi_phy.html#ac365794e06cc92ae1262cbe72b72213d
      phy0->SetSleepMode();
    }

    else {
      // spectrumWiFiPhy
      Ptr<SpectrumWifiPhy> phy0 = wifidevice->GetPhy()->GetObject<SpectrumWifiPhy>();

      if (myverbose > 1)
        std::cout << Simulator::Now().GetSeconds()
                  << "\t[SleepNetworkDevice]\tNetwork device on STA with MAC " << deviceslink.Get (i)->GetAddress ()
                  << "set to sleep mode. Currently in channel " << (uint16_t)phy0->GetChannelNumber ()
                  << std::endl;

      if (phy0->IsStateCcaBusy()) {
        std::cout << Simulator::Now().GetSeconds()
                  << "\t[DisableNetworkDevice]\tNetwork device on STA with MAC " << deviceslink.Get (i)->GetAddress ()
                  << " is in state CCA busy "
                  << std::endl;    

        phy0->ResetCca(false, 0, 0);
      }
      phy0->SetSleepMode();
    }

    if (myverbose > 1)
      std::cout << Simulator::Now().GetSeconds()
                << "\t[SleepNetworkDevice]\tSTA with MAC " << deviceslink.Get (i)->GetAddress () 
                << ". Network device set to SLEEP mode  "
                << std::endl;  
  }
}

// Return 'true' if all the devices in 'deviceslink' are disabled
bool AreAllTheseDevicesDisabled (NetDeviceContainer deviceslink, uint32_t mywifiModel, uint32_t myverbose) {

  // it will be set to 'false' if any of the devices in 'deviceslink' is enabled
  bool areDisabled = true;

  for (uint32_t i = 0; i < deviceslink.GetN (); i++) {

    if (myverbose > 1)
      std::cout << Simulator::Now().GetSeconds()
                << "\t[AreTheseDevicesDisabled]\tSTA with MAC " << deviceslink.Get (i)->GetAddress ();

    Ptr<WifiNetDevice> wifidevice = DynamicCast<WifiNetDevice> (deviceslink.Get(i));

    if (wifidevice == 0) {
      std::cout << "\tWARNING: wifidevice IS NULL" << '\n';
      NS_ASSERT(false);
    }

    // YansWifiPhy
    if (mywifiModel == 0) {
      Ptr<WifiPhy> phy0 = wifidevice->GetPhy();
      if (phy0->IsStateOff() == true) {
        if (myverbose > 1)
          std::cout << " is disabled"
                    << std::endl;               
      }
      else {
        areDisabled = false;
        if (myverbose > 1)
          std::cout << " is enabled"
                    << std::endl; 
      }
    }

    // spectrumWiFiPhy
    else {
      Ptr<SpectrumWifiPhy> phy0 = wifidevice->GetPhy()->GetObject<SpectrumWifiPhy>(); 
      if (phy0->IsStateOff() == true) {
        if (myverbose > 1)
          std::cout << " is disabled"
                    << std::endl;               
      }
      else {
        areDisabled = false;
        if (myverbose > 1)
          std::cout << " is enabled"
                    << std::endl; 
      }
    }
  }

  return areDisabled;
}

// Return the channel of the network device of a STA
uint8_t GetChannelOfaDevice ( NetDeviceContainer deviceslink, uint32_t mywifiModel, uint32_t myverbose ) {

  uint8_t channel;

  NS_ASSERT(deviceslink.GetN() == 1);

  for (uint32_t i = 0; i < deviceslink.GetN (); i++) {

    if (myverbose > 1)
      std::cout << Simulator::Now().GetSeconds()
                << "\t[GetChannelOfaDevice]\tGetting channel of the STA with MAC " << deviceslink.Get (i)->GetAddress () 
                << std::endl;

    Ptr<WifiNetDevice> wifidevice = DynamicCast<WifiNetDevice> (deviceslink.Get(i));

    if (wifidevice == 0) {
      std::cout << "[GetChannelOfaDevice]\tWARNING: wifidevice IS NULL" << '\n';
      NS_ASSERT(false);
    }

    // YansWifiPhy
    if (mywifiModel == 0) {
      Ptr<WifiPhy> phy0 = wifidevice->GetPhy();

      // check if the STA is switching its channel
      if (phy0->IsStateSwitching()) {
        if (myverbose > 1)
          std::cout << Simulator::Now().GetSeconds()
                    << "\t[GetChannelOfaDevice]\tSTA with MAC " << deviceslink.Get (i)->GetAddress () 
                    << "  is switching its channel"
                    << std::endl; 
      }
      else {
        // as the STA is NOT switching its channel automatically, I do it
        channel = phy0->GetChannelNumber ();

        if (myverbose > 1)
          std::cout << Simulator::Now().GetSeconds()
                    << "\t[GetChannelOfaDevice]\tChannel on STA with MAC " << deviceslink.Get (i)->GetAddress () 
                    << "  is:  " << int(channel)
                    << std::endl;      
      }
    }

    // spectrumWiFiPhy
    else {
      Ptr<SpectrumWifiPhy> phy0 = wifidevice->GetPhy()->GetObject<SpectrumWifiPhy>(); 

      // check if the STA is already switching its channel
      if (phy0->IsStateSwitching()) {
        if (myverbose > 1)
          std::cout << Simulator::Now().GetSeconds()
                    << "\t[GetChannelOfaDevice]\tSTA with MAC " << deviceslink.Get (i)->GetAddress () 
                    << "  is switching its channel"
                    << std::endl; 
      }
      else {
        // as the STA is NOT switching its channel automatically, I do it
        phy0->SetChannelNumber (channel);

        if (myverbose > 1)
          std::cout << Simulator::Now().GetSeconds()
                    << "\t[GetChannelOfaDevice]\tChannel on STA with MAC " << deviceslink.Get (i)->GetAddress () 
                    << "  is:  " << int(channel)
                    << std::endl;
      }
    }
  }

  return channel;
}

// Change the frequency of the network device of a STA
// Copied from https://groups.google.com/forum/#!topic/ns-3-users/Ih8Hgs2qgeg
// https://10343742895474358856.googlegroups.com/attach/1b7c2a3108d5e/channel-switch-minimal.cc?part=0.1&view=1&vt=ANaJVrGFRkTkufO3dLFsc9u1J_v2-SUCAMtR0V86nVmvXWXGwwZ06cmTSv7DrQUKMWTVMt_lxuYTsrYxgVS59WU3kBd7dkkH5hQsLE8Em0FHO4jx8NbjrPk
void ChangeFrequencyLocal ( NetDeviceContainer deviceslink, uint8_t channel, uint32_t mywifiModel, uint32_t myverbose ) {

  for (uint32_t i = 0; i < deviceslink.GetN (); i++) {

    if (myverbose > 1)
      std::cout << Simulator::Now().GetSeconds()
                << "\t[ChangeFrequencyLocal]\tChanging channel on STA with MAC " << deviceslink.Get (i)->GetAddress () 
                << "  to:  " << uint16_t(channel) << std::endl;

    Ptr<WifiNetDevice> wifidevice = DynamicCast<WifiNetDevice> (deviceslink.Get(i));

    if (wifidevice == 0) {
      std::cout << "[ChangeFrequencyLocal]\tWARNING: wifidevice IS NULL" << '\n';
      NS_ASSERT(false);
    }

    // YansWifiPhy
    if (mywifiModel == 0) {
      Ptr<WifiPhy> phy0 = wifidevice->GetPhy();

      // make sure that the physical interface is ON
      NS_ASSERT(phy0->IsStateOff() == false);

      // check if the STA is already switching its channel
      if (phy0->IsStateSwitching()) {
        if (myverbose > 1)
          std::cout << Simulator::Now().GetSeconds()
                    << "\t[ChangeFrequencyLocal]\tSTA with MAC " << deviceslink.Get (i)->GetAddress () 
                    << "  is already switching its channel" << std::endl; 
      }
      else {
        // as the STA is NOT switching its channel automatically, I do it
        //https://www.nsnam.org/doxygen/classns3_1_1_wifi_phy.html#a2d13cf6ae4c185cae8516516afe4a32a
        phy0->SetChannelNumber (channel);

        // make sure that the physical interface is ON
        NS_ASSERT(phy0->IsStateOff() == false);

        if (myverbose > 1)
          std::cout << Simulator::Now().GetSeconds()
                    << "\t[ChangeFrequencyLocal]\tChanged channel on STA with MAC " << deviceslink.Get (i)->GetAddress () 
                    << "  to:  " << uint16_t(channel) << std::endl;      
      }

      // FIXME: is this needed?
      // see https://www.nsnam.org/doxygen/classns3_1_1_wifi_phy.html#ac365794e06cc92ae1262cbe72b72213d
      phy0->SetOffMode();
      if (myverbose > 1)
        std::cout << Simulator::Now().GetSeconds()
                  << "\t[ChangeFrequencyLocal]\tSTA with MAC " << deviceslink.Get (i)->GetAddress () 
                  << "  set to off mode  " << std::endl;    

      // see https://www.nsnam.org/doxygen/classns3_1_1_wifi_phy.html#adcc18a46c4f0faed2f05fe813cc75ed3
      phy0->ResumeFromOff();
      if (myverbose > 1)
        std::cout << Simulator::Now().GetSeconds()
                  << "\t[ChangeFrequencyLocal]\tSTA with MAC " << deviceslink.Get (i)->GetAddress () 
                  << "  set to on mode  " << std::endl;
    }

    // spectrumWiFiPhy
    else {
      Ptr<SpectrumWifiPhy> phy0 = wifidevice->GetPhy()->GetObject<SpectrumWifiPhy>(); 

      // check if the STA is already switching its channel
      if (phy0->IsStateSwitching()) {
        if (myverbose > 1)
          std::cout << Simulator::Now().GetSeconds()
                    << "\t[ChangeFrequencyLocal]\tSTA with MAC " << deviceslink.Get (i)->GetAddress () 
                    << "  is already switching its channel" << std::endl; 
      }
      else {
        // as the STA is NOT switching its channel automatically, I do it
        phy0->SetChannelNumber (channel);
        if (myverbose > 1)
          std::cout << Simulator::Now().GetSeconds()
                    << "\t[ChangeFrequencyLocal]\tChanged channel on STA with MAC " << deviceslink.Get (i)->GetAddress () 
                    << "  to:  " << uint16_t(channel) << std::endl;

      }

      /*
      // schedule the switch OFF and ON of this phy - it works, but it is not needed
      Simulator::Schedule (Seconds(0.9), &SetPhySleepMode, phy0);
      Simulator::Schedule (Seconds(1.0), &SetPhyOff, phy0);
      Simulator::Schedule (Seconds(1.5), &SetPhyOn, phy0);
      //phy0->SetOffMode();*/
      
    }
  }
}


/*********** This part is only for the ARPs. Not used **************/
typedef std::pair<Ptr<Packet>, Ipv4Header> Ipv4PayloadHeaderPair;

static void PrintArpCache (Ptr <Node> node, Ptr <NetDevice> nd/*, Ptr <Ipv4Interface> interface*/)
{
  std::cout << "Printing Arp Cache of Node#" << node->GetId() << '\n';
  Ptr <ArpL3Protocol> arpL3 = node->GetObject <ArpL3Protocol> ();
  //Ptr <ArpCache> arpCache = arpL3->FindCache (nd);
  //arpCache->Flush ();

  // Get an interactor to Ipv4L3Protocol instance
   Ptr<Ipv4L3Protocol> ip = node->GetObject<Ipv4L3Protocol> ();
   NS_ASSERT(ip !=0);

  // Get interfaces list from Ipv4L3Protocol iteractor
  ObjectVectorValue interfaces;
  ip->GetAttribute("InterfaceList", interfaces);

  // For each interface
  uint32_t l = 0;
  for(ObjectVectorValue::Iterator j = interfaces.Begin(); j !=interfaces.End (); j ++)
  {

    // Get an interactor to Ipv4L3Protocol instance
    Ptr<Ipv4Interface> ipIface = (j->second)->GetObject<Ipv4Interface> ();
    NS_ASSERT(ipIface != 0);

    std::cout << "Interface #" << l << " IP address" <<  /*<< */'\n';
    l++;

    // Get interfaces list from Ipv4L3Protocol iteractor
    Ptr<NetDevice> device = ipIface->GetDevice();
    NS_ASSERT(device != 0);

    if (device == nd) {
    // Get MacAddress assigned to this device
    Mac48Address addr = Mac48Address::ConvertFrom(device->GetAddress ());

    // For each Ipv4Address in the list of Ipv4Addresses assign to this interface...
    for(uint32_t k = 0; k < ipIface->GetNAddresses (); k++)
      {
        // Get Ipv4Address
        Ipv4Address ipAddr = ipIface->GetAddress (k).GetLocal();

        // If Loopback address, go to the next
        if(ipAddr == Ipv4Address::GetLoopback())
        {
          NS_LOG_UNCOND ("[PrintArpCache] Node #" << node->GetId() << " " << addr << ", " << ipAddr << "");
        } else {

          NS_LOG_UNCOND ("[PrintArpCache] Node #" << node->GetId() << " " << addr << ", " << ipAddr << "");

        Ptr<ArpCache> m_arpCache = nd->GetObject<ArpCache> ();
        m_arpCache = ipIface->GetObject<ArpCache> (); // FIXME: THIS DOES NOT WORK
        //m_arpCache = node->GetObject<ArpCache> ();
        //m_arpCache = nd->GetObject<ArpCache> ();
        //m_arpCache->SetAliveTimeout(Seconds(7));

        //if (m_arpCache != 0) {
          NS_LOG_UNCOND ("[PrintArpCache]       " << node->GetId() << " " << addr << ", " << ipAddr << "");
          AsciiTraceHelper asciiTraceHelper;
          Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream ("arpcache.txt");
          m_arpCache->PrintArpCache(stream);
          m_arpCache->Flush();
          //ArpCache::Entry * entry = m_arpCache->Add(ipAddr);
          //entry->MarkWaitReply(0);
          //entry->MarkAlive(addr);
          //}
        }
      }
    }
  }
  Simulator::Schedule (Seconds(1.0), &PrintArpCache, node, nd);
}

// empty the ARP cache of a node
void emtpyArpCache(Ptr <Node> mynode, uint32_t myverbose)
{
  // Create ARP Cache object
  Ptr<ArpCache> arp = CreateObject<ArpCache> ();
  Ptr<Ipv4L3Protocol> ip = mynode->GetObject<Ipv4L3Protocol> ();

  // do it only if the node has IP
  if ( ip != 0 ) {
    ObjectVectorValue interfaces;
    ip->GetAttribute("InterfaceList", interfaces);

    uint32_t interfaceNum = 0;
    for(ObjectVectorValue::Iterator j = interfaces.Begin(); j !=interfaces.End (); j ++)
    {
      Ptr<Ipv4Interface> ipIface = (j->second)->GetObject<Ipv4Interface> ();

      // Get interfaces list from Ipv4L3Protocol iteractor
      Ptr<NetDevice> device = ipIface->GetDevice();
      NS_ASSERT(device != 0);

      arp->SetDevice (device, ipIface); // https://www.nsnam.org/doxygen/classns3_1_1_arp_cache.html#details
      //ipIface->SetAttribute("ArpCache", PointerValue(arp));

      AsciiTraceHelper asciiTraceHelper;
      Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream ("arpcache.txt");
      arp->PrintArpCache(stream);

      if (myverbose) {
        std::cout << Simulator::Now().GetSeconds() << '\t'
                  << "[emtpyArpCache] Initial Arp cache of Node #" << mynode->GetId() 
                  << ", interface # " << interfaceNum;
            std::cout << stream << '\n';
      }

      arp->Flush(); //Clear the ArpCache of all entries. It sets the default parameters again

      if (myverbose) {
        std::cout << Simulator::Now().GetSeconds() << '\t'
                  << "[emtpyArpCache] Flushed Arp cache of Node #" << mynode->GetId()
                  << ", interface # " << interfaceNum;
        std::cout << stream << '\n';
      }
      interfaceNum ++;
    }
  }
  // If you want to do this periodically:
  //Simulator::Schedule (Seconds(1.0), &emtpyArpCache, mynode, myverbose);
}

// Modify the ARP parameters of a node
void modifyArpParams(Ptr <Node> mynode, double aliveTimeout, uint32_t myverbose)
{
  // Create ARP Cache object
  Ptr<ArpCache> arp = CreateObject<ArpCache> ();
  Ptr<Ipv4L3Protocol> ip = mynode->GetObject<Ipv4L3Protocol> ();

  // do it only if the node has IP
  if ( ip != 0 ) {
    ObjectVectorValue interfaces;
    ip->GetAttribute("InterfaceList", interfaces);

    uint32_t interfaceNum = 0;
    for(ObjectVectorValue::Iterator j = interfaces.Begin(); j !=interfaces.End (); j ++)
    {
      Ptr<Ipv4Interface> ipIface = (j->second)->GetObject<Ipv4Interface> ();

      // Get interfaces list from Ipv4L3Protocol iteractor
      Ptr<NetDevice> device = ipIface->GetDevice();
      NS_ASSERT(device != 0);

      arp->SetDevice (device, ipIface); // https://www.nsnam.org/doxygen/classns3_1_1_arp_cache.html#details
      //ipIface->SetAttribute("ArpCache", PointerValue(arp));

      AsciiTraceHelper asciiTraceHelper;
      Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream ("arpcache.txt");
      arp->PrintArpCache(stream);

      if (myverbose) {
        std::cout << Simulator::Now().GetSeconds() << '\t'
                  << "[modifyArpParams] Initial Arp parameters of Node #" << mynode->GetId() 
                  << '\n';
            std::cout << "\t\t\tInterface # " << interfaceNum
                      << '\n';

            Time mytime = arp->GetAliveTimeout();
            std::cout << "\t\t\t\tAlive Timeout [s]: " << mytime.GetSeconds()
                      << '\n';

            mytime = arp->GetDeadTimeout();
            std::cout << "\t\t\t\tDead Timeout [s]: " << mytime.GetSeconds() 
                      << '\n';
      }

      //arp->Flush(); //Clear the ArpCache of all entries. It sets the default parameters again
      //aliveTimeout = aliveTimeout + 1.0;
      Time alive = Seconds (aliveTimeout);
      arp->SetAliveTimeout (alive);

      if (myverbose) {
        std::cout << Simulator::Now().GetSeconds() << '\t'
                  << "[modifyArpParams] Modified Arp parameters of Node #" << mynode->GetId()
                  << '\n';

        std::cout << "\t\t\tInterface # " << interfaceNum
                  << '\n';

        Time mytime = arp->GetAliveTimeout();
        std::cout << "\t\t\t\tAlive Timeout [s]: " << mytime.GetSeconds()
                  << '\n';

        mytime = arp->GetDeadTimeout();
        std::cout << "\t\t\t\tDead Timeout [s]: " << mytime.GetSeconds() 
                  << '\n';
      }
      interfaceNum ++;
    }
  }
  // If you want to do this periodically:
  //Simulator::Schedule (Seconds(1.0), &modifyArpParams, mynode, aliveTimeout, myverbose);
}

// prints the parameters of the ARP cache
void infoArpCache(Ptr <Node> mynode, uint32_t myverbose)
{
  // Create ARP Cache object
  Ptr<ArpCache> arp = CreateObject<ArpCache> ();
  Ptr<Ipv4L3Protocol> ip = mynode->GetObject<Ipv4L3Protocol> ();
  if (ip!=0) {
    std::cout << Simulator::Now().GetSeconds() << '\t'
              << "[infoArpCache] Arp parameters of Node #" << mynode->GetId() 
              << '\n';

    ObjectVectorValue interfaces;
    ip->GetAttribute("InterfaceList", interfaces);

    uint32_t interfaceNum = 0;
    for(ObjectVectorValue::Iterator j = interfaces.Begin(); j !=interfaces.End (); j ++)
    {
      Ptr<Ipv4Interface> ipIface = (j->second)->GetObject<Ipv4Interface> ();

      // Get interfaces list from Ipv4L3Protocol iteractor
      Ptr<NetDevice> device = ipIface->GetDevice();
      NS_ASSERT(device != 0);

      arp->SetDevice (device, ipIface); // https://www.nsnam.org/doxygen/classns3_1_1_arp_cache.html#details
      //ipIface->SetAttribute("ArpCache", PointerValue(arp));

      AsciiTraceHelper asciiTraceHelper;
      Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream ("arpcache.txt");
      arp->PrintArpCache(stream);

      if (myverbose) {
        std::cout << "\t\t\tInterface # " << interfaceNum
                  << '\n';

        Time mytime = arp->GetAliveTimeout();
        std::cout << "\t\t\t\tAlive Timeout [s]: " << mytime.GetSeconds()
                  << '\n';

        mytime = arp->GetDeadTimeout();
        std::cout << "\t\t\t\tDead Timeout [s]: " << mytime.GetSeconds() 
                  << '\n';
      }
      interfaceNum ++;
    }
  }
  // If you want to do this periodically:
  //Simulator::Schedule (Seconds(1.0), &infoArpCache, mynode, myverbose);
}

// Taken from here https://github.com/MOSAIC-UA/802.11ah-ns3/blob/master/ns-3/scratch/s1g-mac-test.cc
// Two typos corrected here https://groups.google.com/forum/#!topic/ns-3-users/JRE_BsNEJrY

// It seems this is not feasible: https://www.nsnam.org/bugzilla/show_bug.cgi?id=187
void PopulateArpCache (uint32_t nodenumber, Ptr <Node> mynode)
{
  // Create ARP Cache object
  Ptr<ArpCache> arp = CreateObject<ArpCache> ();

  Ptr<Packet> dummy = Create<Packet> ();

  // Set ARP Timeout
  //arp->SetAliveTimeout (Seconds(3600 * 24 )); // 1-year
  //arp->SetWaitReplyTimeout (Seconds(200));

  // Populates ARP Cache with information from all nodes
  /*for (NodeList::Iterator i = NodeList::Begin(); i != NodeList::End(); ++i) {*/

    std::cout << "[PopulateArpCache] Node #" << nodenumber << '\n';

    // Get an interactor to Ipv4L3Protocol instance

    Ptr<Ipv4L3Protocol> ip = mynode->GetObject<Ipv4L3Protocol> ();
    NS_ASSERT(ip !=0);

    // Get interfaces list from Ipv4L3Protocol iteractor
    ObjectVectorValue interfaces;
    ip->GetAttribute("InterfaceList", interfaces);

    // For each interface
    for(ObjectVectorValue::Iterator j = interfaces.Begin(); j !=interfaces.End (); j ++) {
      // Get an interactor to Ipv4L3Protocol instance
      Ptr<Ipv4Interface> ipIface = (j->second)->GetObject<Ipv4Interface> ();
      NS_ASSERT(ipIface != 0);

      // Get interfaces list from Ipv4L3Protocol iteractor
      Ptr<NetDevice> device = ipIface->GetDevice();
      NS_ASSERT(device != 0);

      // Get MacAddress assigned to this device
      Mac48Address addr = Mac48Address::ConvertFrom(device->GetAddress ());

      // For each Ipv4Address in the list of Ipv4Addresses assign to this interface...
      for(uint32_t k = 0; k < ipIface->GetNAddresses (); k++) {
        // Get Ipv4Address
        Ipv4Address ipAddr = ipIface->GetAddress (k).GetLocal();

        // If Loopback address, go to the next
        if(ipAddr == Ipv4Address::GetLoopback())
          continue;

        std::cout << "[PopulateArpCache] Arp Cache: Adding the pair (" << addr << "," << ipAddr << ")" << '\n';

        // Creates an ARP entry for this Ipv4Address and adds it to the ARP Cache
        Ipv4Header ipHeader;
        ArpCache::Entry * entry = arp->Add(ipAddr);
        //entry->IsPermanent();
        //entry->MarkWaitReply();
        //entry->MarkPermanent();
        //entry->MarkAlive(addr);
        //entry->MarkDead();

        entry->MarkWaitReply (Ipv4PayloadHeaderPair(dummy,ipHeader));
        entry->MarkAlive (addr);
        entry->ClearPendingPacket();
        entry->MarkPermanent ();

        NS_LOG_UNCOND ("[PopulateArpCache] Arp Cache: Added the pair (" << addr << "," << ipAddr << ")");

        AsciiTraceHelper asciiTraceHelper;
        Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream ("arpcache.txt");

        arp->PrintArpCache(stream);
      }
//  }

    // Assign ARP Cache to each interface of each node
    for (NodeList::Iterator i = NodeList::Begin(); i != NodeList::End(); ++i)
    {

      Ptr<Ipv4L3Protocol> ip = (*i)->GetObject<Ipv4L3Protocol> ();
      if (ip!=0) {
        std::cout << "[PopulateArpCache] Adding the Arp Cache to Node #" << nodenumber << '\n';

        ObjectVectorValue interfaces;
        ip->GetAttribute("InterfaceList", interfaces);
        for(ObjectVectorValue::Iterator j = interfaces.Begin(); j !=interfaces.End (); j ++)
        {
          Ptr<Ipv4Interface> ipIface = (j->second)->GetObject<Ipv4Interface> ();

          // Get interfaces list from Ipv4L3Protocol iteractor
          Ptr<NetDevice> device = ipIface->GetDevice();
          NS_ASSERT(device != 0);

          arp->SetDevice (device, ipIface); // https://www.nsnam.org/doxygen/classns3_1_1_arp_cache.html#details

          //ipIface->SetAttribute("ArpCache", PointerValue(arp));


          AsciiTraceHelper asciiTraceHelper;
          Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream ("arpcache.txt");

          arp->PrintArpCache(stream);

          Time mytime = arp->GetAliveTimeout();
          std::cout << "Alive Timeout [s]: " << mytime.GetSeconds() << '\n';
        }
      }
    }
  }
}
/************* END of the ARP part (not used) *************/


// Modify the max AMPDU value of a node
void ModifyAmpdu (uint32_t nodeNumber, uint32_t ampduValue, uint32_t myverbose)
{
  // These are the attributes of regular-wifi-mac: https://www.nsnam.org/doxygen/regular-wifi-mac_8cc_source.html
  // You have to build a line like this (e.g. for node 0):
  // Config::Set("/NodeList/0/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BE_MaxAmpduSize", UintegerValue(ampduValue));
  // There are 4 queues: VI, VO, BE and BK

  // FIXME: Check if I only have to modify the parameters of all the devices (*), or only some of them.

  // I use an auxiliar string for creating the first argument of Config::Set
  std::ostringstream auxString;

  // VI queue
  auxString << "/NodeList/" << nodeNumber << "/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/VI_MaxAmpduSize";
  // std::cout << auxString.str() << '\n';
  Config::Set(auxString.str(),  UintegerValue(ampduValue));

  // clean the string
  auxString.str(std::string());

  // VO queue
  auxString << "/NodeList/" << nodeNumber << "/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/VO_MaxAmpduSize"; 
  // std::cout << auxString.str() << '\n';
  Config::Set(auxString.str(),  UintegerValue(ampduValue));

  // clean the string
  auxString.str(std::string());

  // BE queue
  auxString << "/NodeList/" << nodeNumber << "/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BE_MaxAmpduSize"; 
  // std::cout << auxString.str() << '\n';
  Config::Set(auxString.str(),  UintegerValue(ampduValue));

  // clean the string
  auxString.str(std::string());

  // BK queue
  auxString << "/NodeList/" << nodeNumber << "/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BK_MaxAmpduSize"; 
  //std::cout << auxString.str() << '\n';
  Config::Set(auxString.str(),  UintegerValue(ampduValue));  

  if ( myverbose > 1 )
    std::cout << Simulator::Now().GetSeconds()
              << "\t[ModifyAmpdu] Node #" << nodeNumber 
              << " AMPDU max size changed to " << ampduValue << " bytes" 
              << std::endl;
}


/*
// Not used
// taken from https://www.nsnam.org/doxygen/wifi-ap_8cc.html
// set the position of a node
static void
SetPosition (Ptr<Node> node, Vector position)
{
  Ptr<MobilityModel> mobility = node->GetObject<MobilityModel> ();
  mobility->SetPosition (position);
}*/


// Return a vector with the position of a node
// taken from https://www.nsnam.org/doxygen/wifi-ap_8cc.html
static Vector
GetPosition (Ptr<Node> node)
{
  Ptr<MobilityModel> mobility = node->GetObject<MobilityModel> ();
  return mobility->GetPosition ();
}


// Print the simulation time to std::cout
static void printTime (double period, std::string myoutputFileName, std::string myoutputFileSurname)
{
  std::cout << Simulator::Now().GetSeconds() << "\t" 
            << myoutputFileName << "_" 
            << myoutputFileSurname << '\n';

  // re-schedule 
  Simulator::Schedule (Seconds (period), &printTime, period, myoutputFileName, myoutputFileSurname);
}


// function for tracking mobility changes
static void 
CourseChange (std::string foo, Ptr<const MobilityModel> mobility )
{
  Vector pos = mobility->GetPosition ();
  Vector vel = mobility->GetVelocity ();
  std::cout << Simulator::Now ().GetSeconds()
            << "\t[CourseChange] MOBILITY CHANGE. model= " << mobility
            << ". string: " << foo
            << ", POS: x=" << pos.x 
            << ", y=" << pos.y
            << ", z=" << pos.z 
            << "; VEL:" << vel.x 
            << ", y=" << vel.y
            << ", z=" << vel.z << std::endl;
}


// Print the statistics to an output file and/or to the screen
void 
print_stats ( FlowMonitor::FlowStats st, 
              double simulationTime, 
              bool mygenerateHistograms, 
              std::string fileName,
              std::string fileSurname,
              uint32_t myverbose,
              std::string flowID,
              uint32_t printColumnTitles ) 
{

  // print the results to a file (they are written at the end of the file)
  if ( fileName != "" ) {

    std::ofstream ofs;
    ofs.open ( fileName + "_flows.txt", std::ofstream::out | std::ofstream::app); // with "trunc" Any contents that existed in the file before it is open are discarded. with "app", all output operations happen at the end of the file, appending to its existing contents

    // Print a line in the output file, with the title of each column
    if ( printColumnTitles == 1 ) {
      ofs << "Flow_ID" << "\t"
          << "Protocol" << "\t"
          << "source_Address" << "\t"
          << "source_Port" << "\t" 
          << "destination_Address" << "\t"
          << "destination_Port" << "\t"
          << "Num_Tx_Packets" << "\t" 
          << "Num_Tx_Bytes" << "\t" 
          << "Tx_Throughput_[bps]" << "\t"  
          << "Num_Rx_Packets" << "\t" 
          << "Num_RX_Bytes" << "\t" 
          << "Num_lost_packets" << "\t" 
          << "Rx_Throughput_[bps]" << "\t"
          << "Average_Latency_[s]" << "\t"
          << "Average_Jitter_[s]" << "\t"
          << "Average_Number_of_hops" << "\t"
          << "Simulation_time_[s]" << "\n";
    }

    // Print a line in the output file, with the data of this flow
    ofs << flowID << "\t" // flowID includes the protocol, IP addresses and ports
        << st.txPackets << "\t" 
        << st.txBytes << "\t" 
        << st.txBytes * 8.0 / simulationTime << "\t"  
        << st.rxPackets << "\t" 
        << st.rxBytes << "\t" 
        << st.txPackets - st.rxPackets << "\t" 
        << st.rxBytes * 8.0 / simulationTime << "\t";

    if (st.rxPackets > 0) 
    { 
      ofs << (st.delaySum.GetSeconds() / st.rxPackets) <<  "\t";

      if (st.rxPackets > 1) { // I need at least two packets for calculating the jitter
        ofs << (st.jitterSum.GetSeconds() / (st.rxPackets - 1.0)) << "\t";
      } else {
        ofs << "\t";
      }

      ofs << st.timesForwarded / st.rxPackets + 1 << "\t"; 

    } else { //no packets arrived
      ofs << "\t" << "\t" << "\t"; 
    }

    ofs << simulationTime << "\n";

    ofs.close();


    // save the histogram to a file
    if ( mygenerateHistograms == true) 
    { 
      std::ofstream ofs_histo;
      ofs_histo.open ( fileName + fileSurname + "_delay_histogram.txt", std::ofstream::out | std::ofstream::trunc);

      ofs_histo << "Flow #" << flowID << "\n";
      ofs_histo << "number\tinit_interval\tend_interval\tnumber_of_samples" << std::endl; 
      for (uint32_t i=0; i < st.delayHistogram.GetNBins (); i++) 
        ofs_histo << i << "\t" << st.delayHistogram.GetBinStart (i) << "\t" << st.delayHistogram.GetBinEnd (i) << "\t" << st.delayHistogram.GetBinCount (i) << std::endl; 
      ofs_histo.close();

      ofs_histo.open ( fileName + fileSurname + "_jitter_histogram.txt", std::ofstream::out | std::ofstream::trunc); // with "trunc", Any contents that existed in the file before it is open are discarded

      ofs_histo << "Flow #" << flowID << "\n";
      ofs_histo << "number\tinit_interval\tend_interval\tnumber_of_samples" << std::endl; 
      for (uint32_t i=0; i < st.jitterHistogram.GetNBins (); i++ ) 
        ofs_histo << i << "\t" << st.jitterHistogram.GetBinStart (i) << "\t" << st.jitterHistogram.GetBinEnd (i) << "\t" << st.jitterHistogram.GetBinCount (i) << std::endl; 
      ofs_histo.close();

      ofs_histo.open ( fileName + fileSurname + "_packetsize_histogram.txt", std::ofstream::out | std::ofstream::trunc); // with "trunc", Any contents that existed in the file before it is open are discarded

      ofs_histo << "Flow #" << flowID << "\n";
      ofs_histo << "number\tinit_interval\tend_interval\tnumber_of_samples"<< std::endl; 
      for (uint32_t i=0; i < st.packetSizeHistogram.GetNBins (); i++ ) 
        ofs_histo << i << "\t" << st.packetSizeHistogram.GetBinStart (i) << "\t" << st.packetSizeHistogram.GetBinEnd (i) << "\t" << st.packetSizeHistogram.GetBinCount (i) << std::endl; 
      ofs_histo.close();
    }
  }

  // print the results by the screen
  if ( myverbose > 0 ) {
    std::cout << " -Flow #" << flowID << "\n";
    if ( mygenerateHistograms ) {
      std::cout << "   The name of the output files starts with: " << fileName << fileSurname << "\n";
      std::cout << "   Tx Packets: " << st.txPackets << "\n";
      std::cout << "   Tx Bytes:   " << st.txBytes << "\n";
      std::cout << "   TxOffered:  " << st.txBytes * 8.0 / simulationTime / 1000 / 1000  << " Mbps\n";
      std::cout << "   Rx Packets: " << st.rxPackets << "\n";
      std::cout << "   Rx Bytes:   " << st.rxBytes << "\n";
      std::cout << "   Lost Packets: " << st.txPackets - st.rxPackets << "\n";
      std::cout << "   Throughput: " << st.rxBytes * 8.0 / simulationTime / 1000 / 1000  << " Mbps\n";      
    }

    if (st.rxPackets > 0) // some packets have arrived
    { 
      std::cout << "   Mean{Delay}: " << (st.delaySum.GetSeconds() / st.rxPackets); 
      
      if (st.rxPackets > 1) // I need at least two packets for calculating the jitter
      { 
        std::cout << "   Mean{Jitter}: " << (st.jitterSum.GetSeconds() / (st.rxPackets - 1.0 ));
      } else {
        std::cout << "   Mean{Jitter}: only one packet arrived. "; 
      }
      
      std::cout << "   Mean{Hop Count}: " << st.timesForwarded / st.rxPackets + 1 << "\n"; 

    } else { //no packets arrived
      std::cout << "   Mean{Delay}: no packets arrived. ";
      std::cout << "   Mean{Jitter}: no packets arrived. "; 
      std::cout << "   Mean{Hop Count}: no packets arrived. \n"; 
    }

    if (( mygenerateHistograms ) && ( myverbose > 3 )) 
    { 
      std::cout << "   Delay Histogram" << std::endl; 
      for (uint32_t i=0; i < st.delayHistogram.GetNBins (); i++) 
        std::cout << "  " << i << "(" << st.delayHistogram.GetBinStart (i) 
                  << "-" << st.delayHistogram.GetBinEnd (i) 
                  << "): " << st.delayHistogram.GetBinCount (i) 
                  << std::endl; 

      std::cout << "   Jitter Histogram" << std::endl; 
      for (uint32_t i=0; i < st.jitterHistogram.GetNBins (); i++ ) 
        std::cout << "  " << i << "(" << st.jitterHistogram.GetBinStart (i) 
                  << "-" << st.jitterHistogram.GetBinEnd (i) 
                  << "): " << st.jitterHistogram.GetBinCount (i) 
                  << std::endl; 

      std::cout << "   PacketSize Histogram  "<< std::endl; 
      for (uint32_t i=0; i < st.packetSizeHistogram.GetNBins (); i++ ) 
        std::cout << "  " << i << "(" << st.packetSizeHistogram.GetBinStart (i) 
                  << "-" << st.packetSizeHistogram.GetBinEnd (i) 
                  << "): " << st.packetSizeHistogram.GetBinCount (i) 
                  << std::endl; 
    }

    for (uint32_t i=0; i < st.packetsDropped.size (); i++) 
      std::cout << "    Packets dropped by reason " << i << ": " << st.packetsDropped [i] << std::endl; 
    //for (uint32_t i=0; i<st.bytesDropped.size(); i++) 
    //  std::cout << "Bytes dropped by reason " << i << ": " << st.bytesDropped[i] << std::endl;

    std::cout << "\n";
  }
} 


// this class stores a number of records: each one contains a pair AP node id - AP MAC address
// the node id is the one given by ns3 when creating the node
// if the channel is '0' it means that the AP is NOT active. It will have an apId, but will not be active
class AP_record
{
  public:
    AP_record ();
    //void SetApRecord (uint16_t thisId, Mac48Address thisMac);
    void SetApRecord (uint16_t thisId, std::string thisMac, uint32_t thisMaxSizeAmpdu);
    //uint16_t GetApid (Mac48Address thisMac);
    uint16_t GetApid ();
    //Mac48Address GetMac (uint16_t thisId);
    std::string GetMac ();
    uint32_t GetMaxSizeAmpdu ();
    uint8_t GetWirelessChannel();
    void setWirelessChannel(uint8_t thisWirelessChannel);
  private:
    uint16_t apId;
    //Mac48Address apMac;
    std::string apMac;
    uint32_t apMaxSizeAmpdu;
    uint8_t apWirelessChannel; // if the channel is '0' it means that the AP is NOT active
};

typedef std::vector <AP_record * > AP_recordVector;
AP_recordVector AP_vector;

AP_record::AP_record ()
{
  apId = 0;
  apMac = "02-06-00:00:00:00:00:00";
  apMaxSizeAmpdu = 0;
}

void
//AP_record::SetApRecord (uint16_t thisId, Mac48Address thisMac)
AP_record::SetApRecord (uint16_t thisId, std::string thisMac, uint32_t thisMaxSizeAmpdu)
{
  apId = thisId;
  apMac = thisMac;
  apMaxSizeAmpdu = thisMaxSizeAmpdu;
}


uint16_t
//AP_record::GetApid (Mac48Address thisMac)
AP_record::GetApid ()
{
  return apId;
}

//Mac48Address
std::string
AP_record::GetMac ()
{
  return apMac;
}

uint8_t
AP_record::GetWirelessChannel()
{
  return apWirelessChannel;
}

void
AP_record::setWirelessChannel(uint8_t thisWirelessChannel)
{
  apWirelessChannel = thisWirelessChannel;
}

// obtain the nearest AP of a STA, in a certain frequency band (2.4 or 5 GHz)
// if 'frequencyBand == 0', the nearest AP will be searched in both bands
static Ptr<Node>
nearestAp (NodeContainer APs, Ptr<Node> mySTA, int myverbose, std::string frequencyBand)
{
  // the frequency band MUST be "2.4 GHz" or "5 GHz". It can also be "both", meaning both bands
  NS_ASSERT (( frequencyBand == "2.4 GHz" ) || (frequencyBand == "5 GHz" ) || (frequencyBand == "both" ));

  // vector with the position of the STA
  Vector posSta = GetPosition (mySTA);

  if (frequencyBand != "both") {
    if (VERBOSE_FOR_DEBUG > 0)
      std::cout << "\n"
                << Simulator::Now().GetSeconds() 
                << "\t[nearestAp] Looking for the nearest AP of STA #" << mySTA->GetId()
                << ", in position: "  << posSta.x << "," << posSta.y
                << ", in the " << frequencyBand << " band"
                << std::endl;
  }
  else {
    if (VERBOSE_FOR_DEBUG > 0)
      std::cout << "\n"
                << Simulator::Now().GetSeconds() 
                << "\t[nearestAp] Looking for the nearest AP of STA #" << mySTA->GetId()
                << ", in position: "  << posSta.x << "," << posSta.y
                << ", in any frequency band"
                << std::endl;    
  }

  // calculate an initial value for the minimum distance (a very high value)
  double mimimumDistance = APs.GetN() * 100000;

  uint8_t channelNearestAP = 0;

  // variable for storing the nearest AP
  Ptr<Node> nearest;

  // vector with the position of the AP
  Vector posAp;

  // Check all the APs to find the nearest one
  // go through the AP record vector
  AP_recordVector::const_iterator indexAP = AP_vector.begin ();

  // go through the nodeContainer of APs at the same time
  NodeContainer::Iterator i; 
  for (i = APs.Begin (); i != APs.End (); ++i) {
    //(*i)->method ();  // some Node method

    // find the frequency band of this AP
    uint8_t channelThisAP = (*indexAP)->GetWirelessChannel();
    if (channelThisAP != 0) {
      std::string frequencyBandThisAP = getWirelessBandOfChannel(channelThisAP);

      if (VERBOSE_FOR_DEBUG > 0)
        std::cout << Simulator::Now().GetSeconds() 
                  << "\t[nearestAp]\tAP #" << (*i)->GetId()
                  <<  ", channel: "  << (uint16_t)channelThisAP 
                  << ", frequency band: " << frequencyBandThisAP << std::endl;

      // only look for APs in the specified frequency band
      if ((frequencyBand == frequencyBandThisAP ) || (frequencyBand == "both") ) {
        if (VERBOSE_FOR_DEBUG > 0)
          std::cout << "\t\t\t is in the correct band" << std::endl;

        posAp = GetPosition((*i));
        double distance = sqrt ( ( (posSta.x - posAp.x)*(posSta.x - posAp.x) ) + ( (posSta.y - posAp.y)*(posSta.y - posAp.y) ) );
        if (distance < mimimumDistance ) {
          mimimumDistance = distance;
          nearest = *i;
          channelNearestAP = channelThisAP;

          if (VERBOSE_FOR_DEBUG > 0)
            std::cout << "\t\t\t and it is the nearest one so far (distance " << distance << " m)" << std::endl;
        }
        else {
          if (VERBOSE_FOR_DEBUG > 0)
            std::cout << "\t\t\t but it is not the nearest one (distance " << distance << " m)" << std::endl;
        }
      }
      else {
        if (VERBOSE_FOR_DEBUG > 0)
          std::cout << "\t\t\t is not in the correct band" << std::endl;      
      }
    }
    else {
      if (VERBOSE_FOR_DEBUG > 0)
        std::cout << Simulator::Now().GetSeconds() 
                  << "\t[nearestAp]\tAP #" << (*i)->GetId()        
                  << "\t\t\t is not active" << std::endl;        
    }
    indexAP++;
  }

  if(nearest!=NULL) {
    NS_ASSERT(channelNearestAP!=0);
    if (frequencyBand != "both") {
      if (VERBOSE_FOR_DEBUG > 0)
      std::cout << Simulator::Now().GetSeconds()
                << "\t[nearestAp] Result: The nearest AP in the " << frequencyBand << " band "
                << "is AP#" << nearest->GetId() 
                << ". Channel: "  << (uint16_t)channelNearestAP 
                << ". Frequency band: " << getWirelessBandOfChannel(channelNearestAP)
                << ". Position: "  << GetPosition((nearest)).x 
                << "," << GetPosition((nearest)).y
                << std::endl;     
    }
    else {
      if (VERBOSE_FOR_DEBUG > 0)
      std::cout << Simulator::Now().GetSeconds()
                << "\t[nearestAp] Result: The nearest AP in any frequency band "
                << "is AP#" << nearest->GetId() 
                << ". Channel: "  << (uint16_t)channelNearestAP 
                << ". Frequency band: " << getWirelessBandOfChannel(channelNearestAP)
                << ". Position: "  << GetPosition((nearest)).x 
                << "," << GetPosition((nearest)).y
                << std::endl;        
    }
  }
  else {
    if (frequencyBand != "both") {
      // if an AP in the same band cannot be found, finish the simulation
      std::cout << Simulator::Now().GetSeconds()
                << "\t[nearestAp] ERROR: There is no AP in the " << frequencyBand << " band, but STAs have been defined in that band "
                << "- FINISHING SIMULATION"
                << std::endl;
    }
    else {
      // if an AP in the same band cannot be found, finish the simulation
      std::cout << Simulator::Now().GetSeconds()
                << "\t[nearestAp] ERROR: There is no AP in any frequency band "
                << "- FINISHING SIMULATION"
                << std::endl;      
    }
    NS_ASSERT(false);
  }
  if (VERBOSE_FOR_DEBUG > 0)
    std::cout << Simulator::Now().GetSeconds()
              << "\t[nearestAp] Successfully finishing nearestAp" << std::endl;

  return nearest;
}

//MaxSizeAmpdu
uint32_t
AP_record::GetMaxSizeAmpdu ()
{
  return apMaxSizeAmpdu;
}

void
Modify_AP_Record (uint16_t thisId, std::string thisMac, uint32_t thisMaxSizeAmpdu) // FIXME: Can this be done just with Set_AP_Record?
{
  for (AP_recordVector::const_iterator index = AP_vector.begin (); index != AP_vector.end (); index++) {
    //std::cout << Simulator::Now ().GetSeconds() << " ********************** AP with ID " << (*index)->GetApid() << " has MAC: " << (*index)->GetMac() << " *****" << std::endl;

    if ( (*index)->GetMac () == thisMac ) {
      (*index)->SetApRecord (thisId, thisMac, thisMaxSizeAmpdu);
      //std::cout << Simulator::Now ().GetSeconds() << "\t[GetAnAP_Id] AP #" << (*index)->GetApid() << " has MAC: " << (*index)->GetMac() << "" << std::endl;
    }
  }
}

uint32_t
CountAPs ()
{
  uint32_t number = 0;
  for (AP_recordVector::const_iterator index = AP_vector.begin (); index != AP_vector.end (); index++) {
    number ++;
  }
  return number;
}

uint32_t
CountAPs (uint32_t myverbose)
// counts all the APs with their id, mac and current value of MaxAmpdu
{
  uint32_t number = 0;

  for (AP_recordVector::const_iterator index = AP_vector.begin (); index != AP_vector.end (); index++) {
    number ++;
  }

  if (myverbose > 2)
    std::cout << "\n" << Simulator::Now ().GetSeconds() << "\t[CountAPs] Total APs: " << number
              << std::endl;

  return number;
}

uint16_t
GetAnAP_Id (std::string thisMac)
// lists all the STAs associated to an AP, with the MAC of the AP
{
  if (VERBOSE_FOR_DEBUG > 0)
    std::cout << Simulator::Now ().GetSeconds() << "\t[GetAnAP_Id] Looking for MAC " << thisMac << std::endl;

  bool macFound = false;

  uint16_t APid;

  uint32_t numberAPs = CountAPs();
  if (VERBOSE_FOR_DEBUG > 0)
    std::cout << Simulator::Now ().GetSeconds() << "\t[GetAnAP_Id] Number of APs: " << (unsigned int)numberAPs << "\n";
  //std::cout << Simulator::Now ().GetSeconds() << " *** Number of STA associated: " << Get_STA_record_num() << " *****" << std::endl;

  for (AP_recordVector::const_iterator index = AP_vector.begin (); index != AP_vector.end (); index++) {
    if (VERBOSE_FOR_DEBUG > 0)
      std::cout << Simulator::Now ().GetSeconds() << "\t[GetAnAP_Id]   AP with ID " << (*index)->GetApid() << " has MAC:   " << (*index)->GetMac() << std::endl;
    
    if ( (*index)->GetMac () == thisMac ) {
      // check the primary MAC address of each AP
      APid = (*index)->GetApid ();
      macFound = true;
      if (VERBOSE_FOR_DEBUG > 0)
        std::cout << Simulator::Now ().GetSeconds() << "\t[GetAnAP_Id] FOUND: AP #" << (*index)->GetApid() << " has MAC: " << (*index)->GetMac() << "" << std::endl;
    }
  }

  // make sure that an AP with 'thisMac' exists
  if (macFound == false) {
    std::cout << "\n"
              << Simulator::Now ().GetSeconds()
              << "\t[GetAnAP_Id] ERROR: MAC " << thisMac << " not found in the list of APs. Stopping simulation"
              << std::endl;
    NS_ASSERT(macFound == true);
  }
  
  // return the identifier of the AP
  return APid;
}

uint32_t
GetAP_MaxSizeAmpdu (uint16_t thisAPid, uint32_t myverbose)
// returns the max size of the Ampdu of an AP
{
  uint32_t APMaxSizeAmpdu = 0;
  //std::cout << Simulator::Now ().GetSeconds() << " *** Number of STA associated: " << Get_STA_record_num() << " *****" << std::endl;

  for (AP_recordVector::const_iterator index = AP_vector.begin (); index != AP_vector.end (); index++) {

    if ( (*index)->GetApid () == thisAPid ) {
      APMaxSizeAmpdu = (*index)->GetMaxSizeAmpdu ();
      if ( myverbose > 2 )
        std::cout << Simulator::Now ().GetSeconds() 
                  << "\t[GetAP_MaxSizeAmpdu] AP #" << (*index)->GetApid() 
                  << " has AMDPU: " << (*index)->GetMaxSizeAmpdu() 
                  << "" << std::endl;
    }
  }
  return APMaxSizeAmpdu;
}

uint8_t
GetAP_WirelessChannel (uint16_t thisAPid, uint32_t myverbose)
// returns the wireless channel of an AP
{
  uint8_t APWirelessChannel = 0;
  //std::cout << Simulator::Now ().GetSeconds() << " *** Number of STA associated: " << Get_STA_record_num() << " *****" << std::endl;

  for (AP_recordVector::const_iterator index = AP_vector.begin (); index != AP_vector.end (); index++) {

    if ( (*index)->GetApid () == thisAPid ) {
      APWirelessChannel = (*index)->GetWirelessChannel();
      if ( myverbose > 2 )
        std::cout << Simulator::Now ().GetSeconds() 
                  << "\t[GetAP_WirelessChannel] AP #" << (*index)->GetApid() 
                  << " has channel: " << uint16_t((*index)->GetWirelessChannel())
                  << "" << std::endl;
    }
  }
  return APWirelessChannel;
}


void
ListAPs (uint32_t myverbose)
// lists all the APs with their id, mac and current value of MaxAmpdu
{
  std::cout << "\n"
            << Simulator::Now ().GetSeconds()
            << "\t[ListAPs] Report APs. Total "<< CountAPs(0)
            << " APs"
            << std::endl;

  for (AP_recordVector::const_iterator index = AP_vector.begin (); index != AP_vector.end (); index++) {
    std::cout //<< Simulator::Now ().GetSeconds()
              << "                  "
              << "   \tAP #" << (*index)->GetApid() 
              << " with MAC " << (*index)->GetMac() 
              << " Max size AMPDU " << (*index)->GetMaxSizeAmpdu() 
              << " Channel " << uint16_t((*index)->GetWirelessChannel())
              << std::endl;
  }
  std::cout << std::endl;
}



// This part, i.e. the association record is taken from https://github.com/MOSAIC-UA/802.11ah-ns3/blob/master/ns-3/scratch/s1g-mac-test.cc

// this class stores one record per STA, containing 
// - the information of its association: the MAC of the AP where it is associated
// - the type of application it is running
// note: it does NOT store the MAC address of the STA itself
class STA_record
{
  public:
    STA_record ();
    bool GetAssoc ();
    uint16_t GetStaid ();
    Mac48Address GetMacOfitsAP ();
    uint32_t Gettypeofapplication ();
    uint32_t GetMaxSizeAmpdu ();
    bool GetDisabledPermanently ();
    uint16_t GetpeerStaid ();
    uint32_t GetWifiModel ();
    void StaCourseChange (std::string context, Ptr<const ns3::MobilityModel>);
    void SetAssoc (std::string context, Mac48Address AP_MAC_address);
    void UnsetAssoc (std::string context, Mac48Address AP_MAC_address);
    void setstaid (uint16_t id);
    void Settypeofapplication (uint32_t applicationid);
    void SetMaxSizeAmpdu (uint32_t MaxSizeAmpdu);
    void SetVerboseLevel (uint32_t myVerboseLevel);
    void SetnumOperationalChannels (uint32_t mynumOperationalChannels);
    void SetpeerStaid (uint16_t id);
    void SetDisabledPermanently (bool disabled);
    void SetDisablePeerSTAWhenAssociated (bool disable);
    void SetPrimarySTA (bool primary);
    void Setversion80211 (std::string myversion80211);
    void SetaggregationDisableAlgorithm (uint16_t myaggregationDisableAlgorithm);
    void SetAmpduSize (uint32_t myAmpduSize);
    void SetmaxAmpduSizeWhenAggregationLimited (uint32_t mymaxAmpduSizeWhenAggregationLimited);
    void SetWifiModel (uint32_t mywifiModel);
    void PrintAllVariables ();
  private:
    bool assoc;
    uint16_t staid;
    Mac48Address apMac;
    uint32_t typeofapplication; // 0 no application; 1 VoIP upload; 2 VoIP download; 3 TCP upload; 4 TCP download; 5 Video download
    uint32_t staRecordMaxSizeAmpdu;
    uint32_t staRecordVerboseLevel;
    uint32_t staRecordnumOperationalChannels;
    bool disabledPermanently;
    bool disablePeerSTAWhenAssociated;
    bool primarySTA;
    uint16_t peerStaid;  // the ID of the peer STA. It is 0 if there is no peer STA

    std::string staRecordversion80211;
    uint16_t staRecordaggregationDisableAlgorithm;
    uint32_t staRecordMaxAmpduSize;
    uint32_t staRecordmaxAmpduSizeWhenAggregationLimited;
    uint32_t staRecordwifiModel;
};

// this is the constructor. Set the default parameters
STA_record::STA_record ()
{
  assoc = false;
  staid = 0;
  apMac = "00:00:00:00:00:00";    //MAC address of the AP to which the STA is associated
  typeofapplication = 0;
  staRecordMaxSizeAmpdu = 0;
  staRecordVerboseLevel = 0;
  staRecordnumOperationalChannels = 0;
  disabledPermanently = false;
  disablePeerSTAWhenAssociated = false;
  primarySTA = true;
  peerStaid = 0;
  staRecordversion80211 = "";
  staRecordaggregationDisableAlgorithm = 0;
  staRecordMaxAmpduSize = 0;
  staRecordmaxAmpduSizeWhenAggregationLimited = 0;
  staRecordwifiModel = 0;
}

void
STA_record::setstaid (uint16_t id)
{
  staid = id;
}

typedef std::vector <STA_record * > STA_recordVector;
STA_recordVector sta_vector;

/*
STA_record
Get_pointerToSTArecord (uint16_t staid)
// gets a pointer to the STA record
{
  STA_record myPointer;

  for (STA_recordVector::const_iterator index = sta_vector.begin (); index != sta_vector.end (); index++) {
    if ((*index)->GetStaid () == staid) {
      myPointer == &index;
    }
  }
  return myPointer;
}*/


uint32_t
Get_STA_record_num ()
// counts the number or STAs associated
{
  uint32_t AssocNum = 0;
  for (STA_recordVector::const_iterator index = sta_vector.begin (); index != sta_vector.end (); index++) {
    if ((*index)->GetAssoc ()) {
      AssocNum++;
    }
  }
  return AssocNum;
}

Mac48Address
GetAPMACOfAnAssociatedSTA (uint16_t id)
// it returns the channel of the AP where the STA is associated
{
  if (VERBOSE_FOR_DEBUG > 0)
    std::cout << Simulator::Now().GetSeconds()
              << "\t[GetAPMACOfAnAssociatedSTA] Looking for the MAC of the AP of STA#" << id
              << std::endl;

  Mac48Address desiredMAC;

  for (STA_recordVector::const_iterator index = sta_vector.begin (); index != sta_vector.end (); index++) {
    if ((*index)->GetStaid () == id) {

      Mac48Address nullMAC = "00:00:00:00:00:00";

      if ((*index)->GetMacOfitsAP() == nullMAC) {
        if (VERBOSE_FOR_DEBUG > 0)
            std::cout << Simulator::Now().GetSeconds()
                      << "\t[GetAPMACOfAnAssociatedSTA] STA#" << (*index)->GetStaid ()
                      << " nas no associated AP"
                      << std::endl;
      }
      else {
        // auxiliar string
        std::ostringstream auxString;
        // create a string with the MAC
        auxString << "02-06-" << (*index)->GetMacOfitsAP();
        std::string myaddress = auxString.str();

        desiredMAC = Mac48Address::ConvertFrom((*index)->GetMacOfitsAP());

        if (VERBOSE_FOR_DEBUG > 0)
          std::cout << Simulator::Now().GetSeconds()
                    << "\t[GetAPMACOfAnAssociatedSTA] STA#" << (*index)->GetStaid ()
                    << " found"
                    << ". MAC of its associated AP: " << myaddress
                    << std::endl;   
      }
    }
  }

  return desiredMAC;
}

uint8_t
GetChannelOfAnAssociatedSTA (uint16_t id)
// it returns the channel of the AP where the STA is associated
{
  if (VERBOSE_FOR_DEBUG > 0)
    std::cout << Simulator::Now().GetSeconds()
              << "\t[GetChannelOfAnAssociatedSTA] Looking for the channel of the AP of STA#" << id
              << std::endl;

  uint8_t channel = 0;
  for (STA_recordVector::const_iterator index = sta_vector.begin (); index != sta_vector.end (); index++) {
    if ((*index)->GetStaid () == id) {

      Mac48Address nullMAC = "00:00:00:00:00:00";

      if ((*index)->GetMacOfitsAP() == nullMAC) {
        if (VERBOSE_FOR_DEBUG > 0)
            std::cout << Simulator::Now().GetSeconds()
                      << "\t[GetChannelOfAnAssociatedSTA] STA#" << (*index)->GetStaid ()
                      << " nas no associated AP"
                      << std::endl;
      }
      else {
        // auxiliar string
        std::ostringstream auxString;
        // create a string with the MAC
        auxString << "02-06-" << (*index)->GetMacOfitsAP();
        std::string myaddress = auxString.str();

        if (VERBOSE_FOR_DEBUG > 0)
          std::cout << Simulator::Now().GetSeconds()
                    << "\t[GetChannelOfAnAssociatedSTA] STA#" << (*index)->GetStaid ()
                    << " found"
                    << ". MAC of its associated AP: " << myaddress
                    << std::endl; 

        if (VERBOSE_FOR_DEBUG > 0)
          std::cout << Simulator::Now().GetSeconds()
                    << "\t[GetChannelOfAnAssociatedSTA] Calling GetAnAP_Id()"
                    << std::endl;

        // Get the wireless channel of the AP with the corresponding address
        NS_ASSERT(myaddress!="02-06-00:00:00:00:00:00");
        channel = GetAP_WirelessChannel (GetAnAP_Id(myaddress), 0);      
      }
    }
  }
  if (VERBOSE_FOR_DEBUG > 0)
    std::cout << Simulator::Now().GetSeconds()
              << "\t[GetChannelOfAnAssociatedSTA] Return value: channel " << (uint16_t)channel
              << std::endl;

  return channel;
}

// Print the channel of a STA
static void
ReportChannel (double period, uint16_t id, int myverbose)
{
  if (myverbose > 2) {
    // Find the AP to which the STA is associated
    for (STA_recordVector::const_iterator index = sta_vector.begin (); index != sta_vector.end (); index++) {
      if ((*index)->GetStaid () == id) {
        // auxiliar string
        std::ostringstream auxString;
        // create a string with the MAC
        auxString << "02-06-" << (*index)->GetMacOfitsAP();
        std::string myaddress = auxString.str();
        NS_ASSERT(!myaddress.empty());

        if (myaddress=="02-06-00:00:00:00:00:00") {
          std::cout << Simulator::Now().GetSeconds()
                    << "\t[ReportChannel] STA #" << id 
                    << " Not associated to any AP"
                    << std::endl;          
        }
        else {
          NS_ASSERT(myaddress!="02-06-00:00:00:00:00:00");
          std::cout << Simulator::Now().GetSeconds()
                    << "\t[ReportChannel] STA #" << id 
                    << " Associated to AP#" << GetAnAP_Id(myaddress)
                    << ". Channel: " << uint16_t(GetChannelOfAnAssociatedSTA (id))
                    << std::endl;          
        }
      }
    }
  }

  // re-schedule
  Simulator::Schedule (Seconds (period), &ReportChannel, period, id, myverbose);
}

// lists all the STAs, with the MAC of the AP if they are associated to it
void
List_STA_record ()
{
  std::cout << "\n" << Simulator::Now ().GetSeconds() << "\t[List_STA_record] Report STAs. Total associated: " << Get_STA_record_num() << "" << std::endl;

  for (STA_recordVector::const_iterator index = sta_vector.begin (); index != sta_vector.end (); index++) {
    if ((*index)->GetAssoc ()) {

      // auxiliar string
      std::ostringstream auxString;
      // create a string with the MAC
      auxString << "02-06-" << (*index)->GetMacOfitsAP();
      std::string myaddress = auxString.str();
      NS_ASSERT(!myaddress.empty());

      if (VERBOSE_FOR_DEBUG > 0)
        std::cout << Simulator::Now().GetSeconds()
                  << "\t[List_STA_record] Calling GetAnAP_Id()"
                  << std::endl; 

      std::cout //<< Simulator::Now ().GetSeconds() 
                << "\t\t\t\tSTA #" << (*index)->GetStaid() 
                << "\tassociated to AP #" << GetAnAP_Id(myaddress) 
                << "\twith MAC " << (*index)->GetMacOfitsAP() 
                << "\ttype of application " << (*index)->Gettypeofapplication()
                << "\tValue of Max AMPDU " << (*index)->GetMaxSizeAmpdu()
                << std::endl;
    } else {
      std::cout //<< Simulator::Now ().GetSeconds() 
                << "\t\t\t\tSTA #" << (*index)->GetStaid()
                << "\tnot associated to any AP \t\t\t" 
                << "\ttype of application " << (*index)->Gettypeofapplication()
                << "\tValue of Max AMPDU " << (*index)->GetMaxSizeAmpdu()
                << std::endl;      
    }
  }
}

// This is called with a callback every time a STA changes its course
void STA_record::StaCourseChange (std::string context, Ptr<const ns3::MobilityModel> mobility) {
  if(VERBOSE_FOR_DEBUG > 0)
    std::cout << "\t[StaCourseChange] context: " << context << std::endl;

  // get the position and velocity of the main STA
  Vector pos = mobility->GetPosition ();
  Vector vel = mobility->GetVelocity ();
  if(staRecordVerboseLevel >= 1)
    std::cout << Simulator::Now ().GetSeconds()
              << "\t[StaCourseChange] MOBILITY CHANGE"
              << ". STA#" << staid
              //<< ". string: " << context
              << ", POS: x=" << pos.x 
              << ", y=" << pos.y
              << ", z=" << pos.z 
              << "; VEL:" << vel.x 
              << ", y=" << vel.y
              << ", z=" << vel.z
              //<< ". staRecordNumberWiFiCards: " << staRecordNumberWiFiCards
              << std::endl;

  // check if this is a primary STA
  if (primarySTA == true) {
    // this is a primary STA
    if (peerStaid != 0) {
      // a peer STA exists

      // I have to move the associated secondary STA accordingly

      // get a pointer to the associated STA
      Ptr<Node> myAssociatedSTA;
      for (NodeList::Iterator i = NodeList::Begin(); i != NodeList::End(); ++i) {
        uint32_t identif = (*i)->GetId();
        if ( identif == peerStaid) {
          myAssociatedSTA = (*i);
          if (VERBOSE_FOR_DEBUG > 0)
            std::cout << Simulator::Now().GetSeconds()
                      << "\t[StaCourseChange] Index of the associated STA: " << identif
                      << std::endl;
        }
      }
      // set the position and velocity of the STA (they become the ones of the main STA)
      Ptr<ConstantVelocityMobilityModel> poninterToMobilityModelSecondary = myAssociatedSTA->GetObject<ConstantVelocityMobilityModel>();
      poninterToMobilityModelSecondary->SetPosition(pos);
      poninterToMobilityModelSecondary->SetVelocity(vel);

      Ptr<MobilityModel> poninterToMobilityModel = myAssociatedSTA->GetObject<MobilityModel> ();
      Vector posSecondary = poninterToMobilityModel->GetPosition ();
      Vector velSecondary = poninterToMobilityModel->GetVelocity ();

      if(staRecordVerboseLevel >= 1)
        std::cout << Simulator::Now ().GetSeconds()
                  << "\t[StaCourseChange] CHANGING secondary STA#" << peerStaid
                  //<< ". string: " << context
                  << ", POS: x=" << posSecondary.x 
                  << ", y=" << posSecondary.y
                  << ", z=" << posSecondary.z 
                  << "; VEL:" << velSecondary.x 
                  << ", y=" << velSecondary.y
                  << ", z=" << velSecondary.z
                  << std::endl; 

      if(VERBOSE_FOR_DEBUG >= 1)
        PrintAllVariables();      
    }
  }  
}

// returns the value of 'disabledPermanently' of a STA
bool
GetstaRecordDisabledPermanently (uint16_t thisSTAid, uint32_t myverbose)
{
  for (STA_recordVector::const_iterator index = sta_vector.begin (); index != sta_vector.end (); index++) {

    if ( (*index)->GetStaid () == thisSTAid ) {
      if ( myverbose > 0 )
        std::cout << Simulator::Now ().GetSeconds() 
                  << "\t[GetstaRecordDisabledPermanently]\tSTA #" << thisSTAid 
                  << " has 'disabledPermanently': " << (*index)->GetDisabledPermanently() 
                  << std::endl;

      return (*index)->GetDisabledPermanently();
    }
  }
  // if I am here, I have not found the requested STA
  NS_ASSERT(false);
  return false;
}

// get a pointer to a STA with an ID
Ptr<Node> GetPointerToSTA(uint32_t staId)
{
  
  Ptr<Node> mySTA;
  for (NodeList::Iterator i = NodeList::Begin(); i != NodeList::End(); ++i) {
    if ( (*i)->GetId() == staId) {
      mySTA = (*i);
    }
  }
  return mySTA;
}

// returns the channel of a STA
// WARNING: it does NOT work properly
uint8_t GetChannelOfaSTA (uint32_t staId, uint32_t mywifiModel, uint32_t myverbose) {

  Ptr<Node> mySTA = GetPointerToSTA(staId);

  NetDeviceContainer thisDevice;


  thisDevice.Add( (mySTA)->GetDevice(1) ); // this adds the device to the NetDeviceContainer. It has to be device 1, not device 0. I don't know why

  return GetChannelOfaDevice ( thisDevice, mywifiModel, myverbose );
}

// This is called with a callback every time a STA is associated to an AP
void
STA_record::SetAssoc (std::string context, Mac48Address AP_MAC_address)
{
  // 'context' is something like "/NodeList/9/DeviceList/1/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/$ns3::StaWifiMac/Assoc"
  if(VERBOSE_FOR_DEBUG > 0)
    std::cout << "\t[SetAssoc] context: " << context << std::endl;

  if(staRecordVerboseLevel >= 1) {
    std::cout << "\n";
    std::cout << Simulator::Now ().GetSeconds()    
              << "\t[SetAssoc] STA #" << staid << " has associated to the AP with MAC " << AP_MAC_address
              << ". Performing required actions"
              << std::endl;
  }
  
  if(VERBOSE_FOR_DEBUG >=1)
    PrintAllVariables();

  // update the data in the STA_record structure
  assoc = true;
  apMac = AP_MAC_address;

  // I have this info available in the STA record:
  //  staid
  //  typeofapplication
  //  staRecordMaxSizeAmpdu

  // auxiliar string
  std::ostringstream auxString;
  // create a string with the MAC
  auxString << "02-06-" << AP_MAC_address;
  std::string myaddress = auxString.str();

  NS_ASSERT(!myaddress.empty());

  if (VERBOSE_FOR_DEBUG > 0)
    std::cout << Simulator::Now().GetSeconds()
              << "\t[SetAssoc] Calling GetAnAP_Id()"
              << std::endl;

  uint32_t apId = GetAnAP_Id(myaddress);

  uint8_t apChannel = GetAP_WirelessChannel ( apId, 0 /*staRecordVerboseLevel*/ );

  if (staRecordVerboseLevel >= 1)
    std::cout << Simulator::Now ().GetSeconds() 
              << "\t[SetAssoc] STA #" << staid 
              << "\twith AMPDU size " << staRecordMaxSizeAmpdu 
              << "\trunning application " << typeofapplication 
              << "\thas associated to AP #" << apId
              << " with MAC " << apMac  
              << " with channel " <<  uint16_t (apChannel)
              << "" << std::endl;

  if (staRecordVerboseLevel >= 1)
    std::cout << Simulator::Now ().GetSeconds() 
              << "\t[SetAssoc]  The STA has a WiFi interface 802." << staRecordversion80211
              << std::endl;

  // This part only runs if the aggregation algorithm is activated
  if (staRecordaggregationDisableAlgorithm == 1) {
    // check if the STA associated to the AP is running VoIP. In this case, I have to disable aggregation:
    // - in the AP
    // - in all the associated STAs
    if ( typeofapplication == 1 || typeofapplication == 2 ) {

      // disable aggregation in the AP

      // check if the AP is aggregating
      if ( GetAP_MaxSizeAmpdu ( apId, staRecordVerboseLevel ) > 0 ) {

        // I modify the A-MPDU of this AP
        ModifyAmpdu ( apId, staRecordmaxAmpduSizeWhenAggregationLimited, 1 );

        // Modify the data in the table of APs
        //for (AP_recordVector::const_iterator index = AP_vector.begin (); index != AP_vector.end (); index++) {
          //if ( (*index)->GetMac () == myaddress ) {
            Modify_AP_Record ( apId, myaddress, staRecordmaxAmpduSizeWhenAggregationLimited);
            //std::cout << Simulator::Now ().GetSeconds() << "\t[GetAnAP_Id] AP #" << (*index)->GetApid() << " has MAC: " << (*index)->GetMac() << "" << std::endl;
        //  }
        //}

        if (staRecordVerboseLevel > 0)
          std::cout << Simulator::Now ().GetSeconds() 
                    << "\t[SetAssoc] Aggregation in AP #" << apId 
                    << "\twith MAC: " << myaddress 
                    << "\tset to " << staRecordmaxAmpduSizeWhenAggregationLimited 
                    << "\t(limited)" << std::endl;

        // disable aggregation in all the STAs associated to that AP
        for (STA_recordVector::const_iterator index = sta_vector.begin (); index != sta_vector.end (); index++) {

          // I only have to disable aggregation for TCP STAs
          if ((*index)->Gettypeofapplication () > 2) {

            // if the STA is associated
            if ((*index)->GetAssoc ()) {

              // if the STA is associated to this AP
              if ((*index)->GetMacOfitsAP() == AP_MAC_address ) {

                ModifyAmpdu ((*index)->GetStaid(), staRecordmaxAmpduSizeWhenAggregationLimited, 1);   // modify the AMPDU in the STA node
                (*index)->SetMaxSizeAmpdu(staRecordmaxAmpduSizeWhenAggregationLimited);               // update the data in the STA_record structure

                if (staRecordVerboseLevel > 0)
                  std::cout << Simulator::Now ().GetSeconds() 
                            << "\t[SetAssoc] Aggregation in STA #" << (*index)->GetStaid() 
                            << ", associated to AP #" << apId
                            << "\twith MAC " << (*index)->GetMacOfitsAP() 
                            << "\tset to " << staRecordmaxAmpduSizeWhenAggregationLimited 
                            << "\t(limited)" << std::endl;
              }
            }
          }
        }
      }
    }

    // If this associated STA is using TCP
    else {
      // If the new AP is not aggregating
      if ( GetAP_MaxSizeAmpdu ( apId, staRecordVerboseLevel ) == 0) {

        // Disable aggregation in this STA
        ModifyAmpdu (staid, staRecordmaxAmpduSizeWhenAggregationLimited, 1);  // modify the AMPDU in the STA node
        staRecordMaxSizeAmpdu = staRecordmaxAmpduSizeWhenAggregationLimited;        // update the data in the STA_record structure

        if (staRecordVerboseLevel > 0)
          std::cout << Simulator::Now ().GetSeconds() 
                    << "\t[SetAssoc] Aggregation in STA #" << staid 
                    << ", associated to AP #" << apId 
                    << "\twith MAC " << apMac
                    << "\tset to " << staRecordMaxSizeAmpdu 
                    << "\t(limited)" << std::endl;

  /*    for (STA_recordVector::const_iterator index = sta_vector.begin (); index != sta_vector.end (); index++) {

          if ( (*index)->GetMac () == AP_MAC_address ) {
              ModifyAmpdu ((*index)->GetStaid(), 0, 1);  // modify the AMPDU in the STA node
              (*index)->SetMaxSizeAmpdu(0);// update the data in the STA_record structure

              if (staRecordVerboseLevel > 0)
                std::cout << Simulator::Now ().GetSeconds() 
                          << "\t[SetAssoc] Aggregation in STA #" << (*index)->GetStaid() 
                          << ", associated to AP #" << GetAnAP_Id(myaddress) 
                          << "\twith MAC " << (*index)->GetMac() 
                          << "\tset to " << 0 
                          << "\t(limited)" << std::endl;
          }
        }*/
      }

      // If the new AP is aggregating, I have to enable aggregation in this STA
      else {
        // Enable aggregation in this STA
        ModifyAmpdu (staid, staRecordMaxAmpduSize, 1);  // modify the AMPDU in the STA node
        staRecordMaxSizeAmpdu = staRecordMaxAmpduSize;        // update the data in the STA_record structure

        if (staRecordVerboseLevel > 0)
          std::cout << Simulator::Now ().GetSeconds() 
                    << "\t[SetAssoc] Aggregation in STA #" << staid 
                    << ", associated to AP #" << apId 
                    << "\twith MAC " << apMac
                    << "\tset to " << staRecordMaxSizeAmpdu 
                    << "(enabled)" << std::endl;
        /*// Enable aggregation in the STA
          for (STA_recordVector::const_iterator index = sta_vector.begin (); index != sta_vector.end (); index++) {
            if ( (*index)->GetMac () == AP_MAC_address ) {
              ModifyAmpdu ((*index)->GetStaid(), maxAmpduSize, 1);  // modify the AMPDU in the STA node
              (*index)->SetMaxSizeAmpdu(maxAmpduSize);// update the data in the STA_record structure

              if (myverbose > 0)
                std::cout << Simulator::Now ().GetSeconds() 
                          << "\t[SetAssoc] Aggregation in STA #" << (*index)->GetStaid() 
                          << ", associated to AP #" << GetAnAP_Id(myaddress) 
                          << "\twith MAC " << (*index)->GetMac() 
                          << "\tset to " << maxAmpduSize 
                          << "\t(enabled)" << std::endl;
          }
        }*/
      }
    }
  }
  if (staRecordVerboseLevel > 0) {
    List_STA_record ();
    ListAPs (staRecordVerboseLevel);
  }

  /* check if I have to disable the peer STA */
  if (disablePeerSTAWhenAssociated == false) {
    // I don't have to disable the peer STA
    if (staRecordVerboseLevel > 0)
      std::cout << Simulator::Now ().GetSeconds() 
                << "\t[SetAssoc] STA#" << staid 
                << ", associated to AP#" << apId
                << ". 'disablePeerSTAWhenAssociated' is " << disablePeerSTAWhenAssociated
                << ", so I don't have to disable the peer STA#" << peerStaid
                << std::endl; 
  }
  else {
    // I have to disable the peer STA
    if (staRecordVerboseLevel > 0)
      std::cout << Simulator::Now ().GetSeconds() 
                << "\t[SetAssoc] STA#" << staid 
                << ", associated to AP#" << apId
                << ". I have to disable the peer STA#" << peerStaid
                << std::endl;

    // check if the peer STA is permanently disabled
    if (GetstaRecordDisabledPermanently(peerStaid, 0/*staRecordVerboseLevel*/) == true) {
      // the peer STA is permanently disabled. Do nothing
      if (staRecordVerboseLevel > 0)
        std::cout << Simulator::Now ().GetSeconds() 
                  << "\t[SetAssoc] STA#" << staid 
                  //<< ", associated to AP#" << apId
                  << ". The peer STA #" << peerStaid
                  << " is disabled permanently, so I don't have to disable it"
                  << std::endl;       
    }
    else {
      // the peer STA is NOT permanently disabled
      if (staRecordVerboseLevel > 0)
        std::cout << Simulator::Now ().GetSeconds() 
                  << "\t[SetAssoc] STA#" << staid 
                  //<< ", associated to AP#" << apId
                  << ". The peer STA #" << peerStaid
                  << " is NOT disabled permanently, so I have to disable it"
                  << std::endl;    

      // get a pointer to the peer STA
      Ptr<Node> myPeerSTA = GetPointerToSTA(peerStaid);

      // create a device container including the device of the peer STA
      NetDeviceContainer thisDevice;
      thisDevice.Add( (myPeerSTA)->GetDevice(1) ); // this adds the device to the NetDeviceContainer. It has to be device 1, not device 0. I don't know why

      if (AreAllTheseDevicesDisabled (thisDevice, staRecordwifiModel, staRecordVerboseLevel)) {
        // the peer STA is already disabled. Do nothing
        if (staRecordVerboseLevel > 0)
          std::cout << Simulator::Now ().GetSeconds() 
                    << "\t[SetAssoc] STA#" << staid 
                    //<< ", associated to AP#" << apId
                    << ". The peer STA #" << peerStaid
                    << " is already disabled, so I don't have to disable it again"
                    << std::endl;
      }
      else {
        if (staRecordVerboseLevel > 0)
          std::cout << Simulator::Now ().GetSeconds() 
                    << "\t[SetAssoc] STA#" << staid 
                    //<< ", associated to AP#" << apId
                    << ". The peer STA#" << peerStaid
                    << " has to be disabled."
                    //<< " It is currently in channel " << (uint16_t)GetChannelOfaDevice(thisDevice, staRecordwifiModel, staRecordVerboseLevel)
                    << std::endl;

        // disable the network device            
        DisableNetworkDevice (thisDevice, staRecordwifiModel, staRecordVerboseLevel);

        if (staRecordVerboseLevel > 0)
          std::cout << Simulator::Now ().GetSeconds() 
                    << "\t[SetAssoc] STA#" << staid 
                    //<< ", associated to AP#" << apId
                    << ". The peer STA#" << peerStaid
                    << " has been disabled" // << add the status here
                    << std::endl;
      }
    }
  }
  /* end of - check if I have to disable the peer STA */

  if (false) {
    // empty the ARP caches of all the nodes after a new association
    for (NodeList::Iterator i = NodeList::Begin(); i != NodeList::End(); ++i) {
      //uint32_t identif;
      //identif = (*i)->GetId();
      emtpyArpCache ( (*i), 2);
    }
  }

  // after a new association, momentarily modify the ARP timeout to 1 second, and then to 50 seconds again
  if (false) {
    for (NodeList::Iterator i = NodeList::Begin(); i != NodeList::End(); ++i) {
        //uint32_t identif;
        //identif = (*i)->GetId();
        modifyArpParams( (*i), 1.0, staRecordVerboseLevel);
        Simulator::Schedule (Seconds(5.0), &modifyArpParams, (*i), 50.0, staRecordVerboseLevel);
    }
  }
}

// This is called with a callback every time a STA is de-associated from an AP
void
STA_record::UnsetAssoc (std::string context, Mac48Address AP_MAC_address)
{
  // 'context' is something like "/NodeList/9/DeviceList/1/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/$ns3::StaWifiMac/Assoc"

  if (VERBOSE_FOR_DEBUG > 0)
    std::cout << "STARTING UnsetAssoc. context: " << context << "\n";

  // update the data in the STA_record structure
  assoc = false;
  apMac = "00:00:00:00:00:00";
   
  // auxiliar string
  std::ostringstream auxString;
  // create a string with the MAC
  auxString << "02-06-" << AP_MAC_address;
  std::string myaddress = auxString.str();
  NS_ASSERT(!myaddress.empty());

  if (VERBOSE_FOR_DEBUG > 0)
    std::cout << Simulator::Now().GetSeconds()
              << "\n\t[UnsetAssoc] Calling GetAnAP_Id() in order to get the Id of the AP from which the STA has deassociated"
              << std::endl;

  uint32_t apId = GetAnAP_Id(myaddress);

  uint8_t apChannel = GetAP_WirelessChannel ( apId, 0 /*staRecordVerboseLevel*/ );

  if (staRecordVerboseLevel > 0) {
    std::cout << Simulator::Now ().GetSeconds() 
              << "\t[UnsetAssoc] STA #" << staid
              << "\twith AMPDU size " << staRecordMaxSizeAmpdu               
              << "\trunning application " << typeofapplication 
              << "\tde-associated from AP #" << apId
              << " with MAC " << AP_MAC_address 
              << " with channel " << uint16_t (apChannel)
              << ". 802.11 version: " << staRecordversion80211
              << std::endl;              
  }

  // this is the frequency band where the STA can find an AP
  std::string frequencybandsSupportedBySTA = getWirelessBandOfStandard(convertVersionToStandard(staRecordversion80211));

  // This only runs if the aggregation algorithm is running
  if(staRecordaggregationDisableAlgorithm == 1) {

    // check if there is some VoIP STA already associated to the AP. In this case, I have to enable aggregation:
    // - in the AP
    // - in all the associated STAs
    if ( typeofapplication == 1 || typeofapplication == 2 ) {

      // check if the AP is not aggregating
      /*if ( GetAP_MaxSizeAmpdu ( apId, staRecordVerboseLevel ) == 0 ) {
        if (staRecordVerboseLevel > 0)
          std::cout << Simulator::Now ().GetSeconds() 
                    << "\t[UnsetAssoc] This AP is not aggregating" 
                    << std::endl;*/

        // check if there is no STA running VoIP associated
        bool anyStaWithVoIPAssociated = false;

        // Check all the associated STAs, except the one de-associating
        for (STA_recordVector::const_iterator index = sta_vector.begin (); index != sta_vector.end (); index++) {

          // Only consider STAs associated to this AP
          if ( (*index)->GetMacOfitsAP () == AP_MAC_address ) {

            // Only consider VoIP STAs
            if ( ( (*index)->Gettypeofapplication() == 1 ) || ( (*index)->Gettypeofapplication() == 2 ) ) {

              // It cannot be the one being de-associated
              if ( (*index)->GetStaid() != staid)

                anyStaWithVoIPAssociated = true;
            }
          }
        }

        // If there is no remaining STA running VoIP associated
        if ( anyStaWithVoIPAssociated == false ) {
          // enable aggregation in the AP
          // Modify the A-MPDU of this AP
          ModifyAmpdu (apId, staRecordMaxAmpduSize, 1);
          Modify_AP_Record (apId, myaddress, staRecordMaxAmpduSize);

          if (staRecordVerboseLevel > 0)
            std::cout << Simulator::Now ().GetSeconds() 
                      << "\t[UnsetAssoc]\tAggregation in AP #" << apId 
                      << "\twith MAC: " << myaddress 
                      << "\tset to " << staRecordMaxAmpduSize 
                      << "\t(enabled)" << std::endl;

          // enable aggregation in all the STAs associated to that AP
          for (STA_recordVector::const_iterator index = sta_vector.begin (); index != sta_vector.end (); index++) {

            // if the STA is associated
            if ((*index)->GetAssoc()) {

              // if the STA is associated to this AP
              if ( (*index)->GetMacOfitsAP() == AP_MAC_address ) {

                // if the STA is not running VoIP. NOT NEEDED. IF I AM HERE IT MEANS THAT ALL THE STAs ARE TCP
                //if ((*index)->Gettypeofapplication () > 2) {

                  ModifyAmpdu ((*index)->GetStaid(), staRecordMaxAmpduSize, 1);  // modify the AMPDU in the STA node
                  (*index)->SetMaxSizeAmpdu(staRecordMaxAmpduSize);// update the data in the STA_record structure

                  if (staRecordVerboseLevel > 0)  
                    std::cout << Simulator::Now ().GetSeconds() 
                              << "\t[UnsetAssoc] Aggregation in STA #" << (*index)->GetStaid() 
                              << "\tassociated to AP #" << apId 
                              << "\twith MAC " << (*index)->GetMacOfitsAP() 
                              << "\tset to " << staRecordMaxAmpduSize 
                              << "\t(enabled)" << std::endl;
                //}
              }
            }
          }
        }
        else {
          // there is still some VoIP STA associatedm so aggregation cannot be enabled
          if (staRecordVerboseLevel > 0)
            std::cout << Simulator::Now ().GetSeconds() 
                      << "\t[UnsetAssoc] There is still at least a VoIP STA in this AP " << apId 
                      << " so aggregation cannot be enabled" << std::endl;
        }
      //}

    // If the de-associated STA is using TCP
    } else {

      // If the AP is not aggregating
      if ( GetAP_MaxSizeAmpdu ( apId, staRecordVerboseLevel ) == staRecordmaxAmpduSizeWhenAggregationLimited) {

        // Enable aggregation in this STA
        ModifyAmpdu (staid, staRecordMaxAmpduSize, 1);  // modify the AMPDU in the STA node
        staRecordMaxSizeAmpdu = staRecordMaxAmpduSize;  // update the data in the STA_record structure

        if (staRecordVerboseLevel > 0)
          std::cout << Simulator::Now ().GetSeconds() 
                    << "\t[UnsetAssoc] Aggregation in STA #" << staid 
                    << ", de-associated from AP #" << apId
                    << "\twith MAC " << apMac
                    << "\tset to " << staRecordMaxSizeAmpdu 
                    << "\t(enabled)" << std::endl;

      // Enable aggregation in the STA
      /*
      for (STA_recordVector::const_iterator index = sta_vector.begin (); index != sta_vector.end (); index++) {

        // if the STA is associated to this AP
        if ( (*index)->GetMac() == AP_MAC_address ) {

          // if the STA is not running VoIP
          if ((*index)->Gettypeofapplication () > 2) {

            ModifyAmpdu ((*index)->GetStaid(), maxAmpduSize, 1);  // modify the AMPDU in the STA node
            (*index)->SetMaxSizeAmpdu(maxAmpduSize);// update the data in the STA_record structure

            if (staRecordVerboseLevel > 0)
              std::cout << Simulator::Now ().GetSeconds() 
                        << "\t[UnsetAssoc] Aggregation in STA #" << (*index)->GetStaid() 
                        << ", de-associated from AP #" << apId
                        << "\twith MAC " << (*index)->GetMac() 
                        << "\tset to " << maxAmpduSize 
                        << "\t(enabled)" << std::endl; 
          }
        }*/      
      }
    }
  }
  if (staRecordVerboseLevel > 0) {
    List_STA_record ();
    ListAPs (staRecordVerboseLevel);
  }

  
  // If wifiModel==1, I don't need to manually change the channel. It will do it automatically
  // If wifiModel==0, I have to manually set the channel of the STA to that of the nearest AP
  //if (staRecordwifiModel == 0) {  // staRecordwifiModel is the local version of the variable wifiModel
  
    // wifiModel = 0
    // Put the STA in the channel of the nearest AP
    if (staRecordnumOperationalChannels > 1) {
      // Only for wifiModel = 0. With WifiModel = 1 it is supposed to scan for other APs in other channels 
      //if (staRecordwifiModel == 0) {
        // Put all the APs in a nodecontainer
        // and get a pointer to the STA
        Ptr<Node> mySTA;
        NodeContainer APs;
        uint32_t numberAPs = CountAPs (staRecordVerboseLevel);

        for (NodeList::Iterator i = NodeList::Begin(); i != NodeList::End(); ++i) {
          uint32_t identif;
          identif = (*i)->GetId();
       
          if ( (identif >= 0) && (identif < numberAPs) ) {
            APs.Add(*i);
          } else if ( identif == staid) {
            mySTA = (*i);
          }
        }


        // Find the nearest AP (in order to switch the STA to the channel of the nearest AP)
        Ptr<Node> nearest;
        nearest = nearestAp (APs, mySTA, staRecordVerboseLevel, frequencybandsSupportedBySTA);

        // Move this STA to the channel of the AP identified as the nearest one
        NetDeviceContainer thisDevice;
        thisDevice.Add( (mySTA)->GetDevice(1) ); // this adds the device to the NetDeviceContainer. It has to be device 1, not device 0. I don't know why
     
        uint8_t newChannel = GetAP_WirelessChannel ( (nearest)->GetId(), 0 /*staRecordVerboseLevel*/ );

        if (AreAllTheseDevicesDisabled (thisDevice, staRecordwifiModel, staRecordVerboseLevel)) {
          // the peer STA is already disabled. Do nothing
          if (staRecordVerboseLevel > 0)
            std::cout << Simulator::Now ().GetSeconds() 
                      << "\t[UnsetAssoc] STA#" << staid 
                      //<< " de-associated from AP #" << apId
                      << ". The STA #" << peerStaid
                      << " is disabled, so I don't have to change its channel"
                      << std::endl;
        }
        else {
          if ( HANDOFFMETHOD == 0 )
            ChangeFrequencyLocal (thisDevice, newChannel, staRecordwifiModel, staRecordVerboseLevel);

          if (staRecordVerboseLevel > 0)
            std::cout << Simulator::Now ().GetSeconds() 
                      << "\t[UnsetAssoc] STA #" << staid 
                      << " de-associated from AP #" << apId 
                      << ". Channel set to " << uint16_t (newChannel) 
                      << ", i.e. the channel of the nearest AP (AP #" << (nearest)->GetId()
                      << ")" << std::endl << std::endl;          
        }

    }
    else { // numOperationalChannels == 1
      if (staRecordVerboseLevel > 0)
        std::cout << Simulator::Now ().GetSeconds() 
                  << "\t[UnsetAssoc] STA #" << staid 
                  << " de-associated from AP #" << apId 
                  << "\tnot modified because numOperationalChannels=" << staRecordnumOperationalChannels 
                  << "\tchannel is still " << uint16_t (apChannel) 
                  << std::endl << std::endl;
    }
  //} 

  /*
  else {
    // wifiModel = 1
    if (staRecordVerboseLevel > 0)
      std::cout << Simulator::Now ().GetSeconds() 
                  << "\t[UnsetAssoc] STA #" << staid 
                  << " de-associated from AP #" << GetAnAP_Id(myaddress) 
                  << "\tnot modified because wifimodel=" << staRecordwifiModel
                  << "\tchannel is still " << uint16_t (apChannel) 
                  << std::endl << std::endl;
  }*/



  /* check if I have to enable the peer STA */
  // this STA has de-associated. I may need to enable the peer STA
  if (disablePeerSTAWhenAssociated == false) {
    // I don't have to enable the peer STA
    if (staRecordVerboseLevel > 0)
      std::cout << Simulator::Now ().GetSeconds() 
                << "\t[UnsetAssoc] STA#" << staid 
                << ", deassociated from AP#" << apId
                << ". 'disablePeerSTAWhenAssociated' is " << disablePeerSTAWhenAssociated
                << ", so I don't have to enable the peer STA#" << peerStaid
                << std::endl; 
  }
  else {
    // I have to enable the peer STA
    if (staRecordVerboseLevel > 0)
      std::cout << Simulator::Now ().GetSeconds() 
                << "\t[UnsetAssoc] STA#" << staid 
                << ", deassociated from AP#" << apId
                << ". I have to enable the peer STA#" << peerStaid
                << std::endl;

    // check if the peer STA is permanently disabled
    if (GetstaRecordDisabledPermanently(peerStaid, 0/*staRecordVerboseLevel*/) == true) {
      // the peer STA is permanently disabled. Do nothing
      if (staRecordVerboseLevel > 0)
        std::cout << Simulator::Now ().GetSeconds() 
                  << "\t[UnsetAssoc] STA#" << staid 
                  //<< ", deassociated from AP#" << apId
                  << ". The peer STA #" << peerStaid
                  << " is disabled permanently, so I cannot enable it"
                  << std::endl;       
    }
    else {
      // the peer STA is NOT permanently disabled
      if (staRecordVerboseLevel > 0)
        std::cout << Simulator::Now ().GetSeconds() 
                  << "\t[UnsetAssoc] STA#" << staid 
                  //<< ", deassociated from AP#" << apId
                  << ". The peer STA #" << peerStaid
                  << " is NOT permanently disabled, so I have to enable it"
                  << std::endl;    

      // get a pointer to the peer STA
      Ptr<Node> myPeerSTA = GetPointerToSTA(peerStaid);
      uint8_t channelPeerSTA;

      // create a device container including the device of the peer STA
      NetDeviceContainer thisDevice;
      thisDevice.Add( (myPeerSTA)->GetDevice(1) ); // this adds the device to the NetDeviceContainer. It has to be device 1, not device 0. I don't know why

      if (AreAllTheseDevicesDisabled (thisDevice, staRecordwifiModel, staRecordVerboseLevel) == false) {
        // the peer STA is enabled. Do nothing
        if (staRecordVerboseLevel > 0)
          std::cout << Simulator::Now ().GetSeconds() 
                    << "\t[UnsetAssoc] STA#" << staid 
                    //<< ", deassociated from AP#" << apId
                    << ". The peer STA #" << peerStaid
                    << " is already enabled"
                    << ", so I don't have to enable it again"
                    << std::endl;
      }
      else {
        if (staRecordVerboseLevel > 0)
          std::cout << Simulator::Now ().GetSeconds() 
                    << "\t[UnsetAssoc] STA#" << staid 
                    //<< ", deassociated from AP#" << apId
                    << ". The peer STA#" << peerStaid
                    << " has to be enabled."
                    //<< " It is currently in channel " << (uint16_t)GetChannelOfaDevice(thisDevice, staRecordwifiModel, staRecordVerboseLevel)
                    << std::endl;

        // enable the network device
        EnableNetworkDevice (thisDevice, staRecordwifiModel, staRecordVerboseLevel);
        
        // get the channel number
        channelPeerSTA = ChannelNetworkDevice (thisDevice, staRecordwifiModel, staRecordVerboseLevel);

        if (staRecordVerboseLevel > 0)
          std::cout << Simulator::Now ().GetSeconds() 
                    << "\t[UnsetAssoc] STA#" << staid 
                    //<< ", associated to AP#" << apId
                    << ". The peer STA#" << peerStaid
                    << " has been enabled" // << add the status here
                    << ". It is in channel " << (uint16_t)channelPeerSTA
                    << std::endl;
      }


      // I DON'T KNOW IF THIS IS NEEDED. TO PUT THE STA AGAIN IN THE CHANNEL OF THE NEAREST AP
      // put the STA in the channel of the nearest AP
      // wifiModel = 0
      // Put the STA in the channel of the nearest AP
      if (staRecordnumOperationalChannels > 1) {
        // Only for wifiModel = 0. With WifiModel = 1 it is supposed to scan for other APs in other channels 

        // Put all the APs in a nodecontainer
        NodeContainer APs;
        uint32_t numberAPs = CountAPs (staRecordVerboseLevel);
        for (NodeList::Iterator i = NodeList::Begin(); i != NodeList::End(); ++i) {
          uint32_t identif;
          identif = (*i)->GetId();
          if ( (identif >= 0) && (identif < numberAPs) ) {
            APs.Add(*i);
          }
        }

        std::string bandPeerSTA = getWirelessBandOfChannel(channelPeerSTA);

        // Find the nearest AP (in order to switch the STA to the channel of the nearest AP)
        Ptr<Node> nearest;
        nearest = nearestAp (APs, myPeerSTA, staRecordVerboseLevel, bandPeerSTA);

        // Move this STA to the channel of the AP identified as the nearest one
        NetDeviceContainer thisDevice;
        thisDevice.Add( (myPeerSTA)->GetDevice(1) ); // this adds the device to the NetDeviceContainer. It has to be device 1, not device 0. I don't know why
     
        uint8_t newChannel = GetAP_WirelessChannel ( (nearest)->GetId(), 0);

        if (AreAllTheseDevicesDisabled (thisDevice, staRecordwifiModel, staRecordVerboseLevel)) {
          // the peer STA is already disabled. Do nothing
          if (staRecordVerboseLevel > 0)
            std::cout << Simulator::Now ().GetSeconds() 
                      << "\t[UnsetAssoc] STA#" << peerStaid 
                      //<< " de-associated from AP #" << apId
                      << ". The STA #" << peerStaid
                      << " is disabled, so I don't have to change its channel"
                      << std::endl;
        }
        else {
          if ( HANDOFFMETHOD == 0 )
            ChangeFrequencyLocal (thisDevice, newChannel, staRecordwifiModel, staRecordVerboseLevel);

          if (staRecordVerboseLevel > 0)
            std::cout << Simulator::Now ().GetSeconds() 
                      << "\t[UnsetAssoc] STA #" << peerStaid
                      << ". Channel set to " << uint16_t (newChannel) 
                      << ", i.e. the channel of the nearest AP (AP #" << (nearest)->GetId()
                      << ")" << std::endl << std::endl;          
        }

      }
      else { // numOperationalChannels == 1
        if (staRecordVerboseLevel > 0)
          std::cout << Simulator::Now ().GetSeconds() 
                    << "\t[UnsetAssoc] STA #" << peerStaid 
                    << " de-associated from AP #" << apId 
                    << "\tnot modified because numOperationalChannels=" << staRecordnumOperationalChannels 
                    << "\tchannel is still " << uint16_t (apChannel) 
                    << std::endl << std::endl;
      }
    }
  }
  /* end of - check if I have to enable the peer STA */

  if (VERBOSE_FOR_DEBUG > 0)
    std::cout << "FINISHING UnsetAssoc\n";
}

void
STA_record::Settypeofapplication (uint32_t applicationid)
{
  typeofapplication = applicationid;
}

void
STA_record::SetMaxSizeAmpdu (uint32_t MaxSizeAmpdu)
{
  staRecordMaxSizeAmpdu = MaxSizeAmpdu;
}

void
STA_record::SetVerboseLevel (uint32_t myVerboseLevel)
{
  staRecordVerboseLevel = myVerboseLevel;
}

void
STA_record::SetnumOperationalChannels (uint32_t mynumOperationalChannels)
{
  staRecordnumOperationalChannels = mynumOperationalChannels;
}

void
STA_record::SetpeerStaid (uint16_t id)
{
  peerStaid = id;
}

void
STA_record::SetDisabledPermanently (bool disabled)
{
  disabledPermanently = disabled;
}

void
STA_record::SetDisablePeerSTAWhenAssociated (bool disable)
{
  disablePeerSTAWhenAssociated = disable;
}

void
STA_record::SetPrimarySTA (bool primary)
{
  primarySTA = primary;
}

void
STA_record::Setversion80211 (std::string myversion80211)
{
  staRecordversion80211 = myversion80211;
}

void
STA_record::SetaggregationDisableAlgorithm (uint16_t myaggregationDisableAlgorithm)
{
  staRecordaggregationDisableAlgorithm = myaggregationDisableAlgorithm;
}

void
STA_record::SetAmpduSize (uint32_t myAmpduSize)
{
  staRecordMaxAmpduSize = myAmpduSize;
}

void
STA_record::SetmaxAmpduSizeWhenAggregationLimited (uint32_t mymaxAmpduSizeWhenAggregationLimited)
{
  staRecordmaxAmpduSizeWhenAggregationLimited = mymaxAmpduSizeWhenAggregationLimited;
}

void
STA_record::SetWifiModel (uint32_t mywifiModel)
{
  staRecordwifiModel = mywifiModel;
}

bool
STA_record::GetAssoc ()
// returns true or false depending whether the STA is associated or not
{
  return assoc;
}

uint16_t
STA_record::GetStaid ()
// returns the id of the Sta
{
  return staid;
}

uint16_t
STA_record::GetpeerStaid ()
// returns the id of the associated Sta (secondary)
{
  return peerStaid;
}

Mac48Address
STA_record::GetMacOfitsAP ()
// returns the MAC address of the AP where the STA is associated
{
  return apMac;
}

uint32_t
STA_record::Gettypeofapplication ()
// returns the id of the STA
{
  return typeofapplication;
}

uint32_t
STA_record::GetMaxSizeAmpdu ()
// returns the id of the Sta
{
  return staRecordMaxSizeAmpdu;
}

bool
STA_record::GetDisabledPermanently ()
{
  return disabledPermanently;
}

uint32_t
STA_record::GetWifiModel ()
{
  return staRecordwifiModel;
}

void
STA_record::PrintAllVariables ()
{
  std::cout << Simulator::Now ().GetSeconds()
            << "\t[PrintAllVariables] These are all the member variables of the STA:\n"
            << "asoc: " << (int)assoc << '\n'
            << "staid: " << staid << '\n'
            << "apMac: " << apMac << '\n'
            << "typeofapplication: " << typeofapplication << '\n'
            << "staRecordMaxSizeAmpdu: " << staRecordMaxSizeAmpdu << '\n'
            << "staRecordVerboseLevel: " << staRecordVerboseLevel << '\n'
            << "staRecordnumOperationalChannels: " << staRecordnumOperationalChannels << '\n'
            << "disabledPermanently: " << disabledPermanently << '\n'
            << "disablePeerSTAWhenAssociated: " << disablePeerSTAWhenAssociated << '\n'
            << "primarySTA: " << primarySTA << '\n'
            << "peerStaid: " << peerStaid << '\n'
            << "staRecordversion80211: " << staRecordversion80211 << '\n'
            << "staRecordaggregationDisableAlgorithm: " << staRecordaggregationDisableAlgorithm << '\n'
            << "staRecordMaxAmpduSize: " << staRecordMaxAmpduSize << '\n'
            << "staRecordmaxAmpduSizeWhenAggregationLimited: " << staRecordmaxAmpduSizeWhenAggregationLimited << '\n'
            << "staRecordwifiModel: " << staRecordwifiModel << '\n'
            << std::endl;
}

uint32_t
Get_STA_record_num_AP_app (Mac48Address apMac, uint32_t typeofapplication)
// counts the number or STAs associated to an AP, with a type of application
{
  uint32_t AssocNum = 0;
  for (STA_recordVector::const_iterator index = sta_vector.begin (); index != sta_vector.end (); index++) {
    if (((*index)->GetAssoc ()) && ((*index)->GetMacOfitsAP() == apMac) && ( (*index)->Gettypeofapplication()==typeofapplication) ) {
      AssocNum++;
    }
  }
  return AssocNum;
}


/* I don't need this function
uint32_t
GetstaRecordMaxSizeAmpdu (uint16_t thisSTAid, uint32_t myverbose)
// returns the max size of the Ampdu of an AP
{
  uint32_t staRecordMaxSizeAmpdu = 0;
  //std::cout << Simulator::Now ().GetSeconds() << " *** Number of STA associated: " << Get_STA_record_num() << " *****" << std::endl;

  for (STA_recordVector::const_iterator index = sta_vector.begin (); index != sta_vector.end (); index++) {
  //std::cout << Simulator::Now ().GetSeconds() << " ********************** AP with ID " << (*index)->GetApid() << " has MAC: " << (*index)->GetMacOfitsAP() << " *****" << std::endl;

    if ( (*index)->GetStaid () == thisSTAid ) {
      staRecordMaxSizeAmpdu = (*index)->GetMaxSizeAmpdu ();
      if ( myverbose > 0 )
        std::cout << Simulator::Now ().GetSeconds() 
                  << "\t[GetstaRecordMaxSizeAmpdu]\tAP #" << (*index)->GetMacOfitsAP() 
                  << " has AMDPU: " << (*index)->GetMaxSizeAmpdu() 
                  << "" << std::endl;
    }
  }
  return staRecordMaxSizeAmpdu;
}
*/

// Save the position of a STA in a file (to be performed periodically)
static void
SavePositionSTA (double period, Ptr<Node> node, NodeContainer myApNodes, uint16_t portNumber, std::string fileName)
{
  // print the results to a file (they are written at the end of the file)
  if ( fileName != "" ) {

    std::ofstream ofs;
    ofs.open ( fileName, std::ofstream::out | std::ofstream::app); // with "trunc" Any contents that existed in the file before it is open are discarded. with "app", all output operations happen at the end of the file, appending to its existing contents

    // Find the position of the STA
    Vector posSTA = GetPosition (node);

    // Find the nearest AP
    Ptr<Node> myNearestAP;
    if (VERBOSE_FOR_DEBUG > 0)
      std::cout << Simulator::Now ().GetSeconds()
                << "\t[SavePositionSTA] Looking for the nearest AP of node "
                << node->GetId()
                << std::endl;

    myNearestAP = nearestAp (myApNodes, node, 0, "both");
    if (VERBOSE_FOR_DEBUG > 0)
      std::cout << Simulator::Now ().GetSeconds()
                << "\t[SavePositionSTA] the nearest AP has id "
                << myNearestAP->GetId () << std::endl;

    Vector posMyNearestAP = GetPosition (myNearestAP);
    double distanceToNearestAP = sqrt ( ( (posSTA.x - posMyNearestAP.x)*(posSTA.x - posMyNearestAP.x) ) + ( (posSTA.y - posMyNearestAP.y)*(posSTA.y - posMyNearestAP.y) ) );

    std::string MACaddressAP;

    // find the AP to which the STA is associated
    for (STA_recordVector::const_iterator index = sta_vector.begin (); index != sta_vector.end (); index++) {

      if ((*index)->GetStaid () == node->GetId()) {
        // if the STA is associated, find the MAC of the AP
        if ((*index)->GetAssoc ()) {
          // auxiliar string
          std::ostringstream auxString;
          // create a string with the MAC
          auxString << "02-06-" << (*index)->GetMacOfitsAP();
          MACaddressAP = auxString.str();
          if (VERBOSE_FOR_DEBUG > 0)
            std::cout << Simulator::Now ().GetSeconds() << "\t[SavePositionSTA] STA with id " << (*index)->GetStaid () << " is associated to the AP with MAC " << MACaddressAP << std::endl;
        }
        else {
          if (VERBOSE_FOR_DEBUG > 0)
            std::cout << Simulator::Now ().GetSeconds() << "\t[SavePositionSTA] STA with id " << (*index)->GetStaid () << " is not associated to any AP" << std::endl;
        }
      }
    }

    if (!(MACaddressAP.empty())) {
      // the STA is associated to an AP

      // Find the position and distance of the AP where this STA is associated
      Ptr<Node> myAP;

      NS_ASSERT(!(MACaddressAP.empty()));
      myAP = myApNodes.Get (GetAnAP_Id(MACaddressAP));

      Vector posMyAP = GetPosition (myAP);
      double distanceToMyAP = sqrt ( ( (posSTA.x - posMyAP.x)*(posSTA.x - posMyAP.x) ) + ( (posSTA.y - posMyAP.y)*(posSTA.y - posMyAP.y) ) );

      // print a line in the output file
      ofs << Simulator::Now().GetSeconds() << "\t"
          << (node)->GetId() << "\t"
          << portNumber << "\t"
          << posSTA.x << "\t"
          << posSTA.y << "\t"
          << (myNearestAP)->GetId() << "\t"
          << posMyNearestAP.x << "\t"
          << posMyNearestAP.y << "\t"
          << distanceToNearestAP << "\t"
          << GetAnAP_Id(MACaddressAP) << "\t"
          << posMyAP.x << "\t"
          << posMyAP.y << "\t"
          << distanceToMyAP << "\t"
          << std::endl;
    }
    else {
      // the STA is NOT associated to any AP
      // print a line in the output file
      ofs << Simulator::Now().GetSeconds() << "\t"
          << (node)->GetId() << "\t"
          << portNumber << "\t"
          << posSTA.x << "\t"
          << posSTA.y << "\t"
          << (myNearestAP)->GetId() << "\t"
          << posMyNearestAP.x << "\t"
          << posMyNearestAP.y << "\t"
          << distanceToNearestAP << "\t"
          << "" << "\t"   // as it is not associated, leave this blank
          << "" << "\t"   // as it is not associated, leave this blank
          << "" << "\t"   // as it is not associated, leave this blank
          << "" << "\t"   // as it is not associated, leave this blank
          << std::endl;
    }
    
    // re-schedule
    Simulator::Schedule (Seconds (period), &SavePositionSTA, period, node, myApNodes, portNumber, fileName);
  }
}


// Print the position of a node
// taken from https://www.nsnam.org/doxygen/wifi-ap_8cc.html
static void
ReportPosition (double period, Ptr<Node> node, int i, int type, int myverbose, NodeContainer myApNodes)
{
  Vector posSTA = GetPosition (node);

  if (myverbose > 2) {
    // type = 0 means it will write the position of an AP
    if (type == 0) {
      std::cout << Simulator::Now().GetSeconds()
                << "\t[ReportPosition] AP  #" << i 
                <<  " Position: "  << posSTA.x 
                << "," << posSTA.y 
                << std::endl;
    // type = 1 means it will write the position of an STA
    }
    else {
      // Find the nearest AP
      Ptr<Node> myNearestAP;
      myNearestAP = nearestAp (myApNodes, node, myverbose, "both");
      Vector posMyNearestAP = GetPosition (myNearestAP);
      double distance = sqrt ( ( (posSTA.x - posMyNearestAP.x)*(posSTA.x - posMyNearestAP.x) ) + ( (posSTA.y - posMyNearestAP.y)*(posSTA.y - posMyNearestAP.y) ) );

      std::cout << Simulator::Now().GetSeconds()
                << "\t[ReportPosition] STA #" << i 
                <<  " Position: "  << posSTA.x
                << "," << posSTA.y 
                << ". The nearest AP is AP#" << (myNearestAP)->GetId()
                << ". Distance: " << distance << " m"
                << std::endl;
    }
  }

  // re-schedule
  Simulator::Schedule (Seconds (period), &ReportPosition, period, node, i, type, myverbose, myApNodes);
}


struct coverages {
  double coverage_24GHz;
  double coverage_5GHz;
};
// performs a load balancing: if there is
// - a dual STA connected to a non-dual AP
// and
// - a non-dual STA connected to a dual AP
//
// it exchanges them if possible (if they are under coverage)
void algorithmLoadBalancing ( double period,
                              NodeContainer apNodes,
                              NodeContainer staNodes,
                              NodeContainer routerNode,
                              coverages coverage,
                              //double coverage_24GHz,
                              //double coverage_5GHz,
                              uint32_t myverbose )
{
  uint16_t numberAPpairs = apNodes.GetN() / 2; // number of pairs of APs
  uint16_t numberSTApairs = staNodes.GetN() / 2; // number of pairs of APs

  struct infoAboutEachSTApair {
    uint16_t STAid;
    uint16_t peerSTAid;
    bool STAenabled;
    bool peerSTAenabled;
    uint8_t channelSTA;
    uint8_t channelPeerSTA;
    Ptr<Node> pointerToMainSTA;
    Ptr<Node> pointerToPeerSTA;
    uint16_t APiDwhereThisSTAisAssociated;
    uint16_t APiDwhereThePeerSTAisAssociated;
    bool STAassociated;
    bool peerSTAassociated;
  };
  infoAboutEachSTApair infoSTAs[numberSTApairs];

  // this expresses if a STA pair is under coverage of an AP pair
  // each element can be "2.4 GHz", "5 GHz" or "both"
  //std::string coverageAPinfo[numberSTApairs][numberAPpairs];
  char coverageAPinfo[numberSTApairs][numberAPpairs];

  //myverbose = 2;  // FIXME: remove this

  if (VERBOSE_FOR_DEBUG >= 1)
    std::cout << Simulator::Now ().GetSeconds()
            << "\t[algorithmLoadBalancing] Load balancing running"
            << std::endl;


  /******* fill the variables with information about STAs ***********/
  // go through the STA vector and fill 'infoSTAs' and 'coverageAPinfo'
  // I run the 'for' a number of times: 'STApairIndex'
  STA_recordVector::const_iterator index = sta_vector.begin ();
  for (uint32_t STApairIndex = 0; STApairIndex < numberSTApairs; STApairIndex++) {

    // make sure I have not arrived to the end of the vector
    NS_ASSERT(index != sta_vector.end ());

    infoSTAs[STApairIndex].STAid = (*index)->GetStaid();

    // Id of the AP where this STA is associated
    infoSTAs[STApairIndex].APiDwhereThisSTAisAssociated = 0;
    infoSTAs[STApairIndex].STAassociated = false;
    infoSTAs[STApairIndex].peerSTAassociated = false;

    std::string APaddressWhereThisSTAisAssociated;
    // auxiliar string
    std::ostringstream auxString;
    // create a string with the MAC
    auxString << "02-06-" << (*index)->GetMacOfitsAP();
    APaddressWhereThisSTAisAssociated = auxString.str();

    // if the STA is associated, find the corresponding AP
    if (APaddressWhereThisSTAisAssociated != "02-06-00:00:00:00:00:00") {
      infoSTAs[STApairIndex].APiDwhereThisSTAisAssociated = GetAnAP_Id (APaddressWhereThisSTAisAssociated);
      infoSTAs[STApairIndex].STAassociated = true;
    }

    if ((*index)->GetDisabledPermanently()) {
      // the STA is disabled
      infoSTAs[STApairIndex].STAenabled = false;
      // in this case, the channel is 0
      infoSTAs[STApairIndex].channelSTA = 0;

      if (myverbose >=2)
        std::cout << Simulator::Now().GetSeconds()
                  << "\t[algorithmLoadBalancing] STA #" << infoSTAs[STApairIndex].STAid 
                  <<  " is disabled permanently" 
                  << std::endl;      
    }
    else {
      infoSTAs[STApairIndex].STAenabled = true;
      // get the channel of the STA
      infoSTAs[STApairIndex].channelSTA = GetChannelOfAnAssociatedSTA(infoSTAs[STApairIndex].STAid);

      // the STA is not disabled permanently
      if (VERBOSE_FOR_DEBUG >= 1)
        std::cout << Simulator::Now().GetSeconds()
                  << "\t[algorithmLoadBalancing] STA #" << infoSTAs[STApairIndex].STAid 
                  <<  " is not disabled permanently" 
                  << std::endl;
    }

    // find the peer STA
    infoSTAs[STApairIndex].peerSTAid = (*index)->GetpeerStaid();

    if (infoSTAs[STApairIndex].peerSTAid == 0) {
      if (myverbose >=2)
        std::cout << Simulator::Now().GetSeconds()
                  << "\t[algorithmLoadBalancing] ERROR. STA #" << infoSTAs[STApairIndex].STAid 
                  <<  " does not have a peer STA" 
                  << std::endl;

      NS_ASSERT(false); // this should NOT happen
    }
    else if (infoSTAs[STApairIndex].peerSTAid < infoSTAs[STApairIndex].STAid) {
    //else if (STApairIndex <= numberAPpairs) {
      // I am checking the pairs of STAs, but if I am here it means that I
      //have already checked this pair (in reverse order)
      if (myverbose >=2)
        std::cout << Simulator::Now().GetSeconds()
                  << "\t[algorithmLoadBalancing] I have already checked the pair STA (#" << infoSTAs[STApairIndex].peerSTAid 
                  << ", #" << infoSTAs[STApairIndex].STAid
                  << ")"
                  << std::endl;
    }
    else {
      // a peer STA does exist
      if (myverbose >=2)
        std::cout << Simulator::Now().GetSeconds()
                  << "\t[algorithmLoadBalancing] STA #" << infoSTAs[STApairIndex].STAid 
                  <<  " peer STA is STA #" << infoSTAs[STApairIndex].peerSTAid
                  << std::endl;

      // store the pointer to the main STA
      infoSTAs[STApairIndex].pointerToMainSTA = staNodes.Get(STApairIndex);
      // store the pointer to the peer STA
      infoSTAs[STApairIndex].pointerToPeerSTA = staNodes.Get(STApairIndex + numberSTApairs);

      if (GetstaRecordDisabledPermanently(infoSTAs[STApairIndex].peerSTAid, 0 /*myverbose*/)) {
        // the peer STA is disabled permanently
        infoSTAs[STApairIndex].peerSTAenabled = false;
        // in this case, the channel is 0
        infoSTAs[STApairIndex].channelPeerSTA = 0;
      
        if (myverbose >=2)
          std::cout << Simulator::Now().GetSeconds()
                    << "\t[algorithmLoadBalancing] STA #" << infoSTAs[STApairIndex].STAid 
                    << "' peer STA (STA # " << infoSTAs[STApairIndex].peerSTAid
                    << ") is disabled permanently"
                    << std::endl;          
      }
      else {
        // the peer STA is NOT disabled permanently
        infoSTAs[STApairIndex].peerSTAenabled = true;
        // get the channel of the peer STA
        infoSTAs[STApairIndex].channelPeerSTA = GetChannelOfAnAssociatedSTA(infoSTAs[STApairIndex].peerSTAid);

        if (myverbose >=2)
          std::cout << Simulator::Now().GetSeconds()
                    << "\t[algorithmLoadBalancing] STA (#" << infoSTAs[STApairIndex].STAid 
                    << ", #" << infoSTAs[STApairIndex].peerSTAid
                    << "). Channels " << (uint16_t)infoSTAs[STApairIndex].channelSTA
                    << ", " << (uint16_t)infoSTAs[STApairIndex].channelPeerSTA
                    << std::endl;
      }

      // find the location of the STA in the scenario
      Vector posSTA = GetPosition (infoSTAs[STApairIndex].pointerToMainSTA);
      if (myverbose >=2)
        std::cout << Simulator::Now().GetSeconds()
                  << "\t[algorithmLoadBalancing] STA (#" << infoSTAs[STApairIndex].STAid
                  << ", #" << infoSTAs[STApairIndex].peerSTAid
                  <<  ") position: "  << posSTA.x 
                  << "," << posSTA.y 
                  << std::endl;

      // check if the STA is associated to an AP
      std::string STAband = "";
      std::string peerSTAband = "";

      if ((infoSTAs[STApairIndex].channelSTA == 0) && (infoSTAs[STApairIndex].channelPeerSTA == 0)) {
        // none of the two peered STAs is associated

        if (myverbose >=2)
          std::cout << Simulator::Now().GetSeconds()
                    << "\t[algorithmLoadBalancing] The STA (#" << infoSTAs[STApairIndex].STAid 
                    << ", #" << infoSTAs[STApairIndex].peerSTAid
                    << ") is NOT associated"
                    << std::endl;
      }
      else {
        if (infoSTAs[STApairIndex].channelSTA != 0) {
          // the main STA is associated and the peer STA is not associated

          STAband = getWirelessBandOfChannel(infoSTAs[STApairIndex].channelSTA);

          if (myverbose >=2)
            std::cout << Simulator::Now().GetSeconds()
                      << "\t[algorithmLoadBalancing]  STA #" << infoSTAs[STApairIndex].STAid
                      << " associated to AP " << infoSTAs[STApairIndex].APiDwhereThisSTAisAssociated
                      << " " << APaddressWhereThisSTAisAssociated 
                      << " in channel " << (uint16_t)(infoSTAs[STApairIndex].channelSTA)
                      << " band " << STAband
                      << std::endl;           
        }
        else { // if (channelPeerSTA != 0)
          // the main STA is not associated but the peer STA is associated

          // auxiliar string
          std::ostringstream auxString;
          // create a string with the MAC
          auxString << "02-06-" << GetAPMACOfAnAssociatedSTA (infoSTAs[STApairIndex].peerSTAid);
          APaddressWhereThisSTAisAssociated = auxString.str();

          if (APaddressWhereThisSTAisAssociated != "02-06-00:00:00:00:00:00") {
            infoSTAs[STApairIndex].APiDwhereThePeerSTAisAssociated = GetAnAP_Id (APaddressWhereThisSTAisAssociated);
            infoSTAs[STApairIndex].peerSTAassociated = true;
          }

          peerSTAband = getWirelessBandOfChannel(infoSTAs[STApairIndex].channelPeerSTA);

          if (myverbose >=2)
            std::cout << Simulator::Now().GetSeconds()
                      << "\t[algorithmLoadBalancing]   Status: STA #" << infoSTAs[STApairIndex].peerSTAid
                      << " associated to AP #" << infoSTAs[STApairIndex].APiDwhereThePeerSTAisAssociated
                      << " " << APaddressWhereThisSTAisAssociated 
                      << " in channel " << (uint16_t)(infoSTAs[STApairIndex].channelPeerSTA)
                      << " band " << peerSTAband
                      << std::endl;
        }

        // make sure both STAs are in different bands
        NS_ASSERT(STAband != peerSTAband);
        
        // make sure only one of the paired STAs is associated
        NS_ASSERT(!((infoSTAs[STApairIndex].STAassociated == true) && (infoSTAs[STApairIndex].peerSTAassociated == true)));

        // make sure only one of the bands is active
        if (STAband == "5 GHz") NS_ASSERT(peerSTAband == "");
        if (STAband == "2.4 GHz") NS_ASSERT(peerSTAband == "");
        if (peerSTAband == "5 GHz") NS_ASSERT(STAband == "");
        if (peerSTAband == "2.4 GHz") NS_ASSERT(STAband == "");


        // fill the coverage variable 'coverageAPinfo' for this STA
        // check if the STA is under coverage of each AP on each band
        uint16_t APindex = 0;
        //uint16_t peerAPindex = 0;

        for (AP_recordVector::const_iterator indexAP = AP_vector.begin (); indexAP != AP_vector.end (); indexAP++) {

          // as the APs are in pairs, do this only with the first half of the APs
          if ( APindex < numberAPpairs ) {
            uint8_t APchannel = (*indexAP)->GetWirelessChannel();
            std::string APband = getWirelessBandOfChannel(APchannel);
            // calculate the distance of the STA to this AP
            Ptr<Node> myAP = apNodes.Get(APindex);
            Vector posMyAP = GetPosition (myAP);
            double distance = sqrt ( ( (posSTA.x - posMyAP.x)*(posSTA.x - posMyAP.x) ) + ( (posSTA.y - posMyAP.y)*(posSTA.y - posMyAP.y) ) );

            // obtain data of the peer AP
            //peerAPindex = APindex + numberAPpairs;
            AP_recordVector::const_iterator indexPeerAP = indexAP + numberAPpairs;
            uint8_t peerAPchannel = (*indexPeerAP)->GetWirelessChannel();
            std::string peerAPband = getWirelessBandOfChannel(peerAPchannel);

            // default value
            //coverageAPinfo[STApairIndex][APindex] = "none";
            coverageAPinfo[STApairIndex][APindex] = 'n';

            if ((APchannel == 0) && (peerAPchannel == 0)) {
              // both APs are disabled. There is no coverage
              //coverageAPinfo[STApairIndex][APindex] = "none";

              if (myverbose >= 2)
                std::cout << Simulator::Now ().GetSeconds()
                          << "\t[algorithmLoadBalancing]     AP (#" << (*indexAP)->GetApid()
                          << ", #" << (*indexPeerAP)->GetApid()
                          << ") is not active"
                          << std::endl;
            }
            else if ((APchannel != 0) && (peerAPchannel != 0)) {
              // dual AP
              if (myverbose >= 2)
                std::cout << Simulator::Now ().GetSeconds()
                          << "\t[algorithmLoadBalancing]     AP (#" << (*indexAP)->GetApid()
                          << ", #" << (*indexPeerAP)->GetApid()
                          << ") is dual."
                          << " Channels " << uint16_t(APchannel)
                          << ", " << uint16_t(peerAPchannel)
                          << ". Distance to STA (#" << infoSTAs[STApairIndex].STAid
                          << ", #" << infoSTAs[STApairIndex].peerSTAid
                          << "): " << distance << " m"
                          << std::endl;

              if (VERBOSE_FOR_DEBUG >= 1)
                std::cout << Simulator::Now ().GetSeconds()
                          << "\t[algorithmLoadBalancing]     AP #" << (*indexAP)->GetApid() 
                          << " with MAC " << (*indexAP)->GetMac()
                          << " Channel " << uint16_t(APchannel)
                          << " band " << APband
                          << std::endl;

              if (VERBOSE_FOR_DEBUG >= 1)
                std::cout << Simulator::Now ().GetSeconds()
                          << "\t[algorithmLoadBalancing]     peer AP #" << (*indexPeerAP)->GetApid() 
                          << " with MAC " << (*indexPeerAP)->GetMac()
                          << " Channel " << uint16_t(peerAPchannel)
                          << " band " << peerAPband
                          << std::endl;

              if ( (distance < coverage.coverage_24GHz) && (distance < coverage.coverage_5GHz) ) {
                // the STA is under coverage of this dual AP in both bands
                //coverageAPinfo[STApairIndex][APindex] = "both";
                coverageAPinfo[STApairIndex][APindex] = 'b';

                if (myverbose >= 2)
                  std::cout << Simulator::Now ().GetSeconds()
                            << "\t[algorithmLoadBalancing]      The STA is under coverage of the AP in both bands: " << distance
                            << " < " << coverage.coverage_24GHz
                            << " m (2.4 GHz) & " << distance
                            << " < " << coverage.coverage_5GHz
                            << " m (5 GHz) "
                            << std::endl;             
              }
              else if ( (distance < coverage.coverage_24GHz) && (distance >= coverage.coverage_5GHz) ) {
                // the STA is under coverage of this dual AP in both bands
                //coverageAPinfo[STApairIndex][APindex] = "2.4 GHz";
                coverageAPinfo[STApairIndex][APindex] = '2';

                if (myverbose >= 2)
                  std::cout << Simulator::Now ().GetSeconds()
                            << "\t[algorithmLoadBalancing]      The STA is under coverage of the AP in the 2.4 GHz band: " << distance
                            << " < " << coverage.coverage_24GHz
                            << " m"
                            << std::endl;
              }
              else if ( (distance >= coverage.coverage_24GHz) && (distance < coverage.coverage_5GHz) ) {
                // the STA is under coverage of this dual AP in both bands
                //coverageAPinfo[STApairIndex][APindex] = "5 GHz";
                coverageAPinfo[STApairIndex][APindex] = '5';

                if (myverbose >= 2)
                  std::cout << Simulator::Now ().GetSeconds()
                            << "\t[algorithmLoadBalancing]      The STA is under coverage of the AP in the 5 GHz band: " << distance
                            << " < " << coverage.coverage_5GHz
                            << " m"
                            << std::endl;             
              }
            }
            else {
              // only one of the APs is active

              if ( ((APchannel != 0) && (APband == "2.4 GHz")) || ((peerAPchannel != 0) && (peerAPband == "2.4 GHz")) ) {
                // the active interface is in 2.4 GHz
                if (myverbose >= 2) {
                  std::cout << Simulator::Now ().GetSeconds()
                            << "\t[algorithmLoadBalancing]     AP (#" << (*indexAP)->GetApid()
                            << ", #" << (*indexPeerAP)->GetApid()
                            << ") is only active in 2.4 GHz.";
                  if (APchannel != 0)
                    std::cout << " Channel " << uint16_t(APchannel);
                  else
                    std::cout << " Channel " << uint16_t(peerAPchannel);

                  std::cout << ". Distance to STA (#" << infoSTAs[STApairIndex].STAid
                            << ", #" << infoSTAs[STApairIndex].peerSTAid
                            << "): " << distance << " m"
                            << std::endl;
                }

                if (distance < coverage.coverage_24GHz) {
                  // the STA is under coverage of this dual AP in both bands
                  //coverageAPinfo[STApairIndex][APindex] = "2.4 GHz";
                  coverageAPinfo[STApairIndex][APindex] = '2';

                  if (myverbose >= 2)
                    std::cout << Simulator::Now ().GetSeconds()
                              << "\t[algorithmLoadBalancing]      The STA is under coverage of the AP in the 2.4 GHz band: " << distance
                              << " < " << coverage.coverage_24GHz
                              << " m"
                              << std::endl;
                }
              }
              else if ( ((APchannel != 0) && (APband == "5 GHz")) || ((peerAPchannel != 0) && (peerAPband == "5 GHz")) ) {
                // the active interface is in 5 GHz
                if (myverbose >= 2) {
                  std::cout << Simulator::Now ().GetSeconds()
                            << "\t[algorithmLoadBalancing]     AP (#" << (*indexAP)->GetApid()
                            << ", #" << (*indexPeerAP)->GetApid()
                            << ") is only active in 5 GHz.";
                  if (APchannel != 0)
                    std::cout << " Channel " << uint16_t(APchannel);
                  else
                    std::cout << " Channel " << uint16_t(peerAPchannel);

                  std::cout << ". Distance to STA (#" << infoSTAs[STApairIndex].STAid
                            << ", #" << infoSTAs[STApairIndex].peerSTAid
                            << "): " << distance << " m"
                            << std::endl;
                }

                if (distance < coverage.coverage_5GHz) {
                  // the STA is under coverage of this dual AP in both bands
                  //coverageAPinfo[STApairIndex][APindex] = "5 GHz";
                  coverageAPinfo[STApairIndex][APindex] = '5';

                  if (myverbose >= 2)
                    std::cout << Simulator::Now ().GetSeconds()
                              << "\t[algorithmLoadBalancing]      The STA is under coverage of the AP in the 5 GHz band: " << distance
                              << " < " << coverage.coverage_5GHz
                              << " m"
                              << std::endl;
                }
              }
            }
          }
          APindex++;
        }
        // ensure I have checked all the AP nodes
        NS_ASSERT (APindex == apNodes.GetN());
      }
    }
    index++;
  }

  if (VERBOSE_FOR_DEBUG >= 2)
    for (uint16_t k=0;k<numberSTApairs;k++)
      for (uint16_t l=0;l<numberAPpairs;l++)
        std::cout << "STA " << k << " AP " << l << " " << coverageAPinfo[k][l] << '\n';
  /******* end of - fill the variables with information about STAs ***********/


  /******* fill the variables with information about APs ***********/
  // fill a variale with the bands of the APs
  // fill the coverage variable 'infoAboutEachAPpair' for this STA
  // check if the STA is under coverage of each AP on each band
  struct infoAboutEachAPpair {
    uint16_t APid;
    uint16_t peerAPid;
    uint8_t channelAP;
    uint8_t channelPeerAP;
    //Ptr<Node> pointerToMainAP;
    //Ptr<Node> pointerToPeerAP;
    std::string APband;
    std::string peerAPband;
    std::string APbandsActive; //must be 'none', 'both', '2.4 GHz', '5 GHz'
  };
  infoAboutEachAPpair infoAPs[numberAPpairs];

  if (myverbose >= 2)
    std::cout << Simulator::Now ().GetSeconds()
              << "\t[algorithmLoadBalancing] AP report"
              << std::endl;

  uint16_t APindex = 0;
  for (AP_recordVector::const_iterator indexAP = AP_vector.begin (); indexAP != AP_vector.end (); indexAP++) {

    AP_recordVector::const_iterator indexPeerAP = indexAP + numberAPpairs;

    // as the APs are in pairs, do this only with the first half of the APs
    if ( APindex < numberAPpairs ) {

      infoAPs[APindex].APid = (*indexAP)->GetApid();
      infoAPs[APindex].peerAPid = (*indexPeerAP)->GetApid();

      // obtain the channels
      infoAPs[APindex].channelAP = (*indexAP)->GetWirelessChannel();
      infoAPs[APindex].channelPeerAP = (*indexPeerAP)->GetWirelessChannel();      

      // obtain the pointers
      //infoAPs[APindex].pointerToMainAP = apNodes.Get(APindex);
      //infoAPs[APindex].pointerToPeerAP = apNodes.Get(APindex + numberAPpairs);

      // obtain the bands
      std::string APband = getWirelessBandOfChannel(infoAPs[APindex].channelAP);
      std::string peerAPband = getWirelessBandOfChannel(infoAPs[APindex].channelPeerAP);

      // fill the 'APbandsActive' field
      if ((infoAPs[APindex].channelAP == 0) && (infoAPs[APindex].channelPeerAP == 0)) {
        // both APs are disabled. There is no coverage
        infoAPs[APindex].APbandsActive = "none";
        if ( APband == "2.4 GHz" ) {
          infoAPs[APindex].APband = "2.4 GHz";
          infoAPs[APindex].peerAPband = "5 GHz";
        }
        else {
          infoAPs[APindex].APband = "5 GHz";
          infoAPs[APindex].peerAPband = "2.4 GHz";          
        }
      }
      else if ((infoAPs[APindex].channelAP != 0) && (infoAPs[APindex].channelPeerAP != 0)) {
        // dual AP
        infoAPs[APindex].APbandsActive = "both";
        if ( APband == "2.4 GHz" ) {
          infoAPs[APindex].APband = "2.4 GHz";
          infoAPs[APindex].peerAPband = "5 GHz";
        }
        else {
          infoAPs[APindex].APband = "5 GHz";
          infoAPs[APindex].peerAPband = "2.4 GHz";          
        }
      }
      else {
        // only one of the APs is active
        if ( ((infoAPs[APindex].channelAP != 0) && (APband == "2.4 GHz")) || ((infoAPs[APindex].channelPeerAP != 0) && (peerAPband == "2.4 GHz")) ) {
          // the active interface is in 2.4 GHz
          infoAPs[APindex].APbandsActive = "2.4 GHz";
          if (infoAPs[APindex].channelAP != 0) {
            infoAPs[APindex].APband = "2.4 GHz";
            infoAPs[APindex].peerAPband = "5 GHz";
          }
          else {
            infoAPs[APindex].APband = "5 GHz";
            infoAPs[APindex].peerAPband = "2.4 GHz";
          }
        }
        else if ( ((infoAPs[APindex].channelAP != 0) && (APband == "5 GHz")) || ((infoAPs[APindex].channelPeerAP != 0) && (peerAPband == "5 GHz")) ) {
          // the active interface is in 5 GHz
          infoAPs[APindex].APbandsActive = "5 GHz";
          if (infoAPs[APindex].channelAP != 0) {
            infoAPs[APindex].APband = "5 GHz";
            infoAPs[APindex].peerAPband = "2.4 GHz";
          }
          else {
            infoAPs[APindex].APband = "2.4 GHz";
            infoAPs[APindex].peerAPband = "5 GHz";
          }
        }
      }

      // make sure these fields have been filled correctly
      NS_ASSERT( (infoAPs[APindex].APband == "2.4 GHz" ) || (infoAPs[APindex].APband == "5 GHz" ) );
      NS_ASSERT( (infoAPs[APindex].peerAPband == "2.4 GHz" ) || (infoAPs[APindex].peerAPband == "5 GHz" ) );

      if (myverbose >= 2) {
        std::cout << Simulator::Now ().GetSeconds()
                  << "\t[algorithmLoadBalancing]     AP (#" << infoAPs[APindex].APid
                  << ", #" << infoAPs[APindex].peerAPid
                  << "). Bands: " << infoAPs[APindex].APbandsActive;
        if (infoAPs[APindex].APbandsActive == "both") {
          std::cout << ". Channels " << uint16_t(infoAPs[APindex].channelAP)
                    << "," << uint16_t(infoAPs[APindex].channelPeerAP);
        }
        else if (infoAPs[APindex].APbandsActive == "5 GHz") {
          if (infoAPs[APindex].APband == "5 GHz")
            std::cout << ". Channel " << uint16_t(infoAPs[APindex].channelAP);
          else
            std::cout << ". Channel " << uint16_t(infoAPs[APindex].channelPeerAP);
        }
        else if (infoAPs[APindex].APbandsActive == "2.4 GHz") {
          if (infoAPs[APindex].APband == "2.4 GHz")
            std::cout << ". Channel " << uint16_t(infoAPs[APindex].channelAP);
          else
            std::cout << ". Channel " << uint16_t(infoAPs[APindex].channelPeerAP);
        }
        std::cout << std::endl;
      }
    }
    APindex++;
  }
  // ensure I have checked all the AP nodes
  NS_ASSERT (APindex == numberAPpairs * 2);
  //NS_ASSERT (APindex == apNodes.GetN());
  /******* end of -fill the variables with information about APs ***********/


  /*********** load balancing algorithm **********/
  // I try to find a pair of STAs such as:
  // - candidateDualSTA: a dual STA (11n & 11ac) that is under coverage of a dual (11n & 11ac) and a non-dual AP (only 11n), but it is associated to the non-dual AP
  // - candidateNonDualSTA: a non-dual STA (11n) that is under coverage of a a dual (11n & 11ac) and a non-dual AP (only 11n), but it is associated to the dual AP (with 11n)
  //
  // if I find both STAs, I can exchange their APs (i.e. modify their channels so they will associate to another AP)
  // only one exchange between 2 STAs can be performed by the algorithm
  //i.e. every 'period', an exchange can be done
  uint16_t candidateDualSTA = 0;
  uint16_t candidateNonDualSTA = 0;
  uint16_t newAPforCandidateDualSTA = 65535;
  uint16_t newAPforCandidateNonDualSTA = 65535;
  uint16_t channelNewAPforCandidateDualSTA = 0;
  uint16_t channelNewAPforCandidateNonDualSTA = 0;
  uint16_t currentAPCandidateDualSTA = 65535;
  uint16_t currentAPCandidateNonDualSTA = 65535;

  if (false) {
    if (myverbose >= 2)
      std::cout << Simulator::Now ().GetSeconds()
                << "\t[algorithmLoadBalancing] Starting the load balancing algorithm: looking for candidate STAs"
                << std::endl;

    for (uint16_t i=0; i < numberSTApairs; i++) {
        
      /****** find a candidate dual STA **********/
      if (infoSTAs[i].STAenabled && infoSTAs[i].peerSTAenabled) {
        // both paired STAs are enabled => this is a dual STA

        if (candidateDualSTA == 0) {
          // I have not found a candidate yet

          if (myverbose >= 2)
            std::cout << Simulator::Now ().GetSeconds()
                      << "\t[algorithmLoadBalancing]   STA #" << infoSTAs[i].STAid
                      << ", #" << infoSTAs[i].peerSTAid
                      << " is dual";
                    
          // check if it is associated to a 2.4 GHz AP
          if ((!infoSTAs[i].STAassociated) && (!infoSTAs[i].peerSTAassociated)) {
            // it is not associated
            if (myverbose >= 2)
              std::cout << " but it is not associated";
          }
          else if ((infoSTAs[i].STAassociated) || (infoSTAs[i].peerSTAassociated)) {
            // the STA is associated
            uint16_t APwhereSTAisAssociated, peerAPwhereSTAisAssociated;

            if (infoSTAs[i].STAassociated) {
              // the primary STA is the associated one
              // it must be associated to the primary AP (primary APs and primary STAs are in the same band)
              APwhereSTAisAssociated = infoSTAs[i].APiDwhereThisSTAisAssociated;
              peerAPwhereSTAisAssociated = APwhereSTAisAssociated + numberAPpairs;
            }
            else {
              // the secondary STA is the associated one
              // it must be associated to the secondary AP (secondary APs and secondary STAs are in the same band)
              peerAPwhereSTAisAssociated = infoSTAs[i].APiDwhereThePeerSTAisAssociated;
              APwhereSTAisAssociated = peerAPwhereSTAisAssociated - numberAPpairs;
            }

            // it is associated to the AP with id 'APwhereSTAisAssociated'
            if (myverbose >= 2)
              std::cout << ". Associated to AP (#" << APwhereSTAisAssociated
                        << ", #" << peerAPwhereSTAisAssociated
                        << ")";

            if (infoAPs[ APwhereSTAisAssociated ].APbandsActive == "2.4 GHz") {
              // 'APwhereSTAisAssociated' is a 2.4GHz AP
              if (myverbose >= 2)
                std::cout << "(a 2.4 GHz only AP)\n";

              // look for another AP that can provide coverage in both bands to STA #i

              for (uint16_t j=0; j < numberAPpairs; j++) {
                if (candidateDualSTA == 0) {
                  // I have not found a candidate yet

                  if (j != APwhereSTAisAssociated) {
                    // the AP has to be different from 'APwhereSTAisAssociated'

                    //if ( coverageAPinfo[i][j] == "both") {
                    if ( coverageAPinfo[i][j] == 'b') {
                      // I have found a candidate dual STA

                      // I store the AP to which it can be switched
                      // it is currently associated to a 2.4 GHz AP
                      // it has to be switched to a 5 GHz AP

                      if (infoAPs[j].APband == "5 GHz") {
                        // the primary band is 5GHz
                        // the STA has to be switched there
                        candidateDualSTA = infoSTAs[i].STAid + numberSTApairs;
                        newAPforCandidateDualSTA = infoAPs[j].APid;
                        channelNewAPforCandidateDualSTA = infoAPs[j].channelAP;

                        // the STA is associated in 2.4 GHz
                        // and 2.4 GHz is the secondary band
                        currentAPCandidateDualSTA = APwhereSTAisAssociated + numberAPpairs;
                      }
                      else if (infoAPs[j].peerAPband == "5 GHz") {
                        // the secondary AP is in 5GHz
                        // the STA has to be switched there
                        candidateDualSTA = infoSTAs[i].STAid;
                        newAPforCandidateDualSTA = infoAPs[j].peerAPid;
                        channelNewAPforCandidateDualSTA = infoAPs[j].channelPeerAP;

                        // the STA is associated in 2.4 GHz
                        // and 2.4 GHz is the primary band
                        currentAPCandidateDualSTA = APwhereSTAisAssociated;
                      }
                      else {
                        NS_ASSERT(false);
                      }

                      if (VERBOSE_FOR_DEBUG >= 2)
                        std::cout << "newAPforCandidateDualSTA: " << newAPforCandidateDualSTA
                                  << "\nchannelNewAPforCandidateDualSTA: " << channelNewAPforCandidateDualSTA
                                  << "\ncurrentAPCandidateDualSTA: " << currentAPCandidateDualSTA
                                  << "\n";

                      if (myverbose >= 2)
                        std::cout << Simulator::Now ().GetSeconds()
                                  << "\t[algorithmLoadBalancing]    STA(#" << infoSTAs[i].STAid
                                  << ",#" << infoSTAs[i].peerSTAid
                                  << ") is a candidate for being switched to AP(#" << infoAPs[j].APid
                                  << ",#" << infoAPs[j].peerAPid 
                                  << ") (bands: " << infoAPs[j].APbandsActive << ")";              
                    }
                    else {
                      if (myverbose >= 2)
                        std::cout << Simulator::Now ().GetSeconds()
                                  << "\t[algorithmLoadBalancing]    STA(#" << infoSTAs[i].STAid
                                  << ",#" << infoSTAs[i].peerSTAid
                                  << ") is NOT under coverage of AP(#" << infoAPs[j].APid
                                  << ",#" << infoAPs[j].peerAPid
                                  << ") (bands: " << infoAPs[j].APbandsActive << ")"
                                  << std::endl;                     
                    }
                  }
                }
              }
              if (candidateDualSTA == 0) {
                if (myverbose >= 2)
                  std::cout << Simulator::Now ().GetSeconds()
                            << "\t[algorithmLoadBalancing]   Dual STA candidate not found ";
              }
            }
            else if (infoAPs[ APwhereSTAisAssociated ].APbandsActive == "5 GHz") {
              if (myverbose >= 2)
                std::cout << "(a 5 GHz only AP). Not a candidate for switch";
            }
            else if (infoAPs[ APwhereSTAisAssociated ].APbandsActive == "both") {
              if (myverbose >= 2)
                std::cout << "(a dual AP). Not a candidate for switch";
            }
            else {
              std::cout << "\n" << infoAPs[ APwhereSTAisAssociated ].APbandsActive << "\n";
              NS_ASSERT(infoAPs[ APwhereSTAisAssociated ].APbandsActive == "none");
              if (myverbose >= 2)
                std::cout << "(an inactive AP). Not a candidate for switch";
            }
          }
          else {
            // the STA is not associated
            if (myverbose >= 2)
              std::cout << ". Not associated to any AP";  
          }
          if (myverbose >= 2)
                std::cout << std::endl;
        }          
      }

      /****** find a candidate non-dual STA **********/
      else {
        // this is a non-dual STA
        // try to find a candidate non-dual STA   

        if (candidateNonDualSTA == 0) {
          // I have not found a candidate yet

          if (myverbose >= 2)
            std::cout << Simulator::Now ().GetSeconds()
                      << "\t[algorithmLoadBalancing]   STA(#" << infoSTAs[i].STAid
                      << ",#" << infoSTAs[i].peerSTAid
                      << ") is not dual";

          // check if it is associated
          if (( ! infoSTAs[i].STAassociated) && ( ! infoSTAs[i].peerSTAassociated)) {
            // none of the paired STAs is associated
            if (myverbose >= 2)
              std::cout << " but it is not associated. Not a candidate for switch";
          }
          else if ((infoSTAs[i].STAassociated) != (infoSTAs[i].peerSTAassociated)) { // note: '!=' means XOR
            // one of the paired STAs is associated

            if (((infoSTAs[i].STAassociated) && (getWirelessBandOfChannel(infoSTAs[i].channelSTA) == "5 GHz")) || 
                ((infoSTAs[i].peerSTAassociated) && (getWirelessBandOfChannel(infoSTAs[i].channelPeerSTA) == "5 GHz"))) {
                // one of the paired STAs is associated in 5GHz
                // this is not relevant for the algorithm
                if (myverbose >= 2)
                  std::cout << " but is is already associated in 5 GHz. Not a candidate for switch";
            }

            else if (((infoSTAs[i].STAassociated) && (getWirelessBandOfChannel(infoSTAs[i].channelSTA) == "2.4 GHz")) || 
                      ((infoSTAs[i].peerSTAassociated) && (getWirelessBandOfChannel(infoSTAs[i].channelPeerSTA) == "2.4 GHz"))) {

              // one of the paired STAs is associated in 2.4GHZ

              // here I will store the IDs of the two correspoding APs
              uint16_t APwhereSTAisAssociated, peerAPwhereSTAisAssociated;

              if (infoSTAs[i].STAassociated) {
                // the primary STA is the associated one
                // it must be associated to the primary AP (primary APs and primary STAs are in the same band)
                APwhereSTAisAssociated = infoSTAs[i].APiDwhereThisSTAisAssociated;
                peerAPwhereSTAisAssociated = APwhereSTAisAssociated + numberAPpairs;
              }
              else {
                // the secondary STA is the associated one
                // it must be associated to the secondary AP (secondary APs and secondary STAs are in the same band)
                peerAPwhereSTAisAssociated = infoSTAs[i].APiDwhereThePeerSTAisAssociated;
                APwhereSTAisAssociated = peerAPwhereSTAisAssociated - numberAPpairs;
              }

              if (myverbose >= 2)
                std::cout << ". Associated to AP(#" << APwhereSTAisAssociated
                          << ",#" << peerAPwhereSTAisAssociated
                          << ")";

              if (infoAPs[ APwhereSTAisAssociated ].APbandsActive == "both") {
                // 'APwhereSTAisAssociated' is a dual AP
                if (myverbose >= 2)
                  std::cout << "(a dual AP)\n";

                // look for another AP that can provide coverage in the 2.4 GHz band to STA #i

                for (uint16_t j=0; j < numberAPpairs; j++) {
                  if (candidateNonDualSTA == 0) {
                    // I have not found a candidate yet

                    if (j != APwhereSTAisAssociated) {
                      // the AP has to be different from 'APwhereSTAisAssociated'

                      //if ( coverageAPinfo[i][j] == "2.4 GHz") {
                      if ( coverageAPinfo[i][j] == '2') {
                        // I have found a candidate non-dual STA

                        // I also store the AP to which it can be switched
                        // it is currently associated to a 2.4 GHz AP
                        // it has to be switched to a 2.4 GHz-only AP
                        if (infoAPs[j].APband == "2.4 GHz") {
                          // the primary band is 2.4 GHz
                          // the STA has to be switched there
                          candidateNonDualSTA = infoSTAs[i].STAid;
                          newAPforCandidateNonDualSTA = infoAPs[j].APid;
                          channelNewAPforCandidateNonDualSTA = infoAPs[j].channelAP;

                          // 2.4 GHz is the primary band
                          currentAPCandidateNonDualSTA = APwhereSTAisAssociated;
                        }
                        else if (infoAPs[j].peerAPband == "2.4 GHz") {
                          // the secondary AP is 2.4 GHz
                          // the STA has to be switched there
                          candidateNonDualSTA = infoSTAs[i].STAid + numberSTApairs;
                          newAPforCandidateNonDualSTA = infoAPs[j].peerAPid;
                          channelNewAPforCandidateNonDualSTA = infoAPs[j].channelPeerAP;

                          // 2.4 GHz is the secondary band
                          currentAPCandidateNonDualSTA = APwhereSTAisAssociated + numberAPpairs;
                        }
                        else {
                          NS_ASSERT(false);
                        }

                        if (myverbose >= 2)
                          std::cout << Simulator::Now ().GetSeconds()
                                    << "\t[algorithmLoadBalancing]    STA(#" << infoSTAs[i].STAid
                                    << ",#" << infoSTAs[i].peerSTAid
                                    << ") is a candidate for being switched to AP(#" << infoAPs[j].APid
                                    << ",#" << infoAPs[j].peerAPid 
                                    << ") (bands: " << infoAPs[j].APbandsActive << ")"
                                    /*<< std::endl*/;              
                      }
                      //else if ( coverageAPinfo[i][j] == "both") {
                      else if ( coverageAPinfo[i][j] == 'b') {
                        // I have found a candidate non-dual STA

                        // I also store the AP to which it can be switched
                        // it is currently associated to a dual AP
                        // it has to be switched to the 2.4 GHz AP
                        if (infoAPs[j].APband == "2.4 GHz") {
                          // the primary band is 2.4 GHz
                          // the STA has to be switched there
                          candidateNonDualSTA = infoSTAs[i].STAid;
                          newAPforCandidateNonDualSTA = infoAPs[j].APid;
                          channelNewAPforCandidateNonDualSTA = infoAPs[j].channelAP;

                          // 2.4 GHz is the primary band
                          currentAPCandidateNonDualSTA = APwhereSTAisAssociated;
                        }
                        else if (infoAPs[ infoAPs[j].APid ].peerAPband == "2.4 GHz") {
                          candidateNonDualSTA = infoSTAs[i].STAid + numberSTApairs;
                          newAPforCandidateNonDualSTA = infoAPs[j].peerAPid;
                          channelNewAPforCandidateNonDualSTA = infoAPs[j].channelPeerAP;
                          // 2.4 GHz is the secondary band
                          currentAPCandidateNonDualSTA = APwhereSTAisAssociated + numberAPpairs;
                        }

                        if (myverbose >= 2)
                          std::cout << Simulator::Now ().GetSeconds()
                                    << "\t[algorithmLoadBalancing]    STA(#" << infoSTAs[i].STAid
                                    << ",#" << infoSTAs[i].peerSTAid
                                    << " is a candidate for being switched to AP#" << infoAPs[j].APid
                                    << ",#" << infoAPs[j].peerAPid 
                                    << ") (bands: " << infoAPs[j].APbandsActive << ")"
                                    /*<< std::endl*/;              
                      }
                      else {
                        if (myverbose >= 2)
                          std::cout << Simulator::Now ().GetSeconds()
                                    << "\t[algorithmLoadBalancing]    STA(#" << infoSTAs[i].STAid
                                    << ",#" << infoSTAs[i].peerSTAid
                                    << ") is NOT under coverage of AP(#" << infoAPs[j].APid
                                    << ",#" << infoAPs[j].peerAPid 
                                    << ") (bands: " << infoAPs[j].APbandsActive << ")"
                                    << std::endl;                     
                      }
                    }
                  }
                }
                if (candidateNonDualSTA == 0) {
                  if (myverbose >= 2)
                    std::cout << Simulator::Now ().GetSeconds()
                              << "\t[algorithmLoadBalancing]   Non-dual STA candidate not found ";
                }
              }
              else if (infoAPs[ APwhereSTAisAssociated ].APbandsActive == "5 GHz") {
                if (myverbose >= 2)
                  std::cout << "(a 5 GHz only AP). Not a candidate for switch";
              }
              else if (infoAPs[ APwhereSTAisAssociated ].APbandsActive == "2.4 GHz") {
                if (myverbose >= 2)
                  std::cout << "(a 2.4 GHz only AP). Not a candidate for switch";
              }
              else {
                std::cout << "\n" << infoAPs[ APwhereSTAisAssociated ].APbandsActive << "\n";
                NS_ASSERT(infoAPs[ APwhereSTAisAssociated ].APbandsActive == "none");
                if (myverbose >= 2)
                  std::cout << "(an inactive AP). Not a candidate for switch";
              }
            }
          }
          else {
            // both STAs are associated. This is an error because this is a non-dual STA
            NS_ASSERT(false);
          }
          if (myverbose >= 2)
                std::cout << std::endl;             
        }
      }
    }


    /* if two candidates have been found, exchange them if possible */
    //myverbose = 2;  // FIXME: remove this

    if (candidateDualSTA != 0) {
      if (myverbose >= 2)
        std::cout << Simulator::Now ().GetSeconds()
                  << "\t[algorithmLoadBalancing] I have found a candidate dual STA#" << candidateDualSTA
                  << ". It is in AP#" << currentAPCandidateDualSTA
                  << " and can be switched to AP#" << newAPforCandidateDualSTA
                  << " in channel " << channelNewAPforCandidateDualSTA
                  << std::endl;

      NS_ASSERT(newAPforCandidateDualSTA != 65535);
      NS_ASSERT(channelNewAPforCandidateDualSTA != 0);
      NS_ASSERT(currentAPCandidateDualSTA != 65535);
    }

    if (candidateNonDualSTA != 0) {
      if (myverbose >= 2)
        std::cout << Simulator::Now ().GetSeconds()
                  << "\t[algorithmLoadBalancing] I have found a candidate non-dual STA#" << candidateNonDualSTA
                  << ". It is in AP#" << currentAPCandidateNonDualSTA
                  << " and can be switched to AP#" << newAPforCandidateNonDualSTA
                  << " in channel " << channelNewAPforCandidateNonDualSTA
                  << std::endl;

      NS_ASSERT(newAPforCandidateNonDualSTA != 65535);
      NS_ASSERT(channelNewAPforCandidateNonDualSTA != 0);
      NS_ASSERT(currentAPCandidateNonDualSTA != 65535);
    }

    if ((candidateDualSTA != 0) && (candidateNonDualSTA != 0)) {
      // I have found two candidate STAs
      if (myverbose >= 2)
        std::cout << Simulator::Now ().GetSeconds()
                  << "\t[algorithmLoadBalancing] I have found two candidate STAs. Let's see if they can be switched"
                  << std::endl;

      bool canBeSwitched = false;

      if (currentAPCandidateDualSTA == newAPforCandidateNonDualSTA) {
        // the current AP of the dual STA is the new AP of the non-dual STA

        if (infoAPs[0].APband == "2.4 GHz") {
          // the primary band is 2.4 GHZ
          if (currentAPCandidateNonDualSTA == newAPforCandidateDualSTA - numberAPpairs) {
            canBeSwitched = true;
            if (myverbose >= 2)
              std::cout << Simulator::Now ().GetSeconds()
                        << "\t[algorithmLoadBalancing] The candidate STAs can be switched"
                        << std::endl;
          }
        }
        else if (infoAPs[0].APband == "5 GHz") {
          // the primary band is 5 GHZ
          if (currentAPCandidateNonDualSTA == newAPforCandidateDualSTA + numberAPpairs) {
            canBeSwitched = true;
            if (myverbose >= 2)
              std::cout << Simulator::Now ().GetSeconds()
                        << "\t[algorithmLoadBalancing] The candidate STAs can be switched"
                        << std::endl;
          }
        }
        else {
          NS_ASSERT(false);
        }
      }

      if (canBeSwitched) {

        /* dual STA */
        // deactivate the 2.4 GHz STA
        NetDeviceContainer device24GDualSTA;
        device24GDualSTA.Add( (staNodes.Get(candidateDualSTA - apNodes.GetN()))->GetDevice(1) ); // this adds the device to the NetDeviceContainer. It has to be device 1, not device 0. I don't know why

        // find the STA in the record or STAs
        STA_recordVector::const_iterator index;
        uint32_t mywifiModel;
        for (index = sta_vector.begin (); index != sta_vector.end (); index++) {
          if ((*index)->GetStaid () == candidateDualSTA) {
            mywifiModel = (*index)->GetWifiModel();
          }
        }

        // disable the network device
        DisableNetworkDevice (device24GDualSTA, mywifiModel, 0 /*myverbose*/);


        // switch the 5 GHz interface to 'channelNewAPforCandidateDualSTA'
        // Move this STA to the channel of the AP identified
        NetDeviceContainer device5GDualSTA;

        uint16_t peerOfCandidateDualSTA;
        if (infoAPs[0].APband == "5 GHz") {
          // the primary band is 5 GHZ
          // the index of the dual STA is that of 2.4 GHz
          peerOfCandidateDualSTA = candidateDualSTA - apNodes.GetN() - numberSTApairs;
        }
        else {
          // the primary band is 2.4 GHz
          // the index of the dual STA is that of 2.4 GHz
          peerOfCandidateDualSTA = candidateDualSTA - apNodes.GetN();
        }

        device5GDualSTA.Add( (staNodes.Get(peerOfCandidateDualSTA))->GetDevice(1) ); // this adds the device to the NetDeviceContainer. It has to be device 1, not device 0. I don't know why

        //if ( HANDOFFMETHOD == 0 )
          ChangeFrequencyLocal (device5GDualSTA, channelNewAPforCandidateDualSTA, mywifiModel, 0 /*myverbose*/);

        if (myverbose >= 2)
          std::cout << Simulator::Now ().GetSeconds() 
                    << "\t[algorithmLoadBalancing] Dual STA #" << peerOfCandidateDualSTA + apNodes.GetN()
                    << ". Channel set to " << uint16_t (channelNewAPforCandidateDualSTA) 
                    << ", i.e. the channel of AP #" << newAPforCandidateDualSTA
                    << std::endl;

        // activate the 5 GHz STA        
        // enable the network device
        EnableNetworkDevice (device5GDualSTA, mywifiModel, 0 /*myverbose*/);

        /* non-dual STA */
        // switch the 2.4 GHz interface to 'channelNewAPforCandidateNonDualSTA'
        // Move this STA to the channel of the AP identified
        NetDeviceContainer deviceNonDualSTA;
        deviceNonDualSTA.Add( (staNodes.Get(candidateNonDualSTA - apNodes.GetN()))->GetDevice(1) ); // this adds the device to the NetDeviceContainer. It has to be device 1, not device 0. I don't know why

        //if ( HANDOFFMETHOD == 0 )
          ChangeFrequencyLocal (deviceNonDualSTA, channelNewAPforCandidateNonDualSTA, mywifiModel, 0 /*myverbose*/);

        if (myverbose >= 2)
          std::cout << Simulator::Now ().GetSeconds() 
                    << "\t[algorithmLoadBalancing] Non-dual STA #" << candidateNonDualSTA
                    << ". Channel set to " << uint16_t (channelNewAPforCandidateNonDualSTA) 
                    << ", i.e. the channel of AP #" << newAPforCandidateNonDualSTA
                    << std::endl;

        if (false) {
          // this works but it is not needed
          SleepNetworkDevice (deviceNonDualSTA, mywifiModel, myverbose);

          //ResumeNetworkDevice (deviceNonDualSTA, mywifiModel, myverbose);
          Simulator::Schedule(Seconds(1.0), &ResumeNetworkDevice, deviceNonDualSTA, mywifiModel, myverbose);        
        }

        if (false) {
          // this does not work:
          //msg="Invalid WifiPhy state.", file=../src/wifi/model/wifi-phy-state-helper.cc, line=619
          //terminate called without an active exception

          // disable the network device
          //DisableNetworkDevice (deviceNonDualSTA, mywifiModel, myverbose);
          Simulator::Schedule(Seconds(1.0), &DisableNetworkDevice, deviceNonDualSTA, mywifiModel, myverbose);

          // enable the network device
          //EnableNetworkDevice (deviceNonDualSTA, mywifiModel, myverbose);
          Simulator::Schedule(Seconds(2.0), &EnableNetworkDevice, deviceNonDualSTA, mywifiModel, myverbose);
        }
      }
    }    
  }
  /*********** end of - load balancing algorithm **********/


  /*********** move-to-5GHz algorithm **********/
  // I try to find a STA that:
  // - is dual (11n & 11ac)
  // - is associated to a 11n AP
  // - is under coverage of an 11ac AP
  //
  // if I find it, I will move it to the 11ac AP (i.e. modify its channel so it will associate to another AP)

  myverbose = 2; // FIXME: Remove this

  if (myverbose >= 2)
    std::cout << Simulator::Now ().GetSeconds()
              << "\t[algorithmLoadBalancing2] Starting the 'move-to-5GHz' algorithm: looking for candidate STAs"
              << std::endl;

  for (uint16_t STApairIndex=0; STApairIndex < numberSTApairs; STApairIndex++) {
      
    /****** find a candidate dual STA **********/
    if ((infoSTAs[STApairIndex].STAenabled == true) && (infoSTAs[STApairIndex].peerSTAenabled == true)) {
      // both paired STAs are enabled => this is a dual STA

      if (myverbose >= 2)
        std::cout << Simulator::Now ().GetSeconds()
                  << "\t[algorithmLoadBalancing2]   STA(#" << infoSTAs[STApairIndex].STAid
                  << ",#" << infoSTAs[STApairIndex].peerSTAid
                  << ") is dual";

      // check if it is associated to an AP
      if ((infoSTAs[STApairIndex].STAassociated == false) && (infoSTAs[STApairIndex].peerSTAassociated == false)) {
        // it is not associated
        if (myverbose >= 2)
          std::cout << " but it is not associated";
      }

      else if ((infoSTAs[STApairIndex].STAassociated == true) || (infoSTAs[STApairIndex].peerSTAassociated == true)) {
        // the STA is associated
        uint16_t APwhereSTAisAssociated, peerAPwhereSTAisAssociated;
        std::string bandWhereTheSTAisAssociated;


        if (infoSTAs[STApairIndex].STAassociated == true) {
          // the primary STA is the associated one
          // it must be associated to the primary AP (primary APs and primary STAs are in the same band)
          APwhereSTAisAssociated = infoSTAs[STApairIndex].APiDwhereThisSTAisAssociated;
          peerAPwhereSTAisAssociated = APwhereSTAisAssociated + numberAPpairs;
          bandWhereTheSTAisAssociated = getWirelessBandOfChannel(infoSTAs[STApairIndex].channelSTA);
          //bandWhereTheSTAisAssociated = getWirelessBandOfChannel(infoAPs[APwhereSTAisAssociated % numberAPpairs].channelAP);
        }
        else {
          // the secondary STA is the associated one
          // it must be associated to the secondary AP (secondary APs and secondary STAs are in the same band)
          peerAPwhereSTAisAssociated = infoSTAs[STApairIndex].APiDwhereThePeerSTAisAssociated;
          APwhereSTAisAssociated = peerAPwhereSTAisAssociated - numberAPpairs;
          bandWhereTheSTAisAssociated = getWirelessBandOfChannel(infoSTAs[STApairIndex].channelPeerSTA);
          //bandWhereTheSTAisAssociated = getWirelessBandOfChannel(infoAPs[peerAPwhereSTAisAssociated % numberAPpairs].channelAP);
        }

        // it is associated to the AP with id 'APwhereSTAisAssociated'
        if (myverbose >= 2)
          std::cout << ". Associated to AP(#" << APwhereSTAisAssociated
                    << ",#" << peerAPwhereSTAisAssociated
                    << ") in " << bandWhereTheSTAisAssociated;

        if (bandWhereTheSTAisAssociated == "5 GHz") {
          std::cout << ". Already in 5 GHz. Not interesting";

          //std::cout << "\nnumberAPpairs:" << numberAPpairs << "\n";
          //std::cout << "STApairIndex: " << STApairIndex << "\n";
        } 

        else if (bandWhereTheSTAisAssociated == "2.4 GHz") {
          std::cout << ". Looking for coverage of a 5 GHz AP"
                    << '\n';

          // look for a 5 GHz AP that can provide coverage to STA #i
          candidateDualSTA = 0;
          //bool newAPforCandidateDualSTAfound = false;
          newAPforCandidateDualSTA = 65535;
          channelNewAPforCandidateDualSTA = 0;
          currentAPCandidateDualSTA = 65535;

          for (uint16_t APpairIndex=0; APpairIndex < numberAPpairs; APpairIndex++) {
            
            if (newAPforCandidateDualSTA == 65535) {
              // I have not found an AP candidate yet

              //if ( (coverageAPinfo[STApairIndex][APpairIndex] == "both") || (coverageAPinfo[STApairIndex][APpairIndex] == "5 GHz") ) {
              if ( (coverageAPinfo[STApairIndex][APpairIndex] == 'b') || (coverageAPinfo[STApairIndex][APpairIndex] == '5') ) {
                // this AP can give coverage in 5 GHz

                // I store the AP to which it can be switched
                // it is currently associated to a 2.4 GHz AP
                // it has to be switched to a 5 GHz AP

                if (infoAPs[APpairIndex].APband == "5 GHz") {
                  
                  // the primary band is 5GHz
                  // the STA has to be switched there
                  candidateDualSTA = infoSTAs[STApairIndex].STAid + numberSTApairs;
                  newAPforCandidateDualSTA = infoAPs[APpairIndex].APid;
                  channelNewAPforCandidateDualSTA = infoAPs[APpairIndex].channelAP;
                  //newAPforCandidateDualSTAfound = true;

                  // the STA is associated in 2.4 GHz
                  // and 2.4 GHz is the secondary band
                  currentAPCandidateDualSTA = APwhereSTAisAssociated + numberAPpairs;
                  
                }
                else if (infoAPs[APpairIndex].peerAPband == "5 GHz") {
                  
                  // the secondary AP is in 5GHz
                  // the STA has to be switched there
                  candidateDualSTA = infoSTAs[STApairIndex].STAid;
                  newAPforCandidateDualSTA = infoAPs[APpairIndex].peerAPid;
                  channelNewAPforCandidateDualSTA = infoAPs[APpairIndex].channelPeerAP;
                  //newAPforCandidateDualSTAfound = true;

                  // the STA is associated in 2.4 GHz
                  // and 2.4 GHz is the primary band
                  currentAPCandidateDualSTA = APwhereSTAisAssociated;
                  
                }
                else {
                  NS_ASSERT(false);
                }
                
                if (VERBOSE_FOR_DEBUG >= 2)
                  std::cout << "newAPforCandidateDualSTA: " << newAPforCandidateDualSTA
                            << "\nchannelNewAPforCandidateDualSTA: " << channelNewAPforCandidateDualSTA
                            << "\ncurrentAPCandidateDualSTA: " << currentAPCandidateDualSTA
                            << "\n";

                if (myverbose >= 2)
                  std::cout << Simulator::Now ().GetSeconds()
                            << "\t[algorithmLoadBalancing2]     STA(#" << infoSTAs[STApairIndex].STAid
                            << ",#" << infoSTAs[STApairIndex].peerSTAid
                            << ") is a candidate for being switched to AP(#" << infoAPs[APpairIndex].APid
                            << ",#" << infoAPs[APpairIndex].peerAPid 
                            << ") (bands: " << infoAPs[APpairIndex].APbandsActive << ")"
                            << std::endl;   
                      
              }
              else {
                
                if (myverbose >= 2)
                  std::cout << Simulator::Now ().GetSeconds()
                            << "\t[algorithmLoadBalancing2]     STA(#" << infoSTAs[STApairIndex].STAid
                            << ",#" << infoSTAs[STApairIndex].peerSTAid
                            << ") is NOT under optimal 5 GHz coverage of AP(#" << infoAPs[APpairIndex].APid
                            << ",#" << infoAPs[APpairIndex].peerAPid
                            << ") (bands: " << infoAPs[APpairIndex].APbandsActive << ")"
                            << std::endl;         
              }
            }
          }

          if (candidateDualSTA == 0) {
            if (myverbose >= 2)
              std::cout << Simulator::Now ().GetSeconds()
                        << "\t[algorithmLoadBalancing2]    The STA cannot be switched to 5 GHz";
          }
          else { //if (candidateDualSTA != 0)

            // if a candidate has been found, move it to 5 GHz if possible
            //myverbose = 2;  // FIXME: remove this

            if (myverbose >= 2)
              std::cout << Simulator::Now ().GetSeconds()
                        << "\t[algorithmLoadBalancing2]    I have found a candidate dual STA#" << candidateDualSTA
                        << ". It is in AP#" << currentAPCandidateDualSTA
                        << " and can be switched to AP#" << newAPforCandidateDualSTA
                        << " in channel " << channelNewAPforCandidateDualSTA
                        << std::endl;

            NS_ASSERT(newAPforCandidateDualSTA != 65535);
            NS_ASSERT(channelNewAPforCandidateDualSTA != 0);
            NS_ASSERT(currentAPCandidateDualSTA != 65535);

            // dual STA
            // deactivate the 2.4 GHz STA
            NetDeviceContainer device24GDualSTA;
            device24GDualSTA.Add( (staNodes.Get(candidateDualSTA - apNodes.GetN()))->GetDevice(1) ); // this adds the device to the NetDeviceContainer. It has to be device 1, not device 0. I don't know why

            // find the STA in the record or STAs
            STA_recordVector::const_iterator index;
            uint32_t mywifiModel;
            for (index = sta_vector.begin (); index != sta_vector.end (); index++) {
              if ((*index)->GetStaid () == candidateDualSTA) {
                mywifiModel = (*index)->GetWifiModel();
              }
            }

            // disable the network device
            DisableNetworkDevice (device24GDualSTA, mywifiModel, myverbose /*0*/ );
            if (myverbose >= 2)
              std::cout << Simulator::Now ().GetSeconds() 
                        << "\t[algorithmLoadBalancing2] STA #" << candidateDualSTA
                        << " network device disabled"
                        << std::endl;

            // switch the 5 GHz interface to 'channelNewAPforCandidateDualSTA'
            // Move this STA to the channel of the AP identified
            NetDeviceContainer device5GDualSTA;

            uint16_t peerOfCandidateDualSTA;
            if (infoAPs[0].APband == "5 GHz") {
              // the primary band is 5 GHZ
              // the index of the dual STA is that of 2.4 GHz
              peerOfCandidateDualSTA = candidateDualSTA /*- apNodes.GetN()*/ - numberSTApairs;
            }
            else {
              // the primary band is 2.4 GHz
              // the index of the dual STA is that of 2.4 GHz
              peerOfCandidateDualSTA = candidateDualSTA /*- apNodes.GetN()*/;
            }

            if (myverbose >= 2)
              std::cout << Simulator::Now ().GetSeconds() 
                        << "\t[algorithmLoadBalancing2] The peer STA of STA #" << candidateDualSTA
                        << " is STA#" << peerOfCandidateDualSTA
                        << std::endl;

            device5GDualSTA.Add( (staNodes.Get(peerOfCandidateDualSTA - apNodes.GetN()))->GetDevice(1) ); // this adds the device to the NetDeviceContainer. It has to be device 1, not device 0. I don't know why

            // activate the 5 GHz STA        
            // enable the 5 GHz network device
            EnableNetworkDevice (device5GDualSTA, mywifiModel, myverbose /*0*/ );
            if (myverbose >= 2)
              std::cout << Simulator::Now ().GetSeconds() 
                        << "\t[algorithmLoadBalancing2] STA #" << peerOfCandidateDualSTA
                        << " network device enabled"
                        << std::endl;

            // change the frequency
            //if ( HANDOFFMETHOD == 0 )
              ChangeFrequencyLocal (device5GDualSTA, channelNewAPforCandidateDualSTA, mywifiModel, myverbose /*0*/ );

            // this is not needed
            if (false) {
              // reset the ARP cache of the STA
              infoArpCache(staNodes.Get(peerOfCandidateDualSTA - apNodes.GetN()), myverbose);
              emtpyArpCache(staNodes.Get(peerOfCandidateDualSTA - apNodes.GetN()), myverbose);
              infoArpCache(staNodes.Get(peerOfCandidateDualSTA - apNodes.GetN()), myverbose);

              // reset the ARP cache of the router
              infoArpCache(routerNode.Get(0), myverbose);
              emtpyArpCache(routerNode.Get(0), myverbose);
              infoArpCache(routerNode.Get(0), myverbose);              
            }

            if (myverbose >= 1)
              std::cout << Simulator::Now ().GetSeconds() 
                        << "\t[algorithmLoadBalancing2] ***** Dual STA #" << peerOfCandidateDualSTA + apNodes.GetN()
                        << " switched: channel set to " << uint16_t (channelNewAPforCandidateDualSTA) 
                        << ", i.e. the channel of AP #" << newAPforCandidateDualSTA
                        << " *****"
                        << std::endl;
          }        
        }
        else {
          NS_ASSERT(false);
        }
      }
      else {
        // the STA is not associated
        if (myverbose >= 2)
          std::cout << ". Not associated to any AP";  
      }
      if (myverbose >= 2)
            std::cout << std::endl;          
    }

    else {
      // this is a non-dual STA
      if (myverbose >= 2)
        std::cout << Simulator::Now ().GetSeconds()
                  << "\t[algorithmLoadBalancing2]   STA(#" << infoSTAs[STApairIndex].STAid
                  << ",#" << infoSTAs[STApairIndex].peerSTAid
                  << ") is not dual\n";
    }
  }
  /*********** end of - move-to-5GHz algorithm **********/


  /*********** move-to-2.4GHz algorithm **********/
  // I try to find a STA that:
  // - is dual (11n & 11ac)
  // - is associated to a 11ac AP
  // - but is out of "ideal coverage" of the 11ac AP (plus 5.0 meters, to avoid ping-pong effect)
  // - is under coverage of an 11n AP
  //
  // if I find it, I will move it to an 11n AP (i.e. modify its channel so it will associate to another AP)

  myverbose = 2; // FIXME: Remove this

  if (myverbose >= 2)
    std::cout << Simulator::Now ().GetSeconds()
              << "\t[algorithmLoadBalancing3] Starting the 'move-to-2.4GHz' algorithm: looking for candidate STAs"
              << std::endl;

  for (uint16_t STApairIndex=0; STApairIndex < numberSTApairs; STApairIndex++) {
      
    /****** find a candidate dual STA **********/
    if ((infoSTAs[STApairIndex].STAenabled == true) && (infoSTAs[STApairIndex].peerSTAenabled == true)) {
      // both paired STAs are enabled => this is a dual STA

      if (myverbose >= 2)
        std::cout << Simulator::Now ().GetSeconds()
                  << "\t[algorithmLoadBalancing3]   STA(#" << infoSTAs[STApairIndex].STAid
                  << ",#" << infoSTAs[STApairIndex].peerSTAid
                  << ") is dual";

      // check if it is associated to an AP
      if ((infoSTAs[STApairIndex].STAassociated == false) && (infoSTAs[STApairIndex].peerSTAassociated == false)) {
        // it is not associated
        if (myverbose >= 2)
          std::cout << " but it is not associated";
      }

      else if ((infoSTAs[STApairIndex].STAassociated == true) || (infoSTAs[STApairIndex].peerSTAassociated == true)) {
        // the STA is associated
        uint16_t APwhereSTAisAssociated, peerAPwhereSTAisAssociated;
        std::string bandWhereTheSTAisAssociated;


        if (infoSTAs[STApairIndex].STAassociated == true) {
          // the primary STA is the associated one
          // it must be associated to the primary AP (primary APs and primary STAs are in the same band)
          APwhereSTAisAssociated = infoSTAs[STApairIndex].APiDwhereThisSTAisAssociated;
          peerAPwhereSTAisAssociated = APwhereSTAisAssociated + numberAPpairs;
          bandWhereTheSTAisAssociated = getWirelessBandOfChannel(infoSTAs[STApairIndex].channelSTA);
          //bandWhereTheSTAisAssociated = getWirelessBandOfChannel(infoAPs[APwhereSTAisAssociated % numberAPpairs].channelAP);
        }
        else {
          // the secondary STA is the associated one
          // it must be associated to the secondary AP (secondary APs and secondary STAs are in the same band)
          peerAPwhereSTAisAssociated = infoSTAs[STApairIndex].APiDwhereThePeerSTAisAssociated;
          APwhereSTAisAssociated = peerAPwhereSTAisAssociated - numberAPpairs;
          bandWhereTheSTAisAssociated = getWirelessBandOfChannel(infoSTAs[STApairIndex].channelPeerSTA);
          //bandWhereTheSTAisAssociated = getWirelessBandOfChannel(infoAPs[peerAPwhereSTAisAssociated % numberAPpairs].channelAP);
        }

        // it is associated to the AP with id 'APwhereSTAisAssociated'
        if (myverbose >= 2)
          std::cout << ". Associated to AP(#" << APwhereSTAisAssociated
                    << ",#" << peerAPwhereSTAisAssociated
                    << ") in " << bandWhereTheSTAisAssociated;

        if (bandWhereTheSTAisAssociated == "2.4 GHz") {
          std::cout << ". Associated in 2.4 GHz. Not interesting"
                    << '\n';

          //std::cout << "\nnumberAPpairs:" << numberAPpairs << "\n";
          //std::cout << "STApairIndex: " << STApairIndex << "\n";
        } 

        else if (bandWhereTheSTAisAssociated == "5 GHz") {
          std::cout << ". Checking if it is out of optimal coverage of the current 5 GHz AP"
                    << '\n';

          Ptr<Node> myAP = apNodes.Get(APwhereSTAisAssociated);
          Vector posMyAP = GetPosition (myAP);

          // find the location of the STA in the scenario
          Vector posSTA = GetPosition (infoSTAs[STApairIndex].pointerToMainSTA);

          double distance = sqrt ( ( (posSTA.x - posMyAP.x)*(posSTA.x - posMyAP.x) ) + ( (posSTA.y - posMyAP.y)*(posSTA.y - posMyAP.y) ) );
          
          if (myverbose >= 2) {
            std::cout << Simulator::Now ().GetSeconds()
                      << "\t[algorithmLoadBalancing3]     AP#" << APwhereSTAisAssociated
                      << ". Distance to STA (#" << infoSTAs[STApairIndex].STAid
                      << ", #" << infoSTAs[STApairIndex].peerSTAid
                      << "): " << distance << " m"
                      << '\n';
          }

          // I add 5.0 meters to avoid ping-pong effect
          if ( distance < coverage.coverage_5GHz + 5.0) {
            std::cout << Simulator::Now ().GetSeconds()
                      << "\t[algorithmLoadBalancing3] The STA is still under optimal coverage of the current 5 GHz AP. Distance " << distance
                      << " < " << coverage.coverage_5GHz + 5.0
                      << '\n';
          }
          else {
            std::cout << Simulator::Now ().GetSeconds()
                      << "\t[algorithmLoadBalancing3] The STA is NOT under optimal coverage of the current 5 GHz AP. Distance " << distance
                      << " >= " << coverage.coverage_5GHz + 5.0
                      << ". Looking for coverage of a 2.4 GHz AP"
                      << '\n';

            // Find the nearest AP
            Ptr<Node> myNearestAP;

            if (infoAPs[0].APband == "5 GHz")
              // the primary band is 5 GHz
              myNearestAP = nearestAp (apNodes, infoSTAs[STApairIndex].pointerToPeerSTA, myverbose, "2.4 GHz");
            else if (infoAPs[0].APband == "2.4 GHz")
              // the primary band is 2.4 GHz
              myNearestAP = nearestAp (apNodes, infoSTAs[STApairIndex].pointerToMainSTA, myverbose, "2.4 GHz");
            else
              NS_ASSERT(false);

            if (myNearestAP == NULL) {
              // there is no AP in 2.4 GHz
              candidateDualSTA = 0;
              if (myverbose >= 2)
                std::cout << Simulator::Now ().GetSeconds()
                          << "\t[algorithmLoadBalancing3]    The STA cannot be switched to 2.4 GHz"
                          << '\n';
            }
            else {
              newAPforCandidateDualSTA = (myNearestAP)->GetId();
              Vector posMyNearestAP = GetPosition (myNearestAP);
              double distance2 = sqrt ( ( (posSTA.x - posMyNearestAP.x)*(posSTA.x - posMyNearestAP.x) ) + ( (posSTA.y - posMyNearestAP.y)*(posSTA.y - posMyNearestAP.y) ) );

              std::cout << Simulator::Now().GetSeconds()
                        << "\t[ReportPosition] STA #" << STApairIndex 
                        <<  " Position: "  << posSTA.x
                        << "," << posSTA.y 
                        << ". The nearest AP in 2.4 GHz is AP#" << newAPforCandidateDualSTA
                        << ". Distance: " << distance2 << " m"
                        << std::endl;

              uint16_t indexForAP;
              if (infoAPs[0].APband == "2.4 GHz") {
                // the primary band is 2.4 GHZ
                indexForAP = newAPforCandidateDualSTA;
              }
              else {
                // the primary band is 5 GHZ
                indexForAP = newAPforCandidateDualSTA - numberAPpairs;                
              }
std::cout << "indexForAP " << indexForAP << '\n';

              if (infoAPs[indexForAP].APband == "2.4 GHz") {
                // the primary band is 2.4GHz
                // the STA has to be switched there
                candidateDualSTA = infoSTAs[STApairIndex].STAid + numberSTApairs;
                channelNewAPforCandidateDualSTA = infoAPs[indexForAP].channelAP;

                // the STA is associated in 5 GHz
                // and 5 GHz is the secondary band
                currentAPCandidateDualSTA = APwhereSTAisAssociated + numberAPpairs;
              }
              else if (infoAPs[indexForAP].peerAPband == "2.4 GHz") {
                // the secondary AP is in 2.4 GHz
                // the STA has to be switched there
                candidateDualSTA = infoSTAs[STApairIndex].STAid;
                channelNewAPforCandidateDualSTA = infoAPs[indexForAP].channelPeerAP;
                //newAPforCandidateDualSTAfound = true;

                // the STA is associated in 5 GHz
                // and 5 GHz is the primary band
                currentAPCandidateDualSTA = APwhereSTAisAssociated;
              }

              if (VERBOSE_FOR_DEBUG >= 2)
                std::cout << "newAPforCandidateDualSTA: " << newAPforCandidateDualSTA
                          << "\nchannelNewAPforCandidateDualSTA: " << channelNewAPforCandidateDualSTA
                          << "\ncurrentAPCandidateDualSTA: " << currentAPCandidateDualSTA
                          << "\n";

              if (myverbose >= 2)
                std::cout << Simulator::Now ().GetSeconds()
                          << "\t[algorithmLoadBalancing3]     STA(#" << infoSTAs[STApairIndex].STAid
                          << ",#" << infoSTAs[STApairIndex].peerSTAid
                          << ") is a candidate for being switched to AP(#" << infoAPs[indexForAP].APid
                          << ",#" << infoAPs[indexForAP].peerAPid 
                          << ") (bands: " << infoAPs[indexForAP].APbandsActive << ")"
                          << '\n';  

              // if a candidate has been found, move it to 2.4 GHz if possible
              //myverbose = 2;  // FIXME: remove this
              if (myverbose >= 2)
                std::cout << Simulator::Now ().GetSeconds()
                          << "\t[algorithmLoadBalancing3]    ***** I have found a candidate dual STA#" << candidateDualSTA
                          << ". It is in AP#" << currentAPCandidateDualSTA
                          << " and can be switched to AP#" << newAPforCandidateDualSTA
                          << " in channel " << channelNewAPforCandidateDualSTA
                          << " *****"
                          << '\n';

              NS_ASSERT(channelNewAPforCandidateDualSTA != 0);

              // dual STA
              // deactivate the 5 GHz STA
              NetDeviceContainer device5GDualSTA;
              device5GDualSTA.Add( (staNodes.Get(candidateDualSTA - apNodes.GetN()))->GetDevice(1) ); // this adds the device to the NetDeviceContainer. It has to be device 1, not device 0. I don't know why

              // find the STA in the record or STAs
              STA_recordVector::const_iterator index;
              uint32_t mywifiModel;
              for (index = sta_vector.begin (); index != sta_vector.end (); index++) {
                if ((*index)->GetStaid () == candidateDualSTA) {
                  mywifiModel = (*index)->GetWifiModel();
                }
              }

              // disable the network device
              DisableNetworkDevice (device5GDualSTA, mywifiModel, myverbose /*0*/ );
              if (myverbose >= 2)
                std::cout << Simulator::Now ().GetSeconds() 
                          << "\t[algorithmLoadBalancing3] STA #" << candidateDualSTA
                          << " network device disabled"
                          << '\n';

              // switch the 5 GHz interface to 'channelNewAPforCandidateDualSTA'
              // Move this STA to the channel of the AP identified
              NetDeviceContainer device24GDualSTA;

              uint16_t peerOfCandidateDualSTA;
              if (infoAPs[0].APband == "2.4 GHz") {
                // the primary band is 2.4 GHZ
                // the index of the dual STA is that of 2.4 GHz
                peerOfCandidateDualSTA = candidateDualSTA /*- apNodes.GetN()*/ - numberSTApairs;
              }
              else {
                // the primary band is 5 GHz
                // the index of the dual STA is that of 5 GHz
                peerOfCandidateDualSTA = candidateDualSTA /*- apNodes.GetN()*/;
              }

              if (myverbose >= 2)
                std::cout << Simulator::Now ().GetSeconds() 
                          << "\t[algorithmLoadBalancing3] The peer STA of STA #" << candidateDualSTA
                          << " is STA#" << peerOfCandidateDualSTA
                          << '\n';

              device24GDualSTA.Add( (staNodes.Get(peerOfCandidateDualSTA - apNodes.GetN()))->GetDevice(1) ); // this adds the device to the NetDeviceContainer. It has to be device 1, not device 0. I don't know why

              // activate the 5 GHz STA        
              // enable the 5 GHz network device
              EnableNetworkDevice (device24GDualSTA, mywifiModel, myverbose /*0*/ );
              if (myverbose >= 2)
                std::cout << Simulator::Now ().GetSeconds() 
                          << "\t[algorithmLoadBalancing3] STA #" << peerOfCandidateDualSTA
                          << " network device enabled"
                          << '\n';

              // change the frequency
              //if ( HANDOFFMETHOD == 0 )
                ChangeFrequencyLocal (device24GDualSTA, channelNewAPforCandidateDualSTA, mywifiModel, myverbose /*0*/ );
            }



            if (false) {
              // look for a 2.4 GHz AP that can provide coverage to STA #i
              candidateDualSTA = 0;
              //bool newAPforCandidateDualSTAfound = false;
              newAPforCandidateDualSTA = 65535;
              channelNewAPforCandidateDualSTA = 0;
              currentAPCandidateDualSTA = 65535;

              for (uint16_t APpairIndex=0; APpairIndex < numberAPpairs; APpairIndex++) {
                
                if (newAPforCandidateDualSTA == 65535) {
                  // I have not found an AP candidate yet

                  if ( (coverageAPinfo[STApairIndex][APpairIndex] == 'b') || (coverageAPinfo[STApairIndex][APpairIndex] == '2') ) {
                    // this AP can give coverage in 2.4 GHz

                    // I store the AP to which it can be switched
                    // it is currently associated to a 5 GHz AP
                    // it has to be switched to a 2.4 GHz AP

                    if (infoAPs[APpairIndex].APband == "2.4 GHz") {
                      
                      // the primary band is 2.4GHz
                      // the STA has to be switched there
                      candidateDualSTA = infoSTAs[STApairIndex].STAid + numberSTApairs;
                      newAPforCandidateDualSTA = infoAPs[APpairIndex].APid;
                      channelNewAPforCandidateDualSTA = infoAPs[APpairIndex].channelAP;
                      //newAPforCandidateDualSTAfound = true;

                      // the STA is associated in 5 GHz
                      // and 5 GHz is the secondary band
                      currentAPCandidateDualSTA = APwhereSTAisAssociated + numberAPpairs;
                      
                    }
                    else if (infoAPs[APpairIndex].peerAPband == "2.4 GHz") {
                      
                      // the secondary AP is in 2.4 GHz
                      // the STA has to be switched there
                      candidateDualSTA = infoSTAs[STApairIndex].STAid;
                      newAPforCandidateDualSTA = infoAPs[APpairIndex].peerAPid;
                      channelNewAPforCandidateDualSTA = infoAPs[APpairIndex].channelPeerAP;
                      //newAPforCandidateDualSTAfound = true;

                      // the STA is associated in 5 GHz
                      // and 5 GHz is the primary band
                      currentAPCandidateDualSTA = APwhereSTAisAssociated;
                      
                    }
                    else {
                      NS_ASSERT(false);
                    }
                    
                    if (VERBOSE_FOR_DEBUG >= 2)
                      std::cout << "newAPforCandidateDualSTA: " << newAPforCandidateDualSTA
                                << "\nchannelNewAPforCandidateDualSTA: " << channelNewAPforCandidateDualSTA
                                << "\ncurrentAPCandidateDualSTA: " << currentAPCandidateDualSTA
                                << "\n";

                    if (myverbose >= 2)
                      std::cout << Simulator::Now ().GetSeconds()
                                << "\t[algorithmLoadBalancing3]     STA(#" << infoSTAs[STApairIndex].STAid
                                << ",#" << infoSTAs[STApairIndex].peerSTAid
                                << ") is a candidate for being switched to AP(#" << infoAPs[APpairIndex].APid
                                << ",#" << infoAPs[APpairIndex].peerAPid 
                                << ") (bands: " << infoAPs[APpairIndex].APbandsActive << ")"
                                << '\n'; 
                  }
                  else {
                    
                    if (myverbose >= 2)
                      std::cout << Simulator::Now ().GetSeconds()
                                << "\t[algorithmLoadBalancing3]     STA(#" << infoSTAs[STApairIndex].STAid
                                << ",#" << infoSTAs[STApairIndex].peerSTAid
                                << ") is NOT under 2.4 GHz coverage of AP(#" << infoAPs[APpairIndex].APid
                                << ",#" << infoAPs[APpairIndex].peerAPid
                                << ") (bands: " << infoAPs[APpairIndex].APbandsActive << ")"
                                << '\n';         
                  }
                }
              }

              if (candidateDualSTA == 0) {
                if (myverbose >= 2)
                  std::cout << Simulator::Now ().GetSeconds()
                            << "\t[algorithmLoadBalancing3]    The STA cannot be switched to 2.4 GHz"
                            << '\n';
              }
              else { //if (candidateDualSTA != 0)

                // if a candidate has been found, move it to 2.4 GHz if possible
                //myverbose = 2;  // FIXME: remove this

                if (myverbose >= 2)
                  std::cout << Simulator::Now ().GetSeconds()
                            << "\t[algorithmLoadBalancing3]    I have found a candidate dual STA#" << candidateDualSTA
                            << ". It is in AP#" << currentAPCandidateDualSTA
                            << " and can be switched to AP#" << newAPforCandidateDualSTA
                            << " in channel " << channelNewAPforCandidateDualSTA
                            << '\n';

                NS_ASSERT(newAPforCandidateDualSTA != 65535);
                NS_ASSERT(channelNewAPforCandidateDualSTA != 0);
                NS_ASSERT(currentAPCandidateDualSTA != 65535);

                // dual STA
                // deactivate the 5 GHz STA
                NetDeviceContainer device5GDualSTA;
                device5GDualSTA.Add( (staNodes.Get(candidateDualSTA - apNodes.GetN()))->GetDevice(1) ); // this adds the device to the NetDeviceContainer. It has to be device 1, not device 0. I don't know why

                // find the STA in the record or STAs
                STA_recordVector::const_iterator index;
                uint32_t mywifiModel;
                for (index = sta_vector.begin (); index != sta_vector.end (); index++) {
                  if ((*index)->GetStaid () == candidateDualSTA) {
                    mywifiModel = (*index)->GetWifiModel();
                  }
                }

                // disable the network device
                DisableNetworkDevice (device5GDualSTA, mywifiModel, myverbose /*0*/ );
                if (myverbose >= 2)
                  std::cout << Simulator::Now ().GetSeconds() 
                            << "\t[algorithmLoadBalancing3] STA #" << candidateDualSTA
                            << " network device disabled"
                            << '\n';

                // switch the 5 GHz interface to 'channelNewAPforCandidateDualSTA'
                // Move this STA to the channel of the AP identified
                NetDeviceContainer device24GDualSTA;

                uint16_t peerOfCandidateDualSTA;
                if (infoAPs[0].APband == "2.4 GHz") {
                  // the primary band is 2.4 GHZ
                  // the index of the dual STA is that of 2.4 GHz
                  peerOfCandidateDualSTA = candidateDualSTA /*- apNodes.GetN()*/ - numberSTApairs;
                }
                else {
                  // the primary band is 5 GHz
                  // the index of the dual STA is that of 5 GHz
                  peerOfCandidateDualSTA = candidateDualSTA /*- apNodes.GetN()*/;
                }

                if (myverbose >= 2)
                  std::cout << Simulator::Now ().GetSeconds() 
                            << "\t[algorithmLoadBalancing3] The peer STA of STA #" << candidateDualSTA
                            << " is STA#" << peerOfCandidateDualSTA
                            << '\n';

                device24GDualSTA.Add( (staNodes.Get(peerOfCandidateDualSTA - apNodes.GetN()))->GetDevice(1) ); // this adds the device to the NetDeviceContainer. It has to be device 1, not device 0. I don't know why

                // activate the 5 GHz STA        
                // enable the 5 GHz network device
                EnableNetworkDevice (device24GDualSTA, mywifiModel, myverbose /*0*/ );
                if (myverbose >= 2)
                  std::cout << Simulator::Now ().GetSeconds() 
                            << "\t[algorithmLoadBalancing3] STA #" << peerOfCandidateDualSTA
                            << " network device enabled"
                            << '\n';

                // change the frequency
                //if ( HANDOFFMETHOD == 0 )
                  ChangeFrequencyLocal (device24GDualSTA, channelNewAPforCandidateDualSTA, mywifiModel, myverbose /*0*/ );

                if (false) {
                  // reset the ARP cache of the STA
                  infoArpCache(staNodes.Get(peerOfCandidateDualSTA - apNodes.GetN()), myverbose);
                  emtpyArpCache(staNodes.Get(peerOfCandidateDualSTA - apNodes.GetN()), myverbose);
                  infoArpCache(staNodes.Get(peerOfCandidateDualSTA - apNodes.GetN()), myverbose);

                  // reset the ARP cache of the router
                  infoArpCache(routerNode.Get(0), myverbose);
                  emtpyArpCache(routerNode.Get(0), myverbose);
                  infoArpCache(routerNode.Get(0), myverbose);                
                }


                if (myverbose >= 1)
                  std::cout << Simulator::Now ().GetSeconds() 
                            << "\t[algorithmLoadBalancing3] ***** Dual STA #" << peerOfCandidateDualSTA + apNodes.GetN()
                            << " switched: channel set to " << uint16_t (channelNewAPforCandidateDualSTA) 
                            << ", i.e. the channel of AP #" << newAPforCandidateDualSTA
                            << " *****"
                            << '\n';
              }              
            }


          }
        }
        else {
          NS_ASSERT(false);
        }
      }
      else {
        // the STA is not associated
        if (myverbose >= 2)
          std::cout << ". Not associated to any AP";  
      }       
    }

    else {
      // this is a non-dual STA
      if (myverbose >= 2)
        std::cout << Simulator::Now ().GetSeconds()
                  << "\t[algorithmLoadBalancing3]   STA(#" << infoSTAs[STApairIndex].STAid
                  << ",#" << infoSTAs[STApairIndex].peerSTAid
                  << ") is not dual"
                  << '\n';
    }
  }
  if (myverbose >= 2)
    std::cout << Simulator::Now ().GetSeconds()
              << "\t[algorithmLoadBalancing3] End of the algorithm"
              << '\n';
  /*********** end of - move-to-2.4GHz algorithm **********/    



  myverbose = 0; // FIXME remove this

  // re-schedule the algorithm
  Simulator::Schedule(Seconds(period), &algorithmLoadBalancing, period, apNodes, staNodes, routerNode, coverage, /*coverage_24GHz, coverage_5GHz,*/ myverbose);

}


/********* FUNCTIONS ************/
FlowMonitorHelper flowmon;  // FIXME avoid this global variable


// Struct for storing the statistics of the VoIP flows
struct FlowStatistics {
  double acumDelay;
  double acumJitter;
  uint32_t acumRxPackets;
  uint32_t acumLostPackets;
  uint32_t acumRxBytes;
  double lastIntervalDelay;
  double lastIntervalJitter;
  uint32_t lastIntervalRxPackets;
  uint32_t lastIntervalLostPackets;
  uint32_t lastIntervalRxBytes;
  uint16_t destinationPort;
  uint16_t finalDestinationPort;
};

struct AllTheFlowStatistics {
  uint32_t numberVoIPUploadFlows;
  uint32_t numberVoIPDownloadFlows;
  uint32_t numberTCPUploadFlows;
  uint32_t numberTCPDownloadFlows;
  uint32_t numberVideoDownloadFlows;
  FlowStatistics* FlowStatisticsVoIPUpload;
  FlowStatistics* FlowStatisticsVoIPDownload;
  FlowStatistics* FlowStatisticsTCPUpload;
  FlowStatistics* FlowStatisticsTCPDownload;
  FlowStatistics* FlowStatisticsVideoDownload;
};


// The number of parameters for calling functions using 'schedule' is limited to 6, so I have to create a struct
struct adjustAmpduParameters {
  uint32_t verboseLevel;
  double timeInterval;
  double latencyBudget;
  uint32_t maxAmpduSize;
  std::string mynameAMPDUFile;
  uint16_t methodAdjustAmpdu;
  uint32_t stepAdjustAmpdu;
  bool eachSTArunsAllTheApps;
  std::string APsActive;
};


// Dynamically adjust the size of the AMPDU
void adjustAMPDU (//FlowStatistics* myFlowStatistics,
                  AllTheFlowStatistics myAllTheFlowStatistics,
                  adjustAmpduParameters myparam,
                  uint32_t* belowLatencyAmpduValue,
                  uint32_t* aboveLatencyAmpduValue,
                  uint32_t myNumberAPs)  
{
  int numberSTAsNonAssociated; // counts the number of STAs that are not associated to any AP

  uint32_t i = 0; // index for the APs

  // For each AP, find the highest value of the delay of the associated STAs
  for (AP_recordVector::const_iterator indexAP = AP_vector.begin (); indexAP != AP_vector.end (); indexAP++) {

    if (myparam.APsActive.at(i)=='0') {
      // this AP is not active
      if (myparam.verboseLevel > 0)
        std::cout << Simulator::Now ().GetSeconds()
                  << "\t[adjustAMPDU]"
                  << "\tAP #" << (*indexAP)->GetApid() 
                  << " NOT ACTIVE"
                  << std::endl;
    }
    else {
      // this AP is active
      if (myparam.verboseLevel > 0)
        std::cout << Simulator::Now ().GetSeconds()
                  << "\t[adjustAMPDU]"
                  << "\tAP #" << (*indexAP)->GetApid() 
                  << " with MAC " << (*indexAP)->GetMac() 
                  << " Max size AMPDU " << (*indexAP)->GetMaxSizeAmpdu() 
                  << " Channel " << uint16_t((*indexAP)->GetWirelessChannel())
                  << std::endl;

      // find the highest latency of all the VoIP STAs associated to that AP
      double highestLatencyVoIPFlows = 0.0;

      numberSTAsNonAssociated = 0;
      for (STA_recordVector::const_iterator indexSTA = sta_vector.begin (); indexSTA != sta_vector.end (); indexSTA++) {

        // check if the STA is associated to an AP
        if ((*indexSTA)->GetAssoc()) {
          // auxiliar string
          std::ostringstream auxString;
          // create a string with the MAC
          auxString << "02-06-" << (*indexSTA)->GetMacOfitsAP();
          std::string addressOfTheAPwhereThisSTAis = auxString.str();
          NS_ASSERT(!addressOfTheAPwhereThisSTAis.empty());

          // check if the STA is associated to this AP
          if ( (*indexAP)->GetMac() == addressOfTheAPwhereThisSTAis ) {

            if (myparam.verboseLevel > 0) 
              std::cout << Simulator::Now ().GetSeconds() 
                        << "\t[adjustAMPDU]"
                        << "\t\tSTA #" << (*indexSTA)->GetStaid() 
                        << "\tassociated to AP #" << GetAnAP_Id(addressOfTheAPwhereThisSTAis) 
                        << "\twith MAC " << (*indexSTA)->GetMacOfitsAP();
            /*
            uint32_t total_number_of_flows;
            if (myparam.eachSTArunsAllTheApps == false)
              total_number_of_flows = 
            */

            // VoIP upload
            if ((*indexSTA)->Gettypeofapplication () == 1) {
              if (myparam.verboseLevel > 0)
                std::cout << "\tVoIP upload";

              // index for the vector of statistics of VoIPDownload flows
              uint32_t indexForVector = (*indexSTA)->GetStaid()
                                        - AP_vector.size();

              // 'std::isnan' checks if the value is not a number
              if (!std::isnan(myAllTheFlowStatistics.FlowStatisticsVoIPUpload[indexForVector].lastIntervalDelay)) {
                if (myparam.verboseLevel > 0)
                  std::cout << "\tDelay: " << myAllTheFlowStatistics.FlowStatisticsVoIPUpload[indexForVector].lastIntervalDelay 
                            << "\tThroughput: " << myAllTheFlowStatistics.FlowStatisticsVoIPUpload[indexForVector].lastIntervalRxBytes * 8 / myparam.timeInterval
                            //<< "\t indexForVector is " << indexForVector
                            ;

                // if the latency of this STA is the highest one so far, update the value of the highest latency
                if (  myAllTheFlowStatistics.FlowStatisticsVoIPUpload[ indexForVector ].lastIntervalDelay > highestLatencyVoIPFlows && 
                      !std::isnan(myAllTheFlowStatistics.FlowStatisticsVoIPUpload[ indexForVector].lastIntervalDelay))

                  highestLatencyVoIPFlows = myAllTheFlowStatistics.FlowStatisticsVoIPUpload[ indexForVector].lastIntervalDelay;
              }
              else {
                if (myparam.verboseLevel > 0) 
                  std::cout << "\tDelay not defined in this period" 
                            //<< "\t (*indexSTA)->GetStaid()  - AP_vector.size() is " << (*indexSTA)->GetStaid() - AP_vector.size()
                            ;
              }
            }

            // VoIP download
            else if ((*indexSTA)->Gettypeofapplication () == 2) {
              if (myparam.verboseLevel > 0)
                std::cout << "\tVoIP download";

              // index for the vector of statistics of VoIPDownload flows
              uint32_t indexForVector;
              if (myparam.eachSTArunsAllTheApps == false)
                indexForVector =  (*indexSTA)->GetStaid()
                                  - AP_vector.size()
                                  - myAllTheFlowStatistics.numberVoIPUploadFlows;
              else
                indexForVector =  (*indexSTA)->GetStaid()
                                  - AP_vector.size();

              // 'std::isnan' checks if the value is not a number                                      
              if (!std::isnan(myAllTheFlowStatistics.FlowStatisticsVoIPDownload[indexForVector].lastIntervalDelay)) {
                if (myparam.verboseLevel > 0)
                  std::cout << "\tDelay: " << myAllTheFlowStatistics.FlowStatisticsVoIPDownload[indexForVector ].lastIntervalDelay 
                            << "\tThroughput: " << myAllTheFlowStatistics.FlowStatisticsVoIPDownload[indexForVector ].lastIntervalRxBytes * 8 / myparam.timeInterval
                            //<< "\t indexForVector is " << indexForVector
                            ;
                // if the latency of this STA is the highest one so far, update the value of the highest latency
                if (  myAllTheFlowStatistics.FlowStatisticsVoIPDownload[ indexForVector ].lastIntervalDelay > highestLatencyVoIPFlows && 
                      !std::isnan(myAllTheFlowStatistics.FlowStatisticsVoIPDownload[ indexForVector ].lastIntervalDelay))

                  highestLatencyVoIPFlows = myAllTheFlowStatistics.FlowStatisticsVoIPDownload[ indexForVector ].lastIntervalDelay;
              }
              else {
                if (myparam.verboseLevel > 0) 
                  std::cout << "\tDelay not defined in this period" 
                            //<< "\t indexForVector is " << indexForVector
                            ;
              }
            } 

            // TCP upload
            else if ((*indexSTA)->Gettypeofapplication () == 3) {
              if (myparam.verboseLevel > 0)
                std::cout << "\tTCP upload";

              // index for the vector of statistics of TCPload flows
              uint32_t indexForVector;
              if (myparam.eachSTArunsAllTheApps == false)
                indexForVector =  (*indexSTA)->GetStaid()
                                  - AP_vector.size()
                                  - myAllTheFlowStatistics.numberVoIPDownloadFlows
                                  - myAllTheFlowStatistics.numberTCPUploadFlows;
              else
                indexForVector =  (*indexSTA)->GetStaid()
                                  - AP_vector.size();

              // 'std::isnan' checks if the value is not a number
              if (!std::isnan(myAllTheFlowStatistics.FlowStatisticsTCPUpload[ indexForVector ].lastIntervalRxBytes)) {
                if (myparam.verboseLevel > 0)
                std::cout << "\tThroughput: " << myAllTheFlowStatistics.FlowStatisticsTCPUpload[ indexForVector ].lastIntervalRxBytes * 8 / myparam.timeInterval 
                          //<< "\t indexForVector is " << indexForVector
                          ;
              }
              else {
                if (myparam.verboseLevel > 0)
                  std::cout << "\tThroughput not defined in this period" 
                            //<< "\t (*indexSTA)->GetStaid()  - AP_vector.size() is " << (*indexSTA)->GetStaid() - AP_vector.size()
                            ;
              }
            }

            // TCP download
            else if ((*indexSTA)->Gettypeofapplication () == 4) {
              if (myparam.verboseLevel > 0)
                std::cout << "\tTCP download";

              // index for the vector of statistics of TCPDownload flows
              uint32_t indexForVector;
              if (myparam.eachSTArunsAllTheApps == false)
                indexForVector =  (*indexSTA)->GetStaid()
                                  - AP_vector.size()
                                  - myAllTheFlowStatistics.numberVoIPDownloadFlows
                                  - myAllTheFlowStatistics.numberTCPUploadFlows
                                  - myAllTheFlowStatistics.numberTCPUploadFlows;
              else
                indexForVector =  (*indexSTA)->GetStaid()
                                  - AP_vector.size();

              // 'std::isnan' checks if the value is not a number
              if (!std::isnan(myAllTheFlowStatistics.FlowStatisticsTCPDownload[ indexForVector ].lastIntervalRxBytes)) {
                if (myparam.verboseLevel > 0)
                std::cout << "\tThroughput: " << myAllTheFlowStatistics.FlowStatisticsTCPDownload[ indexForVector ].lastIntervalRxBytes * 8 / myparam.timeInterval 
                          //<< "\t indexForVector is " << indexForVector
                          ;

              } else {
                if (myparam.verboseLevel > 0)
                  std::cout << "\tThroughput not defined in this period" 
                            //<< "\t indexForVector is " << indexForVector
                            ;
              }
            }

            // Video download
            else if ((*indexSTA)->Gettypeofapplication () == 5) {
              if (myparam.verboseLevel > 0)
                std::cout << "\tVideo download";

              // index for the vector of statistics of VideoDownload flows
              uint32_t indexForVector;
              if (myparam.eachSTArunsAllTheApps == false)
                indexForVector =  (*indexSTA)->GetStaid()
                                  - AP_vector.size()
                                  - myAllTheFlowStatistics.numberVoIPDownloadFlows
                                  - myAllTheFlowStatistics.numberTCPUploadFlows
                                  - myAllTheFlowStatistics.numberTCPUploadFlows
                                  - myAllTheFlowStatistics.numberTCPDownloadFlows;
              else
                indexForVector =  (*indexSTA)->GetStaid()
                                  - AP_vector.size();


              // 'std::isnan' checks if the value is not a number
              if (!std::isnan(myAllTheFlowStatistics.FlowStatisticsVideoDownload[ indexForVector ].lastIntervalRxBytes)) {
                if (myparam.verboseLevel > 0)
                std::cout << "\t\t"
                          << "\tThroughput: " << myAllTheFlowStatistics.FlowStatisticsVideoDownload[ indexForVector ].lastIntervalRxBytes * 8 / myparam.timeInterval 
                          //<< "\t indexForVector is " << indexForVector
                          ;
              } else {
                if (myparam.verboseLevel > 0)
                  std::cout << "\tThroughput not defined in this period" 
                            //<< "\t indexForVector is " << indexForVector
                            ;
              }
            }
            if (myparam.verboseLevel > 0)           
              std::cout << "\n";
          }

        }
        else {
          // this STA is not associated to any AP
          // these STAs will be listed together below
          numberSTAsNonAssociated ++;
        }
      }

      // Adjust the value of the AMPDU

      // Variable to store the new value of the max AMPDU
      uint32_t newAmpduValue;
      
      // Variable to store the current value of the max AMPDU
      uint32_t currentAmpduValue = (*indexAP)->GetMaxSizeAmpdu();

      // Variable to store the minimum AMPDU value
      uint32_t minimumAmpduValue = MTU + 100;

      // First method to adjust AMPDU: linear increase and linear decrease
      if ( myparam.methodAdjustAmpdu == 0 ) {
        // if the latency is above the latency budget, we decrease the AMPDU value
        if ( highestLatencyVoIPFlows > myparam.latencyBudget ) {

          // linearly decrease the AMPDU value

          // check if the current value is smaller than the step
          if (currentAmpduValue < ( AGGRESSIVENESS * myparam.stepAdjustAmpdu ) ) {
            // I can only decrease to the minimum
            newAmpduValue = minimumAmpduValue;

          } else {
            // decrease a step
            newAmpduValue = currentAmpduValue - ( AGGRESSIVENESS * myparam.stepAdjustAmpdu );

            // make sure that the value is at least the minimum
            if ( newAmpduValue < minimumAmpduValue )
              newAmpduValue = minimumAmpduValue;
          }

        // if the latency is below the latency budget, we increase the AMPDU value
        } else {
          // increase the AMPDU value
          newAmpduValue = std::min(( currentAmpduValue + myparam.stepAdjustAmpdu ), myparam.maxAmpduSize); // avoid values above the maximum
        }
      }

      // Second method to adjust AMPDU: linear increase (double aggressiveness, i.e. factor of 2), drastic decrease (instantaneous reduction to the minimum),  
      else if ( myparam.methodAdjustAmpdu == 1 ) {

        //  if the latency is above the latency budget
        if ( highestLatencyVoIPFlows > myparam.latencyBudget ) {
          // decrease the AMPDU value
          newAmpduValue = minimumAmpduValue;

        // if the latency is below the latency budget  
        } else {
          // increase the AMPDU value
          newAmpduValue = std::min(( currentAmpduValue + ( 2 * myparam.stepAdjustAmpdu) ), myparam.maxAmpduSize); // avoid values above the maximum
        }
      }

      // Third method to adjust AMPDU: half of what is left 
      else if ( myparam.methodAdjustAmpdu == 2 ) {

        //  if the latency is above the latency budget
        if ( highestLatencyVoIPFlows > myparam.latencyBudget ) {
          // decrease the AMPDU value
          newAmpduValue = std::floor((currentAmpduValue - minimumAmpduValue) / 2);

        // if the latency is below the latency budget  
        } else {
          // increase the AMPDU value
          newAmpduValue = currentAmpduValue + std::ceil(( myparam.maxAmpduSize - currentAmpduValue + 1 ) / 2);
        }
      }

      // Fourth method to adjust AMPDU: geometric increase (x2) and geometric decrease (x0.618)
      else if ( myparam.methodAdjustAmpdu == 3 ) {

        //  if the latency is below the latency budget
        if ( highestLatencyVoIPFlows < myparam.latencyBudget ) {
          // increase the AMPDU value
          newAmpduValue = std::min(uint32_t( currentAmpduValue * 2), myparam.maxAmpduSize);

        // if the latency is above the latency budget  
        } else {
          // decrease the AMPDU value
          newAmpduValue = std::max(uint32_t( currentAmpduValue * 0.618 ), minimumAmpduValue); // avoid values above the maximum
        }
      }

      // Fifth method for adjusting the AMPDU
      else if ( myparam.methodAdjustAmpdu == 4 ) {

        //  if the latency is above the latency budget
        if ( highestLatencyVoIPFlows > myparam.latencyBudget ) {
          if (myparam.verboseLevel > 2)
            std::cout << "[adjustAMPDU] above latency\n";

          *aboveLatencyAmpduValue = std::max( currentAmpduValue - myparam.stepAdjustAmpdu, minimumAmpduValue);
          *belowLatencyAmpduValue = std::max( *belowLatencyAmpduValue - myparam.stepAdjustAmpdu, minimumAmpduValue);
          newAmpduValue = std::ceil((*aboveLatencyAmpduValue + *belowLatencyAmpduValue + 1 ) / 2);
          if ( newAmpduValue > myparam.maxAmpduSize ) 
            newAmpduValue = myparam.maxAmpduSize;
          if ( newAmpduValue < minimumAmpduValue ) 
            newAmpduValue = minimumAmpduValue;

        } else if (std::abs( myparam.latencyBudget - highestLatencyVoIPFlows ) > 0.001 ) {      
          // if the latency is not very close to the latency budget (epsilon = 0.001 s)
          if (myparam.verboseLevel > 2)
            std::cout << "[adjustAMPDU] not very close to the limit\n";

          *belowLatencyAmpduValue = std::min( currentAmpduValue + myparam.stepAdjustAmpdu, myparam.maxAmpduSize); // avoid values above the maximum
          *aboveLatencyAmpduValue = std::min( *aboveLatencyAmpduValue + myparam.stepAdjustAmpdu, myparam.maxAmpduSize); // avoid values above the maximum

          newAmpduValue = std::ceil((*aboveLatencyAmpduValue + *belowLatencyAmpduValue + 1 ) / 2);
          if ( newAmpduValue > myparam.maxAmpduSize ) 
            newAmpduValue = myparam.maxAmpduSize;
          if ( newAmpduValue < minimumAmpduValue ) 
            newAmpduValue = minimumAmpduValue;

        } else {
          // do nothing
          if (myparam.verboseLevel > 2)
            std::cout << "[adjustAMPDU] very close to the limit\n";
          newAmpduValue = currentAmpduValue;
        }

        if (myparam.verboseLevel > 2) {
          std::cout << Simulator::Now ().GetSeconds()  << '\t';
          std::cout << "[adjustAMPDU] latencyBudget: " << myparam.latencyBudget << '\t';
          std::cout << "highest latency: " << highestLatencyVoIPFlows << '\t';
          std::cout << "currentAmpduValue: " << currentAmpduValue << '\t';
          std::cout << "belowLatencyAmpduValue: " << *belowLatencyAmpduValue << '\t';
          std::cout << "aboveLatencyAmpduValue: " << *aboveLatencyAmpduValue << '\t';
          std::cout << "newAmpduValue: " << newAmpduValue << '\n';
        }
      }

      // Sixth method for adjusting the AMPDU: drastic increase (instantaneous increase to the maximum) and linear decrease (double aggressiveness, i.e. factor of 2)
      else if ( myparam.methodAdjustAmpdu == 5 ) {

        //  if the latency is above the latency budget
        if ( highestLatencyVoIPFlows > myparam.latencyBudget ) {
          // linearly decrease the AMPDU value

          // check if the current value is smaller than the step
          if (currentAmpduValue < ( AGGRESSIVENESS * myparam.stepAdjustAmpdu ) ) {
            // I can only decrease to the minimum
            newAmpduValue = minimumAmpduValue;

          } else {
            // decrease a step
            newAmpduValue = currentAmpduValue - ( AGGRESSIVENESS * myparam.stepAdjustAmpdu );

            // make sure that the value is at least the minimum
            if ( newAmpduValue < minimumAmpduValue )
              newAmpduValue = minimumAmpduValue;
          }

        // if the latency is below the latency budget  
        } else {
          // increase the AMPDU value to the maximum
          newAmpduValue = myparam.maxAmpduSize;
        }
      }

      // If the selected method does not exist, exit
      else {
        std::cout << "AMPDU adjust method unknown\n";
        exit (1);
      }

      // write the AMPDU value to a file (it is written at the end of the file)
      if ( myparam.mynameAMPDUFile != "" ) {

        std::ofstream ofsAMPDU;
        ofsAMPDU.open ( myparam.mynameAMPDUFile, std::ofstream::out | std::ofstream::app); // with "trunc" Any contents that existed in the file before it is open are discarded. with "app", all output operations happen at the end of the file, appending to its existing contents

        ofsAMPDU << Simulator::Now().GetSeconds() << "\t";    // timestamp
        ofsAMPDU << GetAnAP_Id((*indexAP)->GetMac()) << "\t"; // write the ID of the AP to the file
        ofsAMPDU << "AP\t";                                   // type of node
        ofsAMPDU << "-\t";                                    // It is not associated to any AP, since it is an AP
        ofsAMPDU << newAmpduValue << "\n";                    // new value of the AMPDU
      }

      // Check if the AMPDU has to be modified or not
      if (newAmpduValue == currentAmpduValue) {

        // Report that the AMPDU has not been modified
        if (myparam.verboseLevel > 0)
          std::cout << Simulator::Now ().GetSeconds()
                    << "\t[adjustAMPDU]"
                    //<< "\tAP #" << GetAnAP_Id((*indexAP)->GetMac())
                    << "\t    Highest Latency of VoIP flows: " << highestLatencyVoIPFlows << "s (limit " << myparam.latencyBudget << " s)"
                    //<< "\twith MAC: " << (*indexAP)->GetMac() 
                    << "\tAMPDU of the AP not changed (" << (*indexAP)->GetMaxSizeAmpdu() << ")"
                    << std::endl;

      // the AMPDU of the AP has to be modified
      } else {

        // Modify the AMPDU value of the AP itself
        ModifyAmpdu ( GetAnAP_Id((*indexAP)->GetMac()), newAmpduValue, 1 );
        Modify_AP_Record (GetAnAP_Id((*indexAP)->GetMac()), (*indexAP)->GetMac(), newAmpduValue );

        // Report the AMPDU modification
        if (myparam.verboseLevel > 0) {
          std::cout << Simulator::Now ().GetSeconds()
                    << "\t[adjustAMPDU]"
                    //<< "\tAP #" << GetAnAP_Id((*indexAP)->GetMac())
                    << "\t    Highest Latency of VoIP flows: " << highestLatencyVoIPFlows;
                    //<< "\twith MAC: " << (*indexAP)->GetMac();

          if ( newAmpduValue > currentAmpduValue )
            std::cout << "\tAMPDU of the AP increased to " << (*indexAP)->GetMaxSizeAmpdu();
          else 
            std::cout << "\tAMPDU of the AP reduced to " << (*indexAP)->GetMaxSizeAmpdu();

          std::cout << std::endl;
        }


        // Modify the AMPDU value of the STAs associated to the AP which are NOT running VoIP (VoIP STAs never use aggregation)
        for (STA_recordVector::const_iterator indexSTA = sta_vector.begin (); indexSTA != sta_vector.end (); indexSTA++) {

          // if the STA is associated
          if ((*indexSTA)->GetAssoc()) {

            // if the STA is NOT running VoIP
            if ( ( (*indexSTA)->Gettypeofapplication () > 2) ) {

              // auxiliar string
              std::ostringstream auxString;
              // create a string with the MAC
              auxString << "02-06-" << (*indexSTA)->GetMacOfitsAP();
              std::string addressOfTheAPwhereThisSTAis = auxString.str();

              // if the STA is associated to this AP
              if ( (*indexAP)->GetMac() == addressOfTheAPwhereThisSTAis ) {
                // modify the AMPDU value
                ModifyAmpdu ((*indexSTA)->GetStaid(), newAmpduValue, 1);  // modify the AMPDU in the STA node
                (*indexSTA)->SetMaxSizeAmpdu(newAmpduValue);              // update the data in the STA_record structure

                // Report this modification
                if (myparam.verboseLevel > 0) {
                  std::cout << Simulator::Now ().GetSeconds() 
                            << "\t[adjustAMPDU]"
                            << "\t\t\tSTA #" << (*indexSTA)->GetStaid() 
                            //<< "\tassociated to AP #" << GetAnAP_Id(addressOfTheAPwhereThisSTAis) 
                            //<< "\twith MAC " << (*indexSTA)->GetMacOfitsAP()
                            ;

                  if ((*indexSTA)->Gettypeofapplication () == 3)
                    std::cout << "\t TCP upload";
                  else if ((*indexSTA)->Gettypeofapplication () == 4)
                    std::cout << "\t TCP download";
                  else if ((*indexSTA)->Gettypeofapplication () == 5)
                    std::cout << "\t Video download";

                  if ( newAmpduValue > currentAmpduValue )
                    std::cout << "\t\tAMPDU of the STA increased to " << newAmpduValue;
                  else 
                    std::cout << "\t\tAMPDU of the STA reduced to " << newAmpduValue;

                  std::cout << "\n";              
                }

                // write the new AMPDU value to a file (it is written at the end of the file)
                if ( myparam.mynameAMPDUFile != "" ) {

                  std::ofstream ofsAMPDU;
                  ofsAMPDU.open ( myparam.mynameAMPDUFile, std::ofstream::out | std::ofstream::app); // with "trunc" Any contents that existed in the file before it is open are discarded. with "app", all output operations happen at the end of the file, appending to its existing contents

                  ofsAMPDU << Simulator::Now().GetSeconds() << "\t";    // timestamp
                  ofsAMPDU << (*indexSTA)->GetStaid() << "\t";          // ID of the AP
                  ofsAMPDU << "STA \t";
                  ofsAMPDU << GetAnAP_Id((*indexAP)->GetMac()) << "\t";
                  ofsAMPDU << newAmpduValue << "\n";                    // new value of the AMPDU
                }
              }
            }
          }
        }
      }
    }
    i++;
  }

  // if needed, list the STAs that are NOT associated to any AP
  if (numberSTAsNonAssociated > 0) {
    if (myparam.verboseLevel > 0)
      std::cout << Simulator::Now ().GetSeconds() 
                << "\t[adjustAMPDU]"
                << "\tThere are " << numberSTAsNonAssociated
                << " STAs not associated to any AP:"
                << "\n";
    for (STA_recordVector::const_iterator indexSTA = sta_vector.begin (); indexSTA != sta_vector.end (); indexSTA++) {

      // check if the STA is associated to an AP
      if ((*indexSTA)->GetAssoc() == false)
        if (myparam.verboseLevel > 0)
          std::cout << Simulator::Now ().GetSeconds() 
                    << "\t[adjustAMPDU]"
                    << "\t\tSTA #" << (*indexSTA)->GetStaid() 
                    << "\tnot associated to any AP"
                    << "\n";
    }    
  }


  // Reschedule the calculation
  Simulator::Schedule(  Seconds(myparam.timeInterval),
                        &adjustAMPDU,
                        myAllTheFlowStatistics,
                        myparam,
                        belowLatencyAmpduValue,
                        aboveLatencyAmpduValue,
                        myNumberAPs);
}


// Periodically obtain the statistics of the VoIP flows, using Flowmonitor
void obtainKPIs ( Ptr<FlowMonitor> monitor/*, FlowMonitorHelper flowmon*/, 
                  FlowStatistics* myFlowStatistics,
                  uint16_t typeOfFlow,
                  uint32_t verboseLevel,
                  double timeInterval)  //Interval between monitoring moments
{
  if (VERBOSE_FOR_DEBUG >= 1)
    std::cout << Simulator::Now().GetSeconds()
              << "\t[obtainKPIs] Starting function 'obtainKPIs' with type of flow " << (uint16_t)typeOfFlow
              << " (" << typeOfFlow * 10000 
              << ")\n"; 

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  // for each flow, obtain and update the statistics
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i) {

    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);

    // note: i->first starts from '1', not from '0'
    // to use it as an index for the statistics, you must use 'i->first - 1'

    if (VERBOSE_FOR_DEBUG >= 1)
      std::cout << Simulator::Now().GetSeconds()
                << "\t[obtainKPIs] flow #" << i->first 
                << ". dst port: " << t.destinationPort 
                << ". src port: " << t.sourcePort
                << "\n";

    uint32_t RxPacketsThisInterval;
    uint32_t lostPacketsThisInterval;
    uint32_t RxBytesThisInterval;
    double averageLatencyThisInterval;
    double averageJitterThisInterval;

    // avoid the flows of ACKs (TCP also generates flows of ACKs in the opposite direction)
    // TCP download flows generate:
    //  - the information flow:
    //      - destination port 40000, 40001, etc.
    //      - source port 49153. If two flows depart from the same server (e.g. in TCP upload), port 49154 is also used
    //  - the ACK flow
    //      - destination port 49153, 49154.
    //      - source port 40000, 40001, etc.
    if (t.destinationPort / 10000 != typeOfFlow ) {
      if (VERBOSE_FOR_DEBUG >= 1)
        std::cout << Simulator::Now().GetSeconds()
                  << "\t[obtainKPIs] flow #" << i->first 
                  //<< ". dst port: " << t.destinationPort 
                  //<< ". src port: " << t.sourcePort
                  << ". This is not one of the desired flows (" << typeOfFlow * 10000
                  << ")\n";
    }
    else {
      // this MAY be one of the desired flows (or a flow of ACKs)

      if ((t.destinationPort == 49153) || (t.destinationPort == 49154)) {
        // this is a flow of ACKs
        if (VERBOSE_FOR_DEBUG >= 1)
          std::cout << Simulator::Now().GetSeconds()
                    << "\t[obtainKPIs] flow #" << i->first 
                    //<< ". dst port: " << t.destinationPort 
                    //<< ". src port: " << t.sourcePort
                    << ". This is a flow of ACKs"
                    << "\n";
      }
      else {
        // this is NOT a flow of ACKs
        if (VERBOSE_FOR_DEBUG >= 1)
          std::cout << Simulator::Now().GetSeconds()
                    << "\t[obtainKPIs] flow #" << i->first 
                    //<< ". dst port: " << t.destinationPort 
                    //<< ". src port: " << t.sourcePort
                    << ". This is one of the desired flows (" << typeOfFlow * 10000
                    << ")\n";

        // obtain the average latency and jitter only in the last interval

        // note: i->first starts from '1', not from '0'
        // to use it as an index for the statistics, you must use 'i->first - 1'
        
        // the numbers MUST be positive
        if (i->second.rxPackets >= myFlowStatistics[i->first - 1].acumRxPackets)
          RxPacketsThisInterval = i->second.rxPackets - myFlowStatistics[i->first - 1].acumRxPackets;
        else
          RxPacketsThisInterval = 0;

        if (i->second.lostPackets >= myFlowStatistics[i->first - 1].acumLostPackets)
          lostPacketsThisInterval = i->second.lostPackets - myFlowStatistics[i->first - 1].acumLostPackets;
        else
          lostPacketsThisInterval = 0;

        if (i->second.rxBytes > myFlowStatistics[i->first - 1].acumRxBytes)
          RxBytesThisInterval = i->second.rxBytes - myFlowStatistics[i->first - 1].acumRxBytes;
        else
          RxBytesThisInterval = 0;

        if (RxPacketsThisInterval > 0) {
          averageLatencyThisInterval = (i->second.delaySum.GetSeconds() - myFlowStatistics[i->first - 1].acumDelay) / RxPacketsThisInterval;
          averageJitterThisInterval = (i->second.jitterSum.GetSeconds() - myFlowStatistics[i->first - 1].acumJitter) / RxPacketsThisInterval;          
        }
        else {
          averageLatencyThisInterval = 0;
          averageJitterThisInterval = 0;
        }


        // update the values of the statistics
        myFlowStatistics[i->first - 1].acumDelay = i->second.delaySum.GetSeconds();
        myFlowStatistics[i->first - 1].acumJitter = i->second.jitterSum.GetSeconds();
        myFlowStatistics[i->first - 1].acumRxPackets = i->second.rxPackets;
        myFlowStatistics[i->first - 1].acumLostPackets = i->second.lostPackets;
        myFlowStatistics[i->first - 1].acumRxBytes = i->second.rxBytes;
        myFlowStatistics[i->first - 1].lastIntervalDelay = averageLatencyThisInterval;
        myFlowStatistics[i->first - 1].lastIntervalJitter = averageJitterThisInterval;
        myFlowStatistics[i->first - 1].lastIntervalRxPackets = RxPacketsThisInterval;
        myFlowStatistics[i->first - 1].lastIntervalLostPackets = lostPacketsThisInterval;
        myFlowStatistics[i->first - 1].lastIntervalRxBytes = RxBytesThisInterval;

        if (verboseLevel > 1) {

          std::cout << Simulator::Now().GetSeconds()
                    << "\t[obtainKPIs] flow #" << i->first
                    << ". dst port: " << t.destinationPort
                    << ". src port: " << t.sourcePort;

          if (t.destinationPort / 10000 == 1 )
            std::cout << "\tVoIP upload\n";
          else if (t.destinationPort / 10000 == 2 )
            std::cout << "\tVoIP download\n";
          else if (t.destinationPort / 1000 == 55 )
            std::cout << "\tTCP upload\n";
          else if (t.destinationPort / 10000 == 3 )
            std::cout << "\tTCP download\n";
          else if (t.destinationPort / 10000 == 6 )
            std::cout << "\tVideo download\n";

          if (verboseLevel > 2) {
            std::cout << "\t\t\tAcum delay at the end of the period: " << i->second.delaySum.GetSeconds() << " [s]\n";
            std::cout << "\t\t\tAcum number of Rx packets: " << i->second.rxPackets << "\n";
            std::cout << "\t\t\tAcum number of Rx bytes: " << i->second.rxBytes << "\n";
            std::cout << "\t\t\tAcum number of lost packets: " << i->second.lostPackets << "\n"; // FIXME
            std::cout << "\t\t\tAcum throughput: " << i->second.rxBytes * 8.0 / (Simulator::Now().GetSeconds() - INITIALTIMEINTERVAL) << "  [bps]\n";  // throughput
            //The previous line does not work correctly. If you add 'monitor->CheckForLostPackets (0.01)' at the beginning of the function, the number
            //of lost packets seems to be higher. However, the obtained number does not correspond to the final number
          }
          std::cout << "\t\t\tAverage delay this period: " << averageLatencyThisInterval << " [s]\n";
          std::cout << "\t\t\tAverage jitter this period: " << averageJitterThisInterval << " [s]\n";
          std::cout << "\t\t\tNumber of Rx packets this period: " << RxPacketsThisInterval << "\n";
          std::cout << "\t\t\tNumber of Rx bytes this period: " << RxBytesThisInterval << "\n";
          std::cout << "\t\t\tNumber of lost packets this period: " << lostPacketsThisInterval << "\n"; // FIXME: This does not work correctly
          std::cout << "\t\t\tThroughput this period: " << RxBytesThisInterval * 8.0 / timeInterval << "  [bps]\n\n";  // throughput
        }
      }
    }
  }

  // Reschedule the calculation
  Simulator::Schedule(  Seconds(timeInterval),
                        &obtainKPIs,
                        monitor/*, flowmon*/, 
                        myFlowStatistics,
                        typeOfFlow,
                        verboseLevel,
                        timeInterval);

  if (VERBOSE_FOR_DEBUG >= 1)
    std::cout << Simulator::Now().GetSeconds()
              << "\t[obtainKPIs] Ending function 'obtainKPIs'"
              << '\n'; 
}


// Periodically obtain the statistics of the flows, using Flowmonitor
void obtainKPIsMultiTCP ( Ptr<FlowMonitor> monitor, 
                          FlowStatistics* myFlowStatistics,
                          uint16_t numSTAs,
                          uint16_t TcpDownMultiConnection,
                          uint32_t verboseLevel,
                          double timeInterval)  //Interval between monitoring moments
{
  // this function is only used for multi TCP connections
  uint16_t typeOfFlow = INITIALPORT_TCP_DOWNLOAD / 10000;

  // I am using all 3xxxx ports
  NS_ASSERT(TcpDownMultiConnection * numSTAs < 10000);

  if (VERBOSE_FOR_DEBUG >= 1)
    std::cout << Simulator::Now().GetSeconds()
              << "\t[obtainKPIsMultiTCP] Starting function 'obtainKPIsMultiTCP' with " << (uint16_t)TcpDownMultiConnection
              << " connections for each of the " << numSTAs
              << " TCP STAs. Total " << TcpDownMultiConnection * numSTAs
              << " (port " << typeOfFlow 
              << "xxxx) flows\n"; 

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  
  // FIXME: add the rest of the variables 'ThisInterval'

  uint32_t RxPacketsThisInterval[numSTAs];
  //uint32_t lostPacketsThisInterval = 0;
  uint32_t RxBytesThisInterval[numSTAs];
  //double averageLatencyThisInterval = 0;
  //double averageJitterThisInterval = 0;

  uint32_t RxPacketsBeforeThisInterval[numSTAs];
  uint32_t RxBytesBeforeThisInterval[numSTAs];

  for (uint16_t k = 0; k < numSTAs; k++) {
    RxPacketsThisInterval[k] = 0;
    RxBytesThisInterval[k] = 0;

    RxPacketsBeforeThisInterval[k] = myFlowStatistics[k].acumRxPackets;
    RxBytesBeforeThisInterval[k] = myFlowStatistics[k].acumRxBytes;
  }

  // for each flow found, obtain and update the statistics
  // in the case of having a TCP download with multiple TCP connections, the statistics
  //of a number of flows will be accumulated
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i) {

    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);

    // note: i->first starts from '1', not from '0'
    // to use it as an index for the statistics, you must use 'i->first - 1'

    if (verboseLevel >= 2)
      std::cout << Simulator::Now().GetSeconds()
                << "\t[obtainKPIsMultiTCP] flow #" << i->first 
                << ". dst port: " << t.destinationPort 
                << ". src port: " << t.sourcePort;

    // avoid the flows of ACKs (TCP also generates flows of ACKs in the opposite direction)
    // TCP download flows generate:
    //  - the information flow:
    //      - destination port 50000, 50001, etc.
    //      - source port 49153. If two flows depart from the same server (e.g. in TCP upload), port 49154 is also used
    //  - the ACK flow
    //      - destination port 49153, 49154.
    //      - source port 50000, 50001, etc.
    if (t.destinationPort / 10000 != typeOfFlow ) {
      if (VERBOSE_FOR_DEBUG >= 1)
        std::cout //<< Simulator::Now().GetSeconds()
                  //<< "\t[obtainKPIsMultiTCP] flow #" << i->first 
                  //<< ". dst port: " << t.destinationPort 
                  //<< ". src port: " << t.sourcePort
                  << ". This is not one of the desired flows (" << typeOfFlow
                  << "xxxx)\n";
    }
    else { // if (t.destinationPort / 10000 == typeOfFlow ) 

      uint16_t flowIndex = (t.destinationPort - INITIALPORT_TCP_DOWNLOAD) % TcpDownMultiConnection;
      NS_ASSERT (flowIndex <= TcpDownMultiConnection);

      uint16_t staIndex = (t.destinationPort - INITIALPORT_TCP_DOWNLOAD) / TcpDownMultiConnection;
      NS_ASSERT (staIndex <= numSTAs);

      if (verboseLevel >= 2)
        // note: 'k' is NOT the STA id, but the index of the STA here
        std::cout //<< Simulator::Now().GetSeconds()
                  //<< "\t[obtainKPIsMultiTCP] flow #" << i->first 
                  //<< ". dst port: " << t.destinationPort 
                  //<< ". src port: " << t.sourcePort
                  << " (" << typeOfFlow
                  << "xxxx). Corresponds to flow #" << flowIndex
                  << " of STA num" << staIndex
                  << ". Rx packets " << i->second.rxPackets
                  << ". Rx bytes " << i->second.rxBytes
                  << "\n";

      //myFlowStatistics[staIndex].acumRxPackets = myFlowStatistics[staIndex].acumRxPackets + i->second.rxPackets;
      //myFlowStatistics[staIndex].acumRxBytes = myFlowStatistics[staIndex].acumRxBytes + i->second.rxBytes;

      RxPacketsThisInterval[staIndex] = RxPacketsThisInterval[staIndex] + i->second.rxPackets;
      RxBytesThisInterval[staIndex] = RxBytesThisInterval[staIndex] + i->second.rxBytes;
      // obtain the average latency and jitter only in the last interval

      // note: i->first starts from '1', not from '0'      
    }

    // ACK flows start in 49153
    if ((t.destinationPort / 1000 == 49)) {
      // this is a flow of ACKs
      if (VERBOSE_FOR_DEBUG >= 1)
        std::cout //<< Simulator::Now().GetSeconds()
                  //<< "\t[obtainKPIs] flow #" << i->first 
                  //<< ". dst port: " << t.destinationPort 
                  //<< ". src port: " << t.sourcePort
                  << ". This is a flow of ACKs"
                  << "\n\n";
    }
  }
  // the 'for' ends here


  if (verboseLevel > 1) {
    std::cout << Simulator::Now().GetSeconds()
              << "\t[obtainKPIsMultiTCP] Summary multiTCP flows\n";
  }

  for (uint16_t k = 0; k < numSTAs; k++) {
    myFlowStatistics[k].acumRxPackets = RxPacketsThisInterval[k];
    myFlowStatistics[k].acumRxBytes = RxBytesThisInterval[k];

    if (verboseLevel >= 2)
      // note: 'k' is NOT the STA id, but the index of the STA here
      std::cout << "\t\t\tSTA mum" << k
                << ". Acum Rx packets: " << myFlowStatistics[k].acumRxPackets
                << ". Acum Rx bytes: " << myFlowStatistics[k].acumRxBytes
                << "\n";

    myFlowStatistics[k].lastIntervalRxPackets = myFlowStatistics[k].acumRxPackets - RxPacketsBeforeThisInterval[k];
    myFlowStatistics[k].lastIntervalRxBytes = myFlowStatistics[k].acumRxBytes - RxBytesBeforeThisInterval[k];
  }    

  // Reschedule the calculation
  Simulator::Schedule(  Seconds(timeInterval),
                        &obtainKPIsMultiTCP,
                        monitor/*, flowmon*/, 
                        myFlowStatistics,
                        numSTAs,
                        TcpDownMultiConnection,
                        verboseLevel,
                        timeInterval);

  if (VERBOSE_FOR_DEBUG >= 1)
    std::cout << Simulator::Now().GetSeconds()
              << "\t[obtainKPIsMultiTCP] Ending function 'obtainKPIsMultiTCP'"
              << "\n\n"; 
}


// Periodically obtain the statistics of the VoIP flows, using Flowmonitor
void saveKPIs ( std::string mynameKPIFile,
                AllTheFlowStatistics myAllTheFlowStatistics,
                uint32_t verboseLevel,
                double timeInterval)  //Interval between monitoring moments
{
  // print the results to a file (they are written at the end of the file)
  if ( mynameKPIFile != "" ) {

    std::ofstream ofs;
    ofs.open ( mynameKPIFile, std::ofstream::out | std::ofstream::app); // with "trunc" Any contents that existed in the file before it is open are discarded. with "app", all output operations happen at the end of the file, appending to its existing contents

    for (uint32_t i = 0; i < myAllTheFlowStatistics.numberVoIPUploadFlows; i++) {
      ofs << Simulator::Now().GetSeconds() << "\t"; // timestamp
      ofs << i << "\t"; // number of the flow
      ofs << "VoIP_upload\t";
      ofs << myAllTheFlowStatistics.FlowStatisticsVoIPUpload[i].destinationPort << "\t"
          << myAllTheFlowStatistics.FlowStatisticsVoIPUpload[i].finalDestinationPort << "\t"
          << myAllTheFlowStatistics.FlowStatisticsVoIPUpload[i].lastIntervalDelay << "\t"
          << myAllTheFlowStatistics.FlowStatisticsVoIPUpload[i].lastIntervalJitter << "\t"
          << myAllTheFlowStatistics.FlowStatisticsVoIPUpload[i].lastIntervalRxPackets << "\t"
          << myAllTheFlowStatistics.FlowStatisticsVoIPUpload[i].lastIntervalLostPackets << "\t"
          << myAllTheFlowStatistics.FlowStatisticsVoIPUpload[i].lastIntervalRxBytes * 8.0 / timeInterval << "\n"; // throughput
    }

    for (uint32_t i = 0; i < myAllTheFlowStatistics.numberVoIPDownloadFlows; i++) {
      ofs << Simulator::Now().GetSeconds() << "\t"; // timestamp
      ofs << i
              + myAllTheFlowStatistics.numberVoIPUploadFlows << "\t"; // number of the flow
      ofs << "VoIP_download\t";
      ofs << myAllTheFlowStatistics.FlowStatisticsVoIPDownload[i].destinationPort << "\t"
          << myAllTheFlowStatistics.FlowStatisticsVoIPDownload[i].finalDestinationPort << "\t"
          << myAllTheFlowStatistics.FlowStatisticsVoIPDownload[i].lastIntervalDelay << "\t"
          << myAllTheFlowStatistics.FlowStatisticsVoIPDownload[i].lastIntervalJitter << "\t"
          << myAllTheFlowStatistics.FlowStatisticsVoIPDownload[i].lastIntervalRxPackets << "\t"
          << myAllTheFlowStatistics.FlowStatisticsVoIPDownload[i].lastIntervalLostPackets << "\t"
          << myAllTheFlowStatistics.FlowStatisticsVoIPDownload[i].lastIntervalRxBytes * 8.0 / timeInterval << "\n"; // throughput
    }

    for (uint32_t i = 0; i < myAllTheFlowStatistics.numberTCPUploadFlows; i++) {
      ofs << Simulator::Now().GetSeconds() << "\t"; // timestamp
      ofs << i 
              + myAllTheFlowStatistics.numberVoIPUploadFlows 
              + myAllTheFlowStatistics.numberVoIPDownloadFlows << "\t"; // number of the flow
      ofs << "TCP_upload\t";
      ofs << myAllTheFlowStatistics.FlowStatisticsTCPUpload[i].destinationPort << "\t"
          << myAllTheFlowStatistics.FlowStatisticsTCPUpload[i].finalDestinationPort << "\t"
          << myAllTheFlowStatistics.FlowStatisticsTCPUpload[i].lastIntervalDelay << "\t"
          << myAllTheFlowStatistics.FlowStatisticsTCPUpload[i].lastIntervalJitter << "\t"
          << myAllTheFlowStatistics.FlowStatisticsTCPUpload[i].lastIntervalRxPackets << "\t"
          << myAllTheFlowStatistics.FlowStatisticsTCPUpload[i].lastIntervalLostPackets << "\t"
          << myAllTheFlowStatistics.FlowStatisticsTCPUpload[i].lastIntervalRxBytes * 8.0 / timeInterval << "\n"; // throughput
    }

    for (uint32_t i = 0; i < myAllTheFlowStatistics.numberTCPDownloadFlows; i++) {
      ofs << Simulator::Now().GetSeconds() << "\t"; // timestamp
      ofs << i 
              + myAllTheFlowStatistics.numberVoIPUploadFlows 
              + myAllTheFlowStatistics.numberVoIPDownloadFlows 
              + myAllTheFlowStatistics.numberTCPUploadFlows << "\t"; // number of the flow
      ofs << "TCP_download\t";
      ofs << myAllTheFlowStatistics.FlowStatisticsTCPDownload[i].destinationPort << "\t"
          << myAllTheFlowStatistics.FlowStatisticsTCPDownload[i].finalDestinationPort << "\t"
          << myAllTheFlowStatistics.FlowStatisticsTCPDownload[i].lastIntervalDelay << "\t"
          << myAllTheFlowStatistics.FlowStatisticsTCPDownload[i].lastIntervalJitter << "\t"
          << myAllTheFlowStatistics.FlowStatisticsTCPDownload[i].lastIntervalRxPackets << "\t"
          << myAllTheFlowStatistics.FlowStatisticsTCPDownload[i].lastIntervalLostPackets << "\t"
          << myAllTheFlowStatistics.FlowStatisticsTCPDownload[i].lastIntervalRxBytes * 8.0 / timeInterval << "\n"; // throughput
    }

    for (uint32_t i = 0; i < myAllTheFlowStatistics.numberVideoDownloadFlows; i++) {
      ofs << Simulator::Now().GetSeconds() << "\t"; // timestamp
      ofs << i  
              + myAllTheFlowStatistics.numberVoIPUploadFlows 
              + myAllTheFlowStatistics.numberVoIPDownloadFlows 
              + myAllTheFlowStatistics.numberTCPUploadFlows 
              + myAllTheFlowStatistics.numberTCPDownloadFlows << "\t"; // number of the flow
      ofs << "Video_download\t"; 
      ofs << myAllTheFlowStatistics.FlowStatisticsVideoDownload[i].destinationPort << "\t"
          << myAllTheFlowStatistics.FlowStatisticsVideoDownload[i].finalDestinationPort << "\t"
          << myAllTheFlowStatistics.FlowStatisticsVideoDownload[i].lastIntervalDelay << "\t"
          << myAllTheFlowStatistics.FlowStatisticsVideoDownload[i].lastIntervalJitter << "\t"
          << myAllTheFlowStatistics.FlowStatisticsVideoDownload[i].lastIntervalRxPackets << "\t"
          << myAllTheFlowStatistics.FlowStatisticsVideoDownload[i].lastIntervalLostPackets << "\t"
          << myAllTheFlowStatistics.FlowStatisticsVideoDownload[i].lastIntervalRxBytes * 8.0 / timeInterval << "\n"; // throughput
    }
  }

  // Reschedule the writing
  Simulator::Schedule(  Seconds(timeInterval),
                        &saveKPIs,
                        mynameKPIFile,
                        myAllTheFlowStatistics,
                        verboseLevel,
                        timeInterval);
}


/*****************************/
/************ main ***********/
/*****************************/
int main (int argc, char *argv[]) {

  //bool populatearpcache = false; // Provisional variable FIXME: It should not be necessary

  /* Variables to store some fixed parameters */
  static uint32_t VoIPg729PayoladSize = 32; // Size of the UDP payload (also includes the RTP header) of a G729a packet with 2 samples
  static double VoIPg729IPT = 0.02; // Time between g729a packets (50 pps)

  static double x_position_first_AP = 0.0;
  static double y_position_first_AP = 0.0;

  double initial_x_position_STA = -15.0; // initial X distance from the first STA to the first AP
  static double y_distance_STA_to_AP = 5.0; // initial Y distance from the first STA to the first AP

  static double pause_time = 2.0;           // maximum pause time for the random waypoint mobility model
  /* end of - Variables to store some fixed parameters */


  /* Variables to store the input parameters. I add the default value */
  uint32_t number_of_APs = 4;
  uint32_t number_of_APs_per_row = 2;
  double distance_between_APs = 50.0; // X-axis and Y-axis distance between APs (meters)

  double distance_between_STAs = 0.0;
  double distanceToBorder = 0.0; // It is used for establishing the coordinates of the square where the STA move randomly

  uint32_t number_of_STAs_per_row;

  uint32_t nodeMobility = 0;
  double constantSpeed = 3.5;  // X-axis speed (m/s) in the case the constant speed model is used (https://en.wikipedia.org/wiki/Preferred_walking_speed)

  double rateAPsWithAMPDUenabled = 1.0; // rate of APs with A-MPDU enabled at the beginning of the simulation

  uint16_t aggregationDisableAlgorithm = 0;  // Set this to 1 in order to make the central control algorithm limiting the AMPDU run
  uint32_t maxAmpduSizeWhenAggregationLimited = 0;  // Only for TCP. Minimum size (to be used when aggregation is 'limited')

  uint16_t aggregationDynamicAlgorithm = 0;  // Set this to 1 in order to make the central control algorithm dynamically modifying AMPDU run
  double latencyBudget = 0.0;  // This is the maximum latency (seconds) tolerated by VoIP applications

  uint16_t topology = 2;    // 0: all the server applications are in a single server
                            // 1: each server application is in a node connected to the hub
                            // 2: each server application is in a node behind the router, connected to it with a P2P connection

  double arpAliveTimeout = 5.0;       // seconds by default
  double arpDeadTimeout = 0.1;       // seconds by default
  uint16_t arpMaxRetries = 30;       // maximum retries or ARPs

  uint32_t TcpPayloadSize = MTU - 52; // 1448 bytes. Prevent fragmentation. Taken from https://www.nsnam.org/doxygen/codel-vs-pfifo-asymmetric_8cc_source.html
  uint32_t VideoMaxPacketSize = MTU - 20 - 8;  //1472 bytes. Remove 20 (IP) + 8 (UDP) bytes from MTU (1500)

  std::string TcpVariant = "TcpNewReno"; // other options "TcpHighSpeed", "TcpWestwoodPlus"
  //std::string TcpVariant = "TcpWestwoodPlus"; // other options "TcpHighSpeed", "TcpWestwoodPlus"

  double simulationTime = 10.0; //seconds

  double timeMonitorKPIs = 0.25;  //seconds

  uint32_t numberVoIPupload = 0;
  uint32_t numberVoIPdownload = 0;
  uint32_t numberTCPupload = 0;
  uint32_t numberTCPdownload = 0;
  uint32_t numberVideoDownload = 0;

  // If 'eachSTArunsAllTheApps' is 
  //  - 'false': a STA will be created per application (default)  
  //  - 'true': each STA will also run all the other active applications (the ones not set to 0).
  //            the total number of STAs will be the sum of the values
  //            if you set a value to 0, then that application will not be run                
  bool eachSTArunsAllTheApps = false;

  //Using different priorities for VoIP and TCP
  //https://groups.google.com/forum/#!topic/ns-3-users/J3BvzGVJhXM
  //https://groups.google.com/forum/#!topic/ns-3-users/n8h8VbIekoQ
  //http://code.nsnam.org/ns-3-dev/file/06676d0e299f/src/wifi/doc/source/wifi-user.rst
  // the selection of the Access Category (AC) for an MSDU is based on the
  // value of the DS field in the IP header of the packet (ToS field in case of IPv4).
  // You can see the values in WireShark:
  //   IEEE 802.11 QoS data
  //     QoS Control
  uint32_t prioritiesEnabled = 0;
  uint8_t VoIpPriorityLevel = 0xc0;   // corresponds to 1100 0000 (AC_VO)
  uint8_t TcpPriorityLevel = 0x00;    // corresponds to 0000 0000 (AC_BK)
  uint8_t VideoPriorityLevel = 0x80;  // corresponds to 1000 0000 (AC_VI) FIXME check if this is what we want

  uint32_t RtsCtsThreshold24GHz = 999999;  // RTS/CTS is disabled by defalult
  uint32_t RtsCtsThreshold5GHz = 999999;  // RTS/CTS is disabled by defalult

  double powerLevel = 30.0;  // in dBm

  std::string rateModel = "Ideal"; // Model for 802.11 rate control (Constant; Ideal; Minstrel)

  bool saveMobility = false;
  bool enablePcap = 0; // set this to 1 and .pcap files will be generated (in the ns-3.26 folder)
  uint32_t verboseLevel = 0; // verbose level.
  uint32_t printSeconds = 0; // print the time every 'printSeconds' simulation seconds
  bool generateHistograms = false; // generate histograms
  std::string outputFileName; // the beginning of the name of the output files to be generated during the simulations
  std::string outputFileSurname; // this will be added to certain files
  bool saveXMLFile = false; // save per-flow results in an XML file

  uint32_t numOperationalChannelsPrimary = 4; // by default, 4 different channels are used in the APs
  uint32_t numOperationalChannelsSecondary = 4; // by default, 4 different channels are used in the APs

  uint32_t channelWidthPrimary = 20;
  uint32_t channelWidthSecondary = 20;

  // https://www.nsnam.org/doxygen/wifi-spectrum-per-example_8cc_source.html
  uint32_t wifiModel = 0;

  uint32_t propagationLossModel = 0; // 0: LogDistancePropagationLossModel (default); 1: FriisPropagationLossModel; 2: FriisSpectrumPropagationLossModel

  uint32_t errorRateModel = 0; // 0 means NistErrorRateModel (default); 1 means YansErrorRateModel


  // estimated coverage of the APs
  // you need to do some preliminary calibration in order to estimate it
  // for that aim, you can use linear mobility and VoIP
  double coverage_24GHz = 0.0;  // estimated coverage of an AP in 2.4 GHz
  double coverage_5GHz = 0.0;  // estimated coverage of an AP in 5 GHz
  
  bool algorithm_load_balancing = false;  // activate (or not) the load balancing between 2.4 an 5GHz
  double periodLoadBalancing = 2.0; // period for the load balancing [seconds]


  uint32_t maxAmpduSize, maxAmpduSizeSecondary;     // taken from https://www.nsnam.org/doxygen/minstrel-ht-wifi-manager-example_8cc_source.html

  uint16_t methodAdjustAmpdu = 0;  // method for adjusting the AMPDU size

  uint32_t stepAdjustAmpdu = STEPADJUSTAMPDUDEFAULT; // step for adjusting the AMPDU size. Assign the default value

  //uint32_t version80211primary = 0; // 0 means 802.11n in 5GHz; 1 means 802.11ac; 2 means 802.11n in 2.4GHz 
  std::string version80211primary = "11ac";

  //uint32_t version80211secondary = 1; // Version of 802.11 in secondary APs and in the secondary device of STAs. 0 means 802.11n in 5GHz; 1 means 802.11ac; 2 means 802.11n in 2.4GHz 
  std::string version80211secondary = "11n2.4";


  uint32_t numberAPsSamePlace = 1;    // Default 1 (one AP on each place). If set to 2, there will be 2 APs on each place, i.e. '2 x number_of_APs' will be created

  std::string APsActive = "*";  // if set to '*', then all the APs will be created
                                // if set to '1011', then APs 0, 2 and 3 will be created, and AP#1 will NOT be created

  uint32_t numberSTAsSamePlace = 1; // Default '1' (one STA on each place). If set to 2, there will be 2 STAs on each place (traveling together), i.e. '2 x number_of_STAs' will be created", numberSTAsSamePlace);

  std::string STAsActive = "*"; // if set to '*', then all the STAs will be created
                                // if set to '1011', then STAs 0, 2 and 3 will be created, and STA#1 will NOT be created

  bool onlyOnePeerSTAallowedAtATime = true; // If 'true' (default), only one of the two peer STAs that are in the same place will be used at the same time

  uint16_t TcpDownMultiConnection = 10;
  /* end of - Variables to store the input parameters */


  // Assign the selected value of the MAX AMPDU
  if ( (version80211primary == "11n5") || (version80211primary == "11n2.4") ) {
    // 802.11n
    maxAmpduSize = MAXSIZE80211n;
  }
  else if (version80211primary == "11ac") {
    // 802.11ac
    maxAmpduSize = MAXSIZE80211ac;
  }
  else if (version80211primary == "11g") {
    // 802.11g
    maxAmpduSize = 0;
  }
  else if (version80211primary == "11a") {
    // 802.11a
    maxAmpduSize = 0;
  }

  // Assign the selected value of the MAX AMPDU
  if ( (version80211secondary == "11n5") || (version80211secondary == "11n2.4") ) {
    // 802.11n
    maxAmpduSizeSecondary = MAXSIZE80211n;
  }
  else if (version80211secondary == "11ac") {
    // 802.11ac
    maxAmpduSizeSecondary = MAXSIZE80211ac;
  }
  else if (version80211secondary == "11g") {
    // 802.11g
    maxAmpduSizeSecondary = 0;
  }
  else if (version80211secondary == "11a") {
    // 802.11a
    maxAmpduSizeSecondary = 0;
  }


  /***************** declaring the command line parser (input parameters) **********************/
  CommandLine cmd;

  // General scenario topology parameters
  cmd.AddValue ("simulationTime", "Simulation time [s]", simulationTime);
  
  cmd.AddValue ("timeMonitorKPIs", "Time interval to monitor KPIs of the flows. Also used for adjusting the AMPDU algorithm. 0 (default) means no monitoring", timeMonitorKPIs);

  cmd.AddValue ("numberVoIPupload", "Number of nodes running VoIP up", numberVoIPupload);
  cmd.AddValue ("numberVoIPdownload", "Number of nodes running VoIP down", numberVoIPdownload);
  cmd.AddValue ("numberTCPupload", "Number of nodes running TCP up", numberTCPupload);
  cmd.AddValue ("numberTCPdownload", "Number of nodes running TCP down", numberTCPdownload);
  cmd.AddValue ("TcpDownMultiConnection", "Number of TCP connections on each node that runs TCP down", TcpDownMultiConnection);
  cmd.AddValue ("numberVideoDownload", "Number of nodes running video down", numberVideoDownload);

  cmd.AddValue ("eachSTArunsAllTheApps", "If this is 'true', all the STAs will run all the other applications. Otherwise, a STA will be created per application", eachSTArunsAllTheApps);

  cmd.AddValue ("number_of_APs", "Number of wifi APs", number_of_APs);
  cmd.AddValue ("number_of_APs_per_row", "Number of wifi APs per row", number_of_APs_per_row);
  cmd.AddValue ("distance_between_APs", "Distance in meters between the APs", distance_between_APs);
  cmd.AddValue ("distanceToBorder", "Distance in meters between the AP and the border of the scenario", distanceToBorder);

  cmd.AddValue ("number_of_STAs_per_row", "Number of wifi STAs per row", number_of_STAs_per_row);
  cmd.AddValue ("distance_between_STAs", "Initial distance in meters between the STAs (only for static and linear mobility)", distance_between_STAs);
  cmd.AddValue ("initial_x_position_STA", "Initial X position of the first STA, in meters (only for static and linear mobility)", initial_x_position_STA);

  cmd.AddValue ("nodeMobility", "Kind of movement of the nodes: '0' static (default); '1' linear; '2' Random Walk 2d; '3' Random Waypoint", nodeMobility);
  cmd.AddValue ("constantSpeed", "Speed of the nodes (in linear and random mobility), default 1.5 m/s", constantSpeed);

  cmd.AddValue ("topology", "Topology: '0' all server applications in a server; '1' all the servers connected to the hub (default); '2' all the servers behind a router", topology);

  // ARP parameters, see https://www.nsnam.org/doxygen/classns3_1_1_arp_cache.html
  cmd.AddValue ("arpAliveTimeout", "ARP Alive Timeout [s]: Time an ARP entry will be in ALIVE state (unless refreshed)", arpAliveTimeout);
  cmd.AddValue ("arpDeadTimeout", "ARP Dead Timeout [s]: Time an ARP entry will be in DEAD state before being removed", arpDeadTimeout);
  cmd.AddValue ("arpMaxRetries", "ARP max retries for a resolution", arpMaxRetries);

  // Aggregation parameters
  // The central controller runs an algorithm that dynamically
  // disables aggregation in an AP if a VoIP flow appears, and
  // enables it when the VoIP user leaves. Therefore, VoIP users
  // will always see a non-aggregating AP, whereas TCP users
  // will receive non-aggregated frames in some moments
  cmd.AddValue ("rateAPsWithAMPDUenabled", "Initial rate of APs with AMPDU aggregation enabled", rateAPsWithAMPDUenabled);
  cmd.AddValue ("aggregationDisableAlgorithm", "Is the algorithm enabling/disabling AMPDU aggregation enabled?", aggregationDisableAlgorithm);
  cmd.AddValue ("maxAmpduSize", "Maximum value of the AMPDU [bytes]", maxAmpduSize);
  // The objective of '--maxAmpduSizeWhenAggregationLimited=8000' is the next:
  // the algorithm is used but, instead of deactivating the aggregation,
  // a maximum size of e.g. 8 kB is set when a VoIP flow is present
  // in an AP, in order to limit the added delay.
  cmd.AddValue ("maxAmpduSizeWhenAggregationLimited", "Max AMPDU size to use when aggregation is limited", maxAmpduSizeWhenAggregationLimited);

  // This algorithm dynamically adjusts AMPDU trying to keep VoIP latency below a threshold ('latencyBudget')
  cmd.AddValue ("aggregationDynamicAlgorithm", "Is the algorithm dynamically adapting AMPDU aggregation enabled?", aggregationDynamicAlgorithm);
  cmd.AddValue ("latencyBudget", "Maximum latency [s] tolerated by VoIP applications", latencyBudget);
  cmd.AddValue ("methodAdjustAmpdu", "Method for adjusting AMPDU size: '0' (default), '1' ...", methodAdjustAmpdu);
  cmd.AddValue ("stepAdjustAmpdu", "Step for adjusting AMPDU size [bytes]", stepAdjustAmpdu);

  // TCP parameters
  cmd.AddValue ("TcpPayloadSize", "Payload size [bytes]", TcpPayloadSize);
  cmd.AddValue ("TcpVariant", "TCP variant: TcpNewReno (default), TcpHighSpeed, TcpWestwoodPlus", TcpVariant);

  // 802.11 priorities, version, channels
  cmd.AddValue ("prioritiesEnabled", "Use different 802.11 priorities for VoIP / TCP: '0' no (default); '1' yes", prioritiesEnabled);
  cmd.AddValue ("numOperationalChannelsPrimary", "Number of different channels to use on the APs: 1, 4 (default), 9, 16", numOperationalChannelsPrimary);
  cmd.AddValue ("channelWidthPrimary", "Width of the wireless channels: 20 (default), 40, 80, 160", channelWidthPrimary);
  cmd.AddValue ("numOperationalChannelsSecondary", "Number of different channels to use on the APs: 1, 4 (default), 9, 16", numOperationalChannelsSecondary);
  cmd.AddValue ("channelWidthSecondary", "Width of the wireless channels: 20 (default), 40, 80, 160", channelWidthSecondary);
  cmd.AddValue ("rateModel", "Model for 802.11 rate control: 'Constant'; 'Ideal'; 'Minstrel')", rateModel);  
  cmd.AddValue ("RtsCtsThreshold24GHz", "Threshold for using RTS/CTS [bytes] in 2.4 GHz. Examples: '0' always; '500' only 500 bytes-packes or higher will require RTS/CTS; '999999' never (default)", RtsCtsThreshold24GHz);
  cmd.AddValue ("RtsCtsThreshold5GHz", "Threshold for using RTS/CTS [bytes] in 5 GHz. Examples: '0' always; '500' only 500 bytes-packes or higher will require RTS/CTS; '999999' never (default)", RtsCtsThreshold5GHz);

  // Wi-Fi power, propagation and error models
  cmd.AddValue ("powerLevel", "Power level of the wireless interfaces (dBm), default 30", powerLevel);
  cmd.AddValue ("wifiModel", "WiFi model: '0' YansWifiPhy (default); '1' SpectrumWifiPhy with MultiModelSpectrumChannel", wifiModel);
  // Path loss exponent in LogDistancePropagationLossModel is 3 and in Friis it is supposed to be lower maybe 2.
  cmd.AddValue ("propagationLossModel", "Propagation loss model: '0' LogDistancePropagationLossModel (default); '1' FriisPropagationLossModel; '2' FriisSpectrumPropagationLossModel", propagationLossModel);
  cmd.AddValue ("errorRateModel", "Error Rate model: '0' NistErrorRateModel (default); '1' YansErrorRateModel", errorRateModel);

  // variables needed for the load balandcing between 2.4 and 5 GHz algorithm
  cmd.AddValue ("coverage_24GHz", "Estimated coverage of APs in 2.4GHz [meters]", coverage_24GHz);
  cmd.AddValue ("coverage_5GHz", "Estimated coverage of APs in 5GHz [meters]", coverage_5GHz);
  cmd.AddValue ("algorithm_load_balancing", "Set to 1 to activate the load balancing between 2.4 and 5 GHz", algorithm_load_balancing);
  cmd.AddValue ("periodLoadBalancing", "Period of the load balancing between 2.4 and 5 GHz", periodLoadBalancing);
  

  // Parameters of the output of the program
  cmd.AddValue ("saveMobility", "Write mobility trace", saveMobility); // creates an output file with the positions of the nodes
  cmd.AddValue ("enablePcap", "Enable/disable pcap file generation", enablePcap);
  cmd.AddValue ("verboseLevel", "Tell echo applications to log if true", verboseLevel);
  cmd.AddValue ("printSeconds", "Periodically print simulation time (even in verboseLevel=0)", printSeconds);
  cmd.AddValue ("generateHistograms", "Generate histograms?", generateHistograms);
  cmd.AddValue ("outputFileName", "First characters to be used in the name of the output files", outputFileName);
  cmd.AddValue ("outputFileSurname", "Other characters to be used in the name of the output files (not in the average one)", outputFileSurname);
  cmd.AddValue ("saveXMLFile", "Save per-flow results to an XML file?", saveXMLFile);

  /* Parameters that allow the manual definition of the scenario */
  cmd.AddValue ("version80211primary", "Version of 802.11 in primary APs and in the primary device of STAs: '11ac' (default); '11n5'; '11n2.4'; '11g'; '11a'", version80211primary);
  cmd.AddValue ("version80211secondary", "Version of 802.11 in secondary APs and in the secondary device of STAs: '11ac' (default); '11n5'; '11n2.4'; '11g'; '11a'", version80211secondary);
  
  // define the APs individually
  cmd.AddValue ("numberAPsSamePlace", "Default 1 (one AP on each place). If set to 2, there will be 2 APs on each place, i.e. '2 x number_of_APs' will be created", numberAPsSamePlace);
  cmd.AddValue ("APsActive", "Define which APs will be active", APsActive);
  // the inactive APs will have no network device

  // define the STAs individually
  cmd.AddValue ("numberSTAsSamePlace", "Default '1' (one STA on each place). If set to 2, there will be 2 STAs on each place (traveling together), i.e. '2 x number_of_STAs' will be created", numberSTAsSamePlace);
  cmd.AddValue ("STAsActive", "Define which STAs will be active", STAsActive);
  cmd.AddValue ("onlyOnePeerSTAallowedAtATime", "If 'true' (default), only one of the two peer STAs that are in the same place will be used at the same time", onlyOnePeerSTAallowedAtATime);
  // the inactive STAs will have network device, but it will be put in an unused channel, where there are no APs

  cmd.Parse (argc, argv);


  // Variable to store the last AMPDU value for which latency was below the limit
  // I use a pointer because it has to be modified by a function
  uint32_t *belowLatencyAmpduValue;
  belowLatencyAmpduValue = (uint32_t *) malloc (1);
  *belowLatencyAmpduValue = MTU + 100;

  // Variable to store the last AMPDU value for which latency was above the limit
  // I use a pointer because it has to be modified by a function
  uint32_t *aboveLatencyAmpduValue;
  aboveLatencyAmpduValue = (uint32_t *) malloc (1);
  *aboveLatencyAmpduValue = maxAmpduSize;


  // If these parameters have not been set, set the default values
  if ( distance_between_STAs == 0.0 )
    distance_between_STAs = distance_between_APs;

  if ( distanceToBorder == 0.0 )
    distanceToBorder = 0.5 * distance_between_APs; // It is used for establishing the coordinates of the square where the STA move randomly


  // Number of STAs
  uint32_t number_of_STAs = numberVoIPupload + numberVoIPdownload + numberTCPupload + numberTCPdownload + numberVideoDownload;;

  double x_position_first_STA = initial_x_position_STA;
  double y_position_first_STA = y_position_first_AP + y_distance_STA_to_AP; // by default, the first STA is located some meters above the first AP
  uint32_t number_of_Servers = number_of_STAs;  // the number of servers is the same as the number of STAs. Each server attends a STA


  //the list of channels is here: https://www.nsnam.org/docs/models/html/wifi-user.html
  // see https://en.wikipedia.org/wiki/List_of_WLAN_channels
 
  //uint8_t availableChannels24GHz20MHz[NUM_CHANNELS_24GHZ] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
  uint8_t availableChannels24GHz20MHz[NUM_CHANNELS_24GHZ] = {1, 6, 11};

  uint8_t availableChannels20MHz[NUM_CHANNELS_5GHZ_20MHZ] = {36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112,
                                                            116, 120, 124, 128, 132, 136, 140, 144, 149, 153, 157, 
                                                            161, 165, 169, 173, 184, 188, 192, 196, 8, 12, 16};

  uint8_t availableChannels40MHz[NUM_CHANNELS_5GHZ_40MHZ] = {38, 46, 54, 62, 102, 110, 118, 126, 134, 142, 151, 159}; 

  uint8_t availableChannels80MHz[NUM_CHANNELS_5GHZ_80MHZ] = {42, 58, 106, 122, 138, 155}; 

  uint8_t availableChannels160MHz[NUM_CHANNELS_5GHZ_160MHZ] = {50, 114};


  // One server interacts with each STA
  if (topology == 0) {
    number_of_Servers = 0;
  } else {
    number_of_Servers = number_of_STAs;
  }


  /********** check input parameters **************/
  // if this is set to 1, a parameter error has been found and the program will be stopped
  BooleanValue error = 0;

  // Test some conditions before starting

  if (( numberTCPdownload == 0 ) && (TcpDownMultiConnection > 0)) {
      std::cout << "INPUT PARAMETER ERROR: You have " << numberTCPdownload << " TCP download STAs. But you have set " << TcpDownMultiConnection << " TCP flows. Stopping the simulation." << '\n';
      error = 1; 
  }

  if ( (version80211primary == "11n5") || (version80211primary == "11n2.4") ) {
    if ( maxAmpduSize > MAXSIZE80211n ) {
      std::cout << "INPUT PARAMETER ERROR: Too high AMPDU size. Selected: " << maxAmpduSize << ". Limit: " << MAXSIZE80211n << ". Stopping the simulation." << '\n';
      error = 1;      
    }
  }
  else if (version80211primary == "11ac") {
    if ( maxAmpduSize > MAXSIZE80211ac ) {
      std::cout << "INPUT PARAMETER ERROR: Too high AMPDU size. Selected: " << maxAmpduSize << ". Limit: " << MAXSIZE80211ac << ". Stopping the simulation." << '\n';
      error = 1;  
    }
  }
  else if ((version80211primary == "11g") || (version80211primary == "11a") ) {
    if ( maxAmpduSize > 0 ) {
      std::cout << "INPUT PARAMETER ERROR: Too high AMPDU size. Selected: " << maxAmpduSize << ". Limit: " << 0 << ". Stopping the simulation." << '\n';
      error = 1;  
    }
  }
  else {
    std::cout << "INPUT PARAMETER ERROR: Wrong version of 802.11. Selected: " << version80211primary << ". Stopping the simulation." << '\n';
    error = 1;      
  }

  if ( (version80211secondary == "11n5") || (version80211secondary == "11n2.4") ) {
    if ( maxAmpduSize > MAXSIZE80211n ) {
      std::cout << "INPUT PARAMETER ERROR: Too high AMPDU size. Selected: " << maxAmpduSize << ". Limit: " << MAXSIZE80211n << ". Stopping the simulation." << '\n';
      error = 1;      
    }
  }
  else if (version80211secondary == "11ac") {
    if ( maxAmpduSize > MAXSIZE80211ac ) {
      std::cout << "INPUT PARAMETER ERROR: Too high AMPDU size. Selected: " << maxAmpduSize << ". Limit: " << MAXSIZE80211ac << ". Stopping the simulation." << '\n';
      error = 1;  
    }
  }
  else if ((version80211secondary == "11g") || (version80211secondary == "11a") ) {
    if ( maxAmpduSize > 0 ) {
      std::cout << "INPUT PARAMETER ERROR: Too high AMPDU size. Selected: " << maxAmpduSize << ". Limit: " << 0 << ". Stopping the simulation." << '\n';
      error = 1;  
    }
  }
  else {
    std::cout << "INPUT PARAMETER ERROR: Wrong version of 802.11. Selected: " << version80211secondary << ". Stopping the simulation." << '\n';
    error = 1;      
  }

  if ((aggregationDisableAlgorithm == 1 ) && (rateAPsWithAMPDUenabled < 1.0 )) {
    std::cout << "INPUT PARAMETER ERROR: The algorithm has to start with all the APs with A-MPDU enabled (--rateAPsWithAMPDUenabled=1.0). Stopping the simulation." << '\n';
    error = 1;
  }

  if ( maxAmpduSizeWhenAggregationLimited > maxAmpduSize ) {
    std::cout << "INPUT PARAMETER ERROR: The Max AMPDU size to use when aggregation is limited (" << maxAmpduSizeWhenAggregationLimited << ") has to be smaller or equal than the Max AMPDU size (" << maxAmpduSize << "). Stopping the simulation." << '\n';      
    error = 1;        
  }

  if ((maxAmpduSizeWhenAggregationLimited > 0 ) && ( aggregationDisableAlgorithm == 0 ) ) {
    std::cout << "INPUT PARAMETER ERROR: You cannot set 'maxAmpduSizeWhenAggregationLimited' if 'aggregationDisableAlgorithm' is not active. Stopping the simulation." << '\n';      
    error = 1;        
  }

  if ((aggregationDisableAlgorithm == 1 ) && (aggregationDynamicAlgorithm == 1 )) {
    std::cout << "INPUT PARAMETER ERROR: Only one algorithm for modifying AMPDU can be active ('aggregationDisableAlgorithm' and 'aggregationDynamicAlgorithm'). Stopping the simulation." << '\n';      
    error = 1;
  }

  if ((aggregationDynamicAlgorithm == 1 ) && (rateAPsWithAMPDUenabled < 1.0 )) {
    std::cout << "INPUT PARAMETER ERROR: The algorithm has to start with all the APs with A-MPDU enabled (--rateAPsWithAMPDUenabled=1.0). Stopping the simulation." << '\n';
    error = 1;
  }

  if ((aggregationDynamicAlgorithm == 1 ) && (timeMonitorKPIs == 0)) {
    std::cout << "INPUT PARAMETER ERROR: The algorithm for dynamic AMPDU adaptation (--aggregationDynamicAlgorithm=1) requires KPI monitoring ('timeMonitorKPIs' should not be 0.0). Stopping the simulation." << '\n';
    error = 1;
  }

  if ((aggregationDynamicAlgorithm == 1 ) && (latencyBudget == 0.0)) {
    std::cout << "INPUT PARAMETER ERROR: The algorithm for dynamic AMPDU adaptation (--aggregationDynamicAlgorithm=1) requires a latency budget ('latencyBudget' should not be 0.0). Stopping the simulation." << '\n';
    error = 1;
  }

  if ((aggregationDynamicAlgorithm == 1 ) && (numberVoIPupload + numberVoIPdownload == 0)) {
    std::cout << "INPUT PARAMETER ERROR: The algorithm for dynamic AMPDU adaptation (--aggregationDynamicAlgorithm=1) cannot work if there are no VoIP applications. Stopping the simulation." << '\n';
    error = 1;
  }

  if ((rateModel != "Constant") && (rateModel != "Ideal") && (rateModel != "Minstrel")) {
    std::cout << "INPUT PARAMETER ERROR: The wifi rate model MUST be 'Constant', 'Ideal' or 'Minstrel'. Stopping the simulation." << '\n';
    error = 1;
  }

  if (number_of_APs % number_of_APs_per_row != 0) {
    std::cout << "INPUT PARAMETER ERROR: The number of APs MUST be a multiple of the number of APs per row. Stopping the simulation." << '\n';
    error = 1;
  }

  if ((nodeMobility == 0) || (nodeMobility == 1)) {
    if (number_of_APs % number_of_APs_per_row != 0) {
      std::cout << "INPUT PARAMETER ERROR: With static and linear mobility, the number of APs MUST be a multiple of the number of APs per row. Stopping the simulation." << '\n';
      error = 1;
    }
  }

  if ((nodeMobility == 0) || (nodeMobility == 1)) {
    if ( number_of_STAs_per_row == 0 ) {
      std::cout << "INPUT PARAMETER ERROR: With static and linear mobility, the number of STAs per row MUST be defined. Stopping the simulation." << '\n';
      error = 1;
    }
  }

  if ((nodeMobility == 2) || (nodeMobility == 3)) {
    if ( number_of_STAs_per_row != 0 ) {
      std::cout << "INPUT PARAMETER ERROR: With random mobility, the number of STAs per row CANNOT be defined as a parameter. The initial positions are random. Stopping the simulation." << '\n';
      error = 1;
    }
  }


  // LogDistancePropagationLossModel does not work properly in 2.4 GHz
  if ((version80211primary == "11n2.4" ) && (propagationLossModel == 0)) {
    std::cout << "INPUT PARAMETER ERROR: LogDistancePropagationLossModel does not work properly in 2.4 GHz. Stopping the simulation." << '\n';
    error = 1;      
  }

  if ((version80211secondary == "11n2.4" ) && (propagationLossModel == 0)) {
    std::cout << "INPUT PARAMETER ERROR: LogDistancePropagationLossModel does not work properly in 2.4 GHz. Stopping the simulation." << '\n';
    error = 1;
  }

  if (( numberAPsSamePlace != 1 ) && (numberAPsSamePlace != 2) ) {
    std::cout << "INPUT PARAMETER ERROR: numberAPsSamePlace can only be '1' or '2' so far. Stopping the simulation." << '\n';
    error = 1;
  }

  if (( version80211primary == version80211secondary )) {
    std::cout << "INPUT PARAMETER ERROR: frequencyBandPrimaryAP cannot be equal to version80211secondary. Stopping the simulation." << '\n';
    error = 1;
  }


  // check if APsActive is correct
  uint16_t APsActiveCorrectLength;
  APsActiveCorrectLength = number_of_APs * numberAPsSamePlace;

  // if the user has specified '*', I substitute it by the correct number of '1's
  if (APsActive == "*") {
    std::string aux(APsActiveCorrectLength, '1');
    APsActive = aux;
  }
  // check if the length of APsActive is correct
  if (APsActive.length() != APsActiveCorrectLength) {
    std::cout << "INPUT PARAMETER ERROR: wrong 'APsActive' length. It is " << APsActive.length()
              << " but it should be " << APsActiveCorrectLength
              << ", i.e. 'number_of_APs' * 'numberAPsSamePlace'"
              << ". Stopping the simulation." << '\n';
    error = 1;     
  }
  // check if all the chars in APsActive are '0' or '1'
  for (uint16_t t=0; t < APsActive.length(); t++) {
    if ((APsActive.at(t) != '0') && (APsActive.at(t) != '1')) {
      std::cout << "INPUT PARAMETER ERROR: All the characters of 'APsActive' MUST be '0' or '1'"
                << ". Found '" << APsActive.at(t)
                << "' in position " << t
                << ". Stopping the simulation." << '\n';
      error = 1; 
    }
    // check that APsActive is only used if the number of APs in the same place is 2
    if ((numberAPsSamePlace == 1) && (APsActive.at(t) != '1') ) {
      std::cout << "INPUT PARAMETER ERROR: Only 1 AP at the same place is to be created ('numberAPsSamePlace == 1')"
                << ", but an inactive AP has been specified at position " << t
                << " of 'APsActive': " << APsActive
                << ". Stopping the simulation."
                << '\n';
      error = 1; 
    }    
  }
  

  if ((onlyOnePeerSTAallowedAtATime == 1) && (numberSTAsSamePlace == 1)) {
    std::cout << "INPUT PARAMETER ERROR: Only 1 STA at the same place is to be created ('numberSTAsSamePlace == 1')"
              << ", but 'onlyOnePeerSTAallowedAtATime' = " << onlyOnePeerSTAallowedAtATime
              << ". This does not make sense. 'onlyOnePeerSTAallowedAtATime' should be 0"
              << ". Stopping the simulation."
              << '\n';
    error = 1; 
  }

  // the primary and secondary interfaces CANNOT be in the same band
  std::string frequencyBandPrimary, frequencyBandSecondary;
  if ( (version80211primary == "11n5") || (version80211primary == "11ac") || (version80211primary == "11a") )
    frequencyBandPrimary = "5 GHz"; // 5GHz
  else
    frequencyBandPrimary = "2.4 GHz"; // 2.4 GHz

  if ( (version80211secondary == "11n5") || (version80211secondary == "11ac") || (version80211secondary == "11a") )
    frequencyBandSecondary = "5 GHz"; // 5GHz
  else
    frequencyBandSecondary = "2.4 GHz"; // 2.4 GHz  


  if ((numberAPsSamePlace == 2) || (numberSTAsSamePlace == 2)) {
    if ( frequencyBandPrimary == frequencyBandSecondary ) {
      std::cout << "INPUT PARAMETER ERROR: Primary and Secondary devices CANNOT be in the same band. Stopping the simulation." << '\n';
      error = 1;    
    }    
  }


  // check if STAsActive is correct
  uint16_t STAsActiveCorrectLength;
  STAsActiveCorrectLength = number_of_STAs * numberSTAsSamePlace;

  // if the user has specified '*', I substitute it by the correct number of '1's
  if (STAsActive == "*") {
    std::string aux(STAsActiveCorrectLength, '1');
    STAsActive = aux;
  }
  // check if the length of STAsActive is correct
  if (STAsActive.length() != STAsActiveCorrectLength) {
    std::cout << "INPUT PARAMETER ERROR: wrong 'STAsActive' length. It is " << STAsActive.length()
              << " but it should be " << STAsActiveCorrectLength
              << ", i.e. 'number_of_STAs' * 'numberSTAsSamePlace'"
              << ". Stopping the simulation." << '\n';
    error = 1;     
  }
  // check if all the chars in STAsActive are '0' or '1'
  for (uint16_t t=0; t < STAsActive.length(); t++) {
    if ((STAsActive.at(t) != '0') && (STAsActive.at(t) != '1')) {
      std::cout << "INPUT PARAMETER ERROR: All the characters of 'STAsActive' MUST be '0' or '1'"
                << ". Found '" << STAsActive.at(t)
                << "' in position " << t
                << ". Stopping the simulation." << '\n';
      error = 1; 
    }
    // check that STAsActive is only used if the number of STAs in the same place is 2
    if ((numberSTAsSamePlace == 1) && (STAsActive.at(t) != '1') ) {
      std::cout << "INPUT PARAMETER ERROR: Only 1 STA at the same place is to be created ('numberSTAsSamePlace == 1')"
                << ", but an inactive STA has been specified at position " << t
                << " of 'STAsActive': " << STAsActive
                << ". Stopping the simulation."
                << '\n';
      error = 1; 
    }    
  }

  // check if the parameters of the load balancing algorithm are correct
  if (algorithm_load_balancing == true) {
    if (coverage_24GHz == 0.0) {
      std::cout << "INPUT PARAMETER ERROR: The load balancing algorithm is active, but 'coverage_24GHz' is null. Stopping the simulation." << '\n';
      error = 1;         
    }
    if (coverage_5GHz == 0.0) {
      std::cout << "INPUT PARAMETER ERROR: The load balancing algorithm is active, but 'coverage_5GHz' is null. Stopping the simulation." << '\n';
      error = 1;         
    }
    if (periodLoadBalancing <= 0.0) {
      std::cout << "INPUT PARAMETER ERROR: The load balancing algorithm is active, but 'periodLoadBalancing' is null. Stopping the simulation." << '\n';
      error = 1;       
    }
    if (onlyOnePeerSTAallowedAtATime != true) {
      std::cout << "INPUT PARAMETER ERROR: The load balancing algorithm is active, but 'onlyOnePeerSTAallowedAtATime' is false. It does not make sense to run the algorithm if dual STAs can be connected to both bands. Stopping the simulation." << '\n';
      error = 1;       
    }
  }


  // check if the channel width is correct
  // this is only applicable to the 5GHz band
  if (frequencyBandPrimary == "5 GHz" ) {
    if ((channelWidthPrimary != 20) && (channelWidthPrimary != 40) && (channelWidthPrimary != 80) && (channelWidthPrimary != 160)) {
      std::cout << "INPUT PARAMETER ERROR: The witdth of the channels in 5GHz band has to be 20, 40, 80 or 160. Stopping the simulation." << '\n';
      error = 1;    
    }
    if ((channelWidthPrimary == 20) && (numOperationalChannelsPrimary > NUM_CHANNELS_5GHZ_20MHZ) ) {
      std::cout << "INPUT PARAMETER ERROR: The maximum number of 20 MHz channels in 5GHz band is " << NUM_CHANNELS_5GHZ_20MHZ << ". Stopping the simulation." << '\n';
      error = 1;    
    }
    if ((channelWidthPrimary == 40) && (numOperationalChannelsPrimary > NUM_CHANNELS_5GHZ_40MHZ) ) {
      std::cout << "INPUT PARAMETER ERROR: The maximum number of 40 MHz channels in 5GHz band is " << NUM_CHANNELS_5GHZ_40MHZ << ". Stopping the simulation." << '\n';
      error = 1;    
    }
    if ((channelWidthPrimary == 80) && (numOperationalChannelsPrimary > NUM_CHANNELS_5GHZ_80MHZ) ) {
      std::cout << "INPUT PARAMETER ERROR: The maximum number of 80 MHz channels in 5GHz band is " << NUM_CHANNELS_5GHZ_80MHZ << ". Stopping the simulation." << '\n';
      error = 1;    
    }
    if ((channelWidthPrimary == 160) && (numOperationalChannelsPrimary > NUM_CHANNELS_5GHZ_160MHZ) ) {
      std::cout << "INPUT PARAMETER ERROR: The maximum number of 160 MHz channels in 5GHz band is " << NUM_CHANNELS_5GHZ_160MHZ << ". Stopping the simulation." << '\n';
      error = 1;    
    }
  }
  else {
    // in the 2.4 GHz band, only 20 MHz channels are supported (by this program so far)    
    if (channelWidthPrimary != 20) {
      std::cout << "INPUT PARAMETER ERROR: Only 20 MHz channels can be used (so far) in the 2.4GHz band. Stopping the simulation." << '\n';
      error = 1;
    }
    if (numOperationalChannelsPrimary > NUM_CHANNELS_24GHZ) {
      std::cout << "INPUT PARAMETER ERROR: The maximum number of channels in 2.4 GHz is " << NUM_CHANNELS_24GHZ << ". Stopping the simulation." << '\n';
      error = 1;    
    }
  }

  if (frequencyBandSecondary == "5 GHz" ) {
    if ((channelWidthSecondary != 20) && (channelWidthSecondary != 40) && (channelWidthSecondary != 80) && (channelWidthSecondary != 160)) {
      std::cout << "INPUT PARAMETER ERROR: The witdth of the channels has to be 20, 40, 80 or 160. Stopping the simulation." << '\n';
      error = 1;    
    }
    if ((channelWidthSecondary == 20) && (numOperationalChannelsSecondary > NUM_CHANNELS_5GHZ_20MHZ) ) {
      std::cout << "INPUT PARAMETER ERROR: The maximum number of 20 MHz channels in 5GHz band is " << NUM_CHANNELS_5GHZ_20MHZ << ". Stopping the simulation." << '\n';
      error = 1;    
    }
    if ((channelWidthSecondary == 40) && (numOperationalChannelsSecondary > NUM_CHANNELS_5GHZ_40MHZ) ) {
      std::cout << "INPUT PARAMETER ERROR: The maximum number of 40 MHz channels in 5GHz band is " << NUM_CHANNELS_5GHZ_40MHZ << ". Stopping the simulation." << '\n';
      error = 1;    
    }
    if ((channelWidthSecondary == 80) && (numOperationalChannelsSecondary > NUM_CHANNELS_5GHZ_80MHZ) ) {
      std::cout << "INPUT PARAMETER ERROR: The maximum number of 80 MHz channels in 5GHz band is " << NUM_CHANNELS_5GHZ_80MHZ << ". Stopping the simulation." << '\n';
      error = 1;    
    }
    if ((channelWidthSecondary == 160) && (numOperationalChannelsSecondary > NUM_CHANNELS_5GHZ_160MHZ) ) {
      std::cout << "INPUT PARAMETER ERROR: The maximum number of 160 MHz channels in 5GHz band is " << NUM_CHANNELS_5GHZ_160MHZ << ". Stopping the simulation." << '\n';
      error = 1;    
    }
  }
  else {
    // in the 2.4 GHz band, only 20 MHz channels are supported (by this program so far)    
    if (channelWidthSecondary != 20) {
      std::cout << "INPUT PARAMETER ERROR: Only 20 MHz channels can be used (so far) in the 2.4GHz band. Stopping the simulation." << '\n';
      error = 1;
    }
    if (numOperationalChannelsSecondary > NUM_CHANNELS_24GHZ) {
      std::cout << "INPUT PARAMETER ERROR: The maximum number of channels in 2.4 GHz is " << NUM_CHANNELS_24GHZ << ". Stopping the simulation." << '\n';
      error = 1;    
    }
  }


  if (error) return 0;
  /********** end of - check input parameters **************/

  
  /******** fill the variable with the available channels *************/
  uint8_t availableChannels[numOperationalChannelsPrimary];
  for (uint32_t i = 0; i < numOperationalChannelsPrimary; ++i) {
    if (frequencyBandPrimary == "5 GHz") {
      if (channelWidthPrimary == 20)
        availableChannels[i] = availableChannels20MHz[i % NUM_CHANNELS_5GHZ_20MHZ];
      else if (channelWidthPrimary == 40)
        availableChannels[i] = availableChannels40MHz[i % NUM_CHANNELS_5GHZ_40MHZ];
      else if (channelWidthPrimary == 80)
        availableChannels[i] = availableChannels80MHz[i % NUM_CHANNELS_5GHZ_80MHZ];
      else if (channelWidthPrimary == 160)
        availableChannels[i] = availableChannels160MHz[i % NUM_CHANNELS_5GHZ_160MHZ];      
    }
    else {
      // 2.4 GHz band
      availableChannels[i] = availableChannels24GHz20MHz[i % NUM_CHANNELS_24GHZ];
    }
  }

  uint8_t availableChannelsSecondary[numOperationalChannelsSecondary];
  for (uint32_t i = 0; i < numOperationalChannelsSecondary; ++i) {
    if (frequencyBandSecondary == "5 GHz") {
      if (channelWidthSecondary == 20)
        availableChannelsSecondary[i] = availableChannels20MHz[i % NUM_CHANNELS_5GHZ_20MHZ];
      else if (channelWidthSecondary == 40)
        availableChannelsSecondary[i] = availableChannels40MHz[i % NUM_CHANNELS_5GHZ_40MHZ];
      else if (channelWidthSecondary == 80)
        availableChannelsSecondary[i] = availableChannels80MHz[i % NUM_CHANNELS_5GHZ_80MHZ];
      else if (channelWidthSecondary == 160)
        availableChannelsSecondary[i] = availableChannels160MHz[i % NUM_CHANNELS_5GHZ_160MHZ];      
    }
    else {
      // 2.4 GHz band
      availableChannelsSecondary[i] = availableChannels24GHz20MHz[i % NUM_CHANNELS_24GHZ];
    }
  }
  /******** end of - fill the variable with the available channels *************/


  /************* Show the parameters by the screen *****************/
  if (verboseLevel > 0) {
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("SimpleMpduAggregation", LOG_LEVEL_INFO);

    // write the input parameters to the screen

    // General scenario topology parameters
    std::cout << "Simulation Time: " << simulationTime <<" sec" << '\n';
    std::cout << "Time interval to monitor KPIs of the flows. Also used for adjusting the AMPDU algorithm. (0 means no monitoring): " << timeMonitorKPIs <<" sec" << '\n';
    std::cout << "Each STA runs all the Applications?': " << eachSTArunsAllTheApps << '\n';
    if (eachSTArunsAllTheApps == false) {
      std::cout << "Number of nodes running VoIP up: " << numberVoIPupload << '\n';
      std::cout << "Number of nodes running VoIP down: " << numberVoIPdownload << '\n';
      std::cout << "Number of nodes running TCP up: " << numberTCPupload << '\n';
      std::cout << "Number of nodes running TCP down: " << numberTCPdownload << '\n';
      std::cout << "Number of nodes running video down: " << numberVideoDownload << '\n';      
    }
    else {
      if (numberVoIPupload == 0) {
        std::cout << "Number of nodes running VoIP up (none): " << numberVoIPupload << '\n'; 
      } else {
        std::cout << "Number of nodes running VoIP up (all): " << number_of_STAs << '\n';        
      }
      if (numberVoIPdownload == 0) {
        std::cout << "Number of nodes running VoIP down (none): " << numberVoIPdownload << '\n';
      } else {
        std::cout << "Number of nodes running VoIP down (all): " << number_of_STAs << '\n';
      }
      if (numberTCPupload == 0) {
        std::cout << "Number of nodes running TCP up (none): " << numberTCPupload << '\n';
      } else {
        std::cout << "Number of nodes running TCP up (all): " << number_of_STAs << '\n';
      }
      if (numberTCPdownload == 0) {
        std::cout << "Number of nodes running TCP down (none): " << numberTCPdownload << '\n';
      } else {
        std::cout << "Number of nodes running TCP down (all): " << number_of_STAs << '\n';
      }
      if (numberVideoDownload == 0) {
        std::cout << "Number of nodes running video down (none): " << numberVideoDownload << '\n';
      } else {
        std::cout << "Number of nodes running video down (all): " << number_of_STAs << '\n';         
      } 
    }
    std::cout << "Number of APs: " << number_of_APs * numberAPsSamePlace 
              << " (" << number_of_APs << " * " << numberAPsSamePlace << ")"
              << '\n';    
    std::cout << "Number of APs per row: " << number_of_APs_per_row << '\n'; 
    std::cout << "Distance between APs: " << distance_between_APs << " meters" << '\n';
    std::cout << "Distance between the AP and the border of the scenario: " << distanceToBorder << " meters" << '\n';
    std::cout << "Total number of STAs: " << number_of_STAs << '\n'; 
    std::cout << "Number of STAs per row: " << number_of_STAs_per_row << '\n';
    std::cout << "Initial distance between STAs (only for static and linear mobility): " << distance_between_STAs << " meters" << '\n';
    std::cout << "Node mobility: '0' static; '1' linear; '2' Random Walk 2d; '3' Random Waypoint: " << nodeMobility << '\n';
    std::cout << "Speed of the nodes (in linear and random mobility): " << constantSpeed << " m/s"<< '\n';
    std::cout << "Topology: '0' all server applications in a server; '1' all the servers connected to the hub; '2' all the servers behind a router: " << topology << '\n';
    std::cout << "ARP Alive Timeout [s]: Time an ARP entry will be in ALIVE state (unless refreshed): " << arpAliveTimeout << " s\n";
    std::cout << "ARP Dead Timeout [s]: Time an ARP entry will be in DEAD state before being removed: " << arpDeadTimeout << " s\n";
    std::cout << "ARP max retries for a resolution: " << arpMaxRetries << " s\n";
    std::cout << '\n';
    // Aggregation parameters    
    std::cout << "Initial rate of APs with AMPDU aggregation enabled: " << rateAPsWithAMPDUenabled << '\n';
    std::cout << "Is the algorithm enabling/disabling AMPDU aggregation enabled?: " << aggregationDisableAlgorithm << '\n';
    std::cout << "Maximum value of the AMPDU size: " << maxAmpduSize << " bytes" << '\n';
    std::cout << "Maximum value of the AMPDU size when aggregation is limited: " << maxAmpduSizeWhenAggregationLimited << " bytes" << '\n';
    std::cout << "Is the algorithm dynamically adapting AMPDU aggregation enabled?: " << aggregationDynamicAlgorithm << '\n';
    std::cout << std::setprecision(3) << std::fixed; // to specify the latency budget with 3 decimal digits
    std::cout << "Maximum latency tolerated by VoIP applications: " << latencyBudget << " s" << '\n';
    std::cout << "Method for adjusting AMPDU size: '0' (default), '1' PENDING: " << methodAdjustAmpdu << '\n';
    std::cout << "Step for adjusting AMPDU size: " << stepAdjustAmpdu << " bytes" << '\n';

    std::cout << '\n';
    // TCP parameters
    std::cout << "TCP Payload size: " << TcpPayloadSize << " bytes"  << '\n';
    std::cout << "TCP variant: " << TcpVariant << '\n';
    std::cout << '\n';
    // 802.11 priorities, version, channels  
    std::cout << "Use different 802.11 priorities for VoIP / TCP?: '0' no; '1' yes: " << prioritiesEnabled << '\n';

    // parameters of primary APs and primary devices of STAs
    std::cout << "Version of 802.11: '0' 802.11n 5GHz; '1' 802.11ac; '2' 802.11n 2.4GHz: " << version80211primary << '\n';
    std::cout << "Number of different channels to use on the primary APs: " << numOperationalChannelsPrimary << '\n';
    std::cout << "Channels being used in primary interfaces: ";
    for (uint32_t i = 0; i < numOperationalChannelsPrimary; ++i) {
      std::cout << uint16_t (availableChannels[i]) << " ";
    }
    std::cout << '\n';
    std::cout << "Width of the wireless channels of primary interfaces: " << channelWidthPrimary << '\n';

    // parameters of secondary APs and secondary interfaces of STAs
    if (numberAPsSamePlace == 2) {
      std::cout << "Version of 802.11 in secondary APs: '0' 802.11n 5GHz; '1' 802.11ac; '2' 802.11n 2.4GHz: " << version80211secondary << '\n';
      std::cout << "Number of different channels to use on secondary APs: " << numOperationalChannelsSecondary << '\n';
      std::cout << "Channels being used in secondary interfaces: ";
      for (uint32_t i = 0; i < numOperationalChannelsSecondary; ++i) {
        std::cout << uint16_t (availableChannelsSecondary[i]) << " ";
      }
      std::cout << '\n';
      std::cout << "Width of the wireless channels of primary interfaces: " << channelWidthSecondary << '\n';      
    }

    std::cout << "Model for 802.11 rate control 'Constant'; 'Ideal'; 'Minstrel': " << rateModel << '\n';  
    std::cout << "Threshold for using RTS/CTS in 2.4 GHz. Examples. '0' always; '500' only 500 bytes-packes or higher will require RTS/CTS; '999999' never: " << RtsCtsThreshold24GHz << " bytes" << '\n';
    std::cout << "Threshold for using RTS/CTS in 5 GHz. Examples. '0' always; '500' only 500 bytes-packes or higher will require RTS/CTS; '999999' never: " << RtsCtsThreshold5GHz << " bytes" << '\n';

    std::cout << '\n';
    // Wi-Fi power, propagation and error models  
    std::cout << "Power level of the wireless interfaces: " << powerLevel << " dBm" << '\n';
    std::cout << "WiFi model: '0' YansWifiPhy; '1' SpectrumWifiPhy with MultiModelSpectrumChannel: " << wifiModel << '\n';
    std::cout << "Propagation loss model: '0' LogDistancePropagationLossModel; '1' FriisPropagationLossModel; '2' FriisSpectrumPropagationLossModel: " << propagationLossModel << '\n';
    std::cout << "Error Rate model: '0' NistErrorRateModel; '1' YansErrorRateModel: " << errorRateModel << '\n';
    std::cout << '\n';
    // Parameters of the output of the program  
    std::cout << "pcap generation enabled ?: " << enablePcap << '\n';
    std::cout << "verbose level: " << verboseLevel << '\n';
    std::cout << "Periodically print simulation time every " << printSeconds << " seconds" << '\n';    
    std::cout << "Generate histograms (delay, jitter, packet size): " << generateHistograms << '\n';
    std::cout << "First characters to be used in the name of the output file: " << outputFileName << '\n';
    std::cout << "Other characters to be used in the name of the output file (not in the average one): " << outputFileSurname << '\n';
    std::cout << "Save per-flow results to an XML file?: " << saveXMLFile << '\n';
    std::cout << '\n';
  }
  /************* end of - Show the parameters by the screen *****************/


  /************* Create the variables to store the flowmonitor statistics of the flows during the simulation ************/
  uint32_t  numberVoIPuploadConnections,
            numberVoIPdownloadConnections,
            numberTCPuploadConnections,
            numberTCPdownloadConnections,
            numberVideoDownloadConnections;

  if (eachSTArunsAllTheApps == false) {
    numberVoIPuploadConnections = numberVoIPupload * numberSTAsSamePlace;
    numberVoIPdownloadConnections = numberVoIPdownload * numberSTAsSamePlace;
    numberTCPuploadConnections = numberTCPupload * numberSTAsSamePlace;
    // tcp download: take into account that there MAY be different numbers of connections
    //if (TcpDownMultiConnection == 0)
      numberTCPdownloadConnections = numberTCPdownload * numberSTAsSamePlace;
    //else
      //numberTCPdownloadConnections = numberTCPdownload * numberSTAsSamePlace * TcpDownMultiConnection;

    numberVideoDownloadConnections = numberVideoDownload * numberSTAsSamePlace;
  }
  else {
    // all the STAs run all the active applications
    if (numberVoIPupload != 0)
      numberVoIPuploadConnections = number_of_STAs * numberSTAsSamePlace;
    else
      numberVoIPuploadConnections = 0;

    if (numberVoIPdownload != 0)
      numberVoIPdownloadConnections = number_of_STAs * numberSTAsSamePlace;
    else
      numberVoIPdownloadConnections = 0;

    if (numberTCPupload != 0)
      numberTCPuploadConnections = number_of_STAs * numberSTAsSamePlace;
    else
      numberTCPuploadConnections = 0;

    if (numberTCPdownload != 0) {
      //if (TcpDownMultiConnection == 0)
        numberTCPdownloadConnections = number_of_STAs * numberSTAsSamePlace;
      //else
        //numberTCPdownloadConnections = number_of_STAs * numberSTAsSamePlace * TcpDownMultiConnection;      
    }

    else
      numberTCPdownloadConnections = 0;

    if (numberVideoDownload != 0)
      numberVideoDownloadConnections = number_of_STAs * numberSTAsSamePlace;
    else
      numberVideoDownloadConnections = 0;
  }

  // VoIP applications generate 1 flow
  // TCP applications generate 2 flows: 1 for data and 1 for ACKs (statistics are not stored for ACK flows)
  // video applications generate 1 flow
  FlowStatistics myFlowStatisticsVoIPUpload[numberVoIPuploadConnections];
  FlowStatistics myFlowStatisticsVoIPDownload[numberVoIPdownloadConnections];
  FlowStatistics myFlowStatisticsTCPUpload[numberTCPuploadConnections];
  FlowStatistics myFlowStatisticsTCPDownload[numberTCPdownloadConnections];
  FlowStatistics myFlowStatisticsVideoDownload[numberVideoDownloadConnections];

  AllTheFlowStatistics myAllTheFlowStatistics;
  myAllTheFlowStatistics.numberVoIPUploadFlows = numberVoIPuploadConnections;
  myAllTheFlowStatistics.numberVoIPDownloadFlows = numberVoIPdownloadConnections;
  myAllTheFlowStatistics.numberTCPUploadFlows = numberTCPuploadConnections;
  myAllTheFlowStatistics.numberTCPDownloadFlows = numberTCPdownloadConnections;
  myAllTheFlowStatistics.numberVideoDownloadFlows = numberVideoDownloadConnections;

  myAllTheFlowStatistics.FlowStatisticsVoIPUpload = myFlowStatisticsVoIPUpload;
  myAllTheFlowStatistics.FlowStatisticsVoIPDownload = myFlowStatisticsVoIPDownload;
  myAllTheFlowStatistics.FlowStatisticsTCPUpload = myFlowStatisticsTCPUpload;
  myAllTheFlowStatistics.FlowStatisticsTCPDownload = myFlowStatisticsTCPDownload;
  myAllTheFlowStatistics.FlowStatisticsVideoDownload = myFlowStatisticsVideoDownload;

  if (VERBOSE_FOR_DEBUG >= 1)
    std::cout << "numberVoIPuploadConnections: " << numberVoIPuploadConnections << '\n'
              << "numberVoIPdownloadConnections: " << numberVoIPdownloadConnections << '\n'
              << "numberTCPuploadConnections: " << numberTCPuploadConnections << '\n'
              << "numberTCPdownloadConnections: " << numberTCPdownloadConnections << '\n'
              << "numberVideoDownloadConnections: " << numberVideoDownloadConnections << '\n';

  // Initialize to 0
  for (uint32_t i = 0 ; i < numberVoIPuploadConnections; i++) {
    myFlowStatisticsVoIPUpload[i].acumDelay = 0.0;
    myFlowStatisticsVoIPUpload[i].acumJitter = 0.0;
    myFlowStatisticsVoIPUpload[i].acumRxPackets = 0;
    myFlowStatisticsVoIPUpload[i].acumRxBytes = 0;
    myFlowStatisticsVoIPUpload[i].acumLostPackets = 0;
    myFlowStatisticsVoIPUpload[i].destinationPort = INITIALPORT_VOIP_UPLOAD + i;
    myFlowStatisticsVoIPUpload[i].finalDestinationPort = INITIALPORT_VOIP_UPLOAD + i;
  }

  // Initialize to 0
  for (uint32_t i = 0 ; i < numberVoIPdownloadConnections; i++) {
    myFlowStatisticsVoIPDownload[i].acumDelay = 0.0;
    myFlowStatisticsVoIPDownload[i].acumJitter = 0.0;
    myFlowStatisticsVoIPDownload[i].acumRxPackets = 0;
    myFlowStatisticsVoIPDownload[i].acumRxBytes = 0;
    myFlowStatisticsVoIPDownload[i].acumLostPackets = 0;
    myFlowStatisticsVoIPDownload[i].destinationPort = INITIALPORT_VOIP_DOWNLOAD + i;
    myFlowStatisticsVoIPDownload[i].finalDestinationPort = INITIALPORT_VOIP_DOWNLOAD + i;
  }

  // Initialize to 0
  for (uint32_t i = 0 ; i < numberTCPuploadConnections; i++) {
    myFlowStatisticsTCPUpload[i].acumDelay = 0.0;
    myFlowStatisticsTCPUpload[i].acumJitter = 0.0;
    myFlowStatisticsTCPUpload[i].acumRxPackets = 0;
    myFlowStatisticsTCPUpload[i].acumRxBytes = 0;
    myFlowStatisticsTCPUpload[i].acumLostPackets = 0;
    myFlowStatisticsTCPUpload[i].destinationPort = INITIALPORT_TCP_UPLOAD + i;
    myFlowStatisticsTCPUpload[i].finalDestinationPort = INITIALPORT_TCP_UPLOAD + i;
  }

  // Initialize to 0
  for (uint32_t i = 0 ; i < numberTCPdownloadConnections; i++) {
    myFlowStatisticsTCPDownload[i].acumDelay = 0.0;
    myFlowStatisticsTCPDownload[i].acumJitter = 0.0;
    myFlowStatisticsTCPDownload[i].acumRxPackets = 0;
    myFlowStatisticsTCPDownload[i].acumRxBytes = 0;
    myFlowStatisticsTCPDownload[i].acumLostPackets = 0;
    if ( TcpDownMultiConnection == 0 ) {
      // in this case, there is only one port for each TCP download
      myFlowStatisticsTCPDownload[i].destinationPort = INITIALPORT_TCP_DOWNLOAD + i;
      myFlowStatisticsTCPDownload[i].finalDestinationPort = INITIALPORT_TCP_DOWNLOAD + i;
    }
    else {
      // in this case, there is a number of ports for each TCP download
      myFlowStatisticsTCPDownload[i].destinationPort = INITIALPORT_TCP_DOWNLOAD + (i * TcpDownMultiConnection);
      myFlowStatisticsTCPDownload[i].finalDestinationPort = INITIALPORT_TCP_DOWNLOAD + ((i + 1) * TcpDownMultiConnection) - 1;

      // I set to 0 these variables because I am not updating them
      // FIXME: if I update them in 'obtainKPIsMultiTCP', I can remove this
      myFlowStatisticsTCPDownload[i].lastIntervalDelay = 0.0;
      myFlowStatisticsTCPDownload[i].lastIntervalJitter = 0.0;
      myFlowStatisticsTCPDownload[i].lastIntervalRxPackets = 0;
      myFlowStatisticsTCPDownload[i].lastIntervalLostPackets = 0;
      myFlowStatisticsTCPDownload[i].lastIntervalRxBytes = 0;
    }
  }

  // Initialize to 0
  for (uint32_t i = 0 ; i < numberVideoDownloadConnections; i++) {
    myFlowStatisticsVideoDownload[i].acumDelay = 0.0;
    myFlowStatisticsVideoDownload[i].acumJitter = 0.0;
    myFlowStatisticsVideoDownload[i].acumRxPackets = 0;
    myFlowStatisticsVideoDownload[i].acumRxBytes = 0;
    myFlowStatisticsVideoDownload[i].acumLostPackets = 0;
    myFlowStatisticsVideoDownload[i].destinationPort = INITIALPORT_VIDEO_DOWNLOAD + i;
    myFlowStatisticsVideoDownload[i].finalDestinationPort = INITIALPORT_VIDEO_DOWNLOAD + i;
  }
  /************* end of - Create the variables to store the flowmonitor statistics of the flows during the simulation ************/


  // ARP parameters
  Config::SetDefault ("ns3::ArpCache::AliveTimeout", TimeValue (Seconds (arpAliveTimeout)));
  Config::SetDefault ("ns3::ArpCache::DeadTimeout", TimeValue (Seconds (arpDeadTimeout)));
  Config::SetDefault ("ns3::ArpCache::MaxRetries", UintegerValue (arpMaxRetries));


  /******** create the node containers *********/
  NodeContainer apNodes;
  // apNodes have:
  //  - a csma device
  //  - a wifi device
  //  - a bridgeAps connecting them
  //  - I do not install the IP stack, as they do not need it

  // wireless STAs. They have a mobility pattern 
  NodeContainer staNodes;

  // server that interacts with the STAs
  NodeContainer singleServerNode;

  // servers that interact with the STAs
  NodeContainer serverNodes;

  // a router connecting to the servers' network
  NodeContainer routerNode;

  // a single csma hub that connects everything
  NodeContainer csmaHubNode;


  /******** create the nodes *********/
  // The order in which you create the nodes is important
  apNodes.Create (number_of_APs * numberAPsSamePlace);

  staNodes.Create (number_of_STAs * numberSTAsSamePlace);

  // create a container only containing the primary STAs,
  //and another container with the secondary STAs
  NodeContainer staNodesPrimary, staNodesSecondary;
  for (uint32_t i = 0; i < number_of_STAs; ++i) {
    staNodesPrimary.Add(staNodes.Get(i));
    if (numberSTAsSamePlace == 2)
      staNodesSecondary.Add(staNodes.Get(number_of_STAs + i));
  }
  NS_ASSERT(staNodesPrimary.GetN() == number_of_STAs);
  if (numberSTAsSamePlace == 2)
    NS_ASSERT(staNodesSecondary.GetN() == number_of_STAs);
  NS_ASSERT(staNodes.GetN() == staNodesPrimary.GetN() + staNodesSecondary.GetN()); 


  if (topology == 0) {
    // create a single server
    singleServerNode.Create(1);
  }
  else {
    // create one server per STA
    serverNodes.Create (number_of_Servers);

    if (topology == 2)
      // in this case, I also need a router
      routerNode.Create(1);
  }

  // Create a hub
  // Inspired on https://www.nsnam.org/doxygen/csma-bridge_8cc_source.html
  csmaHubNode.Create (1);
  /******** end of - create the nodes *********/


  /************ Install Internet stack in the nodes ***************/
  InternetStackHelper stack;

  //stack.Install (apNodes); // I do not install it because they do not need it

  stack.Install (staNodes);

  if (topology == 0) {
    // single server
    stack.Install (singleServerNode);

  } else {
    // one server per STA
    stack.Install (serverNodes);

    if (topology == 2)
      // in this case, I also need a router
      stack.Install (routerNode);
  }
  /************ end of - Install Internet stack in the nodes ***************/


  /******** create the net device containers *********/
  NetDeviceContainer apCsmaDevices;
  std::vector<NetDeviceContainer> apWiFiDevices;
  std::vector<NetDeviceContainer> staDevices;
  NetDeviceContainer serverDevices;
  NetDeviceContainer singleServerDevices;
  NetDeviceContainer csmaHubDevices;                // each network card of the hub
  //NetDeviceContainer bridgeApDevices;             // A bridge container for the bridge of each AP node
  NetDeviceContainer routerDeviceToAps, routerDeviceToServers;


  /******* IP interfaces *********/
  //Ipv4InterfaceContainer apInterfaces;
  std::vector<Ipv4InterfaceContainer> staInterfaces;
  std::vector<Ipv4InterfaceContainer> staInterfacesSecondary;
  Ipv4InterfaceContainer serverInterfaces;
  Ipv4InterfaceContainer singleServerInterfaces;
  //Ipv4InterfaceContainer csmaHubInterfaces;
  Ipv4InterfaceContainer routerInterfaceToAps, routerInterfaceToServers;


  /********* IP addresses **********/
  Ipv4AddressHelper ipAddressesSegmentA;
  ipAddressesSegmentA.SetBase ("10.0.0.0", "255.255.0.0"); // If you use 255.255.255.0, you can only use 256 nodes

  Ipv4AddressHelper ipAddressesSegmentB;    // The servers are behind the router, so they are in other network
  ipAddressesSegmentB.SetBase ("10.1.0.0", "255.255.0.0");


  /******** mobility *******/
  MobilityHelper mobility;

  /* mobility of APs */
  // constant position
  // I do this 1 or 2 times, depending on 'numberAPsSamePlace'
  // if numberAPsSamePlace == 2, then each position will be applied to 2 APs
  // example: number_of_APs = 3, then APs #0 and #3 will be in the same place
  for (uint32_t j = 0; j < numberAPsSamePlace; ++j) {

    // create an auxiliary container and put only the nodes with the same WiFi configuration
    // i.e. the APs that are in different places
    NodeContainer apNodesAux;
    for (uint32_t i = 0; i < number_of_APs; ++i) {
      apNodesAux.Add(apNodes.Get(i + j * number_of_APs));
    }

    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue (x_position_first_AP),
                                  "MinY", DoubleValue (y_position_first_AP),
                                  "DeltaX", DoubleValue (distance_between_APs),
                                  "DeltaY", DoubleValue (distance_between_APs),
                                  "GridWidth", UintegerValue (number_of_APs_per_row), // size of the row
                                  "LayoutType", StringValue ("RowFirst"));

    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  //mobility.Install (backboneNodes); // backbone nodes do not need a position
    mobility.Install (apNodesAux);
  }

  if (verboseLevel >= 2) {
    for (uint32_t j = 0; j < numberAPsSamePlace; ++j) {
      for (uint32_t i = 0; i < number_of_APs; ++i) {
        //ReportPosition (timeMonitorKPIs, backboneNodes.Get(i), i, 0, 1, apNodes); this would report the position every second
        //Vector pos = GetPosition (backboneNodes.Get (i));
        Vector pos = GetPosition (apNodes.Get (i + (j * number_of_APs)));
        std::cout << "AP#" << i + (j * number_of_APs) << " Position: " << pos.x << "," << pos.y << '\n';
      }      
    }
  }
  /* end of - mobility of APs */


  /* mobility of STAs */
  // Set the positions and the mobility of the STAs
  // Taken from https://www.nsnam.org/docs/tutorial/html/building-topologies.html#building-a-wireless-network-topology

  // add the mobility pattern to the primary STAs
  // STAs do not move
  if (nodeMobility == 0) {
    mobility.SetPositionAllocator ( "ns3::GridPositionAllocator",
                                    "MinX", DoubleValue (x_position_first_STA),
                                    "MinY", DoubleValue (y_position_first_STA),
                                    "DeltaX", DoubleValue (distance_between_STAs),
                                    "DeltaY", DoubleValue (distance_between_STAs),
                                    "GridWidth", UintegerValue (number_of_STAs_per_row),  // size of the row
                                    "LayoutType", StringValue ("RowFirst"));

    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

    mobility.Install (staNodesPrimary);
  }

  // STAs linear mobility: constant speed to the right 
  else if (nodeMobility == 1) {

    mobility.SetPositionAllocator ( "ns3::GridPositionAllocator",
                                    "MinX", DoubleValue (x_position_first_STA),
                                    "MinY", DoubleValue (y_position_first_STA),
                                    "DeltaX", DoubleValue (distance_between_STAs),
                                    "DeltaY", DoubleValue (1),
                                    "GridWidth", UintegerValue (number_of_STAs_per_row),  // size of the row
                                    "LayoutType", StringValue ("RowFirst"));

    Ptr<ConstantVelocityMobilityModel> mob;
    Vector m_velocity = Vector(constantSpeed, 0.0, 0.0);

    // https://www.nsnam.org/doxygen/classns3_1_1_constant_velocity_mobility_model.html#details
    mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");

    mobility.Install (staNodesPrimary);

    // primary STAs
    for (uint32_t k = 0; k < staNodesPrimary.GetN(); ++k) {
      // https://www.nsnam.org/doxygen/classns3_1_1_constant_velocity_mobility_model.html#details
      mob = staNodesPrimary.Get(k)->GetObject<ConstantVelocityMobilityModel>();
      mob->SetVelocity(m_velocity);
    }
  }

  // STAs random walk 2d mobility mobility
  else if (nodeMobility == 2) { 

    // Each instance moves with a speed and direction choosen at random with the user-provided random variables until either a fixed 
    //distance has been walked or until a fixed amount of time. If we hit one of the boundaries (specified by a rectangle), 
    //of the model, we rebound on the boundary with a reflexive angle and speed. This model is often identified as a brownian motion model.
    /*
    // https://www.nsnam.org/doxygen/classns3_1_1_rectangle.html
    // Rectangle (double _xMin, double _xMax, double _yMin, double _yMax)
    mobility.SetMobilityModel ( "ns3::RandomWalk2dMobilityModel",
                                //"Mode", StringValue ("Time"),
                                //"Time", StringValue ("2s"),
                                //"Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                                "Bounds", RectangleValue (Rectangle ( x_position_first_AP - distanceToBorder, 
                                                                      ((number_of_APs_per_row -1) * distance_between_APs) + distanceToBorder,
                                                                      y_position_first_AP - distanceToBorder,
                                                                      ((number_of_APs / number_of_APs_per_row) -1 ) * distance_between_APs + distanceToBorder)));
    */
    /*
    Config::SetDefault ("ns3::RandomWalk2dMobilityModel::Mode", StringValue ("Time"));
    Config::SetDefault ("ns3::RandomWalk2dMobilityModel::Time", StringValue ("2s"));
    Config::SetDefault ("ns3::RandomWalk2dMobilityModel::Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
    Config::SetDefault ("ns3::RandomWalk2dMobilityModel::Bounds", StringValue ("0|200|0|200"));
    */

    // auxiliar string
    std::ostringstream auxString;

    // create a string with the X boundaries
    auxString << "ns3::UniformRandomVariable[Min=" << x_position_first_AP - distanceToBorder 
              << "|Max=" << ((number_of_APs_per_row -1) * distance_between_APs) + distanceToBorder << "]"; 
    std::string XString = auxString.str();
    if ( verboseLevel > 1 )
      std::cout << "Limits for X: " << XString << '\n';

    // clean the string
    auxString.str(std::string());

    // create a string with the Y boundaries
    auxString   << "ns3::UniformRandomVariable[Min=" << y_position_first_AP - distanceToBorder 
                << "|Max=" << ((number_of_APs / number_of_APs_per_row) -1 ) * distance_between_APs + distanceToBorder << "]"; 
    std::string YString = auxString.str();
    if ( verboseLevel > 1 )
      std::cout << "Limits for Y: " << YString << '\n';

    // Locate the STAs initially
    mobility.SetPositionAllocator ( "ns3::RandomRectanglePositionAllocator",
                                    "X", StringValue (XString),
                                    "Y", StringValue (YString));

    // clean the string
    auxString.str(std::string());

    // create a string with the time between speed modifications
    auxString << "5s";
    std::string timeString = auxString.str();
    if ( verboseLevel > 1 )
      std::cout << "The STAs will change their trajectory every " << timeString << '\n';

    // clean the string
    auxString.str(std::string());

    // create a string with the speed
    auxString  << "ns3::ConstantRandomVariable[Constant=" << constantSpeed << "]";
    std::string speedString = auxString.str();
    if ( verboseLevel > 1 )
      std::cout << "Speed with which the STAs move: " << speedString << '\n';

    // clean the string
    auxString.str(std::string());

    // create a string like this: "0|200|0|200"
    auxString << x_position_first_AP - distanceToBorder << "|" 
              << ((number_of_APs_per_row -1) * distance_between_APs) + distanceToBorder << "|"
              << y_position_first_AP - distanceToBorder << "|"
              << ((number_of_APs / number_of_APs_per_row) -1 ) * distance_between_APs + distanceToBorder;
    std::string boundsString = auxString.str();
    if ( verboseLevel > 1 )
      std::cout << "Rectangle where the STAs move: " << boundsString << '\n';

    mobility.SetMobilityModel ( "ns3::RandomWalk2dMobilityModel",
                                "Mode", StringValue ("Time"),
                                "Time", StringValue (timeString),   // the node will modify its speed every 'timeString' seconds
                                "Speed", StringValue (speedString), // the speed is always between [-speedString,speedString]
                                //"Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                                "Bounds", StringValue (boundsString));
                                //"Bounds", StringValue ("-50|250|-50|250"));

    // install the random mobility model in the primary STAs
    mobility.Install (staNodesPrimary);
  }

  // STAs random waypoint model
  else if (nodeMobility == 3) {
    // https://www.nsnam.org/doxygen/classns3_1_1_random_waypoint_mobility_model.html#details
    // Each object starts by pausing at time zero for the duration governed by the random variable 
    // "Pause". After pausing, the object will pick a new waypoint (via the PositionAllocator) and a 
    // new random speed via the random variable "Speed", and will begin moving towards the waypoint 
    // at a constant speed. When it reaches the destination, the process starts over (by pausing).

    // auxiliar string
    std::ostringstream auxString;

    // create a string with the X boundaries
    auxString << "ns3::UniformRandomVariable[Min="
              << x_position_first_AP - distanceToBorder 
              << "|Max=" << ((number_of_APs_per_row -1) * distance_between_APs) + distanceToBorder 
              << "]";

    std::string XString = auxString.str();

    if ( verboseLevel > 2 )
      std::cout << "Limits for X: " << XString << '\n';

    // clean the string
    auxString.str(std::string());

    // create a string with the Y boundaries
    auxString   << "ns3::UniformRandomVariable[Min=" << y_position_first_AP - distanceToBorder 
                << "|Max=" << ((number_of_APs / number_of_APs_per_row) -1 ) * distance_between_APs + distanceToBorder 
                << "]"; 

    std::string YString = auxString.str();

    if ( verboseLevel > 2 )
      std::cout << "Limits for Y: " << YString << '\n';


    ObjectFactory pos;
    pos.SetTypeId ( "ns3::RandomRectanglePositionAllocator");

    pos.Set ("X", StringValue (XString));
    pos.Set ("Y", StringValue (YString));

    // clean the string
    auxString.str(std::string());

    // create a string with the speed
    auxString  << "ns3::UniformRandomVariable[Min=0.0|Max=" << constantSpeed << "]";
    std::string speedString = auxString.str();
    if ( verboseLevel > 2 )
      std::cout << "Speed with which the STAs move: " << speedString << '\n';


    // clean the string
    auxString.str(std::string());

    // create a string with the pause time
    auxString  << "ns3::UniformRandomVariable[Min=0.0|Max=" << pause_time << "]";
    std::string pauseTimeString = auxString.str();
    if ( verboseLevel > 2 )
      std::cout << "The STAs will pause during " << pauseTimeString << '\n';

    Ptr<PositionAllocator> taPositionAlloc = pos.Create ()->GetObject<PositionAllocator> ();
    mobility.SetMobilityModel ( "ns3::RandomWaypointMobilityModel",
                                "Speed", StringValue (speedString),
                                //"Speed", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=10.0]"),
                                "Pause", StringValue (pauseTimeString),
                                //"Pause", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=10.0]"),
                                "PositionAllocator", PointerValue (taPositionAlloc));
    mobility.SetPositionAllocator (taPositionAlloc);
    mobility.Install (staNodesPrimary);
  }

  // add the mobility pattern to the secondary STAs
  // will be copied from that of the primary STAs
  if (numberSTAsSamePlace == 2) {

    MobilityHelper mobilitySecondary;
    // I use a Constant Velocity mobility model, which I will update every
    //time the associated main STA changes its trajectory
    mobilitySecondary.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
    mobilitySecondary.Install (staNodesSecondary);

    // copy the initial positions and velocities of the staNodesPrimary to staNodesSecondary
    NS_ASSERT(number_of_STAs == staNodesPrimary.GetN());

    for ( uint32_t j = 0; j < number_of_STAs; ++j) {
      // I use the basic class 'MobilityModel' in order
      //to get the position and velocity of the primary nodes
      Ptr<MobilityModel> poninterToMobilityModel = staNodesPrimary.Get(j)->GetObject<MobilityModel>();
      
      Vector posPrimary = poninterToMobilityModel->GetPosition();
      // this would also work
      //Vector pos = GetPosition (staNodesPrimary.Get (j));

      Vector speedPrimary = poninterToMobilityModel->GetVelocity();
      // this does NOT work if the primary nodes have a random model,
      //because the model does NOT have the method 'GetVelocity'
      //Vector speed = GetVelocity(staNodesPrimary.Get (j));

      if (verboseLevel >= 1) {    
        std::cout << "STA#" << number_of_APs + number_of_Servers + j << " Position: " << posPrimary.x << "," << posPrimary.y;
        std::cout << " Velocity: " << speedPrimary.x << "," << speedPrimary.y << " (primary)" << '\n';
      }


      // apply the position and speed of the primary node to the secondary one
      Ptr<ConstantVelocityMobilityModel> poninterToMobilityModelSecondary = staNodesSecondary.Get(j)->GetObject<ConstantVelocityMobilityModel>();
      poninterToMobilityModelSecondary->SetPosition(posPrimary);
      poninterToMobilityModelSecondary->SetVelocity(speedPrimary);

      if (verboseLevel >= 1) {
        Vector posSecondary = poninterToMobilityModelSecondary->GetPosition();        
        std::cout << "STA#" << number_of_APs + number_of_Servers + number_of_STAs + j << " Position: " << posSecondary.x << "," << posSecondary.y;

        Vector speedSecondary = poninterToMobilityModelSecondary->GetVelocity();        
        std::cout << " Velocity: " << speedSecondary.x << "," << speedSecondary.y << " (secondary)" << '\n';
      }
    }
  }

  // print debug info
  if (verboseLevel >= 1) {
    for (uint32_t j = 0; j < staNodes.GetN(); ++j) {
      Vector pos = GetPosition (staNodes.Get (j));
      std::cout << "STA#" << number_of_APs + number_of_Servers + j << " Position: " << pos.x << "," << pos.y << '\n';
    }
  }

  // Periodically report the positions of all the active STAs
  if ((verboseLevel >= 3) && (timeMonitorKPIs > 0)) {
    for (uint32_t j = 0; j < staNodes.GetN(); ++j) {
      if (STAsActive.at(j) == '1')
        Simulator::Schedule ( Seconds (INITIALTIMEINTERVAL),
                              &ReportPosition, 
                              timeMonitorKPIs, 
                              staNodes.Get(j), 
                              (number_of_APs * numberAPsSamePlace ) + j, 
                              1, 
                              verboseLevel,
                              apNodes);
      else {
        // the STA is NOT active. Do not report anything
      }
    }


    // This makes a callback every time a STA changes its course
    // see trace sources in https://www.nsnam.org/doxygen/classns3_1_1_random_walk2d_mobility_model.html
    Config::Connect ( "/NodeList/*/$ns3::MobilityModel/CourseChange", MakeCallback (&CourseChange));
  }


  if (timeMonitorKPIs > 0) {
    // Write the values of the positions of the STAs to a file
    // create a string with the name of the output file
    std::ostringstream namePositionsFile;

    namePositionsFile << outputFileName
                << "_"
                << outputFileSurname
                << "_positions.txt";

    std::ofstream ofs;
    ofs.open ( namePositionsFile.str(), std::ofstream::out | std::ofstream::trunc); // with "trunc" Any contents that existed in the file before it is open are discarded. with "app", all output operations happen at the end of the file, appending to its existing contents

    // write the first line in the file (includes the titles of the columns)
    ofs << "timestamp [s]\t"
        << "STA ID\t"
        << "destinationPort\t"
        << "STA x [m]\t"
        << "STA y [m]\t"
        << "Nearest AP ID\t" 
        << "AP x [m]\t"
        << "AP y [m]\t"
        << "distance STA-nearest AP [m]\t"
        << "Associated to AP ID\t"
        << "AP x [m]\t"
        << "AP y [m]\t"
        << "distance STA-my AP [m]\t"
        << "\n";

    // for each STA running each application, schedule the store of statistics
    if (eachSTArunsAllTheApps == false) {

      // do it for primary and secondary STAs
      for (uint32_t i = 0; i < numberSTAsSamePlace; ++i) {

        for (uint32_t j = 0; j < numberVoIPupload; ++j) {
          Simulator::Schedule ( Seconds (INITIALTIMEINTERVAL + timeMonitorKPIs), 
                                &SavePositionSTA, 
                                timeMonitorKPIs, 
                                staNodes.Get(j + (i * number_of_STAs)),
                                apNodes, 
                                INITIALPORT_VOIP_UPLOAD + j, // FIXME ¿ + (i * number_of_STAs)?
                                namePositionsFile.str());
        }

        for (uint32_t j = numberVoIPupload; j < numberVoIPupload + numberVoIPdownload; ++j) {
          Simulator::Schedule ( Seconds (INITIALTIMEINTERVAL + timeMonitorKPIs), 
                                &SavePositionSTA, 
                                timeMonitorKPIs, 
                                staNodes.Get(j + (i * number_of_STAs)), 
                                apNodes, 
                                INITIALPORT_VOIP_DOWNLOAD + j, 
                                namePositionsFile.str());
        }

        for (uint32_t j = numberVoIPupload + numberVoIPdownload; j < numberVoIPupload + numberVoIPdownload + numberTCPupload; ++j) {
          Simulator::Schedule ( Seconds (INITIALTIMEINTERVAL + timeMonitorKPIs), 
                                &SavePositionSTA, 
                                timeMonitorKPIs, 
                                staNodes.Get(j + (i * number_of_STAs)), 
                                apNodes, 
                                INITIALPORT_TCP_UPLOAD + j, 
                                namePositionsFile.str());
        }

        for (uint32_t j = numberVoIPupload + numberVoIPdownload + numberTCPupload; j < numberVoIPupload + numberVoIPdownload + numberTCPupload + numberTCPdownload; ++j) {
          Simulator::Schedule ( Seconds (INITIALTIMEINTERVAL + timeMonitorKPIs), 
                                &SavePositionSTA, 
                                timeMonitorKPIs, 
                                staNodes.Get(j + (i * number_of_STAs)), 
                                apNodes, 
                                INITIALPORT_TCP_DOWNLOAD + j, 
                                namePositionsFile.str());
        }

        for (uint32_t j = numberVoIPupload + numberVoIPdownload + numberTCPupload + numberTCPdownload; j < numberVoIPupload + numberVoIPdownload + numberTCPupload + numberTCPdownload + numberVideoDownload; ++j) {
          Simulator::Schedule ( Seconds (INITIALTIMEINTERVAL + timeMonitorKPIs), 
                                &SavePositionSTA, 
                                timeMonitorKPIs, 
                                staNodes.Get(j + (i * number_of_STAs)), 
                                apNodes, 
                                INITIALPORT_VIDEO_DOWNLOAD + j, 
                                namePositionsFile.str());
        }
      }   
    }
    else {
      // eachSTArunsAllTheApps == true

      for (uint32_t i = 0; i < numberSTAsSamePlace; ++i) {
        // if VoIP upload is active, ALL THE STAs will run a VoIP upload
        if (numberVoIPupload != 0) {
          for (uint32_t j = 0; j < number_of_STAs; ++j) {
            Simulator::Schedule ( Seconds (INITIALTIMEINTERVAL + timeMonitorKPIs), 
                                  &SavePositionSTA, 
                                  timeMonitorKPIs, 
                                  staNodes.Get(j + (i * number_of_STAs)), 
                                  apNodes, 
                                  INITIALPORT_VOIP_UPLOAD + j, 
                                  namePositionsFile.str());
          }
        }
        if (numberVoIPdownload != 0) {
          for (uint32_t j = 0; j < number_of_STAs; ++j) {
            Simulator::Schedule ( Seconds (INITIALTIMEINTERVAL + timeMonitorKPIs), 
                                  &SavePositionSTA, 
                                  timeMonitorKPIs, 
                                  staNodes.Get(j + (i * number_of_STAs)), 
                                  apNodes, 
                                  INITIALPORT_VOIP_DOWNLOAD + j, 
                                  namePositionsFile.str());
          }
        }
        if (numberTCPupload != 0) {
          for (uint32_t j = 0; j < number_of_STAs; ++j) {
            Simulator::Schedule ( Seconds (INITIALTIMEINTERVAL + timeMonitorKPIs), 
                                  &SavePositionSTA, 
                                  timeMonitorKPIs, 
                                  staNodes.Get(j + (i * number_of_STAs)), 
                                  apNodes, 
                                  INITIALPORT_TCP_UPLOAD + j, 
                                  namePositionsFile.str());
          }
        }
        if (numberTCPdownload != 0) {
          for (uint32_t j = 0; j < number_of_STAs; ++j) {
            Simulator::Schedule ( Seconds (INITIALTIMEINTERVAL + timeMonitorKPIs), 
                                  &SavePositionSTA, 
                                  timeMonitorKPIs, 
                                  staNodes.Get(j + (i * number_of_STAs)), 
                                  apNodes, 
                                  INITIALPORT_TCP_DOWNLOAD + j, 
                                  namePositionsFile.str());
          }
        }
        if (numberVideoDownload != 0) {
          for (uint32_t j = 0; j < number_of_STAs; ++j) {
            Simulator::Schedule ( Seconds (INITIALTIMEINTERVAL + timeMonitorKPIs), 
                                  &SavePositionSTA, 
                                  timeMonitorKPIs, 
                                  staNodes.Get(j + (i * number_of_STAs)), 
                                  apNodes, 
                                  INITIALPORT_VIDEO_DOWNLOAD + j, 
                                  namePositionsFile.str());
          }
        }
      }
    }
  }


  /******** create the channels (wifi, csma and point to point) *********/

  // create the wifi phy layer, using 802.11n in 5GHz

  // create the wifi channel

  // if wifiModel == 0
  // we use the "yans" model. See https://www.nsnam.org/doxygen/classns3_1_1_yans_wifi_channel_helper.html#details
  // it is described in this paper: http://cutebugs.net/files/wns2-yans.pdf
  // The yans name stands for "Yet Another Network Simulator"
  //YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();

  // if wifiModel == 1
  // we use the spectrumWifi model
  SpectrumWifiPhyHelper spectrumPhy = SpectrumWifiPhyHelper::Default ();

  if (wifiModel == 0) {

    wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
    YansWifiChannelHelper wifiChannel;

    // propagation models: https://www.nsnam.org/doxygen/group__propagation.html
    if (propagationLossModel == 0) {
      wifiChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel");
    } else if (propagationLossModel == 1) {
      wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
    }

    wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");

    wifiPhy.SetChannel (wifiChannel.Create ());
    wifiPhy.Set ("TxPowerStart", DoubleValue (powerLevel)); // a value of '1' means 1 dBm (1.26 mW)
    wifiPhy.Set ("TxPowerEnd", DoubleValue (powerLevel));
    // Experiences:   at 5GHz,  with '-15' the coverage is less than 70 m
    //                          with '-10' the coverage is about 70 m (recommended for an array with distance 50m between APs)

    wifiPhy.Set ("ShortGuardEnabled", BooleanValue (false));

    wifiPhy.Set ("ChannelWidth", UintegerValue (channelWidthPrimary));
    //wifiPhy.Set ("ChannelNumber", UintegerValue(ChannelNo)); //This is done later

    wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

    if (errorRateModel == 0) { // Nist
      wifiPhy.SetErrorRateModel ("ns3::NistErrorRateModel");
    } else { // errorRateModel == 1 (Yans)
      wifiPhy.SetErrorRateModel ("ns3::YansErrorRateModel");      
    }

    /*     //FIXME: Can this be done with YANS?

    //Ptr<wifiPhy> myphy = node->GetObject<wifiPhy> ();
    if (numOperationalChannelsPrimary > 1) 
      for (uint32_t k = 0; k < numOperationalChannelsPrimary; k++)
        wifiPhy.AddOperationalChannel ( availableChannels[k] );
    */

  }
  else {
    // wifiModel == 1

    // The energy of a received signal should be higher than this threshold (dbm) to allow the PHY layer to declare CCA BUSY state
    // this works until ns3-29:
    // Config::SetDefault ("ns3::WifiPhy::CcaMode1Threshold", DoubleValue (-95.0));
    // this should work for ns3-30 (but it does not work):
    //Config::SetDefault ("ns3::WifiPhy::SetCcaEdThreshold", DoubleValue (-95.0));
    // The energy of a received signal should be higher than this threshold (dbm) to allow the PHY layer to detect the signal.
    Config::SetDefault ("ns3::WifiPhy::EnergyDetectionThreshold", DoubleValue (-95.0));
    

    // Use multimodel spectrum channel, https://www.nsnam.org/doxygen/classns3_1_1_multi_model_spectrum_channel.html
    Ptr<MultiModelSpectrumChannel> spectrumChannel = CreateObject<MultiModelSpectrumChannel> ();

    // propagation models: https://www.nsnam.org/doxygen/group__propagation.html
    if (propagationLossModel == 0) {
      //spectrumChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel");
      Ptr<LogDistancePropagationLossModel> lossModel = CreateObject<LogDistancePropagationLossModel> ();
      spectrumChannel->AddPropagationLossModel (lossModel);
    } else if (propagationLossModel == 1) {
      //spectrumChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
      Ptr<FriisPropagationLossModel> lossModel = CreateObject<FriisPropagationLossModel> ();
      spectrumChannel->AddPropagationLossModel (lossModel);
    } else if (propagationLossModel == 2) {
      //spectrumChannel.AddPropagationLoss ("ns3::FriisSpectrumPropagationLossModel");
      Ptr<FriisSpectrumPropagationLossModel> lossModel = CreateObject<FriisSpectrumPropagationLossModel> ();
      spectrumChannel->AddSpectrumPropagationLossModel (lossModel);
    }

    // delay model
    Ptr<ConstantSpeedPropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel> ();
    spectrumChannel->SetPropagationDelayModel (delayModel);

    spectrumPhy.SetChannel (spectrumChannel);
    if (errorRateModel == 0) { //Nist
      spectrumPhy.SetErrorRateModel ("ns3::NistErrorRateModel");
    } else { // errorRateModel == 1 (Yans)
      spectrumPhy.SetErrorRateModel ("ns3::YansErrorRateModel");      
    }


    //spectrumPhy.Set ("Frequency", UintegerValue (5180));
    //spectrumPhy.Set ("ChannelNumber", UintegerValue (ChannelNo)); //This is done later
    spectrumPhy.Set ("TxPowerStart", DoubleValue (powerLevel));  // a value of '1' means 1 dBm (1.26 mW)
    spectrumPhy.Set ("TxPowerEnd", DoubleValue (powerLevel));

    spectrumPhy.Set ("ShortGuardEnabled", BooleanValue (false));
    spectrumPhy.Set ("ChannelWidth", UintegerValue (channelWidthPrimary));

    Ptr<SpectrumWifiPhy> m_phy;
    m_phy = CreateObject<SpectrumWifiPhy> ();

    // Test of the Addoperationalchannel functionality
    //(*m_phy).DoChannelSwitch (uint8_t(40) ); It is private
    // Add a channel number to the list of operational channels.
    // https://www.nsnam.org/doxygen/classns3_1_1_spectrum_wifi_phy.html#a948c6d197accf2028529a2842ec68816

    // https://groups.google.com/forum/#!topic/ns-3-users/Ih8Hgs2qgeg

/*    // This does not work, i.e. the STA does not scan in other channels
    if (numOperationalChannelsPrimary > 1) 
      for (uint32_t k = 0; k < numOperationalChannelsPrimary; k++)
        (*m_phy).AddOperationalChannel ( availableChannels[k] );*/
  }


  // Create a Wifi helper in an empty state: all its parameters must be set before calling ns3::WifiHelper::Install.
  // https://www.nsnam.org/doxygen/classns3_1_1_wifi_helper.html
  // The default state is defined as being an Adhoc MAC layer with an ARF rate control
  // algorithm and both objects using their default attribute values. By default, configure MAC and PHY for 802.11a.
  WifiHelper wifi;
  WifiHelper wifiSecondary; // only used for the secondary WiFi card of each STA (it is NOT used for the APs in the second frequency)

  if ( verboseLevel > 3 )
    wifi.EnableLogComponents ();  // Turn on all Wifi logging

  // The SetRemoteStationManager method tells the helper the type of rate control algorithm to use.
  // constant_rate_wifi_manager uses always the same transmission rate for every packet sent.
  // https://www.nsnam.org/doxygen/classns3_1_1_constant_rate_wifi_manager.html#details
  //wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("HtMcs7"), "ControlMode", StringValue ("HtMcs0"));

  // Select the most appropriate wifimanager
  // ARF Rate control algorithm.
  // arf_wifi_manager implements the so-called ARF algorithm which was initially described in WaveLAN-II: A High-performance wireless LAN for the unlicensed band, by A. Kamerman and L. Monteban. in Bell Lab Technical Journal, pages 118-133, Summer 1997.
  // https://www.nsnam.org/doxygen/classns3_1_1_arf_wifi_manager.html
  // This RAA (Rate Adaptation Algorithm) does not support HT, VHT nor HE modes and will error exit if the user tries to configure 
  // this RAA with a Wi-Fi MAC that has VhtSupported, HtSupported or HeSupported set
  // wifi.SetRemoteStationManager ("ns3::ArfWifiManager");

  // The next line is not necessary, as it only defines the default WifiRemoteStationManager
  //Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", 
  //                    enableRtsCts ? StringValue ("0") : StringValue ("999999")); // if enableRtsCts is true, I select the first option


  std::string bandPrimary = getWirelessBandOfStandard(convertVersionToStandard(version80211primary));
  std::string bandSecondary = getWirelessBandOfStandard(convertVersionToStandard(version80211secondary));

  // MinstrelHt and Ideal do support HT/VHT (i.e. 802.11n and above)
  if (rateModel == "Constant") {
    if (bandPrimary == "2.4 GHz") {

      // more rates here https://www.nsnam.org/doxygen/wifi-spectrum-per-example_8cc_source.html
      wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                    "DataMode", StringValue ("HtMcs7"),
                                    "ControlMode", StringValue ("HtMcs0"),
                                    "RtsCtsThreshold", UintegerValue (RtsCtsThreshold24GHz));
      if (bandSecondary == "5 GHz") {
        wifiSecondary.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                      "DataMode", StringValue ("HtMcs7"),
                                      "ControlMode", StringValue ("HtMcs0"),
                                      "RtsCtsThreshold", UintegerValue (RtsCtsThreshold5GHz));        
      }

    /*  This is another option:
      wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                    "DataMode", DataRate,
                                    "ControlMode", DataRate,
                                    "RtsCtsThreshold", UintegerValue (ctsThr));
    */      
    }
    else if (bandPrimary == "5 GHz") {

      // more rates here https://www.nsnam.org/doxygen/wifi-spectrum-per-example_8cc_source.html
      wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                    "DataMode", StringValue ("HtMcs7"),
                                    "ControlMode", StringValue ("HtMcs0"),
                                    "RtsCtsThreshold", UintegerValue (RtsCtsThreshold5GHz));

      if (bandSecondary == "2.4 GHz")
        wifiSecondary.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                      "DataMode", StringValue ("HtMcs7"),
                                      "ControlMode", StringValue ("HtMcs0"),
                                      "RtsCtsThreshold", UintegerValue (RtsCtsThreshold24GHz));

      /*  This is another option:
        wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                      "DataMode", DataRate,
                                      "ControlMode", DataRate,
                                      "RtsCtsThreshold", UintegerValue (ctsThr));
      */        
    }
    else {
      NS_ASSERT(false);
    }
  }

  else if (rateModel == "Ideal") {
    // Ideal Wifi Manager, https://www.nsnam.org/doxygen/classns3_1_1_ideal_wifi_manager.html#details
    if (bandPrimary == "2.4 GHz") {
      wifi.SetRemoteStationManager ("ns3::IdealWifiManager",
                                    //"MaxSlrc", UintegerValue (7)  // 7 is the default value
                                    "RtsCtsThreshold", UintegerValue (RtsCtsThreshold24GHz));

      if (bandSecondary == "5 GHz") {
        wifiSecondary.SetRemoteStationManager ("ns3::IdealWifiManager",
                                      //"MaxSlrc", UintegerValue (7)  // 7 is the default value
                                      "RtsCtsThreshold", UintegerValue (RtsCtsThreshold5GHz));
      }
    }
    else if (bandPrimary == "5 GHz") {
      wifi.SetRemoteStationManager ("ns3::IdealWifiManager",
                                    //"MaxSlrc", UintegerValue (7)  // 7 is the default value
                                    "RtsCtsThreshold", UintegerValue (RtsCtsThreshold5GHz));

      if (bandSecondary == "2.4 GHz") {
        wifiSecondary.SetRemoteStationManager ("ns3::IdealWifiManager",
                                      //"MaxSlrc", UintegerValue (7)  // 7 is the default value
                                      "RtsCtsThreshold", UintegerValue (RtsCtsThreshold24GHz));        
      }
    }
    else {
      NS_ASSERT(false);
    }

    /*  This is another option:
    wifi.SetRemoteStationManager ("ns3::IdealWifiManager",
                                  //"MaxSlrc", UintegerValue (7)  // 7 is the default value
                                  "RtsCtsThreshold", enableRtsCts ? UintegerValue (RtsCtsThreshold) : UintegerValue (999999)); // if enableRtsCts is true, I select the first option
    */

  }

  else if (rateModel == "Minstrel") {
    // I obtain some errors when running Minstrel
    // https://www.nsnam.org/bugzilla/show_bug.cgi?id=1797
    // https://www.nsnam.org/doxygen/classns3_1_1_minstrel_ht_wifi_manager.html
    if (bandPrimary == "2.4 GHz") {
      wifi.SetRemoteStationManager ("ns3::MinstrelHtWifiManager",
                                    "RtsCtsThreshold", UintegerValue (RtsCtsThreshold24GHz),
                                    "PrintStats", BooleanValue (false)); // if you set this to true, you will obtain a file with the stats

      if (bandSecondary == "5 GHz") {
        wifiSecondary.SetRemoteStationManager ("ns3::MinstrelHtWifiManager",
                                      "RtsCtsThreshold", UintegerValue (RtsCtsThreshold5GHz),
                                      "PrintStats", BooleanValue (false)); // if you set this to true, you will obtain a file with the stats

      }
    }
    else if (bandPrimary == "5 GHz") {
      wifi.SetRemoteStationManager ("ns3::MinstrelHtWifiManager",
                                    "RtsCtsThreshold", UintegerValue (RtsCtsThreshold5GHz),
                                    "PrintStats", BooleanValue (false)); // if you set this to true, you will obtain a file with the stats

      if (bandSecondary == "2.4 GHz") {
        wifiSecondary.SetRemoteStationManager ("ns3::MinstrelHtWifiManager",
                                      "RtsCtsThreshold", UintegerValue (RtsCtsThreshold24GHz),
                                      "PrintStats", BooleanValue (false)); // if you set this to true, you will obtain a file with the stats  
      }
    }
    else {
      NS_ASSERT(false);
    }
  }



  // Create the MAC helper
  // https://www.nsnam.org/doxygen/classns3_1_1_wifi_mac_helper.html
  WifiMacHelper wifiMacPrimary;

  WifiMacHelper wifiMacSecondary; // used for the secondary APs



  /*************************** Define the APs ******************************/
  // create '2 x number_of_APs' 
  //
  // example: the user selects 'number_of_APs=3' and 'numberAPsSamePlace=2'
  //    in this case, 6 APs will be created:
  //    - AP0 and AP3 will be in the same place, and will have different WiFi versions
  //    - AP1 and AP4 will be in the same place, and will have different WiFi versions
  //    - AP2 and AP5 will be in the same place, and will have different WiFi versions

  for (uint32_t j = 0 ; j < numberAPsSamePlace ; ++j) {

    for (uint32_t i = 0 ; i < number_of_APs ; ++i ) {

      // Create an empty record per AP
      AP_record *m_AP_record = new AP_record;
      AP_vector.push_back(m_AP_record);

      // I use an auxiliary device container and an auxiliary interface container
      NetDeviceContainer apWiFiDev;

      // create an ssid for each wifi AP
      std::ostringstream oss;

      if (j == 0)
        // primary AP
        oss << "wifi-default-" << i; // Each AP will have a different SSID
      else if (j == 1)
        // secondary AP (an AP in the same place)
        oss << "wifi-default-" << i << "-secondary"; // SSID for the secondary AP: just add 'secondary' at the end

      Ssid apssid = Ssid (oss.str());

      // define the standard to follow (see https://www.nsnam.org/doxygen/group__wifi.html#ga1299834f4e1c615af3ca738033b76a49)
      if (j == 0) {
        // primary AP
        wifi.SetStandard (convertVersionToStandard(version80211primary));

        if (verboseLevel >= 1) {
          std::cout << "AP     #" << i + j*number_of_APs
                    << "\tstandard: 802." << version80211primary
                    << '\n';
        }
      }
      else if (j == 1) {
        // secondary AP
        wifi.SetStandard (convertVersionToStandard(version80211secondary));

        if (verboseLevel >= 1) {
          std::cout << "AP     #" << i + j*number_of_APs
                    << "\tstandard: 802." << version80211secondary
                    << '\n';
        }
      }

      // setup the APs. Modify the maxAmpduSize depending on a random variable
      Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable> ();     
      double random = uv->GetValue ();

      // Max AMPDU size of this AP
      uint32_t my_maxAmpduSize;

      if (j == 0) {
        // primary AP
        if ( random < rateAPsWithAMPDUenabled ) {
          my_maxAmpduSize = maxAmpduSize;
        }
        else {
          my_maxAmpduSize = 0;        
        }
      }
      else if (j == 1) {
        // secondary AP
        if ( random < rateAPsWithAMPDUenabled ) {
          my_maxAmpduSize = maxAmpduSizeSecondary;
        }
        else {
          my_maxAmpduSize = 0;        
        }
      }


      // define AMPDU size
      wifiMacPrimary.SetType ("ns3::ApWifiMac",
                              "Ssid", SsidValue (apssid),
                              "QosSupported", BooleanValue (true),
                              "BeaconGeneration", BooleanValue (true),  // Beacon generation is necessary in an AP
                              "BE_MaxAmpduSize", UintegerValue (my_maxAmpduSize),
                              "BK_MaxAmpduSize", UintegerValue (my_maxAmpduSize),
                              "VI_MaxAmpduSize", UintegerValue (my_maxAmpduSize),
                              "VO_MaxAmpduSize", UintegerValue (my_maxAmpduSize)); 

      wifiMacSecondary.SetType ("ns3::ApWifiMac",
                                "Ssid", SsidValue (apssid),
                                "QosSupported", BooleanValue (true),
                                "BeaconGeneration", BooleanValue (true),  // Beacon generation is necessary in an AP
                                "BE_MaxAmpduSize", UintegerValue (my_maxAmpduSize),
                                "BK_MaxAmpduSize", UintegerValue (my_maxAmpduSize),
                                "VI_MaxAmpduSize", UintegerValue (my_maxAmpduSize),
                                "VO_MaxAmpduSize", UintegerValue (my_maxAmpduSize)); 

      /*  Other options:
        // - enable A-MSDU (with maximum size of 8 kB) but disable A-MPDU;
        wifiMacPrimary.SetType ( "ns3::ApWifiMac",
                          "Ssid", SsidValue (apssid),
                          "BeaconGeneration", BooleanValue (true),
                          "BE_MaxAmpduSize", UintegerValue (0), //Disable A-MPDU
                          "BE_MaxAmsduSize", UintegerValue (7935)); //Enable A-MSDU with the highest maximum size allowed by the standard (7935 bytes)

        // - use two-level aggregation (A-MPDU with maximum size of 32 kB and A-MSDU with maximum size of 4 kB).
        wifiMacPrimary.SetType ( "ns3::ApWifiMac",
                          "Ssid", SsidValue (apssid),
                          "BeaconGeneration", BooleanValue (true),
                          "BE_MaxAmpduSize", UintegerValue (32768), //Enable A-MPDU with a smaller size than the default one
                          "BE_MaxAmsduSize", UintegerValue (3839)); //Enable A-MSDU with the smallest maximum size allowed by the standard (3839 bytes)
      */

      // Define the channel of the AP
      // channel to be used by these APs (main and secondary)
      uint8_t ChannelNoForThisAP;

      // Use the available channels in turn
      if (j == 0)
        // primary AP
        ChannelNoForThisAP = availableChannels[i % numOperationalChannelsPrimary];
      else if (j == 1)
        // secondary AP
        ChannelNoForThisAP = availableChannelsSecondary[i % numOperationalChannelsSecondary]; // same channel
      



      /*
      else {
        // define the characteristics of the APs manually
        // define the standard to follow (see https://www.nsnam.org/doxygen/group__wifi.html#ga1299834f4e1c615af3ca738033b76a49)
        if (j == 0) {
          // primary AP
          wifi.SetStandard (convertVersionToStandard(version80211primary));

          if (verboseLevel >= 1) {
            std::cout << "AP     #" << i + j*number_of_APs
                      << "standard: 802." << version80211primary
                      << '\n';
          }
        }
        else if (j == 1) {
          // secondary AP
          wifi.SetStandard (convertVersionToStandard(version80211secondary));

          if (verboseLevel >= 1) {
            std::cout << "AP     #" << i + j*number_of_APs
                      << "standard: 802." << version80211secondary
                      << '\n';
          }
        }

        // Modify the maxAmpduSize depending on the AP number
        uint32_t my_maxAmpduSize = maxAmpduSize;
        if ( j == 0 ) {
          // primary AP
          my_maxAmpduSize = maxAmpduSize;
        }
        else if ( j == 1 ) {
          // secondary AP
          my_maxAmpduSize = maxAmpduSizeSecondary;
        }

        wifiMacPrimary.SetType ("ns3::ApWifiMac",
                                "Ssid", SsidValue (apssid),
                                "QosSupported", BooleanValue (true),
                                "BeaconGeneration", BooleanValue (true),  // Beacon generation is necessary in an AP
                                "BE_MaxAmpduSize", UintegerValue (my_maxAmpduSize),
                                "BK_MaxAmpduSize", UintegerValue (my_maxAmpduSize),
                                "VI_MaxAmpduSize", UintegerValue (my_maxAmpduSize),
                                "VO_MaxAmpduSize", UintegerValue (my_maxAmpduSize)); 

        wifiMacSecondary.SetType ("ns3::ApWifiMac",
                                  "Ssid", SsidValue (apssid),
                                  "QosSupported", BooleanValue (true),
                                  "BeaconGeneration", BooleanValue (true),  // Beacon generation is necessary in an AP
                                  "BE_MaxAmpduSize", UintegerValue (my_maxAmpduSize),
                                  "BK_MaxAmpduSize", UintegerValue (my_maxAmpduSize),
                                  "VI_MaxAmpduSize", UintegerValue (my_maxAmpduSize),
                                  "VO_MaxAmpduSize", UintegerValue (my_maxAmpduSize)); 

        // Define the channel of each AP
        if ( j == 0 ) {
          // primary AP
          ChannelNoForThisAP = availableChannels[0];
        }
        else if ( j == 1 ) {
          // secondary AP
          ChannelNoForThisAP = availableChannels[1];
        }
      }*/

      if (APsActive.at(i + j*number_of_APs) == '1') {
        // install the wifi Phy and MAC in the APs
        if (j == 0) {
          // primary AP
          if (wifiModel == 0) { 
            // Yans wifi
            wifiPhy.Set ("ChannelNumber", UintegerValue(ChannelNoForThisAP));
            apWiFiDev = wifi.Install (wifiPhy, wifiMacPrimary, apNodes.Get (i + (j*number_of_APs)));

            if (verboseLevel >= 3) {
              std::cout << "AP     #" << i + j*number_of_APs
                        << "\tinstalled 'wifiPhy' and 'wifiMacPrimary' with SSID " << apssid
                        << '\n';
            }
          }
          else {
            // spectrumwifi
            spectrumPhy.Set ("ChannelNumber", UintegerValue(ChannelNoForThisAP));
            apWiFiDev = wifi.Install (spectrumPhy, wifiMacPrimary, apNodes.Get (i + (j*number_of_APs)));

            if (verboseLevel >= 3) {
              std::cout << "AP     #" << i + j*number_of_APs
                        << "\tinstalled 'spectrumPhy' and 'wifiMacPrimary' with SSID " << apssid
                        << '\n';
            }
          }           
        }
        else if (j == 1) {
          // secondary AP
          if (wifiModel == 0) { 
            // Yans wifi
            wifiPhy.Set ("ChannelNumber", UintegerValue(ChannelNoForThisAP));
            apWiFiDev = wifi.Install (wifiPhy, wifiMacSecondary, apNodes.Get (i + (j*number_of_APs)));
   
            if (verboseLevel >= 3) {
              std::cout << "AP     #" << i + j*number_of_APs
                        << "\tinstalled 'wifiPhy' and 'wifiMacSecondary' with SSID " << apssid
                        << '\n';
            }
          }
          else {
            // spectrumwifi
            spectrumPhy.Set ("ChannelNumber", UintegerValue(ChannelNoForThisAP));
            apWiFiDev = wifi.Install (spectrumPhy, wifiMacSecondary, apNodes.Get (i + (j*number_of_APs)));

            if (verboseLevel >= 3) {
              std::cout << "AP     #" << i + j*number_of_APs
                        << "\tinstalled 'spectrumPhy' and 'wifiMacSecondary' with SSID " << apssid
                        << '\n';
            }
          } 
        }

        /*
        I try this in order to avoid a comment "Attribute 'ShortGuardEnabled' is deprecated: Use the HtConfiguration instead"
        // define HT capabilities
        uint16_t clientShortGuardInterval = 400;
        if (APsActive.at(i + j*number_of_APs) == '1') {
          // install the wifi Phy and MAC in the APs
          if (j == 0) {

            // primary AP
            if (wifiModel == 0) { 
              // Yans wifi
              std::cout << "wifi phy - i " << i << " j " << j << '\n';
              Ptr<WifiNetDevice> wifidevice = DynamicCast<WifiNetDevice> (apWiFiDev.Get(i + (j*number_of_APs)));
              Ptr<HtConfiguration> clientHtConfiguration = wifidevice->GetHtConfiguration ();
              clientHtConfiguration->SetShortGuardIntervalSupported (clientShortGuardInterval == 400);
            }
            else {
              // spectrumwifiphy
              std::cout << "spectrumwifiphy - i " << i << " j " << j << '\n';
              Ptr<WifiNetDevice> wifidevice = DynamicCast<WifiNetDevice> (apWiFiDev.Get(i + (j*number_of_APs)));
              
              // this line does not work:
              Ptr<HeConfiguration> clientHeConfiguration = wifidevice->GetHeConfiguration ();

              clientHeConfiguration->SetGuardInterval (NanoSeconds (clientShortGuardInterval));
            }
          }
        }*/



        // auxiliar string
        std::ostringstream auxString;
        std::string myaddress;

        // create a string with the MAC
        auxString << apWiFiDev.Get(0)->GetAddress();
        myaddress = auxString.str();

        // update the AP record with the correct value, using the correct version of the function
        AP_vector[i + j*number_of_APs]->SetApRecord (i + j*number_of_APs, myaddress, my_maxAmpduSize);

        // fill the values of the vector of APs
        AP_vector[i + j*number_of_APs]->setWirelessChannel(ChannelNoForThisAP);


        // print the IP and the MAC address, and the WiFi channel
        if (verboseLevel >= 1) {
          std::cout << "AP     #" << i + j*number_of_APs << "\tMAC address:  " << apWiFiDev.Get(0)->GetAddress() << '\n';
          std::cout << "        " << "\tWi-Fi channel: " << uint32_t(AP_vector[i + (j*number_of_APs)]->GetWirelessChannel()) << '\n'; // convert to uint32_t in order to print it
          std::cout << "\t\tAP with MAC " << myaddress << " added to the list of APs. AMPDU size " << my_maxAmpduSize << " bytes" << '\n';
        }

        // also report the position of the AP (defined above)
        if (verboseLevel >= 1) {
          //ReportPosition (timeMonitorKPIs, backboneNodes.Get(i), i, 0, 1, apNodes); this would report the position every second
          //Vector pos = GetPosition (backboneNodes.Get (i));
          Vector pos = GetPosition (apNodes.Get (i + j*number_of_APs));
          std::cout << "\t\tPosition: " << pos.x << "," << pos.y << '\n';
        }

        // save everything in containers (add a line to the vector of containers, including the new AP device and interface)
        apWiFiDevices.push_back (apWiFiDev);

        if (VERBOSE_FOR_DEBUG >=1)
          std::cout << "Size of 'apWiFiDevices': " << apWiFiDevices.size() << '\n';
      }
      else if (APsActive.at(i + j*number_of_APs) == '0') {

        // update the AP record with the correct value, using the correct version of the function
        AP_vector[i + j*number_of_APs]->SetApRecord (i + j*number_of_APs, "0.0.0.0", 0);

        // fill the values of the vector of APs
        AP_vector[i + j*number_of_APs]->setWirelessChannel(0);

        if (verboseLevel >= 1) {
          std::cout << "AP     #" << i + (j * number_of_APs)
                    << "\twill NOT be active"
                    << '\n';
        }
      }
      else {
        std::cout << "ERROR: all the elements of 'APsActive' MUST be '0' or '1'" << '\n';
        NS_ASSERT(false);
      }
    }    
  }
  /*************************** end of - Define the APs ******************************/


  /*************************** Define the STAs ******************************/
  // connect each of the STAs to the wifi channel

  // An ssid variable for the STAs
  Ssid stassid; // If you leave it blank, the STAs will send broadcast assoc requests

  // find the frequency band(s) where a STA can find an AP
  // if it has a single card, only one band can be used
  // if it has two cards, it depends on the band(s) supported by the present AP(s)
  std::string bandsSupportedByAPs = bandsSupportedByTheAPs(numberAPsSamePlace, version80211primary, version80211secondary);
  std::string bandsSupportedBySTAs = bandsSupportedByTheSTAs(numberSTAsSamePlace, version80211primary, version80211secondary);

  //std::string bandPrimary = getWirelessBandOfStandard(convertVersionToStandard(version80211primary));
  //std::string bandSecondary = getWirelessBandOfStandard(convertVersionToStandard(version80211secondary));

  if (VERBOSE_FOR_DEBUG >= 1)
    std::cout << "\t\tFrequency band(s) supported by APs: " << bandsSupportedByAPs
              << ". Supported by STAs: " <<  bandsSupportedBySTAs 
              << std::endl;

  for (uint32_t i = 0; i < number_of_STAs * numberSTAsSamePlace; i++) {

    // I use two auxiliary device containers
    // at the end of the 'for', they are added to staDevices using 'staDevices.push_back (staDev);'
    NetDeviceContainer staDev, staDevSecondary;

    // I use two auxiliary interface containers
    // at the end of the 'for', they are assigned an IP address,
    //and they are added to staInterfaces using 'staInterfaces.push_back (staInterface);'
    Ipv4InterfaceContainer staInterface, staInterfaceSecondary;

    // If none of the aggregation algorithms is enabled, all the STAs aggregate
    if ((aggregationDisableAlgorithm == 0) && (aggregationDynamicAlgorithm == 0)) {
      wifiMacPrimary.SetType ("ns3::StaWifiMac",
                              "Ssid", SsidValue (stassid));

      wifiMacSecondary.SetType ("ns3::StaWifiMac",
                                "Ssid", SsidValue (stassid));
    }

    else {
      // The aggregation algorithm is enabled
      // Install:
      // - non aggregation in the VoIP STAs
      // - aggregation in the TCP STAs

      
      if ( i < numberVoIPupload + numberVoIPdownload ) {
        // The VoIP STAs do NOT aggregate

        if ( (HANDOFFMETHOD == 1) && (wifiModel == 1) ) {
          wifiMacPrimary.SetType ( "ns3::StaWifiMac",
                            "Ssid", SsidValue (stassid),  // left blank, so the STAs will send broadcast assoc requests
                            "QosSupported", BooleanValue (true),
                            "ActiveProbing", BooleanValue (true), // If you set this, STAs will not connect when aggregation algorithm is running
                            "BE_MaxAmpduSize", UintegerValue (0),
                            "BK_MaxAmpduSize", UintegerValue (0),
                            "VI_MaxAmpduSize", UintegerValue (0),
                            "VO_MaxAmpduSize", UintegerValue (0)); //Disable A-MPDU in the STAs

          wifiMacSecondary.SetType ( "ns3::StaWifiMac",
                            "Ssid", SsidValue (stassid),  // left blank, so the STAs will send broadcast assoc requests
                            "QosSupported", BooleanValue (true),
                            "ActiveProbing", BooleanValue (true), // If you set this, STAs will not connect when aggregation algorithm is running
                            "BE_MaxAmpduSize", UintegerValue (0),
                            "BK_MaxAmpduSize", UintegerValue (0),
                            "VI_MaxAmpduSize", UintegerValue (0),
                            "VO_MaxAmpduSize", UintegerValue (0)); //Disable A-MPDU in the STAs
        }
        else {
          wifiMacPrimary.SetType ( "ns3::StaWifiMac",
                    "Ssid", SsidValue (stassid),  // left blank, so the STAs will send broadcast assoc requests
                    "QosSupported", BooleanValue (true),
                    //"ActiveProbing", BooleanValue (true), // If you set this, STAs will not connect when aggregation algorithm is running
                    "BE_MaxAmpduSize", UintegerValue (0),
                    "BK_MaxAmpduSize", UintegerValue (0),
                    "VI_MaxAmpduSize", UintegerValue (0),
                    "VO_MaxAmpduSize", UintegerValue (0)); //Disable A-MPDU in the STAs

          wifiMacSecondary.SetType ( "ns3::StaWifiMac",
                    "Ssid", SsidValue (stassid),  // left blank, so the STAs will send broadcast assoc requests
                    "QosSupported", BooleanValue (true),
                    //"ActiveProbing", BooleanValue (true), // If you set this, STAs will not connect when aggregation algorithm is running
                    "BE_MaxAmpduSize", UintegerValue (0),
                    "BK_MaxAmpduSize", UintegerValue (0),
                    "VI_MaxAmpduSize", UintegerValue (0),
                    "VO_MaxAmpduSize", UintegerValue (0)); //Disable A-MPDU in the STAs          
        }
      }

      else {
        // The TCP and the Video STAs do aggregate
        if ( (HANDOFFMETHOD == 1) && (wifiModel == 1) ) {
          wifiMacPrimary.SetType ( "ns3::StaWifiMac",
                            "Ssid", SsidValue (stassid),  // left blank, so the STAs will send broadcast assoc requests
                            "QosSupported", BooleanValue (true),
                            "ActiveProbing", BooleanValue (true), // If you set this, STAs will not connect when aggregation algorithm is running
                            "BE_MaxAmpduSize", UintegerValue (maxAmpduSize),
                            "BK_MaxAmpduSize", UintegerValue (maxAmpduSize),
                            "VI_MaxAmpduSize", UintegerValue (maxAmpduSize),
                            "VO_MaxAmpduSize", UintegerValue (maxAmpduSize));

          wifiMacSecondary.SetType ( "ns3::StaWifiMac",
                            "Ssid", SsidValue (stassid),  // left blank, so the STAs will send broadcast assoc requests
                            "QosSupported", BooleanValue (true),
                            "ActiveProbing", BooleanValue (true), // If you set this, STAs will not connect when aggregation algorithm is running
                            "BE_MaxAmpduSize", UintegerValue (maxAmpduSize),
                            "BK_MaxAmpduSize", UintegerValue (maxAmpduSize),
                            "VI_MaxAmpduSize", UintegerValue (maxAmpduSize),
                            "VO_MaxAmpduSize", UintegerValue (maxAmpduSize));
        }
        else {
          wifiMacPrimary.SetType ( "ns3::StaWifiMac",
                            "Ssid", SsidValue (stassid), // left blank, so the STAs will send broadcast assoc requests
                            "QosSupported", BooleanValue (true),
                            //"ActiveProbing", BooleanValue (true), // If you set this, STAs will not connect when aggregation algorithm is running
                            "BE_MaxAmpduSize", UintegerValue (maxAmpduSize),
                            "BK_MaxAmpduSize", UintegerValue (maxAmpduSize),
                            "VI_MaxAmpduSize", UintegerValue (maxAmpduSize),
                            "VO_MaxAmpduSize", UintegerValue (maxAmpduSize));          

          wifiMacSecondary.SetType ( "ns3::StaWifiMac",
                            "Ssid", SsidValue (stassid), // left blank, so the STAs will send broadcast assoc requests
                            "QosSupported", BooleanValue (true),
                            //"ActiveProbing", BooleanValue (true), // If you set this, STAs will not connect when aggregation algorithm is running
                            "BE_MaxAmpduSize", UintegerValue (maxAmpduSize),
                            "BK_MaxAmpduSize", UintegerValue (maxAmpduSize),
                            "VI_MaxAmpduSize", UintegerValue (maxAmpduSize),
                            "VO_MaxAmpduSize", UintegerValue (maxAmpduSize));  
        }
      }
      /*
      // Other options (Enable AMSDU, and also enable AMSDU and AMPDU at the same time)
      wifiMacPrimary.SetType ( "ns3::StaWifiMac",
                        "Ssid", SsidValue (stassid),
                        "BE_MaxAmpduSize", UintegerValue (0), //Disable A-MPDU
                        "BE_MaxAmsduSize", UintegerValue (7935)); //Enable A-MSDU with the highest maximum size allowed by the standard (7935 bytes)

      wifiMacPrimary.SetType ( "ns3::StaWifiMac",
                        "Ssid", SsidValue (stassid),
                        "BE_MaxAmpduSize", UintegerValue (32768), //Enable A-MPDU with a smaller size than the default one
                        "BE_MaxAmsduSize", UintegerValue (3839)); //Enable A-MSDU with the smallest maximum size allowed by the standard (3839 bytes)
      */
    }


    // channel number that will be used by this STA
    uint8_t ChannelNoForThisSTA = 0;   // initially set to 0, but it MUST get a value
    // 802.11 version of this STA
    std::string version80211ThisSTA;
    // band of this STA
    std::string bandThisSTA;
    // set it if the STA has to be disabled
    bool STAinNonSupportedBand = false;

    if ( i < number_of_STAs ) {
      // this is a primary STA
      version80211ThisSTA = version80211primary;
      bandThisSTA = getWirelessBandOfStandard(convertVersionToStandard(version80211primary));
      //unusedChannelThisSTA = unusedChannelPrimary;
    }
    else {
      // this is a secondary STA           
      version80211ThisSTA = version80211secondary;
      bandThisSTA = getWirelessBandOfStandard(convertVersionToStandard(version80211secondary));
      //unusedChannelThisSTA = unusedChannelSecondary;
    }

    // pointer to the nearest AP
    Ptr<Node> myNearestAp, myNearestAp24GHz, myNearestAp5GHz;  
    uint32_t myNearestApId;

    if (bandsSupportedByAPs == "both") {
      // there are APs in both bands

      if (VERBOSE_FOR_DEBUG >= 1)
        std::cout << "\n\t\tthere are APs in both bands" << "\n";

      // look for the nearest AP in the corresponding band
      myNearestAp = nearestAp (apNodes, staNodes.Get(i), verboseLevel, bandThisSTA);
      myNearestApId = (myNearestAp)->GetId();
      ChannelNoForThisSTA = GetAP_WirelessChannel (myNearestApId, 0);
      if (verboseLevel >= 1)
        std::cout << "STA\t#" << staNodes.Get(i)->GetId() 
                  << "\tBand " << getWirelessBandOfChannel(ChannelNoForThisSTA)
                  << ". The nearest AP is AP#" <<  myNearestApId 
                  << ", in channel " << uint16_t(ChannelNoForThisSTA)
                  << '\n';
    }
    else {  // (bandsSupportedByAPs != "both")
      // there are APs in only one band
      if (verboseLevel >= 1)
        std::cout << "\n\t\tthere are APs in only one band: " << bandsSupportedByAPs << "\n";

      if (bandsSupportedByAPs == bandThisSTA) {

        // look for the nearest AP in the band supported by the APs
        myNearestAp = nearestAp (apNodes, staNodes.Get(i), verboseLevel, bandsSupportedByAPs);
        myNearestApId = (myNearestAp)->GetId();

        ChannelNoForThisSTA = GetAP_WirelessChannel (myNearestApId, 0 /*verboseLevel*/);
        if (verboseLevel >= 1)
          std::cout << "STA\t#" << staNodes.Get(i)->GetId()
                    << "\tBand " << getWirelessBandOfChannel(ChannelNoForThisSTA)
                    << ". The nearest APis AP#" <<  myNearestApId 
                    << ", in channel " << uint16_t(ChannelNoForThisSTA)
                    << '\n';
      }
      else {
        // this STA is in a band not supported by any AP

        // I must disable it
        STAinNonSupportedBand = true;

        if (verboseLevel >= 1)
          std::cout << "STA\t#" << staNodes.Get(i)->GetId() 
                    << "\tBand " << bandThisSTA
                    << " not supported by any AP (" << bandsSupportedByAPs
                    << "). Put in channel " << uint16_t(ChannelNoForThisSTA)
                    << ". It will be disabled"
                    << '\n';            
      }
    }

    // define the standard to follow (see https://www.nsnam.org/doxygen/group__wifi.html#ga1299834f4e1c615af3ca738033b76a49)
    // Yans wifi
    if (wifiModel == 0) {
      wifiPhy.Set ("ChannelNumber", UintegerValue(ChannelNoForThisSTA));
      //AP_vector[i]->setWirelessChannel(ChannelNoForThisSTAPrimary);

      // primary interface
      wifi.SetStandard (convertVersionToStandard(version80211ThisSTA));

      staDev = wifi.Install (wifiPhy, wifiMacPrimary, staNodes.Get(i));
    }
    else {
      // spectrumwifi
      spectrumPhy.Set ("ChannelNumber", UintegerValue(ChannelNoForThisSTA));
      //AP_vector[i]->setWirelessChannel(ChannelNoForThisSTAPrimary);

      // primary interface
      wifi.SetStandard (convertVersionToStandard(version80211ThisSTA));

      staDev = wifi.Install (spectrumPhy, wifiMacPrimary, staNodes.Get(i));
    }

    if (verboseLevel >= 2) {
      std::cout << "\t\t\tInstalled WiFi card (spectrumPhy)"
                << ", band " << getWirelessBandOfChannel(ChannelNoForThisSTA)
                << ", channel of the AP: " << uint16_t(ChannelNoForThisSTA)
                << '\n';                
    }



    // if the STA is NOT active, it has to be disabled
    if ((STAsActive.at(i) == '0') || (STAinNonSupportedBand == true)) {

      if (verboseLevel >= 1) {
        std::cout << "\t\t\tSTA#" << i + number_of_APs
                  << " has to be disabled";
        if (STAsActive.at(i) == '0') {
          std::cout << " because the user has specified it as permanently disabled"
                    << std::endl;         
        }
        else {
          std::cout << " because it is in a band with no APs"
                    << std::endl;  
        }
      }

      // disable the network device            
      DisableNetworkDevice (staDev, wifiModel, 0);

      if (verboseLevel >= 1)
        std::cout << "\t\t\tSTA#" << i + number_of_APs
                  << " has been disabled"
                  << std::endl;
    }

    /* STA definition - add the device to the vector of devices */
    staDevices.push_back (staDev);


    if (false) { // FIXME: this does NOT Work properly
      
      // just to confirm that the channel of the STA is the same channel of the nearest AP
      uint8_t assignedChannel = GetChannelOfaSTA ( i + number_of_APs, wifiModel, verboseLevel);

      if (verboseLevel >= 2) {
        std::cout << "\t\t\tInstalled WiFi card (spectrumPhy)"
                  << ", in channel " << uint16_t(assignedChannel)
                  << ", band " << getWirelessBandOfChannel(assignedChannel)
                  << ", channel of the AP: " << uint16_t(ChannelNoForThisSTA)
                  << '\n';                
      }

      // make sure the channel of the STA is the same one of the closest AP
      NS_ASSERT(ChannelNoForThisSTA == assignedChannel);      
    }



    /* STA definition - IP addresses */
    // add an IP address (Segment A, i.e. 10.0.0.0) to the interface of the STA
    staInterface = ipAddressesSegmentA.Assign (staDev);
    staInterfaces.push_back (staInterface);


    /* STA definition - debug information */
    if (verboseLevel > 0) {
      Ptr<Node> node;
      Ptr<Ipv4> ipv4;
      Ipv4Address addr;
      node = staNodes.Get (i); // Get pointer to ith node in container
      ipv4 = node->GetObject<Ipv4> (); // Get Ipv4 instance of the node
      //addr = ipv4->GetAddress (0, 0).GetLocal (); // This returns 127.0.0.1

      // first IP address or the STA
      addr = ipv4->GetAddress (1, 0).GetLocal ();
      // I print the node identifier
      std::cout << "\t\t" /*<< node->GetId()*/ << "\tIP address: " << addr << '\n';
      std::cout << "\t\t" << "\tMAC address: " << staDevices[i].Get(0)->GetAddress() << '\n';
      std::cout << "\t\t" << "\tWi-Fi channel: " << uint32_t(ChannelNoForThisSTA) << '\n';

      // I print the position 
      Vector pos = GetPosition (node);
      std::cout << "\t\t" << "\tPosition: " << pos.x << "," << pos.y << '\n';
      std::cout << "\t\t" << "\tInitially near AP#" << myNearestApId << '\n';
    }


    /* STA definition - schedule periodic channel report */
    // Periodically report the channels of all the active STAs
    if ((STAsActive.at(i) == '1') && (STAinNonSupportedBand == false)) {
      if ((verboseLevel > 2) && (timeMonitorKPIs > 0)) {
        Simulator::Schedule ( Seconds (INITIALTIMEINTERVAL),
                              &ReportChannel, 
                              timeMonitorKPIs,
                              ( number_of_APs * numberAPsSamePlace ) + i,
                              verboseLevel);
      }
    }



    // EXPERIMENTAL: on each STA, add support for other operational channels
    // FIXME: I remove AddOperationalChannel Dec 2019
    if (false) {
      if ( (HANDOFFMETHOD == 1) && (wifiModel == 1) ) {
        // This is only needed if more than 1 channel is in use, and if wifiModel == 1
        if (numOperationalChannelsPrimary > 1 && wifiModel == 1) {
          Ptr<SpectrumWifiPhy> wifiPhyPtrClient;
          //Ptr<StaWifiMac> myStaWifiMac;

          wifiPhyPtrClient = staDevices[i].Get(0)->GetObject<WifiNetDevice>()->GetPhy()->GetObject<SpectrumWifiPhy>();

          if (verboseLevel > 0)
            std::cout << "STA\t#" << staNodes.Get(i)->GetId()
                      << "\tAdded operational channels: ";

          for (uint32_t k = 0; k < numOperationalChannelsPrimary; k++) {
            //(*wifiPhyPtrClient).AddOperationalChannel ( availableChannels[k] );
            if (verboseLevel > 0)
              std::cout << uint16_t(availableChannels[k]) << " "; 
          }
          if (verboseLevel > 0)
            std::cout << '\n';

          //myStaWifiMac = staDevices[i].Get(0)->GetObject<WifiNetDevice>()->GetMacOfitsAP()->GetObject<StaWifiMac>();
          //(*myStaWifiMac).TryToEnsureAssociated();
          //if ( (*myStaWifiMac).IsAssociated() )
          //  std::cout << "######################" << '\n';
        }
      }      
    }



    // EXPERIMENTAL: on each STA, clear all the operational channels https://www.nsnam.org/doxygen/classns3_1_1_spectrum_wifi_phy.html#acb3f2f3a32236fa5fdeed1986b28fe04
    // it doesn't work

    // FIXME: I remove ClearOperationalChannelList Dec 2019
    if (false) {
      if (numOperationalChannelsPrimary > 1 && wifiModel == 1) {
        Ptr<SpectrumWifiPhy> wifiPhyPtrClient;

        wifiPhyPtrClient = staDevices[i].Get(0)->GetObject<WifiNetDevice>()->GetPhy()->GetObject<SpectrumWifiPhy>();

        //(*wifiPhyPtrClient).ClearOperationalChannelList ();

        if (verboseLevel > 0)
          std::cout << "STA\t#" << staNodes.Get(i)->GetId()
                    << "\tCleared operational channels in STA #" << ( number_of_APs * numberAPsSamePlace ) + i
                    << '\n';
      }
    }

    /*** create a STA_record per STA ***/
    // the STA has been defined, so now I create a STA_record for it,
    //in order to store its association parameters

    // This calls the constructor, i.e. the function that creates a record to store the association of each STA
    STA_record *m_STArecord = new STA_record();

    // Set the value of the id of the STA in the record
    //m_STArecord->setstaid ((*mynode)->GetId());
    m_STArecord->setstaid ((staNodes.Get(i))->GetId());

    /* Establish the type of application */

    // primary STAs
    if ( i < numberVoIPupload ) {
      m_STArecord->Settypeofapplication (1);  // VoIP upload
      m_STArecord->SetMaxSizeAmpdu (0);       // No aggregation
    } else if (i < numberVoIPupload + numberVoIPdownload ) {
      m_STArecord->Settypeofapplication (2);  // VoIP download
      m_STArecord->SetMaxSizeAmpdu (0);       // No aggregation
    } else if (i < numberVoIPupload + numberVoIPdownload + numberTCPupload) {
      m_STArecord->Settypeofapplication (3);               // TCP upload
      m_STArecord->SetMaxSizeAmpdu (maxAmpduSize);         // aggregation enabled
    } else if (i < numberVoIPupload + numberVoIPdownload + numberTCPupload + numberTCPdownload) {
      m_STArecord->Settypeofapplication (4);                // TCP download
      m_STArecord->SetMaxSizeAmpdu (maxAmpduSize);         // aggregation enabled
    } else if (i < numberVoIPupload + numberVoIPdownload + numberTCPupload + numberTCPdownload + numberVideoDownload) {
      m_STArecord->Settypeofapplication (5);                // Video download
      m_STArecord->SetMaxSizeAmpdu (maxAmpduSize);         // aggregation enabled      
    } else {
      // other applications can be added in the future
    }

    // secondary STAs
    if (numberSTAsSamePlace == 2) {
      if ( i < number_of_STAs + numberVoIPupload ) {
        m_STArecord->Settypeofapplication (1);  // VoIP upload
        m_STArecord->SetMaxSizeAmpdu (0);       // No aggregation
      } else if (i < number_of_STAs + numberVoIPupload + numberVoIPdownload ) {
        m_STArecord->Settypeofapplication (2);  // VoIP download
        m_STArecord->SetMaxSizeAmpdu (0);       // No aggregation
      } else if (i < number_of_STAs + numberVoIPupload + numberVoIPdownload + numberTCPupload) {
        m_STArecord->Settypeofapplication (3);               // TCP upload
        m_STArecord->SetMaxSizeAmpdu (maxAmpduSize);         // aggregation enabled
      } else if (i < number_of_STAs + numberVoIPupload + numberVoIPdownload + numberTCPupload + numberTCPdownload) {
        m_STArecord->Settypeofapplication (4);                // TCP download
        m_STArecord->SetMaxSizeAmpdu (maxAmpduSize);         // aggregation enabled
      } else if (i < number_of_STAs + numberVoIPupload + numberVoIPdownload + numberTCPupload + numberTCPdownload + numberVideoDownload) {
        m_STArecord->Settypeofapplication (5);                // Video download
        m_STArecord->SetMaxSizeAmpdu (maxAmpduSize);         // aggregation enabled      
      } else {
        // other applications can be added in the future
      }
    }
    /* end of - Establish the type of application */    

    // Establish the verbose level in the STA record
    m_STArecord->SetVerboseLevel (verboseLevel);

    // set the value of disabled / not disabled
    if (STAsActive.at(i)=='1')
      m_STArecord->SetDisabledPermanently (false);
    else
      m_STArecord->SetDisabledPermanently (true);

    // set the value of 'onlyOnePeerSTAallowedAtATime'
    if (onlyOnePeerSTAallowedAtATime)
      m_STArecord->SetDisablePeerSTAWhenAssociated(true);
    else
      m_STArecord->SetDisablePeerSTAWhenAssociated(false);

    // store the ID of the asociated STA (in other band)
    if ( i < number_of_STAs ) {
      // this is a primary STA
      
      m_STArecord->SetnumOperationalChannels (numOperationalChannelsPrimary);

      m_STArecord->SetPrimarySTA(true);

      if (numberSTAsSamePlace == 2) {
        // add the ID of the associated STA in the other band
        m_STArecord->SetpeerStaid((staNodes.Get(i))->GetId() + number_of_STAs );        
      }
      else {
        // STAs do not have an associated secondary STA
        m_STArecord->SetpeerStaid(0);         
      }
      // set the 802.11 version
      m_STArecord->Setversion80211 (version80211primary);
    }

    else {
      // this is a secondary STA
      NS_ASSERT(numberSTAsSamePlace == 2);

      m_STArecord->SetnumOperationalChannels (numOperationalChannelsSecondary);

      m_STArecord->SetPrimarySTA(false);

      m_STArecord->SetpeerStaid((staNodes.Get(i))->GetId() - number_of_STAs );        

      // set the 802.11 version
      m_STArecord->Setversion80211 (version80211secondary);
    }

    m_STArecord->SetaggregationDisableAlgorithm (aggregationDisableAlgorithm);
    m_STArecord->SetAmpduSize (maxAmpduSize);
    m_STArecord->SetmaxAmpduSizeWhenAggregationLimited (maxAmpduSizeWhenAggregationLimited);
    m_STArecord->SetWifiModel (wifiModel);

    // Set a callback function to be called each time a STA gets associated to an AP
    std::ostringstream STA;

    STA << (staNodes.Get(i))->GetId();
    std::string strSTA = STA.str();

    // This makes a callback every time a STA gets associated to an AP
    // see trace sources in https://www.nsnam.org/doxygen/classns3_1_1_sta_wifi_mac.html#details
    // trace association. Taken from https://github.com/MOSAIC-UA/802.11ah-ns3/blob/master/ns-3/scratch/s1g-mac-test.cc
    // some info here: https://groups.google.com/forum/#!msg/ns-3-users/zqdnCxzYGM8/MdCshgYKAgAJ
    Config::Connect ( "/NodeList/"+strSTA+"/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/$ns3::StaWifiMac/Assoc", 
                      MakeCallback (&STA_record::SetAssoc, m_STArecord));

    // Set a callback function to be called each time a STA gets de-associated from an AP
    Config::Connect ( "/NodeList/"+strSTA+"/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/$ns3::StaWifiMac/DeAssoc", 
                      MakeCallback (&STA_record::UnsetAssoc, m_STArecord));

    // This makes a callback every time a STA changes its course
    // only do it for primary STAs, to avoid repetitions
    if ( i < number_of_STAs )
      Config::Connect ( "/NodeList/"+strSTA+"/$ns3::MobilityModel/CourseChange",
                        MakeCallback (&STA_record::StaCourseChange, m_STArecord));

    // Add the new record to the vector of STA associations
    sta_vector.push_back (m_STArecord);

    // once filled, print all the member variables
    if (VERBOSE_FOR_DEBUG >= 1)
      m_STArecord->PrintAllVariables ();
    /** end of - create a STA_record per STA **/
  }

  /*************************** end of - Define the STAs ******************************/


  /******************* Set channel width of all the devices **************************/
  // This is an example of what you have to do
  //Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (channelWidthPrimary));

  // I use an auxiliar string for creating the first argument of Config::Set
  std::ostringstream auxString;

  // APs
  // the ID of the first AP is '0'
  for ( uint32_t j = 0; j < numberAPsSamePlace; ++j ) {
    for ( uint32_t i = 0; i < number_of_APs; ++i ) {

    auxString << "/NodeList/" << i + (j * number_of_APs) << "/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth";

    if ( j == 0) {
      Config::Set(auxString.str(), UintegerValue(channelWidthPrimary));
      if (verboseLevel > 0)
        std::cout << "AP\t#" << i + (j * number_of_APs)
              << "\tChannel width: " << channelWidthPrimary
              << "MHz\n";
    }
    else {
      Config::Set(auxString.str(), UintegerValue(channelWidthSecondary));
      if (verboseLevel > 0)
        std::cout << "AP\t#" << i + (j * number_of_APs)
              << "\tChannel width: " << channelWidthSecondary
              << "MHz\n";
    }

    // clean the string
    auxString.str(std::string());
    }
  }

  // STAs
  // the ID of the first STA is 'number_of_APs * numberAPsSamePlace'
  for ( uint32_t j = 0; j < numberSTAsSamePlace; ++j ) {
    for ( uint32_t i = number_of_APs * numberAPsSamePlace; i < (number_of_APs * numberAPsSamePlace) + number_of_STAs; ++i ) {

    auxString << "/NodeList/" << i + (j * number_of_STAs) << "/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth";

    if ( j == 0) {
      Config::Set(auxString.str(), UintegerValue(channelWidthPrimary));
      if (verboseLevel > 0)
        std::cout << "STA\t#" << i + (j * number_of_STAs)
              << "\tChannel width: " << channelWidthPrimary
              << "MHz\n";
    }
    else {
      Config::Set(auxString.str(), UintegerValue(channelWidthSecondary));
      if (verboseLevel > 0)
        std::cout << "STA\t#" << i + (j * number_of_STAs)
              << "\tChannel width: " << channelWidthSecondary
              << "MHz\n";
    }

    // clean the string
    auxString.str(std::string());
    }
  }
  /******************* end of - Set channel width of all the devices **************************/


  /************************** define wired connections *******************************/
  // create the ethernet channel for connecting the APs and the router
  CsmaHelper csma;
  //csma.SetChannelAttribute ("DataRate", StringValue ("10000Mbps")); // to avoid this being the bottleneck
  csma.SetChannelAttribute ("DataRate", DataRateValue (100000000000)); // 100 gbps
  // set the speed-of-light delay of the channel to 6560 nano-seconds (arbitrarily chosen as 1 nanosecond per foot over a 100 meter segment)
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
  //csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (65600)));

  // https://www.nsnam.org/doxygen/classns3_1_1_bridge_helper.html#aba53f6381b7adda00d9163840b072fa6
  // This method creates an ns3::BridgeNetDevice with the attributes configured by BridgeHelper::SetDeviceAttribute, 
  // adds the device to the node (first argument), and attaches the given NetDevices (second argument) as ports of the bridge.
  // In this case, it adds it to the backbonenodes and attaches the backboneDevices of apWiFiDev
  // Returns a container holding the added net device.

  // taken from https://www.nsnam.org/doxygen/csma-bridge-one-hop_8cc_source.html
  //   +----------------+
  //   |   csmaHubNode  |                         The node named csmaHubNode
  //   +----------------+                         has a number CSMA net devices that are bridged
  //   CSMA   CSMA   CSMA   csmaHubDevices        together using a BridgeNetDevice (created with a bridgehelper) called 'bridgehub'.
  //    1|      |      |
  //     |      |      |                          The bridge node talks over three CSMA channels
  //    0|      |      |                          to three other CSMA net devices
  //   CSMA   CSMA   CSMA
  //   +--+   +--+  +------+
  //   |AP|   |AP|  |router|  (the router only appears in topology = 2)
  //   +--+   +--+  +------+
  //   wifi   wifi    p2p
  //                   |
  //                   |
  //                   |
  //                  p2p
  //                   
  //                servers   


  // install a csma channel between the ith AP node and the bridge (csmaHubNode) node
  for ( uint32_t i = 0; i < number_of_APs * numberAPsSamePlace; i++) {
    if (APsActive.at(i) == '1') {
      NetDeviceContainer link = csma.Install (NodeContainer (apNodes.Get(i), csmaHubNode));
      apCsmaDevices.Add (link.Get(0));
      csmaHubDevices.Add (link.Get(1));      
    }
  }

  if (topology == 0) {
    // install a csma channel between the singleServer and the bridge (csmaHubNode) node
    NetDeviceContainer link = csma.Install (NodeContainer (singleServerNode.Get(0), csmaHubNode));
    singleServerDevices.Add (link.Get(0));
    csmaHubDevices.Add (link.Get(1));

    // Assign an IP address (10.0.0.0) to the single server
    singleServerInterfaces = ipAddressesSegmentA.Assign (singleServerDevices);

  }
  else if (topology == 1) {
    // install a csma channel between the ith server node and the bridge (csmaHubNode) node
    for ( uint32_t i = 0; i < number_of_Servers; i++) {
      NetDeviceContainer link = csma.Install (NodeContainer (serverNodes.Get(i), csmaHubNode));
      serverDevices.Add (link.Get(0));
      csmaHubDevices.Add (link.Get(1));
    }  
      
    // Assign IP addresses (10.0.0.0) to the servers
    serverInterfaces = ipAddressesSegmentA.Assign (serverDevices);

  }
  else { // if (topology == 2)
    // install a csma channel between the router and the bridge (csmaHubNode) node
    NetDeviceContainer link = csma.Install (NodeContainer (routerNode.Get(0), csmaHubNode));
    routerDeviceToAps.Add (link.Get(0));
    csmaHubDevices.Add (link.Get(1));

    // Assign an IP address (10.0.0.0) to the router (AP part)
    routerInterfaceToAps = ipAddressesSegmentA.Assign (routerDeviceToAps);
  }
  /************************** end of - define wired connections *******************************/



  // on each AP, I install a bridge between two devices: the WiFi device and the csma device
  BridgeHelper bridgeAps, bridgeHub;
  NetDeviceContainer apBridges;

  if (VERBOSE_FOR_DEBUG >=1) {
    std::cout << "number of 'apNodes': " << apNodes.GetN() << "\n";
    std::cout << "number of 'apCsmaDevices': " << apCsmaDevices.GetN() << "\n";
  }

  uint32_t APindex = 0; // this index is different from 'i'. It only increases if the AP is active (APsActive.at(i) == '1')
  for (uint32_t i = 0; i < number_of_APs * numberAPsSamePlace; i++) {
    if (APsActive.at(i) == '1') {
      if (VERBOSE_FOR_DEBUG >=1)
        std::cout << "AP#" << i << "\n";

      // Create a bridge between two devices (WiFi and CSMA) of the same node: the AP

      // initial version
      //bridgeAps.Install (apNodes.Get (i), NetDeviceContainer ( apWiFiDevices[i].Get(0), apCsmaDevices.Get (i) ) );

      // improved version
      NetDeviceContainer bridge = bridgeAps.Install (apNodes.Get (i), NetDeviceContainer ( apWiFiDevices[APindex].Get(0), apCsmaDevices.Get (APindex) ) );
      apBridges.Add (bridge.Get(0));

      // If needed, I could assign an IP address to the bridge (not to the wifi or the csma devices)
      //apInterface = ipAddressesSegmentA.Assign (bridgeApDevices);

      // only increase the index if the AP exists
      APindex ++;
    }
  }


  //Create the bridge netdevice, which will do the packet switching.  The
  // bridge lives on the node csmaHubNode.Get(0) and bridges together the csmaHubDevices and the routerDeviceToAps
  // which are the CSMA net devices 
  bridgeHub.Install (csmaHubNode.Get(0), csmaHubDevices );


  // create a point to point helper for connecting the servers with the router (if topology == 2)
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10000Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("1ms"));


  //if (populatearpcache)
    // Assign IP address (10.0.0.0) to the bridge
    //csmaHubInterfaces = ipAddressesSegmentA.Assign (bridgeApDevices);

  if (topology == 2) {

    // connect each server to the router with a point to point connection
    for ( uint32_t i = 0; i < number_of_Servers; i++) {
      // Create a point to point link between the router and the server
      NetDeviceContainer devices;
      devices = p2p.Install ( NodeContainer (routerNode.Get (0), serverNodes.Get (i))); // this returns two devices. Add them to the router and the server
      routerDeviceToServers.Add (devices.Get(0));
      serverDevices.Add (devices.Get(1));
    }

    // Assign IP addresses (10.1.0.0) to the servers
    serverInterfaces = ipAddressesSegmentB.Assign (serverDevices);

    // Assign an IP address (10.1.0.0) to the router
    routerInterfaceToServers = ipAddressesSegmentB.Assign (routerDeviceToServers);
  }


  if (verboseLevel > 0) {
    Ptr<Node> node;
    Ptr<Ipv4> ipv4;
    Ipv4Address addr;

    if (topology == 0) { 
      // print the IP and the MAC addresses of the server
      node = singleServerNode.Get (0); // Get pointer to ith node in container
      ipv4 = node->GetObject<Ipv4> (); // Get Ipv4 instance of the node
      //addr = ipv4->GetAddress (0, 0).GetLocal (); // This returns 127.0.0.1
      addr = ipv4->GetAddress (1, 0).GetLocal ();
      // I print the node identifier
      std::cout << "Server\t" << "\tIP address: " << addr << '\n';
      std::cout << "      \t" << "\tMAC address: " << singleServerDevices.Get(0)->GetAddress() << '\n';
      
    } else if (topology == 1) {
      // print the IP and the MAC addresses of each server
      for ( uint32_t i = 0; i < number_of_Servers; i++) {
        node = serverNodes.Get (i); // Get pointer to ith node in container
        ipv4 = node->GetObject<Ipv4> (); // Get Ipv4 instance of the node      
        //addr = ipv4->GetAddress (0, 0).GetLocal (); // This returns 127.0.0.1
        addr = ipv4->GetAddress (1, 0).GetLocal (); 
        std::cout << "Server #" << node->GetId() << "\tIP address: " << addr << '\n';
        std::cout << "        " << "\tMAC address: " << serverDevices.Get(i)->GetAddress() << '\n';  
      }

    } else { //(topology == 2)
      // print the IP and the MAC addresses of the router
      node = routerNode.Get (0); // Get pointer to ith node in container
      ipv4 = node->GetObject<Ipv4> (); // Get Ipv4 instance of the node
      //addr = ipv4->GetAddress (0, 0).GetLocal (); // This returns 127.0.0.1
      addr = ipv4->GetAddress (1, 0).GetLocal ();
      // I print the node identifier
      std::cout << "Router(AP side)" << "\tIP address: " << addr << '\n';
      std::cout << "               " << "\tMAC address: " << routerDeviceToAps.Get(0)->GetAddress() << '\n';

      // print the IP and the MAC addresses of the router and its corresponding server
      for ( uint32_t i = 0; i < number_of_Servers; i++) {
        // print the IP and the MAC of the p2p devices of the router
        node = routerNode.Get(0); // Get pointer to the router
        ipv4 = node->GetObject<Ipv4> (); // Get Ipv4 instance of the router      
        //addr = ipv4->GetAddress (0, 0).GetLocal (); // This returns 127.0.0.1
        addr = ipv4->GetAddress ( i + 2, 0).GetLocal (); // address 0 is local, and address 1 is for the AP side 
        std::cout << "Router IF #" << i + 2 << "\tIP address: " << addr << '\n';
        std::cout << "        " << "\tMAC address: " << routerDeviceToServers.Get(i)->GetAddress() << '\n';
      }

      // print the IP and the MAC addresses of the router and its corresponding server
      for ( uint32_t i = 0; i < number_of_Servers; i++) {
        // addresses of the server
        node = serverNodes.Get (i); // Get pointer to ith node in container
        ipv4 = node->GetObject<Ipv4> (); // Get Ipv4 instance of the node      
        //addr = ipv4->GetAddress (0, 0).GetLocal (); // This returns 127.0.0.1
        addr = ipv4->GetAddress (1, 0).GetLocal (); 
        std::cout << "Server #" << node->GetId() << "\tIP address: " << addr << '\n';
        std::cout << "        " << "\tMAC address: " << serverDevices.Get(i)->GetAddress() << '\n'; 
      }
    } 
  }


  // print a blank line after printing IP addresses
  if (verboseLevel > 0)
    std::cout << "\n";


  // Fill the routing tables. It is necessary in topology 2, which includes two different networks
  if(topology == 2) {
    //NS_LOG_INFO ("Enabling global routing on all nodes");
    //Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    // I obtain this error: aborted. msg="ERROR: L2 forwarding loop detected!", file=../src/internet/model/global-router-interface.cc, line=1523
    // Global routing helper does not work in this case
    // As a STA can connect through many APs, loops appear
    // Therefore, I have to add the routes manually
    // https://www.nsnam.org/doxygen/classns3_1_1_ipv4_static_routing.html

    // get the IPv4 address of the router interface to the APs
    Ptr<Node> node;
    Ptr<Ipv4> ipv4;
    Ipv4Address addrRouterAPs, addrRouterSrv;
    node = routerNode.Get (0); // Get pointer to ith node in container
    ipv4 = node->GetObject<Ipv4> (); // Get Ipv4 instance of the node
    addrRouterAPs = ipv4->GetAddress (1, 0).GetLocal ();

    // On each STA, add a route to the network of the servers
    for (uint32_t i = 0; i < number_of_STAs * numberSTAsSamePlace; i++) {
      // get the ipv4 instance of the STA
      Ptr<Node> node;
      Ptr<Ipv4> ipv4;
      Ipv4Address addr;
      node = staNodes.Get (i); // Get pointer to ith node in container
      ipv4 = node->GetObject<Ipv4> (); // Get Ipv4 instance of the node
      Ipv4StaticRoutingHelper ipv4RoutingHelper;
      Ptr<Ipv4StaticRouting> staticRouting = ipv4RoutingHelper.GetStaticRouting (ipv4);

      // Add a route to the network of the servers
      staticRouting->AddNetworkRouteTo (Ipv4Address ("10.1.0.0"), Ipv4Mask ("255.255.0.0"), addrRouterAPs, 1);

      if (verboseLevel > 0) {
        std::cout << "Routing in STA #" << staNodes.Get(i)->GetId() << " with IP address "<< ipv4->GetAddress (1, 0).GetLocal();
        std::cout << ". added route to network 10.1.0.0/255.255.0.0 through gateway " << addrRouterAPs << '\n';
      }
    }

    // On each server, add a route to the network of the Stas
    for (uint32_t i = 0; i < number_of_Servers; i++) {
      // get the ipv4 instance of the server
      Ptr<Node> node;
      Ptr<Ipv4> ipv4;
      Ipv4Address addr;
      node = serverNodes.Get (i); // Get pointer to ith node in container
      ipv4 = node->GetObject<Ipv4> (); // Get Ipv4 instance of the node
      Ipv4StaticRoutingHelper ipv4RoutingHelper;
      Ptr<Ipv4StaticRouting> staticRouting = ipv4RoutingHelper.GetStaticRouting (ipv4);

      // get the ipv4 instance of the router
      Ptr<Ipv4> ipv4Router;
      ipv4Router = routerNode.Get (0)->GetObject<Ipv4> (); // Get Ipv4 instance of the router

      // Add a route to the network of the Stas
      staticRouting->AddNetworkRouteTo (Ipv4Address ("10.0.0.0"), Ipv4Mask ("255.255.0.0"), ipv4Router->GetAddress ( i + 2, 0).GetLocal(), 1);

      if (verboseLevel > 0) {
        std::cout << "Routing in server #" << serverNodes.Get(i)->GetId() << " with IP address "<< ipv4->GetAddress (1, 0).GetLocal() << ": ";
        std::cout << "\tadded route to network 10.0.0.0/255.255.0.0 through gateway " << ipv4Router->GetAddress ( i + 2, 0).GetLocal() << '\n';        
      }
    }

    // On the router, add a route to each server
    for (uint32_t i = 0; i < number_of_Servers; i++) {
      // get the ipv4 instance of the server
      Ptr<Node> node;
      Ptr<Ipv4> ipv4;
      Ipv4Address addrServer;
      node = serverNodes.Get (i); // Get pointer to ith node in container
      ipv4 = node->GetObject<Ipv4> (); // Get Ipv4 instance of the node
      addrServer = ipv4->GetAddress (1, 0).GetLocal ();

      // get the ipv4 instance of the router
      Ptr<Ipv4> ipv4Router;
      ipv4Router = routerNode.Get (0)->GetObject<Ipv4> (); // Get Ipv4 instance of the router
      Ipv4StaticRoutingHelper ipv4RoutingHelper;
      Ptr<Ipv4StaticRouting> staticRouting = ipv4RoutingHelper.GetStaticRouting (ipv4Router);

      // Tell the interface to be used for going to this server
      staticRouting->AddHostRouteTo (addrServer, i + 2, 1); // use interface 'i + 2': interface #0 is localhost, and interface #1 is for talking with the APs

      if (verboseLevel > 0) {
        std::cout << "Routing in the router (id #" << routerNode.Get(0)->GetId() << "): " ;
        std::cout << "\tadded route to host " << addrServer << " through interface with IP address " << ipv4Router->GetAddress ( i + 2, 0).GetLocal() << '\n';
      }      
    }

    // print a blank line after printing routing information
    if (verboseLevel > 0)
      std::cout << "\n";
  }


  // usage examples of functions controlling ARPs
  if (false) {
    infoArpCache ( staNodes.Get(1), 3);
    emtpyArpCache ( staNodes.Get(1), 2);
    modifyArpParams ( staNodes.Get(1), 100.0, 2);
  }


  /************* Setting applications ***********/

  // Variable for setting the port of each communication
  uint16_t port;


  /*** VoIP upload applications ***/
  // UDPClient runs in the STA and UDPServer runs in the server(s)
  // traffic goes STA -> server
  port = INITIALPORT_VOIP_UPLOAD;
  UdpServerHelper myVoipUpServer;
  ApplicationContainer VoipUpServer;

  if (eachSTArunsAllTheApps == false) {
    for (uint32_t j = 0 ; j < numberSTAsSamePlace ; j++ ) {
      for (uint32_t i = 0 ; i < numberVoIPupload ; i++ ) {
        myVoipUpServer = UdpServerHelper(port); // Each UDP connection requires a different port

        if (topology == 0) {
          VoipUpServer = myVoipUpServer.Install (singleServerNode.Get(0));
        } else {
          VoipUpServer = myVoipUpServer.Install (serverNodes.Get(i));
        }

        VoipUpServer.Start (Seconds (0.0));
        VoipUpServer.Stop (Seconds (simulationTime + INITIALTIMEINTERVAL));

        // UdpClient runs in the STA, so I must create a UdpClient per STA
        UdpClientHelper myVoipUpClient;
        ApplicationContainer VoipUpClient;

        // I associate all the servers (all running in the STAs) to each server application

        Ipv4Address myaddress;

        if (topology == 0) {
          myaddress = singleServerInterfaces.GetAddress (0);
        } else {
          myaddress = serverInterfaces.GetAddress (i);
        }

        InetSocketAddress destAddress (myaddress, port);

        // If priorities ARE NOT enabled, VoIP traffic will have TcpPriorityLevel
        if (prioritiesEnabled == 0) {
          destAddress.SetTos (TcpPriorityLevel);
        // If priorities ARE enabled, VoIP traffic will have a higher priority (VoIpPriorityLevel)
        } else {
          destAddress.SetTos (VoIpPriorityLevel);
        }

        myVoipUpClient = UdpClientHelper(destAddress);

        myVoipUpClient.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
        //myVoipUpClient.SetAttribute ("Interval", TimeValue (Time ("0.02")));
        myVoipUpClient.SetAttribute ("Interval", TimeValue (Seconds (VoIPg729IPT))); //packets/s
        myVoipUpClient.SetAttribute ("PacketSize", UintegerValue ( VoIPg729PayoladSize ));

        VoipUpClient = myVoipUpClient.Install (staNodes.Get(i + (j * number_of_STAs)));
        VoipUpClient.Start (Seconds (INITIALTIMEINTERVAL));
        VoipUpClient.Stop (Seconds (simulationTime + INITIALTIMEINTERVAL));
        if (verboseLevel > 0) {
          if (topology == 0) {
            std::cout << "Application VoIP upload   from STA    #" << staNodes.Get(i + (j * number_of_STAs))->GetId()
                      << "\t with IP address " << staInterfaces[i].GetAddress(0)
                      << "\t-> to the server"
                      << "\t with IP address " << singleServerInterfaces.GetAddress (0) 
                      << "\t and port " << port
                      << '\n';  
          }
          else {
            std::cout << "Application VoIP upload   from STA    #" << staNodes.Get(i + (j * number_of_STAs))->GetId()
                      << "\t with IP address " << staInterfaces[i + (j * number_of_STAs)].GetAddress(0)
                      << "\t-> to server #" << serverNodes.Get(i)->GetId()
                      << "\t with IP address " << serverInterfaces.GetAddress (i) 
                      << "\t and port " << port
                      << '\n';
          }
        }
        port ++; // Each UDP connection requires a different port
      }
    }
  }
  else {
    if (numberVoIPupload != 0) {
      for (uint32_t j = 0 ; j < numberSTAsSamePlace ; j++ ) {
        for (uint32_t i = 0 ; i < number_of_STAs ; i++ ) {
          myVoipUpServer = UdpServerHelper(port); // Each UDP connection requires a different port

          if (topology == 0) {
            VoipUpServer = myVoipUpServer.Install (singleServerNode.Get(0));
          } else {
            VoipUpServer = myVoipUpServer.Install (serverNodes.Get(i));
          }

          VoipUpServer.Start (Seconds (0.0));
          VoipUpServer.Stop (Seconds (simulationTime + INITIALTIMEINTERVAL));

          // UdpClient runs in the STA, so I must create a UdpClient per STA
          UdpClientHelper myVoipUpClient;
          ApplicationContainer VoipUpClient;

          // I associate all the servers (all running in the STAs) to each server application

          Ipv4Address myaddress;

          if (topology == 0) {
            myaddress = singleServerInterfaces.GetAddress (0);
          } else {
            myaddress = serverInterfaces.GetAddress (i);
          }

          InetSocketAddress destAddress (myaddress, port);

          // If priorities ARE NOT enabled, VoIP traffic will have TcpPriorityLevel
          if (prioritiesEnabled == 0) {
            destAddress.SetTos (TcpPriorityLevel);
          // If priorities ARE enabled, VoIP traffic will have a higher priority (VoIpPriorityLevel)
          } else {
            destAddress.SetTos (VoIpPriorityLevel);
          }

          myVoipUpClient = UdpClientHelper(destAddress);

          myVoipUpClient.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
          //myVoipUpClient.SetAttribute ("Interval", TimeValue (Time ("0.02")));
          myVoipUpClient.SetAttribute ("Interval", TimeValue (Seconds (VoIPg729IPT))); //packets/s
          myVoipUpClient.SetAttribute ("PacketSize", UintegerValue ( VoIPg729PayoladSize ));

          VoipUpClient = myVoipUpClient.Install (staNodes.Get(i + (j * number_of_STAs)));
          VoipUpClient.Start (Seconds (INITIALTIMEINTERVAL));
          VoipUpClient.Stop (Seconds (simulationTime + INITIALTIMEINTERVAL));
          if (verboseLevel > 0) {
            if (topology == 0) {
              std::cout << "Application VoIP upload   from STA    #" << staNodes.Get(i + (j * number_of_STAs))->GetId()
                        << "\t with IP address " << staInterfaces[i + (j * number_of_STAs)].GetAddress(0)
                        << "\t-> to the server"
                        << "\t with IP address " << singleServerInterfaces.GetAddress (0) 
                        << "\t and port " << port
                        << '\n';  
            }
            else {
              std::cout << "Application VoIP upload   from STA    #" << staNodes.Get(i + (j * number_of_STAs))->GetId()
                        << "\t with IP address " << staInterfaces[i + (j * number_of_STAs)].GetAddress(0)
                        << "\t-> to server #" << serverNodes.Get(i)->GetId()
                        << "\t with IP address " << serverInterfaces.GetAddress (i) 
                        << "\t and port " << port
                        << '\n';
            }
          }
          port ++; // Each UDP connection requires a different port
        }  
      }
    }
  }
  /*** end of - VoIP upload applications ***/


  /*** VoIP download applications ***/
  // UDPClient in the server(s) and UDPServer in the STA
  // traffic goes Server -> STA
  // I have taken this as an example: https://groups.google.com/forum/#!topic/ns-3-users/ej8LaxQO1Gc
  // UdpServer runs in each STA. It waits for input UDP packets and uses the
  // information carried into their payload to compute delay and to determine
  // if some packets are lost. https://www.nsnam.org/doxygen/classns3_1_1_udp_server_helper.html#details
  port = INITIALPORT_VOIP_DOWNLOAD;
  UdpServerHelper myVoipDownServer;
  ApplicationContainer VoipDownServer;

  if (eachSTArunsAllTheApps == false) {
    for (uint32_t j = 0 ; j < numberSTAsSamePlace ; j++ ) {
      for (uint32_t i = numberVoIPupload ; i < numberVoIPupload + numberVoIPdownload ; i++ ) {
        myVoipDownServer = UdpServerHelper(port);
        VoipDownServer = myVoipDownServer.Install (staNodes.Get(i));
        VoipDownServer.Start (Seconds (0.0));
        VoipDownServer.Stop (Seconds (simulationTime + INITIALTIMEINTERVAL));

        // I must create a UdpClient per STA
        UdpClientHelper myVoipDownClient;
        ApplicationContainer VoipDownClient;
        // I associate all the servers (all running in the Stas) to each server application
        // GetAddress() will return the address of the UdpServer

        InetSocketAddress destAddress (InetSocketAddress (staInterfaces[i + (j * number_of_STAs)].GetAddress(0), port));
        
        // If priorities ARE NOT enabled, VoIP traffic will have TcpPriorityLevel
        if (prioritiesEnabled == 0) {
          destAddress.SetTos (TcpPriorityLevel);
        // If priorities ARE enabled, VoIP traffic will have a higher priority (VoIpPriorityLevel)
        } else {
          destAddress.SetTos (VoIpPriorityLevel);
        }

        myVoipDownClient = UdpClientHelper(destAddress);

        myVoipDownClient.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
        //myVoipDownClient.SetAttribute ("Interval", TimeValue (Time ("0.02"))); //packets/s
        myVoipDownClient.SetAttribute ("Interval", TimeValue (Seconds (VoIPg729IPT))); //packets/s
        myVoipDownClient.SetAttribute ("PacketSize", UintegerValue ( VoIPg729PayoladSize ));

        //VoipDownClient = myVoipDownClient.Install (wifiApNodesA.Get(0));
        if (topology == 0) {
          VoipDownClient = myVoipDownClient.Install (singleServerNode.Get(0));
        } else {
          VoipDownClient = myVoipDownClient.Install (serverNodes.Get (i));
        }

        VoipDownClient.Start (Seconds (INITIALTIMEINTERVAL));
        VoipDownClient.Stop (Seconds (simulationTime + INITIALTIMEINTERVAL));

        if (verboseLevel > 0) {
          if (topology == 0) {
            std::cout << "Application VoIP download from the server"
                      << "\t with IP address " << singleServerInterfaces.GetAddress (0) 
                      << "\t-> to STA    #" << staNodes.Get(i + (j * number_of_STAs))->GetId() 
                      << "\t\t with IP address " << staInterfaces[i + (j * number_of_STAs)].GetAddress(0) 
                      << "\t and port " << port
                      << '\n';          
          } else {
            std::cout << "Application VoIP download from server #" << serverNodes.Get(i)->GetId()
                      << "\t with IP address " << serverInterfaces.GetAddress (i) 
                      << "\t-> to STA    #" << staNodes.Get(i + (j * number_of_STAs))->GetId()
                      << "\t\t with IP address " << staInterfaces[i + (j * number_of_STAs)].GetAddress(0)  
                      << "\t and port " << port
                      << '\n';     
          }     
        }
        port ++;
      }
    }
  }
  else {
    if (numberVoIPdownload != 0) {
      for (uint32_t j = 0 ; j < numberSTAsSamePlace ; j++ ) {
        for (uint32_t i = 0 ; i < number_of_STAs; i++ ) {
          myVoipDownServer = UdpServerHelper(port);
          VoipDownServer = myVoipDownServer.Install (staNodes.Get(i + (j * number_of_STAs)));
          VoipDownServer.Start (Seconds (0.0));
          VoipDownServer.Stop (Seconds (simulationTime + INITIALTIMEINTERVAL));

          // I must create a UdpClient per STA
          UdpClientHelper myVoipDownClient;
          ApplicationContainer VoipDownClient;
          // I associate all the servers (all running in the Stas) to each server application
          // GetAddress() will return the address of the UdpServer

          InetSocketAddress destAddress (InetSocketAddress (staInterfaces[i + (j * number_of_STAs)].GetAddress(0), port));
          
          // If priorities ARE NOT enabled, VoIP traffic will have TcpPriorityLevel
          if (prioritiesEnabled == 0) {
            destAddress.SetTos (TcpPriorityLevel);
          // If priorities ARE enabled, VoIP traffic will have a higher priority (VoIpPriorityLevel)
          } else {
            destAddress.SetTos (VoIpPriorityLevel);
          }

          myVoipDownClient = UdpClientHelper(destAddress);

          myVoipDownClient.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
          //myVoipDownClient.SetAttribute ("Interval", TimeValue (Time ("0.02"))); //packets/s
          myVoipDownClient.SetAttribute ("Interval", TimeValue (Seconds (VoIPg729IPT))); //packets/s
          myVoipDownClient.SetAttribute ("PacketSize", UintegerValue ( VoIPg729PayoladSize ));

          //VoipDownClient = myVoipDownClient.Install (wifiApNodesA.Get(0));
          if (topology == 0) {
            VoipDownClient = myVoipDownClient.Install (singleServerNode.Get(0));
          } else {
            VoipDownClient = myVoipDownClient.Install (serverNodes.Get (i));
          }

          VoipDownClient.Start (Seconds (INITIALTIMEINTERVAL));
          VoipDownClient.Stop (Seconds (simulationTime + INITIALTIMEINTERVAL));

          if (verboseLevel > 0) {
            if (topology == 0) {
              std::cout << "Application VoIP download from the server"
                        << "\t with IP address " << singleServerInterfaces.GetAddress (0) 
                        << "\t-> to STA    #" << staNodes.Get(i + (j * number_of_STAs))->GetId() 
                        << "\t\t with IP address " << staInterfaces[i].GetAddress(0) 
                        << "\t and port " << port
                        << '\n';          
            } else {
              std::cout << "Application VoIP download from server #" << serverNodes.Get(i)->GetId()
                        << "\t with IP address " << serverInterfaces.GetAddress (i) 
                        << "\t-> to STA    #" << staNodes.Get(i + (j * number_of_STAs))->GetId()
                        << "\t\t with IP address " << staInterfaces[i + (j * number_of_STAs)].GetAddress(0)  
                        << "\t and port " << port
                        << '\n';     
            }     
          }
          port ++;
        }
      }
    }
  }
  /*** end of - VoIP download applications ***/


  /*** Configurations for TCP applications ***/
  // This is necessary, or the packets will not be of this size
  // Taken from https://www.nsnam.org/doxygen/codel-vs-pfifo-asymmetric_8cc_source.html
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (TcpPayloadSize));

  // The next lines seem to be useless
  // 4 MB of TCP buffer
  //Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (1 << 21));
  //Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (1 << 21));

  // Activate TCP selective acknowledgement (SACK)
  // bool sack = true;
  // Config::SetDefault ("ns3::TcpSocketBase::Sack", BooleanValue (sack));

  // TCP variants, see 
  //  https://www.nsnam.org/docs/models/html/tcp.html
  //  https://www.nsnam.org/doxygen/tcp-variants-comparison_8cc_source.html

  // TCP NewReno is the default in ns3, and also in this script
  if (TcpVariant.compare ("TcpNewReno") == 0)
  {
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpNewReno::GetTypeId ()));
  } 
  else if (TcpVariant.compare ("TcpHighSpeed") == 0) {
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpHighSpeed::GetTypeId ()));
  } 
  else if (TcpVariant.compare ("TcpWestwoodPlus") == 0) {
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpWestwood::GetTypeId ()));
    Config::SetDefault ("ns3::TcpWestwood::ProtocolType", EnumValue (TcpWestwood::WESTWOODPLUS));
    Config::SetDefault ("ns3::TcpWestwood::FilterType", EnumValue (TcpWestwood::TUSTIN));
  } else {
    std::cout << "INPUT PARAMETER ERROR: Bad TCP variant. Supported: TcpNewReno, TcpHighSpeed, TcpWestwoodPlus. Stopping the simulation." << '\n';
    return 0;  
  }

  // Activate the log of BulkSend application
  if (verboseLevel > 2) {
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
  }
  /*** end of - Configurations for TCP applications ***/


  /*** TCP upload applications ***/
  port = INITIALPORT_TCP_UPLOAD;

  // Create a PacketSinkApplication and install it on the remote nodes
  ApplicationContainer PacketSinkTcpUp;

  // Create a BulkSendApplication and install it on all the staNodes
  // it will send traffic to the servers
  ApplicationContainer BulkSendTcpUp;

  if (eachSTArunsAllTheApps == false) {
    for (uint32_t j = 0 ; j < numberSTAsSamePlace ; j++ ) {
      for (uint16_t i = numberVoIPupload + numberVoIPdownload; i < numberVoIPupload + numberVoIPdownload + numberTCPupload; i++) {

        PacketSinkHelper myPacketSinkTcpUp ("ns3::TcpSocketFactory",
                                            InetSocketAddress (Ipv4Address::GetAny (), port));

        myPacketSinkTcpUp.SetAttribute ("Protocol", TypeIdValue (TcpSocketFactory::GetTypeId ()));

        if (topology == 0) {
          PacketSinkTcpUp.Add(myPacketSinkTcpUp.Install (singleServerNode.Get(0)));
        } else {
          PacketSinkTcpUp.Add(myPacketSinkTcpUp.Install (serverNodes.Get (i)));
        }

        Ipv4Address myaddress;

        if (topology == 0) {
          myaddress = singleServerInterfaces.GetAddress (0); 
        } else {
          myaddress = serverInterfaces.GetAddress (i);
        }

        InetSocketAddress destAddress (InetSocketAddress (myaddress, port));

        // TCP will have TcpPriorityLevel, whether priorities are enabled or not
        destAddress.SetTos (TcpPriorityLevel);

        BulkSendHelper myBulkSendTcpUp ( "ns3::TcpSocketFactory", destAddress);

        // Set the amount of data to send in bytes. Zero is unlimited.
        myBulkSendTcpUp.SetAttribute ("MaxBytes", UintegerValue (0));
        myBulkSendTcpUp.SetAttribute ("SendSize", UintegerValue (TcpPayloadSize));
        // You must also add the Config::SetDefalut  SegmentSize line, or the previous line will have no effect

        // install the application on every staNode
        BulkSendTcpUp = myBulkSendTcpUp.Install (staNodes.Get(i + (j * number_of_STAs)));

        if (verboseLevel > 0) {
          if (topology == 0) {
            std::cout << "Application TCP upload    from STA    #" << staNodes.Get(i + (j * number_of_STAs))->GetId() 
                      << "\t with IP address " << staInterfaces[i + (j * number_of_STAs)].GetAddress(0) 
                      << "\t-> to the server"
                      << "\t with IP address " << singleServerInterfaces.GetAddress (0) 
                      << "\t and port " << port
                      << '\n';
          } else {
            std::cout << "Application TCP upload    from STA    #" << staNodes.Get(i)->GetId() 
                      << "\t with IP address " << staInterfaces[i + (j * number_of_STAs)].GetAddress(0) 
                      << "\t-> to server #" << serverNodes.Get(i)->GetId() 
                      << "\t with IP address " << serverInterfaces.GetAddress (i) 
                      << "\t and port " << port
                      << '\n';
          } 
        }
        port++;
      }      
    }
  }
  else {
    if (numberTCPupload != 0) {
      for (uint32_t j = 0 ; j < numberSTAsSamePlace ; j++ ) {
        for (uint16_t i = 0; i < number_of_STAs; i++) {

          PacketSinkHelper myPacketSinkTcpUp ("ns3::TcpSocketFactory",
                                              InetSocketAddress (Ipv4Address::GetAny (), port));

          myPacketSinkTcpUp.SetAttribute ("Protocol", TypeIdValue (TcpSocketFactory::GetTypeId ()));

          if (topology == 0) {
            PacketSinkTcpUp.Add(myPacketSinkTcpUp.Install (singleServerNode.Get(0)));
          } else {
            PacketSinkTcpUp.Add(myPacketSinkTcpUp.Install (serverNodes.Get (i)));
          }

          Ipv4Address myaddress;

          if (topology == 0) {
            myaddress = singleServerInterfaces.GetAddress (0); 
          } else {
            myaddress = serverInterfaces.GetAddress (i);
          }

          InetSocketAddress destAddress (InetSocketAddress (myaddress, port));

          // TCP will have TcpPriorityLevel, whether priorities are enabled or not
          destAddress.SetTos (TcpPriorityLevel);

          BulkSendHelper myBulkSendTcpUp ( "ns3::TcpSocketFactory", destAddress);

          // Set the amount of data to send in bytes. Zero is unlimited.
          myBulkSendTcpUp.SetAttribute ("MaxBytes", UintegerValue (0));
          myBulkSendTcpUp.SetAttribute ("SendSize", UintegerValue (TcpPayloadSize));
          // You must also add the Config::SetDefalut  SegmentSize line, or the previous line will have no effect

          // install the application on every staNode
          BulkSendTcpUp = myBulkSendTcpUp.Install (staNodes.Get(i + (j * number_of_STAs)));

          if (verboseLevel > 0) {
            if (topology == 0) {
              std::cout << "Application TCP upload    from STA    #" << staNodes.Get(i + (j * number_of_STAs))->GetId() 
                        << "\t with IP address " << staInterfaces[i + (j * number_of_STAs)].GetAddress(0) 
                        << "\t-> to the server"
                        << "\t with IP address " << singleServerInterfaces.GetAddress (0) 
                        << "\t and port " << port
                        << '\n';
            } else {
              std::cout << "Application TCP upload    from STA    #" << staNodes.Get(i + (j * number_of_STAs))->GetId() 
                        << "\t with IP address " << staInterfaces[i + (j * number_of_STAs)].GetAddress(0) 
                        << "\t-> to server #" << serverNodes.Get(i)->GetId() 
                        << "\t with IP address " << serverInterfaces.GetAddress (i) 
                        << "\t and port " << port
                        << '\n';
            } 
          }
          port++;
        }          
      }
    }  
  }
  PacketSinkTcpUp.Start (Seconds (0.0));
  PacketSinkTcpUp.Stop (Seconds (simulationTime + INITIALTIMEINTERVAL));

  BulkSendTcpUp.Start (Seconds (INITIALTIMEINTERVAL));
  BulkSendTcpUp.Stop (Seconds (simulationTime + INITIALTIMEINTERVAL));
  /*** end of - TCP upload applications ***/


  /*** TCP download applications ***/
  port = INITIALPORT_TCP_DOWNLOAD;

  // Create a PacketSink Application and install it on the wifi STAs
  ApplicationContainer PacketSinkTcpDown;

  // Create a BulkSendApplication and install it on all the servers
  // it will send traffic to the STAs
  ApplicationContainer BulkSendTcpDown, BulkSendTcpDownSecondary;

  // create an application container to be used if the option of using many TCP flows is chosen
  ApplicationContainer BulkSendTcpDownMultiConnection[TcpDownMultiConnection];

  if (eachSTArunsAllTheApps == false) {
    for (uint32_t j = 0 ; j < numberSTAsSamePlace ; j++ ) {
      for (uint16_t i = numberVoIPupload + numberVoIPdownload + numberTCPupload; 
                    i < numberVoIPupload + numberVoIPdownload + numberTCPupload + numberTCPdownload; 
                    i++)
      {
        if (TcpDownMultiConnection == 0) {
          // Install a sink on each STA
          // Each sink will have a different port
          PacketSinkHelper myPacketSinkTcpDown ( "ns3::TcpSocketFactory",
                                                  InetSocketAddress (Ipv4Address::GetAny (), port ));

          myPacketSinkTcpDown.SetAttribute ("Protocol", TypeIdValue (TcpSocketFactory::GetTypeId ()));

          PacketSinkTcpDown = myPacketSinkTcpDown.Install(staNodes.Get (i + (j * number_of_STAs)));
          PacketSinkTcpDown.Start (Seconds (0.0));
          PacketSinkTcpDown.Stop (Seconds (simulationTime + INITIALTIMEINTERVAL));

          // Install a sender on the sender node
          InetSocketAddress destAddress (InetSocketAddress (staInterfaces[i + (j * number_of_STAs)].GetAddress(0), port ));

          // TCP will have TcpPriorityLevel, whether priorities are enabled or not
          destAddress.SetTos (TcpPriorityLevel);
          
          BulkSendHelper myBulkSendTcpDown ( "ns3::TcpSocketFactory", destAddress);

          // Set the amount of data to send in bytes.  Zero is unlimited.
          myBulkSendTcpDown.SetAttribute ("MaxBytes", UintegerValue (0));
          myBulkSendTcpDown.SetAttribute ("SendSize", UintegerValue (TcpPayloadSize));
          // You must also add the Config::SetDefalut  SegmentSize line, or the previous line will have no effect

          // Install a sender on the server
          if (topology == 0) {
            BulkSendTcpDown.Add(myBulkSendTcpDown.Install (singleServerNode.Get(0)));
          } else {
            BulkSendTcpDown.Add(myBulkSendTcpDown.Install (serverNodes.Get (i)));
          }

          if (verboseLevel > 0) {
            if (topology == 0) {
              std::cout << "Application TCP download from the server"
                        << "\t with IP address " << singleServerInterfaces.GetAddress (0) 
                        << "\t-> to STA    #" << staNodes.Get(i + (j * number_of_STAs))->GetId() 
                        << "\t\twith IP address " << staInterfaces[i + (j * number_of_STAs)].GetAddress(0) 
                        << "\t and port " << port
                        << '\n';    
            } else {
              std::cout << "Application TCP download from server #" << serverNodes.Get(i)->GetId()
                        << "\t with IP address " << serverInterfaces.GetAddress (i) 
                        << "\t-> to STA    #" << staNodes.Get(i + (j * number_of_STAs))->GetId() 
                        << "\t\twith IP address " << staInterfaces[i + (j * number_of_STAs)].GetAddress(0) 
                        << "\t and port " << port
                        << '\n';
            }
          }
          port++;
        }
        else { // if (TcpDownMultiConnection > 0)
          NS_ASSERT (topology == 2);

          double startTCPperiod = (int)(simulationTime / TcpDownMultiConnection);
          if (VERBOSE_FOR_DEBUG > 0)
            std::cout << "Start TCP period: " << startTCPperiod << '\n';

          for (int k = 0; k < TcpDownMultiConnection; k++) {
            // Install a sink on each STA
            // Each sink will have a different port
            PacketSinkHelper myPacketSinkTcpDown ( "ns3::TcpSocketFactory",
                                                    InetSocketAddress (Ipv4Address::GetAny (), port /*+ (1000 * (j+1)) + k */));

            myPacketSinkTcpDown.SetAttribute ("Protocol", TypeIdValue (TcpSocketFactory::GetTypeId ()));

            PacketSinkTcpDown = myPacketSinkTcpDown.Install(staNodes.Get (i + (j * number_of_STAs)));
            PacketSinkTcpDown.Start (Seconds (0.0));
            PacketSinkTcpDown.Stop (Seconds (simulationTime + INITIALTIMEINTERVAL));

            // the first port will be 61000 for the primary STA, and 62000 for the secondary STA
            InetSocketAddress destAddress (InetSocketAddress (staInterfaces[i + (j * number_of_STAs)].GetAddress(0), port /*+ (1000 * (j+1)) + k */));

            // TCP will have TcpPriorityLevel, whether priorities are enabled or not
            destAddress.SetTos (TcpPriorityLevel);
            
            BulkSendHelper myBulkSendTcpDown ( "ns3::TcpSocketFactory", destAddress);

            // Set the amount of data to send in bytes.  Zero is unlimited.
            myBulkSendTcpDown.SetAttribute ("MaxBytes", UintegerValue (0));
            myBulkSendTcpDown.SetAttribute ("SendSize", UintegerValue (TcpPayloadSize));
            // You must also add the Config::SetDefalut  SegmentSize line, or the previous line will have no effect

            // Install a sender on the server 
            BulkSendTcpDownMultiConnection[k].Add(myBulkSendTcpDown.Install (serverNodes.Get (i)));

            BulkSendTcpDownMultiConnection[k].Start (Seconds (INITIALTIMEINTERVAL + (k * startTCPperiod)));
            BulkSendTcpDownMultiConnection[k].Stop (Seconds (simulationTime + INITIALTIMEINTERVAL));

            if (VERBOSE_FOR_DEBUG > 0)
              std::cout << "STA #" << staNodes.Get(i + (j * number_of_STAs))->GetId()
                        << " multi TCP download #"<< k
                        << " - port " << port //+ (1000 * (j+1)) + k 
                        << " - start " << INITIALTIMEINTERVAL + (k * startTCPperiod)
                        << ". stop " << simulationTime + INITIALTIMEINTERVAL
                        << '\n';  

            port++;
          }
        }
      }      
    }
  }
  else { //if (eachSTArunsAllTheApps == true)
    
    // this option does NOT work for TCP Down multi connection
    NS_ASSERT (TcpDownMultiConnection == 0);

    if (numberTCPdownload != 0) {
      for (uint32_t j = 0 ; j < numberSTAsSamePlace ; j++ ) {
        for (uint16_t i = 0; i < number_of_STAs; i++) {

          // Install a sink on each STA
          // Each sink will have a different port
          PacketSinkHelper myPacketSinkTcpDown ( "ns3::TcpSocketFactory",
                                                  InetSocketAddress (Ipv4Address::GetAny (), port ));

          myPacketSinkTcpDown.SetAttribute ("Protocol", TypeIdValue (TcpSocketFactory::GetTypeId ()));

          PacketSinkTcpDown = myPacketSinkTcpDown.Install(staNodes.Get (i + (j * number_of_STAs)));
          PacketSinkTcpDown.Start (Seconds (0.0));
          PacketSinkTcpDown.Stop (Seconds (simulationTime + INITIALTIMEINTERVAL));

          // Install a sender on the sender node
          InetSocketAddress destAddress (InetSocketAddress (staInterfaces[i + (j * number_of_STAs)].GetAddress(0), port ));

          // TCP will have TcpPriorityLevel, whether priorities are enabled or not
          destAddress.SetTos (TcpPriorityLevel);
          
          BulkSendHelper myBulkSendTcpDown ( "ns3::TcpSocketFactory", destAddress);

          // Set the amount of data to send in bytes.  Zero is unlimited.
          myBulkSendTcpDown.SetAttribute ("MaxBytes", UintegerValue (0));
          myBulkSendTcpDown.SetAttribute ("SendSize", UintegerValue (TcpPayloadSize));
          // You must also add the Config::SetDefalut  SegmentSize line, or the previous line will have no effect


          // Install a sender on the server
          if (topology == 0) {
            BulkSendTcpDown.Add(myBulkSendTcpDown.Install (singleServerNode.Get(0)));
          } else {
            BulkSendTcpDown.Add(myBulkSendTcpDown.Install (serverNodes.Get (i)));
          }

          if (verboseLevel > 0) {
            if (topology == 0) {
              std::cout << "Application TCP download  from the server"
                        << "\t with IP address " << singleServerInterfaces.GetAddress (0) 
                        << "\t-> to STA    #" << staNodes.Get(i + (j * number_of_STAs))->GetId() 
                        << "\t\t with IP address " << staInterfaces[i + (j * number_of_STAs)].GetAddress(0) 
                        << "\t and port " << port
                        << '\n';    
            } else {
              std::cout << "Application TCP download  from server #" << serverNodes.Get(i)->GetId()
                        << "\t with IP address " << serverInterfaces.GetAddress (i) 
                        << "\t-> to STA    #" << staNodes.Get(i + (j * number_of_STAs))->GetId() 
                        << "\t\t with IP address " << staInterfaces[i + (j * number_of_STAs)].GetAddress(0) 
                        << "\t and port " << port
                        << '\n';
            }
          }
          port++;
        }        
      }
    }
  }

  if (TcpDownMultiConnection == 0) {
    // I avoid these flows if TCP Down multi connection is active (!= 0)

    // set the moments for starting and stopping TCP Down
    BulkSendTcpDown.Start (Seconds (INITIALTIMEINTERVAL));
    BulkSendTcpDown.Stop (Seconds (simulationTime + INITIALTIMEINTERVAL));    
  }

  /*** end of - TCP download applications ***/


  /*** Video download applications ***/
  port = INITIALPORT_VIDEO_DOWNLOAD;
  // Using UdpTraceClient, see  https://www.nsnam.org/doxygen/udp-trace-client_8cc_source.html
  //                            https://www.nsnam.org/doxygen/structns3_1_1_udp_trace_client.html
  // The traffic traces are      http://www2.tkn.tu-berlin.de/research/trace/ltvt.html (the 2 first lines of the file should be removed)
  /*A valid trace file is a file with 4 columns:
    -1- the first one represents the frame index
    -2- the second one indicates the type of the frame: I, P or B
    -3- the third one indicates the time on which the frame was generated by the encoder (integer, milliseconds)
    -4- the fourth one indicates the frame size in byte
  */

  // VideoDownServer in the STA (receives the video)
  // Client in the corresponding server
  UdpServerHelper myVideoDownServer;
  ApplicationContainer VideoDownServer;

  if (eachSTArunsAllTheApps == false) {
    for (uint32_t j = 0 ; j < numberSTAsSamePlace ; j++ ) {
      for (uint16_t i = numberVoIPupload + numberVoIPdownload + numberTCPupload + numberTCPdownload; 
                    i < numberVoIPupload + numberVoIPdownload + numberTCPupload + numberTCPdownload + numberVideoDownload; 
                    i++)
      {
        myVideoDownServer = UdpServerHelper(port);
        VideoDownServer = myVideoDownServer.Install (staNodes.Get (i + (j * number_of_STAs)));
        VideoDownServer.Start (Seconds (0.0));
        VideoDownServer.Stop (Seconds (simulationTime + INITIALTIMEINTERVAL));

        UdpTraceClientHelper myVideoDownClient;
        ApplicationContainer VideoDownClient;

        InetSocketAddress destAddress (InetSocketAddress (staInterfaces[i + (j * number_of_STAs)].GetAddress(0), port));

        if (prioritiesEnabled == 0) {
          destAddress.SetTos (VideoPriorityLevel);
        } else {
          destAddress.SetTos (VideoPriorityLevel);
        }

        myVideoDownClient = UdpTraceClientHelper(destAddress, port,"");
        myVideoDownClient.SetAttribute ("MaxPacketSize", UintegerValue (VideoMaxPacketSize));
        myVideoDownClient.SetAttribute ("TraceLoop", BooleanValue (1));


        // Random integer for choosing the movie
        uint16_t numberOfMovies = 4;
        Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();
        double random_number = x->GetValue(0.0, numberOfMovies * 1.0);

        if ( verboseLevel > 2 )
          std::cout << "Random number: " << random_number << "\n\n";

        std::string movieFileName;  // The files have to be in the /ns-3-allinone/ns-3-dev/traces folder

        if (random_number < 1.0 )
          movieFileName = "traces/Verbose_Jurassic.dat";   //http://www2.tkn.tu-berlin.de/research/trace/pics/FrameTrace/mp4/Verbose_Jurassic.dat
        else if (random_number < 2.0 )
        movieFileName = "traces/Verbose_FirstContact.dat"; //http://www2.tkn.tu-berlin.de/research/trace/pics/FrameTrace/mp4/Verbose_FirstContact.dat
        else if (random_number < 3.0 )
          movieFileName = "traces/Verbose_StarWarsIV.dat";  //http://www2.tkn.tu-berlin.de/research/trace/pics/FrameTrace/mp4/Verbose_StarWarsIV.dat
        else if (random_number < 4.0 )
          movieFileName = "traces/Verbose_DieHardIII.dat";  //http://www2.tkn.tu-berlin.de/research/trace/pics/FrameTrace/mp4/Verbose_DieHardIII.dat

        myVideoDownClient.SetAttribute ("TraceFilename", StringValue (movieFileName)); 

        //VoipDownClient = myVoipDownClient.Install (wifiApNodesA.Get(0));
        if (topology == 0) {
          VideoDownClient = myVideoDownClient.Install (singleServerNode.Get(0));
        } else {
          VideoDownClient = myVideoDownClient.Install (serverNodes.Get (i));
        }

        VideoDownClient.Start (Seconds (INITIALTIMEINTERVAL));
        VideoDownClient.Stop (Seconds (simulationTime + INITIALTIMEINTERVAL));

        if (verboseLevel > 0) {
          if (topology == 0) {
            std::cout << "Application Video download from the server"
                      << "\t with IP address " << singleServerInterfaces.GetAddress (0) 
                      << "\t-> to STA    #" << staNodes.Get(i + (j * number_of_STAs))->GetId() 
                      << "\t\t with IP address " << staInterfaces[i + (j * number_of_STAs)].GetAddress(0) 
                      << "\t and port " << port
                      << "\t " << movieFileName
                      << '\n';          
          }
          else {
            std::cout << "Application Video download from server #" << serverNodes.Get(i)->GetId()
                      << "\t with IP address " << serverInterfaces.GetAddress (i) 
                      << "\t-> to STA    #" << staNodes.Get(i + (j * number_of_STAs))->GetId()
                      << "\t\t with IP address " << staInterfaces[i + (j * number_of_STAs)].GetAddress(0)  
                      << "\t and port " << port
                      << "\t " << movieFileName
                      << '\n';     
          }     
        }
        port ++;
      }      
    }
  }
  else {
    if (numberVideoDownload != 0) {
      for (uint32_t j = 0 ; j < numberSTAsSamePlace ; j++ ) {
        for (uint16_t i = 0; i < number_of_STAs; i++) {
          myVideoDownServer = UdpServerHelper(port);
          VideoDownServer = myVideoDownServer.Install (staNodes.Get (i + (j * number_of_STAs)));
          VideoDownServer.Start (Seconds (0.0));
          VideoDownServer.Stop (Seconds (simulationTime + INITIALTIMEINTERVAL));

          UdpTraceClientHelper myVideoDownClient;
          ApplicationContainer VideoDownClient;

          InetSocketAddress destAddress (InetSocketAddress (staInterfaces[i + (j * number_of_STAs)].GetAddress(0), port));

          if (prioritiesEnabled == 0) {
            destAddress.SetTos (VideoPriorityLevel);
          } else {
            destAddress.SetTos (VideoPriorityLevel);
          }

          myVideoDownClient = UdpTraceClientHelper(destAddress, port,"");
          myVideoDownClient.SetAttribute ("MaxPacketSize", UintegerValue (VideoMaxPacketSize));
          myVideoDownClient.SetAttribute ("TraceLoop", BooleanValue (1));


          // Random integer for choosing the movie
          uint16_t numberOfMovies = 4;
          Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();
          double random_number = x->GetValue(0.0, numberOfMovies * 1.0);

          if ( verboseLevel > 2 )
            std::cout << "Random number: " << random_number << "\n\n";

          std::string movieFileName;  // The files have to be in the /ns-3-allinone/ns-3-dev/traces folder

          if (random_number < 1.0 )
            movieFileName = "traces/Verbose_Jurassic.dat";   //http://www2.tkn.tu-berlin.de/research/trace/pics/FrameTrace/mp4/Verbose_Jurassic.dat
          else if (random_number < 2.0 )
          movieFileName = "traces/Verbose_FirstContact.dat"; //http://www2.tkn.tu-berlin.de/research/trace/pics/FrameTrace/mp4/Verbose_FirstContact.dat
          else if (random_number < 3.0 )
            movieFileName = "traces/Verbose_StarWarsIV.dat";  //http://www2.tkn.tu-berlin.de/research/trace/pics/FrameTrace/mp4/Verbose_StarWarsIV.dat
          else if (random_number < 4.0 )
            movieFileName = "traces/Verbose_DieHardIII.dat";  //http://www2.tkn.tu-berlin.de/research/trace/pics/FrameTrace/mp4/Verbose_DieHardIII.dat

          myVideoDownClient.SetAttribute ("TraceFilename", StringValue (movieFileName)); 

          //VoipDownClient = myVoipDownClient.Install (wifiApNodesA.Get(0));
          if (topology == 0) {
            VideoDownClient = myVideoDownClient.Install (singleServerNode.Get(0));
          } else {
            VideoDownClient = myVideoDownClient.Install (serverNodes.Get (i));
          }

          VideoDownClient.Start (Seconds (INITIALTIMEINTERVAL));
          VideoDownClient.Stop (Seconds (simulationTime + INITIALTIMEINTERVAL));

          if (verboseLevel > 0) {
            if (topology == 0) {
              std::cout << "Application Video download from the server"
                        << "\t with IP address " << singleServerInterfaces.GetAddress (0) 
                        << "\t-> to STA    #" << staNodes.Get(i + (j * number_of_STAs))->GetId() 
                        << "\t\t with IP address " << staInterfaces[i + (j * number_of_STAs)].GetAddress(0) 
                        << "\t and port " << port
                        << "\t " << movieFileName
                        << '\n';          
            } else {
              std::cout << "Application Video download from server #" << serverNodes.Get(i)->GetId()
                        << "\t with IP address " << serverInterfaces.GetAddress (i) 
                        << "\t-> to STA    #" << staNodes.Get(i + (j * number_of_STAs))->GetId()
                        << "\t\t with IP address " << staInterfaces[i + (j * number_of_STAs)].GetAddress(0)  
                        << "\t and port " << port
                        << "\t " << movieFileName
                        << '\n';     
            }     
          }
          port ++;
        }          
      }
    }
  }
  /*** end of - Video download applications ***/


  // print a blank line after the info about the applications
  if (verboseLevel > 0)
    std::cout << "\n";


  /****** Enable the creation of pcap files ******/
  if (enablePcap) {

    // pcap trace of the APs and the STAs
    if (wifiModel == 0) {
      // wifiPhy model

      wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

      // pcap trace of the APs
      // the APs marked as inactive by the user, will NOT be createdex
      uint32_t j = 0; // j is the index for the APs that do exist
      for (uint32_t i=0; i < number_of_APs * numberAPsSamePlace; i++) { // i is the index for the possible APs
        if (APsActive.at(i) == '1') {
          // the AP is active. 'i' and 'j' will be increased
          if (VERBOSE_FOR_DEBUG >= 1)
            std::cout << "AP#" << i << " is active"
                      << ". Channel: " << (int)AP_vector[j]->GetWirelessChannel()
                      << ". j: " << j
                      << '\n';

          if (AP_vector[i]->GetWirelessChannel() != 0) {
            wifiPhy.EnablePcap (outputFileName + "_" + outputFileSurname + "_wifi_AP", apWiFiDevices[i]);
          }
          NS_ASSERT(j < AP_vector.size());          
          j++;
        }
        else {
          // the AP is NOT active. 'j' will NOT be increased
          if (VERBOSE_FOR_DEBUG >= 1)
            std::cout << "AP#" << i << " is NOT active"
                      << ". j: " << j
                      << '\n';
        }            
      }

      // pcap trace of the STAs
      // the STAs are all created (even if they are set as inactive)
      for (uint32_t i=0; i < number_of_STAs * numberSTAsSamePlace; i++) {
        wifiPhy.EnablePcap (outputFileName + "_" + outputFileSurname + "_wifi_STA", staDevices[i]);
      }
    }
    else {
      // spectrumPhy model

      spectrumPhy.SetPcapDataLinkType (SpectrumWifiPhyHelper::DLT_IEEE802_11_RADIO);

      // pcap trace of the APs
      // the APs marked as inactive by the user, will NOT be created
      uint32_t j = 0; // j is the index for the APs that do exist
      for (uint32_t i=0; i < number_of_APs * numberAPsSamePlace; i++) { // i is the index for the possible APs

        if (APsActive.at(i) == '1') {
          // the AP is active. 'i' and 'j' will be increased
          if (VERBOSE_FOR_DEBUG >= 1)
            std::cout << "AP#" << i << " is active"
                      << ". Channel: " << (int)AP_vector[j]->GetWirelessChannel()
                      << ". j: " << j
                      << '\n';

          if (AP_vector[j]->GetWirelessChannel() != 0) {
            spectrumPhy.EnablePcap (outputFileName + "_" + outputFileSurname + "_wifi_AP", apWiFiDevices[j]);     
          }
          NS_ASSERT(j < AP_vector.size());          
          j++;
        }
        else {
          // the AP is NOT active. 'j' will NOT be increased
          if (VERBOSE_FOR_DEBUG >= 1)
            std::cout << "AP#" << i << " is NOT active"
                      << ". j: " << j
                      << '\n';
        }
      }

      // pcap trace of the STAs
      // the STAs are all created (even if they are set as inactive)
      for (uint32_t i=0; i < number_of_STAs * numberSTAsSamePlace; i++) {
        spectrumPhy.EnablePcap (outputFileName + "_" + outputFileSurname + "_wifi_STA", staDevices[i]);
      }
    }

    // pcap trace of the server(s)
    if (topology == 0) {
      // pcap trace of the single server
      csma.EnablePcap (outputFileName + "_" + outputFileSurname + "_csma_single_server", singleServerDevices.Get(0));
    }

    else {
      // pcap trace of the servers
      for (uint32_t i=0; i < number_of_Servers; i++)
        p2p.EnablePcap (outputFileName + "_" + outputFileSurname + "_p2p_server", serverDevices.Get(i));
    }


    // pcap trace of the hub ports
    if (topology == 0) {
      // pcap trace of the hub: it has number_of_APs * numberAPsSamePlace + 1 devices (the one connected to the server)
      for (uint32_t i=0; i < (number_of_APs * numberAPsSamePlace) + 1; i++)
        csma.EnablePcap (outputFileName + "_" + outputFileSurname + "_csma_hub", csmaHubDevices.Get(i));
    }

    else if (topology == 1) {
       // pcap trace of the hub: it has number_of_APs * numberAPsSamePlace + number_of_server devices
      for (uint32_t i=0; i < (number_of_APs * numberAPsSamePlace) + number_of_Servers; i++)
        csma.EnablePcap (outputFileName + "_" + outputFileSurname + "_csma_hub", csmaHubDevices.Get(i)); 
    }

    else if (topology == 2) {
      // pcap trace of the hub: it has (number_of_APs * numberAPsSamePlace), but some of them MAY NOT exist
      for (uint32_t i=0; i < (number_of_APs * numberAPsSamePlace); i++) { // i is the index for the possible APs
        uint32_t j = 0; // j is the index for the APs that do exist
        if (APsActive.at(i) == '1') {
          // the AP is active. 'i' and 'j' will be increased
          if (VERBOSE_FOR_DEBUG >= 1)
            std::cout << "channel: " << (int)AP_vector[j]->GetWirelessChannel() << '\n';

          if (AP_vector[i]->GetWirelessChannel() != 0) {
            csma.EnablePcap (outputFileName + "_" + outputFileSurname + "_csma_hubToAPs", csmaHubDevices.Get(j));
          }
          NS_ASSERT(j < AP_vector.size());          
          j++;
        }
        else {
          // the AP is NOT active. 'j' will NOT be increased
        }            
      }

      // the hub has another device (the one connected to the router). This trace is this one:
      csma.EnablePcap (outputFileName + "_" + outputFileSurname + "_csma_hubToRouter", routerDeviceToAps.Get(0)); // would be the same as 'csmaHubDevices.Get(number_of_APs* numberAPsSamePlace)'
    }
  }
  /****** end of - Enable the creation of pcap files ******/


  // Install FlowMonitor on the nodes
  // see https://www.nsnam.org/doxygen/wifi-hidden-terminal_8cc_source.html
  // and https://www.nsnam.org/docs/models/html/flow-monitor.html
  //FlowMonitorHelper flowmon; // FIXME Avoid the use of a global variable 'flowmon' https://groups.google.com/forum/#!searchin/ns-3-users/const$20ns3$3A$3AFlowMonitorHelper$26)$20is$20private%7Csort:date/ns-3-users/1WbpLwvYTcM/dQUQEJKkAQAJ
  Ptr<FlowMonitor> monitor;


  // It is not necessary to monitor the APs, because I am not getting statistics from them
  if (false)
    monitor = flowmon.Install(apNodes);

  // install monitor in all the STAs
  monitor = flowmon.Install(staNodes);

  // install monitor in the active STAs
  /*
  for (uint32_t i = 0; i < number_of_STAs; i++)
    if (STAsActive.at(i) == '1')
      monitor = flowmon.Install(staNodes.Get(i));*/


  // install monitor in the server(s)
  if (topology == 0) {
    monitor = flowmon.Install(singleServerNode);
  } else {
    monitor = flowmon.Install(serverNodes);
  }


  // If the delay monitor is on, periodically calculate the statistics
  if (timeMonitorKPIs > 0.0) {
    // Schedule a periodic task to obtain the statistics of each kind of flow
    if (numberVoIPupload > 0)    
      Simulator::Schedule(  Seconds(INITIALTIMEINTERVAL),
                            &obtainKPIs,
                            monitor
                            /*, flowmon*/, // FIXME Avoid the use of a global variable 'flowmon'
                            myFlowStatisticsVoIPUpload,
                            1,
                            verboseLevel,
                            timeMonitorKPIs);

    if (numberVoIPdownload > 0)   
      Simulator::Schedule(  Seconds(INITIALTIMEINTERVAL),
                            &obtainKPIs,
                            monitor
                            /*, flowmon*/, // FIXME Avoid the use of a global variable 'flowmon'
                            myFlowStatisticsVoIPDownload,
                            2,
                            verboseLevel,
                            timeMonitorKPIs);

    if (numberTCPupload > 0)
      // Schedule a periodic obtaining of statistics    
      Simulator::Schedule(  Seconds(INITIALTIMEINTERVAL),
                            &obtainKPIs,
                            monitor
                            /*, flowmon*/, // FIXME Avoid the use of a global variable 'flowmon'
                            myFlowStatisticsTCPUpload,
                            3,
                            verboseLevel,
                            timeMonitorKPIs);

    if (numberTCPdownload > 0) {
      // Schedule a periodic obtaining of statistics  
      if (TcpDownMultiConnection == 0) {
        Simulator::Schedule(  Seconds(INITIALTIMEINTERVAL),
                              &obtainKPIs,
                              monitor
                              /*, flowmon*/, // FIXME Avoid the use of a global variable 'flowmon'
                              myFlowStatisticsTCPDownload,
                              5,
                              verboseLevel,
                              timeMonitorKPIs);
      }
      else {
        Simulator::Schedule(  Seconds(INITIALTIMEINTERVAL),
                              &obtainKPIsMultiTCP,
                              monitor
                              /*, flowmon*/, // FIXME Avoid the use of a global variable 'flowmon'
                              myFlowStatisticsTCPDownload,
                              number_of_STAs * numberSTAsSamePlace,
                              TcpDownMultiConnection,
                              verboseLevel,
                              timeMonitorKPIs);
      }
    }

    if (numberVideoDownload > 0)
      // Schedule a periodic obtaining of statistics    
      Simulator::Schedule(  Seconds(INITIALTIMEINTERVAL),
                            &obtainKPIs,
                            monitor
                            /*, flowmon*/, // FIXME Avoid the use of a global variable 'flowmon'
                            myFlowStatisticsVideoDownload,
                            6,
                            verboseLevel,
                            timeMonitorKPIs);

    // Write the values of the network KPIs (delay, etc.) to a file
    // create a string with the name of the output file
    std::ostringstream nameKPIFile;

    nameKPIFile << outputFileName
                << "_"
                << outputFileSurname
                << "_KPIs.txt";

    std::ofstream ofs;
    ofs.open ( nameKPIFile.str(), std::ofstream::out | std::ofstream::trunc); // with "trunc" Any contents that existed in the file before it is open are discarded. with "app", all output operations happen at the end of the file, appending to its existing contents

    // write the first line in the file (includes the titles of the columns)
    ofs << "timestamp [s]" << "\t"
        << "flow ID" << "\t"
        << "application" << "\t"
        << "initial destinationPort" << "\t"
        << "final destinationPort" << "\t"
        << "delay [s]" << "\t"
        << "jitter [s]" << "\t" 
        << "numRxPackets" << "\t"
        << "numlostPackets" << "\t"
        << "throughput [bps]" << "\n";

    // schedule this after the first time when statistics have been obtained
    Simulator::Schedule(  Seconds(INITIALTIMEINTERVAL + timeMonitorKPIs + 0.0001),
                          &saveKPIs,
                          nameKPIFile.str(),
                          myAllTheFlowStatistics,
                          verboseLevel,
                          timeMonitorKPIs);

    // Algorithm for dynamically adjusting aggregation
    if (aggregationDynamicAlgorithm ==1) {
      // Write the values of the AMPDU to a file
      // create a string with the name of the output file
      std::ostringstream nameAMPDUFile;

      nameAMPDUFile << outputFileName
                    << "_"
                    << outputFileSurname
                    << "_AMPDUvalues.txt";

      std::ofstream ofsAMPDU;
      ofsAMPDU.open ( nameAMPDUFile.str(), std::ofstream::out | std::ofstream::trunc); // with "trunc" Any contents that existed in the file before it is open are discarded. with "app", all output operations happen at the end of the file, appending to its existing contents

      // write the first line in the file (includes the titles of the columns)
      ofsAMPDU  << "timestamp" << "\t"
                << "ID" << "\t"
                << "type" << "\t"
                << "associated to AP\t"
                << "AMPDU set to [bytes]" << "\n";

      // prepare the parameters to call the function adjustAMPDU
      adjustAmpduParameters myparam;
      myparam.verboseLevel = verboseLevel;
      myparam.timeInterval = timeMonitorKPIs;
      myparam.latencyBudget = latencyBudget;
      myparam.maxAmpduSize = maxAmpduSize;
      myparam.mynameAMPDUFile = nameAMPDUFile.str();
      myparam.methodAdjustAmpdu = methodAdjustAmpdu;
      myparam.stepAdjustAmpdu = stepAdjustAmpdu;
      myparam.eachSTArunsAllTheApps = eachSTArunsAllTheApps;
      myparam.APsActive = APsActive;

      // Modify the AMPDU of the APs where there are VoIP flows
      Simulator::Schedule(  Seconds(INITIALTIMEINTERVAL + timeMonitorKPIs + 0.0002),
                            &adjustAMPDU,
                            myAllTheFlowStatistics,
                            myparam,
                            belowLatencyAmpduValue,
                            aboveLatencyAmpduValue,
                            number_of_APs * numberAPsSamePlace);
    }
  }


  // mobility trace
  if (saveMobility) {
    AsciiTraceHelper ascii;
    MobilityHelper::EnableAsciiAll (ascii.CreateFileStream (outputFileName + "_" + outputFileSurname + "-mobility.txt"));
    /*
    for ( uint32_t j = 0; j < number_of_STAs; ++j) {
      Ptr<const MobilityModel> poninterToMobilityModel = staNodes.Get(j)->GetObject<MobilityModel>();
      MobilityHelper::CourseChanged ( ascii, poninterToMobilityModel);
    }*/
  }


  // FIXME ***************Trial: Change the parameters of the AP (disable A-MPDU) during the simulation
  // how to change attributes: https://www.nsnam.org/docs/manual/html/attributes.html
  // https://www.nsnam.org/doxygen/regular-wifi-mac_8cc_source.html
  //Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/VI_MaxAmpduSize", UintegerValue(0));
  //Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/VO_MaxAmpduSize", UintegerValue(0));
  //Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BE_MaxAmpduSize", UintegerValue(0));
  //Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BK_MaxAmpduSize", UintegerValue(0));
  //Config::Set("/NodeList/0/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BK_MaxAmpduSize", UintegerValue(0));
  if(false) {
    for ( uint32_t j = 0 ; j < (number_of_APs * numberAPsSamePlace) ; ++j ) {
      std::cout << "scheculing AMPDU=0 in second 4.0 for node " << j << '\n';
      Simulator::Schedule (Seconds (4.0), &ModifyAmpdu, j, 0, verboseLevel );

      std::cout << "scheculing AMPDU= " << maxAmpduSize << " in second 6.0 for node " << j << '\n';
      Simulator::Schedule (Seconds (6.0), &ModifyAmpdu, j, maxAmpduSize, verboseLevel );
    }
    for ( uint32_t j = 0 ; j < number_of_STAs ; ++j ) {
      std::cout << "scheculing AMPDU=0 in second 4.0 for node " << (number_of_APs * numberAPsSamePlace )+ number_of_STAs + j << '\n';
      Simulator::Schedule (Seconds (4.0), &ModifyAmpdu, (number_of_APs * numberAPsSamePlace) + number_of_STAs + j, 0, verboseLevel );

      std::cout << "scheculing AMPDU= " << maxAmpduSize << " in second 6.0 for node " << (number_of_APs * numberAPsSamePlace) + number_of_STAs + j << '\n';
      Simulator::Schedule (Seconds (6.0), &ModifyAmpdu, (number_of_APs * numberAPsSamePlace) + number_of_STAs + j, maxAmpduSize, verboseLevel );
    }
  }
  // FIXME *** end of the trial ***


  if ( (verboseLevel > 0) && (aggregationDisableAlgorithm == 1) ) {
    Simulator::Schedule(Seconds(0.0), &List_STA_record);
    Simulator::Schedule(Seconds(0.0), &ListAPs, verboseLevel);
  }

  if (printSeconds > 0) {
    Simulator::Schedule(Seconds(0.0), &printTime, printSeconds, outputFileName, outputFileSurname);
  }

  // Start ARP trial (Failure so far)
  if (false) {
    for ( uint32_t j = 0 ; j < number_of_Servers ; ++j )
      Simulator::Schedule(Seconds(0.0), &PopulateArpCache, j + (number_of_APs * numberAPsSamePlace), serverNodes.Get(j));

    for ( uint32_t j = 0 ; j < number_of_STAs ; ++j )
      Simulator::Schedule(Seconds(0.0), &PopulateArpCache, j + (number_of_APs * numberAPsSamePlace) + number_of_Servers, staNodes.Get(j));

    for ( uint32_t j = 0 ; j < number_of_Servers ; ++j )
      Simulator::Schedule(Seconds(0.0), &infoArpCache, serverNodes.Get(j), verboseLevel);  

    for ( uint32_t j = 0 ; j < number_of_STAs ; ++j )
      Simulator::Schedule(Seconds(0.0), &infoArpCache, staNodes.Get(j), verboseLevel);

    //Simulator::Schedule(Seconds(2.0), &emtpyArpCache);

    for ( uint32_t j = number_of_APs * numberAPsSamePlace ; j < (number_of_APs * numberAPsSamePlace) + number_of_STAs ; ++j )
      Simulator::Schedule(Seconds(0.5), &PrintArpCache, staNodes.Get(j), staDevices[j].Get(0));

    for ( uint32_t j = number_of_APs * numberAPsSamePlace ; j < (number_of_APs * numberAPsSamePlace) + number_of_Servers ; ++j )
      Simulator::Schedule(Seconds(0.5), &PrintArpCache, serverNodes.Get(j), serverDevices.Get(j));


    //  Time mytime2 = Time (10.0);
    //    Config::SetDefault ("ns3::ArpL3Protocol::AliveTimeout", TimeValue(mytime2));

    // Modify the parameters of the ARP caches of the STAs and servers
    // see the parameters of the arp cache https://www.nsnam.org/doxygen/classns3_1_1_arp_cache.html#details
    // I only run this for the STAs and the servers. The APs and the hub do not have the IP stack
    for ( uint32_t j = number_of_APs * numberAPsSamePlace ; j < (number_of_APs * numberAPsSamePlace) + number_of_Servers + number_of_STAs; ++j ) {

      // I use an auxiliar string for creating the first argument of Config::Set
      std::ostringstream auxString;

      // Modify the number of retries
      //auxString << "/NodeList/" << j << "/$ns3::Ipv4L3Protocol/InterfaceList/1/ArpCache::MaxRetries";
      auxString << "/NodeList/" << j << "/$ns3::ArpL3Protocol/CacheList/*::MaxRetries";
      // std::cout << auxString.str() << '\n';
      Config::Set(auxString.str(),  UintegerValue(1));
      // clean the string
      auxString.str(std::string());

      // Modify the size of the queue for packets pending an arp reply
      auxString << "/NodeList/" << j << "/$ns3::Ipv4L3Protocol/InterfaceList/0/ArpCache::PendingQueueSize";
      // std::cout << auxString.str() << '\n';
      Config::Set(auxString.str(),  UintegerValue(1));
      // clean the string
      auxString.str(std::string());

      if (numberAPsSamePlace == 2) {
        // Modify the size of the queue for packets pending an arp reply
        auxString << "/NodeList/" << j << "/$ns3::Ipv4L3Protocol/InterfaceList/1/ArpCache::PendingQueueSize";
        // std::cout << auxString.str() << '\n';
        Config::Set(auxString.str(),  UintegerValue(1));
        // clean the string
        auxString.str(std::string());        
      }


      // Modify the AliveTimeout
      Time mytime = Time (10.0);
      //auxString << "/NodeList/" << j << "/$ns3::Ipv4L3Protocol/InterfaceList/1/ArpCache::AliveTimeout";
      //auxString << "  /NodeList/" << j << "/$ns3::ArpL3Protocol/CacheList/*::AliveTimeout";
      auxString << "/NodeList/*/$ns3::ArpL3Protocol/CacheList/*::AliveTimeout";
      Config::Set(auxString.str(), TimeValue(mytime)); // see https://www.nsnam.org/doxygen/classns3_1_1_time.html#addbf69c7aec0f3fd8c0595426d88622e
      // clean the string
      auxString.str(std::string());

      // Modify the DeadTimeout
      mytime = Time (1.0);
      //auxString << "/NodeList/" << j << "/$ns3::Ipv4L3Protocol/InterfaceList/0/ArpCache::DeadTimeout";
      auxString << "/NodeList/" << j << "/$ns3::ArpL3Protocol/CacheList/*::DeadTimeout";
      Config::Set(auxString.str(), TimeValue(mytime)); // see https://www.nsnam.org/doxygen/classns3_1_1_time.html#addbf69c7aec0f3fd8c0595426d88622e
    }

    std::ostringstream auxString;
    // Modify the AliveTimeout
    Time mytime = Time (10.0);
    //auxString << "/NodeList/" << j << "/$ns3::Ipv4L3Protocol/InterfaceList/1/ArpCache::AliveTimeout";
    //auxString << "  /NodeList/" << j << "/$ns3::ArpL3Protocol/CacheList/*::AliveTimeout";
    auxString << "/NodeList/*/ns3::ArpL3Protocol/CacheList/*::AliveTimeout";
    Config::Set(auxString.str(), TimeValue(mytime)); // see https://www.nsnam.org/doxygen/classns3_1_1_time.html#addbf69c7aec0f3fd8c0595426d88622e

    // clean the string
    auxString.str(std::string());
  }
  // End ARP trial (failure so far)


  // Trial of channel swithing of a STA
  //https://10343742895474358856.googlegroups.com/attach/1b7c2a3108d5e/channel-switch-minimal.cc?part=0.1&view=1&vt=ANaJVrGFRkTkufO3dLFsc9u1J_v2-SUCAMtR0V86nVmvXWXGwwZ06cmTSv7DrQUKMWTVMt_lxuYTsrYxgVS59WU3kBd7dkkH5hQsLE8Em0FHO4jx8NbjrPk
  if(false) {
    NetDeviceContainer devices;
    devices.Add (apWiFiDevices[0]);
    devices.Add (staDevices[0]);
    ChangeFrequencyLocal (devices, 44, wifiModel, verboseLevel); // This works since it is run before the simulation starts.

    Simulator::Schedule(Seconds(2.0), &ChangeFrequencyLocal, devices, 44, wifiModel, verboseLevel); // This does not work with SpectrumWifiPhy. But IT WORKS WITH YANS!!!

    //nearestAp (apNodes, staNodes.Get(0), verboseLevel);
    //Simulator::Schedule(Seconds(3.0), &nearestAp, apNodes, staNodes.Get(0), verboseLevel);
    // This does not work because nearestAp returns a value and you cannot schedule it
  }

  if(false) {
    // change the primary interface of the first STA to the unused channel
    NetDeviceContainer devices;
    devices.Add (staDevices[0]);
    DisableNetworkDevice (devices, wifiModel, verboseLevel);
  }

  // prepare the struct with the coverages
  coverages coverage;
  coverage.coverage_24GHz = coverage_24GHz;
  coverage.coverage_5GHz = coverage_5GHz;
  // run the load balancing algorithm
  if(algorithm_load_balancing==true) {
    Simulator::Schedule(Seconds(periodLoadBalancing),
                        &algorithmLoadBalancing,
                        periodLoadBalancing,
                        apNodes,
                        staNodes,
                        routerNode,
                        coverage,
                        //coverage_24GHz,
                        //coverage_5GHz,
                        verboseLevel);
  }

  if (verboseLevel > 0) {
    NS_LOG_INFO ("Run Simulation");
    NS_LOG_INFO ("");
  }

  Simulator::Stop (Seconds (simulationTime + INITIALTIMEINTERVAL));
  Simulator::Run ();


  //std::cout << "HELLO1 \n";
  //std::cout << "HELLO2. verboseLevel: " << verboseLevel << "\n";
  if (verboseLevel > 0)
    NS_LOG_INFO ("Simulation finished. Writing results");

  // I put this here because, if simulation stops, the values of the variables change
  // THIS IS A TOTAL MISTERY FOR ME
  //std::string outputFileName2 = outputFileName;
  //std::string outputFileSurname2 = outputFileSurname;
  //std::cout << "HELLO3\n";

  /***** Obtain per flow and aggregate statistics *****/

  // This part is inspired on https://www.nsnam.org/doxygen/wifi-hidden-terminal_8cc_source.html
  // and also on https://groups.google.com/forum/#!msg/ns-3-users/iDs9HqrQU-M/ryoVRz4M_fYJ

  monitor->CheckForLostPackets (); // Check right now for packets that appear to be lost.

  // FlowClassifier provides a method to translate raw packet data into abstract flow identifier and packet identifier parameters
  // see https://www.nsnam.org/doxygen/classns3_1_1_flow_classifier.html
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ()); // Returns a pointer to the FlowClassifier object

  // Save the results of flowmon to an XML file
  if (saveXMLFile)
    flowmon.SerializeToXmlFile (outputFileName + "_" + outputFileSurname + "_flowmonitor.xml", true, true);

  // variables used for calculating the averages
  std::string proto; 
  uint32_t this_is_the_first_flow = 1;        // used to add the titles on the first line of the output file

  uint32_t number_of_UDP_upload_flows = 0;      // this index is used for the cumulative calculation of the average
  uint32_t number_of_UDP_download_flows = 0;
  uint32_t number_of_TCP_upload_flows = 0;      // this index is used for the cumulative calculation of the average
  uint32_t number_of_TCP_download_flows = 0;
  uint32_t number_of_video_download_flows = 0;

  uint32_t total_VoIP_upload_tx_packets = 0;
  uint32_t total_VoIP_upload_rx_packets = 0;
  double total_VoIP_upload_latency = 0.0;
  double total_VoIP_upload_jitter = 0.0;

  uint32_t total_VoIP_download_tx_packets = 0;
  uint32_t total_VoIP_download_rx_packets = 0;
  double total_VoIP_download_latency = 0.0;
  double total_VoIP_download_jitter = 0.0;

  double total_TCP_upload_throughput = 0.0;
  double total_TCP_download_throughput = 0.0; // average throughput of all the download TCP flows

  double total_video_download_throughput = 0.0; // average throughput of all the download video flows



  // for each flow
  std::map< FlowId, FlowMonitor::FlowStats > stats = monitor->GetFlowStats(); 
  for (std::map< FlowId, FlowMonitor::FlowStats >::iterator flow=stats.begin(); flow!=stats.end(); flow++) 
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow->first); 


    // create a string with the name of the output file
    std::ostringstream nameFlowFile, surnameFlowFile;

    nameFlowFile  << outputFileName
                  << "_"
                  << outputFileSurname;

    surnameFlowFile << "_flow_"
                    << flow->first;


    switch(t.protocol) 
    { 
      case(6): 
        proto = "TCP"; 
      break; 

      case(17): 
        proto = "UDP"; 
      break; 

      default: 
        std::cout << "Protocol unknown" << std::endl;
        exit(1);
      break;
    }

    // create a string with the characteristics of the flow
    std::ostringstream flowID;

    flowID  << flow->first << "\t"      // identifier of the flow (a number)
            << proto << "\t"
            << t.sourceAddress << "\t"
            << t.sourcePort << "\t" 
            << t.destinationAddress << "\t"
            << t.destinationPort;

    // UDP upload flows
    if (  (t.destinationPort >= INITIALPORT_VOIP_UPLOAD ) && 
          (t.destinationPort <  INITIALPORT_VOIP_UPLOAD + numberVoIPuploadConnections )) {
      flowID << "\t VoIP upload";
    // UDP download flows
    } else if ( (t.destinationPort >= INITIALPORT_VOIP_DOWNLOAD ) && 
                (t.destinationPort <  INITIALPORT_VOIP_DOWNLOAD + numberVoIPdownloadConnections )) { 
      flowID << "\t VoIP download";
    // TCP upload flows
    } else if ( (t.destinationPort >= INITIALPORT_TCP_UPLOAD ) && 
                (t.destinationPort <  INITIALPORT_TCP_UPLOAD + numberTCPuploadConnections )) { 
      flowID << "\t TCP upload";
    // TCP download flows
    } else if ( (t.destinationPort >= INITIALPORT_TCP_DOWNLOAD ) && 
                (t.destinationPort <  INITIALPORT_TCP_DOWNLOAD + 2000 + numberTCPdownloadConnections )) {
      // I add '2000' because TCP multi download starts with ports 51000 and 52000
      flowID << "\t TCP download";
    } else if ( (t.destinationPort >= INITIALPORT_VIDEO_DOWNLOAD ) && 
                (t.destinationPort <  INITIALPORT_VIDEO_DOWNLOAD + numberVideoDownloadConnections )) { 
      flowID << "\t Video download";
    } 


    // Print the statistics of this flow to an output file and to the screen
    print_stats ( flow->second, 
                  simulationTime, 
                  generateHistograms, 
                  nameFlowFile.str(), 
                  surnameFlowFile.str(), 
                  verboseLevel, 
                  flowID.str(), 
                  this_is_the_first_flow );

    // the first time, print_stats will print a line with the title of each column
    // put the flag to 0
    if ( this_is_the_first_flow == 1 )
      this_is_the_first_flow = 0;

    // calculate and print the average of each kind of applications
    // calculate it in a cumulative way

    // UDP upload flows
    if (  (t.destinationPort >= INITIALPORT_VOIP_UPLOAD ) && 
          (t.destinationPort <  INITIALPORT_VOIP_UPLOAD + numberVoIPuploadConnections )) {

        total_VoIP_upload_tx_packets = total_VoIP_upload_tx_packets + flow->second.txPackets;
        total_VoIP_upload_rx_packets = total_VoIP_upload_rx_packets + flow->second.rxPackets;
        total_VoIP_upload_latency = total_VoIP_upload_latency + flow->second.delaySum.GetSeconds();
        total_VoIP_upload_jitter = total_VoIP_upload_jitter + flow->second.jitterSum.GetSeconds();
        number_of_UDP_upload_flows ++;

    // UDP download flows
    } else if ( (t.destinationPort >= INITIALPORT_VOIP_DOWNLOAD ) && 
                (t.destinationPort <  INITIALPORT_VOIP_DOWNLOAD + numberVoIPdownloadConnections )) { 

        total_VoIP_download_tx_packets = total_VoIP_download_tx_packets + flow->second.txPackets;
        total_VoIP_download_rx_packets = total_VoIP_download_rx_packets + flow->second.rxPackets;
        total_VoIP_download_latency = total_VoIP_download_latency + flow->second.delaySum.GetSeconds();
        total_VoIP_download_jitter = total_VoIP_download_jitter + flow->second.jitterSum.GetSeconds();
        number_of_UDP_download_flows ++;

    // TCP upload flows
    } else if ( (t.destinationPort >= INITIALPORT_TCP_UPLOAD ) && 
                (t.destinationPort <  INITIALPORT_TCP_UPLOAD + numberTCPuploadConnections )) {  

        total_TCP_upload_throughput = total_TCP_upload_throughput + ( flow->second.rxBytes * 8.0 / simulationTime );
        number_of_TCP_upload_flows ++;

    // TCP download flows
    } else if ( (t.destinationPort >= INITIALPORT_TCP_DOWNLOAD ) && 
                (t.destinationPort <  INITIALPORT_TCP_DOWNLOAD + 2000 + numberTCPdownloadConnections )) {
        // I add '2000' because TCP multi download starts with ports 51000 and 52000
        total_TCP_download_throughput = total_TCP_download_throughput + ( flow->second.rxBytes * 8.0 / simulationTime );                                          
        number_of_TCP_download_flows ++;

    // video download flows
    } else if ( (t.destinationPort >= INITIALPORT_VIDEO_DOWNLOAD ) && 
                (t.destinationPort <  INITIALPORT_VIDEO_DOWNLOAD + numberVideoDownloadConnections )) { 
      
        total_video_download_throughput = total_video_download_throughput + ( flow->second.rxBytes * 8.0 / simulationTime );                                          
        number_of_video_download_flows ++;
    } 
  }

  if (verboseLevel > 0) {
    std::cout << "\n" 
              << "The next figures are averaged per packet, not per flow:" << std::endl;

    if ( total_VoIP_upload_rx_packets > 0 ) {
      std::cout << " Average VoIP upload latency [s]:\t" << total_VoIP_upload_latency / total_VoIP_upload_rx_packets << std::endl;
      std::cout << " Average VoIP upload jitter [s]:\t" << total_VoIP_upload_jitter / total_VoIP_upload_rx_packets << std::endl;
    } else {
      std::cout << " Average VoIP upload latency [s]:\tno packets received" << std::endl;
      std::cout << " Average VoIP upload jitter [s]:\tno packets received" << std::endl;      
    }
    if ( total_VoIP_upload_tx_packets > 0 ) {
      std::cout << " Average VoIP upload loss rate:\t\t" 
                <<  1.0 - ( double(total_VoIP_upload_rx_packets) / double(total_VoIP_upload_tx_packets) )
                << std::endl;
    } else {
      std::cout << " Average VoIP upload loss rate:\t\tno packets sent" << std::endl;     
    }


    if ( total_VoIP_download_rx_packets > 0 ) {
      std::cout << " Average VoIP download latency [s]:\t" << total_VoIP_download_latency / total_VoIP_download_rx_packets 
                << std::endl;
      std::cout << " Average VoIP download jitter [s]:\t" << total_VoIP_download_jitter / total_VoIP_download_rx_packets 
                << std::endl;
    } else {
      std::cout << " Average VoIP download latency [s]:\tno packets received" << std::endl;
      std::cout << " Average VoIP download jitter [s]:\tno packets received" << std::endl;      
    }
    if ( total_VoIP_download_tx_packets > 0 ) {
      std::cout << " Average VoIP download loss rate:\t" 
                <<  1.0 - ( double(total_VoIP_download_rx_packets) / double(total_VoIP_download_tx_packets) )
                << std::endl;
    } else {
     std::cout << " Average VoIP download loss rate:\tno packets sent" << std::endl;     
    }

    std::cout << "\n" 
              << " Number TCP upload flows\t\t"
              << number_of_TCP_upload_flows << "\n"
              << " Total TCP upload throughput [bps]\t"
              << total_TCP_upload_throughput << "\n"

              << " Number TCP download flows\t\t"
              << number_of_TCP_download_flows << "\n"
              << " Total TCP download throughput [bps]\t"
              << total_TCP_download_throughput << "\n";

    std::cout << "\n" 
              << " Number video download flows\t\t"
              << number_of_video_download_flows << "\n"
              << " Total video download throughput [bps]\t"
              << total_video_download_throughput << "\n";
  }

  // save the average values to a file 
  std::ofstream ofs;
  ofs.open ( outputFileName + "_average.txt", std::ofstream::out | std::ofstream::app); // with "app", all output operations happen at the end of the file, appending to its existing contents
  ofs << outputFileSurname << "\t"
      << "Number VoIP upload flows" << "\t"
      << number_of_UDP_upload_flows << "\t";
  if ( total_VoIP_upload_rx_packets > 0 ) {
    ofs << "Average VoIP upload latency [s]" << "\t"
        << total_VoIP_upload_latency / total_VoIP_upload_rx_packets << "\t"
        << "Average VoIP upload jitter [s]" << "\t"
        << total_VoIP_upload_jitter / total_VoIP_upload_rx_packets << "\t";
  } else {
    ofs << "Average VoIP upload latency [s]" << "\t"
        << "\t"
        << "Average VoIP upload jitter [s]" << "\t"
        << "\t";
  }
  if ( total_VoIP_upload_tx_packets > 0 ) {
    ofs << "Average VoIP upload loss rate" << "\t"
        << 1.0 - ( double(total_VoIP_upload_rx_packets) / double(total_VoIP_upload_tx_packets) ) << "\t";
  } else {
    ofs << "Average VoIP upload loss rate" << "\t"
        << "\t";
  }

  ofs << "Number VoIP download flows" << "\t"
      << number_of_UDP_download_flows << "\t";
  if ( total_VoIP_download_rx_packets > 0 ) {
    ofs << "Average VoIP download latency [s]" << "\t"
        << total_VoIP_download_latency / total_VoIP_download_rx_packets << "\t"
        << "Average VoIP download jitter [s]" << "\t"
        << total_VoIP_download_jitter / total_VoIP_download_rx_packets << "\t";
  } else {
    ofs << "Average VoIP download latency [s]" << "\t"
        << "\t"
        << "Average VoIP download jitter [s]" << "\t"
        << "\t";
  }
  if ( total_VoIP_download_tx_packets > 0 ) {
    ofs << "Average VoIP download loss rate" << "\t"
        << 1.0 - ( double(total_VoIP_download_rx_packets) / double(total_VoIP_download_tx_packets) ) << "\t";
  } else {
    ofs << "Average VoIP download loss rate" << "\t"
        << "\t";
  }

  ofs << "Number TCP upload flows" << "\t"
      << number_of_TCP_upload_flows << "\t"
      << "Total TCP upload throughput [bps]" << "\t"
      << total_TCP_upload_throughput << "\t"

      << "Number TCP download flows" << "\t"
      << number_of_TCP_download_flows << "\t"
      << "Total TCP download throughput [bps]" << "\t"
      << total_TCP_download_throughput << "\t";

  ofs << "Number video download flows" << "\t"
      << number_of_video_download_flows << "\t"
      << "Total video download throughput [bps]" << "\t"
      << total_video_download_throughput << "\t";

  ofs << "Duration of the simulation [s]" << "\t"
      << simulationTime << "\n";

  ofs.close();


  // Cleanup
  if (verboseLevel > 0)
    std::cout << "Destroying the simulator\n";
  Simulator::Destroy ();

  if (verboseLevel > 0)
    NS_LOG_INFO ("Done");

  return 0;
}
