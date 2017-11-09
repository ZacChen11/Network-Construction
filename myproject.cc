
#include "ns3/bridge-helper.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/bridge-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MyProject");


int main (int argc, char *argv[])
{
  LogComponentEnable ("MyProject", LOG_LEVEL_INFO);
  
  bool verbose = true;
  uint32_t nCsma = 4;
  uint32_t nWifi = 5;

  CommandLine cmd;
  cmd.AddValue ("nCsma", "Number of CSMA nodes/devices in the subnet", nCsma);
  cmd.AddValue ("nWifi", "Number of wifi STA devices", nWifi);
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
  
  cmd.Parse (argc,argv);

  if (verbose)
    {
      LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);
      LogComponentEnable ("OnOffApplication", LOG_LEVEL_INFO);
    }


  // Explicitly create the nodes of the subnet
  NS_LOG_INFO ("Create subnet nodes.");
  NodeContainer csmanodes;
  csmanodes.Create (nCsma);

  // Explicitly create two switches
  NS_LOG_INFO ("Create switches.");
  NodeContainer switches;
  switches.Create (2); 

  // Ex[licitly create a router
  NS_LOG_INFO ("Create a router.");
  NodeContainer router;
  router.Create (1);

  // Set up the csma channel
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (1000)));

  NS_LOG_INFO ("Connect router, subnet nodes with swtich.");
  // Create the csma links, from each subnet node to the second switch
  NetDeviceContainer csmaDevices;
  NetDeviceContainer switch2Devices;
  NodeContainer subnet(router,csmanodes);
  for (int i = 0; i < 5 ; i++)
    {
      NetDeviceContainer link = csma.Install (NodeContainer (subnet.Get (i),switches.Get(1)));
      csmaDevices.Add (link.Get (0));
      switch2Devices.Add (link.Get (1));
     }
  
  // Now, Create the bridge netdevice, which will do the packet switching.
  BridgeHelper bridge;
  bridge.Install (switches.Get(1), switch2Devices);


  NS_LOG_INFO ("Create an access point and connect it with switch.");
  // Create an access point and connect it with the first switch
  NodeContainer apnode;
  apnode.Create(1);
  NetDeviceContainer csmaDevices2;
  NetDeviceContainer switch1Devices;
  NodeContainer apnet(router,apnode );
  for (int i = 0; i < 2 ; i++)
    {
      NetDeviceContainer link = csma.Install (NodeContainer (apnet.Get (i),switches.Get(0)));
      csmaDevices2.Add (link.Get (0));
      switch1Devices.Add (link.Get (1));
     }
  bridge.Install (switches.Get(0),switch1Devices);

  NS_LOG_INFO ("Create a wifi mobiles and connect with access point.");
  // Create wifi mobiles and connect them with access point
  NodeContainer wifiMobileNodes;
  wifiMobileNodes.Create (nWifi);
  NodeContainer wifiApNode = apnode.Get(0);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi = WifiHelper::Default ();
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();

  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer wifiMobileDevices;
  wifiMobileDevices = wifi.Install (phy, mac, wifiMobileNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, wifiApNode);

  NS_LOG_INFO ("Set the mobility of the Wifi network.");
  // Set the mobility of the Wifi network
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (nWifi+10),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (-5000, 5000, -5000, 5000)));
  mobility.Install (wifiMobileNodes);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);

  
  // Add internet stack to all the end hosts
  InternetStackHelper stack;
  stack.Install (csmanodes);
  stack.Install (router);
  stack.Install (apnode );
  stack.Install (wifiMobileNodes);

  // Assign IP address
  Ipv4AddressHelper address;
 
  address.SetBase ("10.1.1.0", "255.255.255.0"); 
  address.Assign (csmaDevices2);
 
  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces;
  csmaInterfaces = address.Assign (csmaDevices);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  address.Assign (wifiMobileDevices);
  address.Assign (apDevices);

  // Set up the routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables (); 
    
  //Create a packet sink to receive these packets
  uint16_t port = 50;
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (csmaInterfaces.GetAddress (4), port));
  ApplicationContainer sinkApp = sinkHelper.Install (csmanodes.Get(3));
  sinkApp.Start (Seconds (1.0));
  sinkApp.Stop (Seconds (360.0));

  // Create the OnOff applications to send TCP to the server 
  OnOffHelper clientHelper ("ns3::TcpSocketFactory",Address(InetSocketAddress (csmaInterfaces.GetAddress (4), port)));
  clientHelper.SetAttribute ("PacketSize",UintegerValue(1024));
  clientHelper.SetAttribute("MaxBytes",UintegerValue(102400));
 
  ApplicationContainer clientApps = clientHelper.Install (wifiMobileNodes.Get (nWifi-1));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (360.0));

  FlowMonitorHelper flowhelper;
  Ptr<FlowMonitor> monitor = flowhelper.InstallAll();


  NS_LOG_INFO ("Start simulation.");
  Simulator::Stop (Seconds (360.0));

  // Configure tracing of all enqueue, dequeue, and NetDevice receive events.
  // It will also start a promiscuous mode trace on the Wifi network, and will also start a promiscuous trace on the CSMA network. 
  AsciiTraceHelper ascii;
  csma.EnableAsciiAll (ascii.CreateFileStream ("myproject.tr"));
  phy.EnablePcap ("myproject", apDevices.Get (0));
  csma.EnablePcap ("myproject", csmaDevices.Get (0), true);

  Simulator::Run ();

  monitor->SerializeToXmlFile("myproject.xml", true, true);
  Simulator::Destroy ();
  return 0;
}