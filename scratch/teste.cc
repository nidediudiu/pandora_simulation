    /* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
    /* * Copyright (c) 2018
    *
    * This program is free software; you can redistribute it and/or modify
    * it under the terms of the GNU General Public License version 2 as
    * published by the Free Software Foundation;
    *
    * This program is distributed in the hope that it will be useful,
    * but WITHOUT ANY WARRANTY; without even the implied warranty of
    * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    * GNU General Public License for more details.
    *
    * You should have received a copy of the GNU General Public License
    * along with this program; if not, write to the Free Software
    * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
    *
    * Author: Lucas Pacheco  <lucassidpacheco@gmail.com>
    */

    #include "ns3/core-module.h"

    #include "ns3/network-module.h"

    #include "ns3/internet-module.h"

    #include "ns3/mobility-module.h"

    #include "ns3/applications-module.h"

    #include "ns3/config-store-module.h"

    #include "ns3/point-to-point-module.h"

    #include "ns3/ipv4-global-routing-helper.h"

    #include "ns3/ipv4-address.h"

    #include "ns3/mobility-model.h"
    // NetAnim & Evalvid
    #include "ns3/netanim-module.h"
    // Pacotes LTE
    #include "ns3/point-to-point-helper.h"

    #include "ns3/lte-helper.h"

    #include "ns3/epc-helper.h"

    #include "ns3/lte-module.h"

    #include "ns3/string.h"

    #include "ns3/double.h"

    #include <ns3/boolean.h>

    #include <ns3/enum.h>

    #include "ns3/flow-monitor-helper.h"

    #include "ns3/ipv4-flow-classifier.h"

    #include <fstream>

    #include <iostream>

    #include <sys/stat.h>

    #define SIMULATION_TIME_FORMAT(s) Seconds(s)

    using namespace ns3;

    double TxRate = 0;
    bool useCbr = false;
    bool verbose = false;

    const int number_of_ue = 10;

    const uint16_t number_of_mc = 10;
    const uint16_t number_of_sc = 0;
    const uint16_t number_of_hs = 0;

    const int node_enb = number_of_mc + number_of_sc + number_of_hs;

    uint16_t n_cbr = useCbr ? number_of_mc + number_of_sc : 0;

    int mcTxPower = 46;
    int scTxPower = 23;
    int hsTxPower = 15;

    double simTime = 1000;

    unsigned int handNumber = 0;

    bool randomCellAlloc = true;
    bool rowTopology = false;

    //coeficiente da média exponencial
    unsigned int exp_mean_window = 3;
    double qosLastValue = 0;
    double qoeLastValue = 0;

    NS_LOG_COMPONENT_DEFINE("v2x_3gpp");

    /*------------------------- NOTIFICAÇÕES DE HANDOVER ----------------------*/
    void NotifyConnectionEstablishedUe(std::string context,
    uint64_t imsi,
    uint16_t cellid,
    uint16_t rnti) {
    NS_LOG_DEBUG(Simulator::Now().GetSeconds() <<
        " " << context << " UE IMSI " << imsi <<
        ": connected to CellId " << cellid << " with RNTI " << rnti);

    std::stringstream temp_cell_dir;
    std::stringstream ueId;
    temp_cell_dir << "./v2x_temp/" << cellid;
    ueId << temp_cell_dir.str() << "/" << rnti;
    if (mkdir(temp_cell_dir.str().c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {}
    std::ofstream outfile(ueId.str().c_str());
    outfile << imsi << std::endl;
    outfile.close();
    }

    void NotifyHandoverStartUe(std::string context,
    uint64_t imsi,
    uint16_t cellid,
    uint16_t rnti,
    uint16_t targetCellId) {
    NS_LOG_DEBUG(Simulator::Now().GetSeconds() <<
        " " << context << " UE IMSI " << imsi <<
        ": previously connected to CellId " << cellid << " with RNTI " <<
        rnti << ", doing handover to CellId " << targetCellId);

    std::stringstream ueId;
    ueId << "./v2x_temp/" << cellid << "/" << rnti;
    remove(ueId.str().c_str());

    ++handNumber;
    }

    void NotifyHandoverEndOkUe(std::string context,
    uint64_t imsi,
    uint16_t cellid,
    uint16_t rnti) {
    NS_LOG_DEBUG(Simulator::Now().GetSeconds() <<
        " " << context << " UE IMSI " << imsi <<
        ": successful handover to CellId " << cellid << " with RNTI " <<
        rnti);

    std::stringstream target_cell_dir;
    std::stringstream newUeId;
    target_cell_dir << "./v2x_temp/" << cellid;
    newUeId << target_cell_dir.str() << "/" << rnti;
    if (mkdir(target_cell_dir.str().c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {}
    std::ofstream outfile(newUeId.str().c_str());
    outfile << imsi << std::endl;
    outfile.close();
    }

    void NotifyConnectionEstablishedEnb(std::string context,
    uint64_t imsi,
    uint16_t cellid,
    uint16_t rnti) {
    NS_LOG_DEBUG(Simulator::Now().GetSeconds() <<
        " " << context << " eNB CellId " << cellid <<
        ": successful connection of UE with IMSI " << imsi << " RNTI " <<
        rnti);
    }

    void NotifyHandoverStartEnb(std::string context,
    uint64_t imsi,
    uint16_t cellid,
    uint16_t rnti,
    uint16_t targetCellId) {
    NS_LOG_DEBUG(Simulator::Now().GetSeconds() <<
        " " << context << " eNB CellId " << cellid <<
        ": start handover of UE with IMSI " << imsi << " RNTI " <<
        rnti << " to CellId " << targetCellId);
    }

    void NotifyHandoverEndOkEnb(std::string context,
    uint64_t imsi,
    uint16_t cellid,
    uint16_t rnti) {
    NS_LOG_DEBUG(Simulator::Now().GetSeconds() <<
        " " << context << " eNB CellId " << cellid <<
        ": completed handover of UE with IMSI " << imsi << " RNTI " <<
        rnti);
    }

    void ArrayPositionAllocator(Ptr < ListPositionAllocator > HpnPosition) {
    std::ifstream pos_file("CellsDataset");
    int cellId;
    double x_coord, y_coord;
    char a;

    while (pos_file >> cellId >> x_coord >> y_coord >> a) {
        NS_LOG_INFO("Adding cell " << cellId <<
            " to coordinates x:" << x_coord << " y:" << y_coord);
        HpnPosition -> Add(Vector(x_coord, y_coord, 30));
    }
    }

    void requestService(Ptr <Node> ueNode, Ptr <Node> MECNode, Ipv4Address ueAddress) {
        Time interPacketInterval = MilliSeconds(100);
        // Install and start applications on UEs and remote host
        uint16_t dlPort = 1100;
        uint16_t ulPort = 2000;
        ApplicationContainer clientApps;
        ApplicationContainer serverApps;
            PacketSinkHelper dlPacketSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), dlPort));
            serverApps.Add (dlPacketSinkHelper.Install (ueNode));

            UdpClientHelper dlClient (ueAddress, dlPort);
            dlClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
            dlClient.SetAttribute ("MaxPackets", UintegerValue (1000000));
            clientApps.Add (dlClient.Install (MECNode));

            ++ulPort;
            PacketSinkHelper ulPacketSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), ulPort));
            serverApps.Add (ulPacketSinkHelper.Install (MECNode));

            Ptr < Ipv4 > ipv4 = MECNode -> GetObject < Ipv4 > ();
            Ipv4InterfaceAddress iaddr = ipv4 -> GetAddress(1, 0);
            Ipv4Address addri = iaddr.GetLocal();
            NS_LOG_UNCOND(addri);
            UdpClientHelper ulClient (addri, ulPort);
            ulClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
            ulClient.SetAttribute ("MaxPackets", UintegerValue (1000000));
            clientApps.Add (ulClient.Install (ueNode));


        serverApps.Start (MilliSeconds (500));
        clientApps.Start (MilliSeconds (500));
    }

    // todo
    void migrateService() {}

    int
    main (int argc, char *argv[])
    {
    Time simTime = MilliSeconds (1900);
    Time interPacketInterval = MilliSeconds (100);

    ConfigStore inputConfig;
    inputConfig.ConfigureDefaults ();

    // Bandwidth of Dl and Ul in Resource Blocks
    Config::SetDefault("ns3::LteEnbNetDevice::DlBandwidth", UintegerValue(25));
    Config::SetDefault("ns3::LteEnbNetDevice::UlBandwidth", UintegerValue(25));

    // Modo de transmissão (SISO [0], MIMO [1])
    Config::SetDefault("ns3::LteEnbRrc::DefaultTransmissionMode",
        UintegerValue(1));

    Config::SetDefault("ns3::LteEnbRrc::SrsPeriodicity", UintegerValue(320));

    // ------------------------- lte and epc helper ----------------------
    Ptr < LteHelper > lteHelper = CreateObject < LteHelper > ();
    Ptr < EpcHelper > epcHelper;
    epcHelper = CreateObject < NoBackhaulEpcHelper > ();
    lteHelper -> SetEpcHelper(epcHelper);

    // LTE configuration
    lteHelper -> SetSchedulerType("ns3::PssFfMacScheduler");
    lteHelper -> SetSchedulerAttribute("nMux", UintegerValue(1)); // the maximum number of UE selected by TD scheduler
    lteHelper -> SetSchedulerAttribute("PssFdSchedulerType", StringValue("CoItA")); // PF scheduler type in PSS
    lteHelper -> SetHandoverAlgorithmType("ns3::A2A4RsrqHandoverAlgorithm");
    lteHelper -> EnableTraces();

    // Propagation Parameters
    lteHelper -> SetEnbDeviceAttribute("DlEarfcn", UintegerValue(100));
    lteHelper -> SetEnbDeviceAttribute("UlEarfcn", UintegerValue(18100));
    lteHelper -> SetAttribute("PathlossModel",
        StringValue("ns3::NakagamiPropagationLossModel"));

    //-------------Antenna Parameters
    lteHelper -> SetEnbAntennaModelType("ns3::CosineAntennaModel");
    lteHelper -> SetEnbAntennaModelAttribute("Orientation", DoubleValue(0));
    lteHelper -> SetEnbAntennaModelAttribute("Beamwidth", DoubleValue(60));
    lteHelper -> SetEnbAntennaModelAttribute("MaxGain", DoubleValue(0.0));

    Config::SetDefault("ns3::LteHelper::UseIdealRrc", BooleanValue(false));

    Ptr<Node> pgw = epcHelper->GetPgwNode ();

    NodeContainer ueNodes;
    NodeContainer enbNodes;
    enbNodes.Create (number_of_mc);
    ueNodes.Create (number_of_ue);

    // Create a single RemoteHost
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create (1);
    Ptr<Node> remoteHost = remoteHostContainer.Get (0);
    InternetStackHelper internet;
    internet.Install (remoteHostContainer);

    // Create the Internet
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
    p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
    p2ph.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (10)));
    NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
    // interface 0 is localhost, 1 is the p2p device
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);
    NS_LOG_UNCOND(remoteHostAddr);

    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
    remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

    NodeContainer MecContainer;
    MecContainer.Create (number_of_mc);
    internet.Install (MecContainer);
    NetDeviceContainer internetDevices2;
    ipv4h.SetBase ("3.0.0.0", "255.0.0.0");

    for (uint16_t i = 0; i < enbNodes.GetN (); ++i)
    {
        p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Mb/s")));
        p2ph.SetDeviceAttribute("Mtu", UintegerValue(1200));
        p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.01)));
        internetDevices2.Add(p2ph.Install(pgw, MecContainer.Get(i)));
        internetIpIfaces.Add(ipv4h.Assign(internetDevices2));
        Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (MecContainer.Get(i)->GetObject<Ipv4> ());
        remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);
    }

    /*-----------------POSITION AND MOBILITY----------------------------------*/
    Ptr < ListPositionAllocator > HpnPosition = CreateObject < ListPositionAllocator > ();
    ArrayPositionAllocator(HpnPosition);

    MobilityHelper mobilityEnb;
    mobilityEnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityEnb.SetPositionAllocator(HpnPosition);
    mobilityEnb.Install(enbNodes);

    // remote host mobility (constant)
    MobilityHelper remoteHostMobility;
    remoteHostMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    remoteHostMobility.Install(pgw);

    // User Devices mobility
    Ns2MobilityHelper ue_mobil = Ns2MobilityHelper("mobil/mobility_25_users.tcl");
    MobilityHelper ueMobility;
    MobilityHelper enbMobility;

    ue_mobil.Install(ueNodes.Begin(), ueNodes.End());

    // SGW node
    Ptr<Node> sgw = epcHelper->GetSgwNode ();

    // Install Mobility Model for SGW
    Ptr<ListPositionAllocator> positionAlloc2 = CreateObject<ListPositionAllocator> ();
    positionAlloc2->Add (Vector (0.0,  50.0, 0.0));
    MobilityHelper mobility2;
    mobility2.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility2.SetPositionAllocator (positionAlloc2);
    mobility2.Install (sgw);

    // Install LTE Devices to the nodes
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);
    Ipv4Address addr = remoteHostAddr;
      Ipv4AddressHelper s1uIpv4AddressHelper;

      // Create networks of the S1 interfaces
      s1uIpv4AddressHelper.SetBase ("10.0.0.0", "255.255.255.252");

      for (uint16_t i = 0; i < number_of_mc; ++i)
        {
          Ptr<Node> enb = enbNodes.Get (i);

          // Create a point to point link between the eNB and the SGW with
          // the corresponding new NetDevices on each side
          PointToPointHelper p2ph;
          DataRate s1uLinkDataRate = DataRate ("10Gb/s");
          uint16_t s1uLinkMtu = 2000;
          Time s1uLinkDelay = Time (0);
          p2ph.SetDeviceAttribute ("DataRate", DataRateValue (s1uLinkDataRate));
          p2ph.SetDeviceAttribute ("Mtu", UintegerValue (s1uLinkMtu));
          p2ph.SetChannelAttribute ("Delay", TimeValue (s1uLinkDelay));
          NetDeviceContainer sgwEnbDevices = p2ph.Install (sgw, enb);

          Ipv4InterfaceContainer sgwEnbIpIfaces = s1uIpv4AddressHelper.Assign (sgwEnbDevices);
          s1uIpv4AddressHelper.NewNetwork ();

          Ipv4Address sgwS1uAddress = sgwEnbIpIfaces.GetAddress (0);
          Ipv4Address enbS1uAddress = sgwEnbIpIfaces.GetAddress (1);
          addr = enbS1uAddress;

          // Create S1 interface between the SGW and the eNB
          epcHelper->AddS1Interface (enb, enbS1uAddress, sgwS1uAddress);
        }

    // Install the IP stack on the UEs
    internet.Install (ueNodes);
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));
    // Assign IP address to UEs, and install applications
    for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      Ptr<Node> ueNode = ueNodes.Get (u);
      // Set the default gateway for the UE
      Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
      ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

    lteHelper->Attach(ueLteDevs);

    requestService(ueNodes.Get(0), MecContainer.Get(0), ueIpIface.GetAddress(0));


    // // Install and start applications on UEs and remote host
    // uint16_t dlPort = 1100;
    // uint16_t ulPort = 2000;
    // ApplicationContainer clientApps;
    // ApplicationContainer serverApps;
    // for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    // {
    //     PacketSinkHelper dlPacketSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), dlPort));
    //     serverApps.Add (dlPacketSinkHelper.Install (ueNodes.Get (u)));

    //     UdpClientHelper dlClient (ueIpIface.GetAddress (u), dlPort);
    //     dlClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
    //     dlClient.SetAttribute ("MaxPackets", UintegerValue (1000000));
    //     clientApps.Add (dlClient.Install (MecContainer.Get (u)));

    //     ++ulPort;
    //     PacketSinkHelper ulPacketSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), ulPort));
    //     serverApps.Add (ulPacketSinkHelper.Install (MecContainer.Get(u)));

    //     Ptr < Ipv4 > ipv4 = MecContainer.Get(u) -> GetObject < Ipv4 > ();
    //     Ipv4InterfaceAddress iaddr = ipv4 -> GetAddress(1, 0);
    //     Ipv4Address addri = iaddr.GetLocal();
    //     NS_LOG_UNCOND(addri);
    //     UdpClientHelper ulClient (addri, ulPort);
    //     ulClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
    //     ulClient.SetAttribute ("MaxPackets", UintegerValue (1000000));
    //     clientApps.Add (ulClient.Install (ueNodes.Get(u)));
    // }

    // serverApps.Start (MilliSeconds (500));
    // clientApps.Start (MilliSeconds (500));

    AnimationInterface anim("pandora_anim.xml");

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();
    // Uncomment to enable PCAP tracing
    //p2ph.EnablePcapAll("lena-simple-epc-backhaul");

    Simulator::Stop (simTime);
    Simulator::Run ();

    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      // first 2 FlowIds are for ECHO apps, we don't want to display them
      //
      // Duration for throughput measurement is 9.0 seconds, since
      //   StartTime of the OnOffApplication is at about "second 1"
      // and
      //   Simulator::Stops at "second 10".
      if (i->first > 2)
        {
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
            std::cout << "Flow " << i->first - 2 << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
            std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
            std::cout << "  TxOffered:  " << i->second.txBytes * 8.0 / 9.0 / 1000 / 1000  << " Mbps\n";
            std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
            std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
            std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / 9.0 / 1000 / 1000  << " Mbps\n";
        }
    }

    Simulator::Destroy ();
    return 0;
    }
