#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"  // Inclusão do módulo NetAnim para gerar o XML de animação
#include <iostream>

using namespace ns3;

// Define um componente de log para essa simulação
NS_LOG_COMPONENT_DEFINE("Wifi6OFDMAComparison");

// Função que configura e executa a simulação
void RunSimulation(bool enableOfdma, const std::string &scenarioName)
{
    uint32_t nWifi = 20; // Número de estações WiFi (STA)

    // Criação dos nós:
    // wifiStaNodes: nós que atuam como estações (clientes)
    // wifiApNode: nó que atua como Access Point (AP)
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nWifi);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Configuração do PHY e do canal usando os auxiliares Yans (Yet Another Network Simulator)
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Configura o padrão WiFi para 802.11ax (WiFi 6)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211ax);

    // Configura a utilização (ou não) do recurso OFDMA
    phy.EnableOfdma(enableOfdma);
    NS_LOG_INFO("OFDMA " << (enableOfdma ? "Enabled" : "Disabled") << " for " << scenarioName);

    // Configuração do MAC (Medium Access Control)
    WifiMacHelper mac;
    // Define um SSID único para essa simulação, concatenando "ns3-wifi6" com o nome do cenário
    Ssid ssid = Ssid("ns3-wifi6" + scenarioName);

    // Configura os nós STA (estações)
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Configura o nó AP (Access Point)
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // Configuração da mobilidade:
    // Usa um alocador de posições em grade para posicionar as estações
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));
    // Modelo de mobilidade: posição fixa (nós não se movem)
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);

    // Aplica o modelo de mobilidade ao nó AP
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);

    // Instala a pilha de protocolos de Internet (TCP/IP) em todos os nós
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    // Atribuição de endereços IP aos dispositivos
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // Instalação de aplicativos:
    // Configura um servidor de eco UDP no AP que responde aos pacotes recebidos na porta 9
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Configura um cliente de eco UDP nas estações para enviar pacotes ao AP
    UdpEchoClientHelper echoClient(apInterface.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(wifiStaNodes);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Habilita o FlowMonitor para coletar estatísticas dos fluxos de dados
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> monitor = flowHelper.InstallAll();

    // **Geração do arquivo de animação para o NetAnim:**
    // Cria um objeto AnimationInterface que grava as posições dos nós e eventos durante a simulação.
    // Será gerado um arquivo XML com nome único para cada cenário.
    AnimationInterface anim(("netanim-results-" + scenarioName + ".xml").c_str());

    // Define o tempo de parada da simulação e executa a simulação
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Após a simulação, verifica e imprime as estatísticas dos fluxos monitorados
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << scenarioName << " Flow " << flow.first 
                  << " Source: " << t.sourceAddress
                  << " Destination: " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets: " << flow.second.txPackets 
                  << " Rx Packets: " << flow.second.rxPackets << std::endl;
        std::cout << "  Throughput: " 
                  << (flow.second.rxBytes * 8.0 / 9.0 / 1024) 
                  << " Kbps" << std::endl;
    }

    // Finaliza a simulação e libera os recursos
    Simulator::Destroy();
}

int main(int argc, char *argv[])
{
    // Habilita a saída de log para o componente definido
    LogComponentEnable("Wifi6OFDMAComparison", LOG_LEVEL_INFO);

    std::cout << "Running simulation with OFDMA enabled...\n";
    RunSimulation(true, "OFDMA_ON");

    std::cout << "Running simulation with OFDMA disabled...\n";
    RunSimulation(false, "OFDMA_OFF");

    return 0;
}
