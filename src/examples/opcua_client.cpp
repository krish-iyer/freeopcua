#include <opc/ua/client/client.h>
#include <opc/ua/node.h>
#include <opc/ua/subscription.h>

#include <opc/common/logger.h>

#include <iostream>
#include <stdexcept>
#include <thread>

#include <signal.h>
#include <stdlib.h>
#include <stdio.h>


class opcua_client : public OpcUa::SubscriptionHandler
{
    public:
        opcua_client(std::string url, std::string serverInerface, uint64_t subsPeriod, std::string serverInterfaceName, uint8_t identifierMin, uint8_t subNodeNameSpaceIndex, uint8_t subSubNodeNameSpaceIndex):\
                m_client(), m_subFlag(true), m_serverInterfaceName(serverInterfaceName),\
                m_identifierMin(identifierMin), m_subNodeNameSpaceIndex(subNodeNameSpaceIndex), m_subSubNodeNameSpaceIndex(subSubNodeNameSpaceIndex){
            m_client.Connect(url);
            m_root = m_client.GetRootNode();
            m_objects = m_client.GetObjectsNode();
            m_subsObj = m_client.CreateSubscription(subsPeriod, *this);
        }

        void fetchServerInterfaceVariable(){
            for (OpcUa::Node node : m_objects.GetChildren())
            {
                auto id = node.GetId();  
                if(id.IsString()){
                    if(id.GetStringIdentifier() == m_serverInterfaceName){
                        for(OpcUa::Node subnodes : node.GetChildren()){
                            auto subid = subnodes.GetId();
                            if(subid.GetNamespaceIndex() == m_subNodeNameSpaceIndex){
                                m_serverInterfaceNode = subnodes;
                                for(OpcUa::Node subsubnodes : subnodes.GetChildren()){
                                    auto subsubid = subsubnodes.GetId();
                                    if(subsubid.GetNamespaceIndex() == m_subSubNodeNameSpaceIndex){
                                        if(subsubid.GetIntegerIdentifier() < m_identifierMin){
                                            while(!m_subFlag);
                                            auto display_name = subsubnodes.GetAttribute(OpcUa::AttributeId::DisplayName).Value;
                                            std::cout<<"subscribed to : "<<display_name.ToString()<<std::endl;
                                            m_subsObj->SubscribeDataChange(subsubnodes);
                                            while(!m_subFlag_mx.try_lock());
                                            m_subFlag=false;
                                            m_subFlag_mx.unlock();
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        void DataChange(uint32_t handle, const OpcUa::Node & node, const OpcUa::Variant & val, OpcUa::AttributeId attr) override
        {
            std::cout << "Received DataChange event, value of Node " << node << " is now: "  << val.ToString() << std::endl;
            m_subFlag_mx.lock();
            m_subFlag=true;
            m_subFlag_mx.unlock();
        }

        template <typename T>
        void writeServerInterfaceVariable(std::string serverInterfaceName, std::string var, T val){
            
            for(OpcUa::Node subsubnodes : m_serverInterfaceNode.GetChildren()){        
                auto subsubname = subsubnodes.GetAttribute(OpcUa::AttributeId::DisplayName).Value;
                auto subsubid = subsubnodes.GetId();
                if(subsubname.ToString() == serverInterfaceName){
                    if(subsubid.GetIntegerIdentifier() < 50){
                        auto display_name = subsubnodes.GetAttribute(OpcUa::AttributeId::DisplayName).Value;
                        if(display_name.ToString() == var){
                            subsubnodes.SetValue(OpcUa::Variant(val));
                        }
                    }
                }
            }

        }
    
        virtual ~opcua_client(){
            std::cout<<"disconnecting"<<std::endl;
            m_client.Disconnect();
        }

    public:
        OpcUa::UaClient m_client;
        OpcUa::Node m_root;
        OpcUa::Node m_objects;
        OpcUa::Node m_serverInterfaceNode;
        OpcUa::Subscription::SharedPtr m_subsObj;
        bool m_subFlag;
        std::mutex m_subFlag_mx;
        std::string m_serverInterfaceName;
        uint8_t m_identifierMin;
        uint8_t m_subNodeNameSpaceIndex;
        uint8_t m_subSubNodeNameSpaceIndex;
};

int main(int argc, char ** argv)
{

    opcua_client clientObj("opc.tcp://10.42.0.12:4840","Server interface_1" , 200, "ServerInterfaces", 50, 4, 4);
    
    clientObj.fetchServerInterfaceVariable();

    while(1){
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    return 0;
}

