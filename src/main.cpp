#include "network/Server.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
int main(int argc,char** argv){
 try {
    std::string address="127.0.0.1";std::uint16_t port=27345;
    cardgame::SchedulerConfig config;
    for(int i=1;i<argc;++i){
        const std::string option=argv[i];
        if(option=="--help"){
            std::cout<<"cardgame_server [--bind IPv4] [--port number] [--seed number] "
                       "[--shop-time seconds] [--turn-time seconds]\n";
            return 0;
        }
        if(i+1>=argc)throw std::invalid_argument("Missing option value");
        const std::string value=argv[++i];
        if(option=="--bind"){address=value;continue;}
        std::size_t used=0;const auto number=std::stoull(value,&used);
        if(used!=value.size()||value.front()=='-')throw std::invalid_argument("Invalid option value");
        if(option=="--port"){
            if(number>65535)throw std::invalid_argument("Port out of range");
            port=static_cast<std::uint16_t>(number);
        }else if(option=="--seed"){
            if(number>std::numeric_limits<std::uint32_t>::max())throw std::invalid_argument("Seed out of range");
            config.seed=static_cast<std::uint32_t>(number);
        }else if(option=="--shop-time"||option=="--turn-time"){
            if(!number||number>3600)throw std::invalid_argument("Timeout out of range");
            const auto duration=std::chrono::seconds(static_cast<int>(number));
            if(option=="--shop-time")config.shopTime=duration;else config.turnTime=config.choiceTime=duration;
        }else throw std::invalid_argument("Unknown option");
    }
    cardgame::network::Server server(address,port,config);return server.run();
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
