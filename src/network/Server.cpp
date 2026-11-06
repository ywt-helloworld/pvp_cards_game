#include "network/Server.hpp"
#include "network/Protocol.hpp"
#include "Common/network_types.hpp"
#include <algorithm>
#include <cerrno>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#ifdef _WIN32
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>
#include <unistd.h>
#endif
namespace cardgame::network {
namespace {
void close_socket(NativeSocket socket) noexcept {
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}
bool would_block() {
#ifdef _WIN32
    return WSAGetLastError()==WSAEWOULDBLOCK;
#else
    return errno==EAGAIN||errno==EWOULDBLOCK;
#endif
}
void nonblocking(NativeSocket socket) {
#ifdef _WIN32
    unsigned long value=1;
    if(ioctlsocket(socket,FIONBIO,&value)!=0)throw std::runtime_error("Cannot configure socket");
#else
    const auto flags=fcntl(socket,F_GETFL,0);
    if(flags<0||fcntl(socket,F_SETFL,flags|O_NONBLOCK)<0)throw std::runtime_error("Cannot configure socket");
#endif
}
struct Runtime {
    Runtime(){
#ifdef _WIN32
        WSADATA data{};
        if(WSAStartup(MAKEWORD(2,2),&data)!=0)throw std::runtime_error("WSAStartup failed");
#endif
    }
    ~Runtime(){
#ifdef _WIN32
        WSACleanup();
#endif
    }
};
std::string token() {
    std::random_device random;
    std::ostringstream out;
    for(int i=0;i<4;++i)out<<std::hex<<std::setw(8)<<std::setfill('0')<<random();
    return out.str();
}
}
struct Server::Impl {
    struct Session {
        std::string name,token;
        NativeSocket socket=INVALID_NATIVE_SOCKET;
        RequestId lastRequest=0;
    };
    struct Peer {
        std::string input,output;
        std::optional<Players> seat;
        bool drop=false;
    };
    Runtime runtime;
    std::string address;
    std::uint16_t port;
    SchedulerConfig config;
    std::atomic<bool> running{true};
    NativeSocket listener=INVALID_NATIVE_SOCKET;
    std::map<NativeSocket,Peer> peers;
    std::array<Session,4> sessions;
    std::unique_ptr<Scheduler> game;

    Impl(std::string a,std::uint16_t p,SchedulerConfig c):address(std::move(a)),port(p),config(c){}
    ~Impl(){
        for(const auto& peer:peers)close_socket(peer.first);
        if(listener!=INVALID_NATIVE_SOCKET)close_socket(listener);
    }
    void send(NativeSocket socket,std::string message) {
        const auto it=peers.find(socket);if(it==peers.end())return;
        auto& peer=it->second;
        if(peer.output.size()+message.size()>4*1024*1024){peer.drop=true;return;}
        peer.output+=message;peer.output+='\n';
    }
    void broadcast() {
        for(unsigned i=0;i<4;++i)if(sessions[i].socket!=INVALID_NATIVE_SOCKET) {
            if(game)send(sessions[i].socket,state_message(*game,static_cast<Players>(i)));
            else {
                const auto count=std::count_if(sessions.begin(),sessions.end(),[](const auto& s){return !s.name.empty();});
                send(sessions[i].socket,"{\"type\":\"waiting\",\"players\":"+std::to_string(count)+"}");
            }
        }
    }
    void close_peer(NativeSocket socket) {
        const auto it=peers.find(socket);if(it==peers.end())return;
        const auto seat=it->second.seat;
        close_socket(socket);peers.erase(it);
        if(seat) {
            auto& session=sessions[static_cast<std::size_t>(*seat)];
            if(session.socket==socket) {
                session.socket=INVALID_NATIVE_SOCKET;
                if(game)game->disconnected(*seat);else session={};
            }
        }
        broadcast();
    }
    void line(NativeSocket socket,const std::string& text) {
        auto it=peers.find(socket);if(it==peers.end())return;
        try {
            if(!it->second.seat) {
                if(text.rfind("JOIN ",0)==0) {
                    if(game)throw std::logic_error("Room is full; use RECONNECT");
                    const auto name=text.substr(5);
                    if(name.empty()||name.size()>128)throw std::invalid_argument("Invalid name");
                    std::size_t seat=0;while(seat<4&&!sessions[seat].name.empty())++seat;
                    if(seat==4)throw std::logic_error("Room is full");
                    auto& session=sessions[seat];session.name=name;session.token=token();session.socket=socket;
                    it->second.seat=static_cast<Players>(seat);
                    send(socket,"{\"type\":\"welcome\",\"seat\":"+std::to_string(seat)+",\"token\":"+json_string(session.token)+"}");
                    if(std::all_of(sessions.begin(),sessions.end(),[](const auto& s){return !s.name.empty();}))
                        game=std::make_unique<Scheduler>(std::array<Player,4>{
                            Player(Players::P1,sessions[0].name),Player(Players::P2,sessions[1].name),
                            Player(Players::P3,sessions[2].name),Player(Players::P4,sessions[3].name)},config);
                    broadcast();return;
                }
                if(text.rfind("RECONNECT ",0)==0&&game) {
                    const auto key=text.substr(10);
                    auto found=std::find_if(sessions.begin(),sessions.end(),[&](const auto& s){return s.token==key;});
                    if(found==sessions.end())throw std::invalid_argument("Unknown session");
                    const auto seat=static_cast<Players>(found-sessions.begin());
                    const auto previous=found->socket;found->socket=INVALID_NATIVE_SOCKET;
                    if(previous!=INVALID_NATIVE_SOCKET)close_peer(previous);
                    found->socket=socket;peers.at(socket).seat=seat;game->reconnected(seat);
                    send(socket,"{\"type\":\"reconnected\",\"seat\":"+std::to_string(static_cast<unsigned>(seat))+
                        ",\"lastRequest\":"+std::to_string(found->lastRequest)+"}");
                    broadcast();return;
                }
                throw std::invalid_argument("Send JOIN <name> or RECONNECT <token>");
            }
            if(!game)throw std::logic_error("Waiting for four players");
            std::istringstream input(text);std::string idToken;
            if(!(input>>idToken)||idToken.empty()||idToken.front()=='-')throw std::invalid_argument("Missing request ID");
            std::size_t used=0;const auto id=std::stoull(idToken,&used);
            if(!id||used!=idToken.size())throw std::invalid_argument("Invalid request ID");
            const auto seat=*it->second.seat;auto& session=sessions[static_cast<std::size_t>(seat)];
            if(id<=session.lastRequest) {
                send(socket,"{\"type\":\"duplicate\",\"request\":"+std::to_string(id)+"}");
                send(socket,state_message(*game,seat));return;
            }
            session.lastRequest=id;
            std::string command;std::getline(input>>std::ws,command);
            auto event=parse_request(seat,command);
            auto report=game->submit(std::move(event),id);
            if(report.failures.empty())
                send(socket,"{\"type\":\"ack\",\"request\":"+std::to_string(id)+"}");
            else {
                std::string message="Card operation failed";
                try{std::rethrow_exception(report.failures.front().error);}
                catch(const std::exception& e){message=e.what();}
                catch(...){}
                send(socket,"{\"type\":\"error\",\"request\":"+std::to_string(id)+",\"message\":"+json_string(message)+"}");
            }
            broadcast();
        } catch(const std::exception& e) {
            send(socket,"{\"type\":\"error\",\"message\":"+json_string(e.what())+"}");
        }
    }
    int run() {
        listener=::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
        if(listener==INVALID_NATIVE_SOCKET)throw std::runtime_error("Cannot create listener");
        int reuse=1;
        setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,reinterpret_cast<const char*>(&reuse),sizeof(reuse));
        sockaddr_in endpoint{};endpoint.sin_family=AF_INET;endpoint.sin_port=htons(port);
        if(inet_pton(AF_INET,address.c_str(),&endpoint.sin_addr)!=1)
            throw std::invalid_argument("Expected an IPv4 bind address");
        if(::bind(listener,reinterpret_cast<sockaddr*>(&endpoint),sizeof(endpoint))!=0||
            ::listen(listener,16)!=0)throw std::runtime_error("Cannot bind/listen");
        nonblocking(listener);
#ifdef _WIN32
        int length=sizeof(endpoint);
#else
        socklen_t length=sizeof(endpoint);
#endif
        getsockname(listener,reinterpret_cast<sockaddr*>(&endpoint),&length);port=ntohs(endpoint.sin_port);
        std::cout<<"LISTENING "<<address<<':'<<port<<std::endl;
        while(running.load()) {
            fd_set readSet,writeSet;FD_ZERO(&readSet);FD_ZERO(&writeSet);FD_SET(listener,&readSet);
            NativeSocket highest=listener;
            for(const auto& entry:peers) {
                FD_SET(entry.first,&readSet);
                if(!entry.second.output.empty())FD_SET(entry.first,&writeSet);
                highest=std::max(highest,entry.first);
            }
            timeval wait{0,100000};
#ifdef _WIN32
            const int ready=select(0,&readSet,&writeSet,nullptr,&wait);
#else
            const int ready=select(highest+1,&readSet,&writeSet,nullptr,&wait);
#endif
            if(ready<0)throw std::runtime_error("Socket select failed");
            if(FD_ISSET(listener,&readSet)) {
                const auto socket=::accept(listener,nullptr,nullptr);
                if(socket!=INVALID_NATIVE_SOCKET) {
                    if(peers.size()>=16)close_socket(socket);
                    else {nonblocking(socket);peers.emplace(socket,Peer{});send(socket,"{\"type\":\"hello\",\"protocol\":1}");}
                }
            }
            std::vector<NativeSocket> sockets;for(const auto& p:peers)sockets.push_back(p.first);
            for(auto socket:sockets) {
                auto it=peers.find(socket);if(it==peers.end())continue;
                if(FD_ISSET(socket,&readSet)) {
                    char buffer[4096];
                    const int got=static_cast<int>(::recv(socket,buffer,sizeof(buffer),0));
                    if(got==0||(got<0&&!would_block())){close_peer(socket);continue;}
                    if(got>0) {
                        it->second.input.append(buffer,static_cast<std::size_t>(got));
                        if(it->second.input.size()>65536){close_peer(socket);continue;}
                        for(;;) {
                            it=peers.find(socket);if(it==peers.end())break;
                            const auto newline=it->second.input.find('\n');if(newline==std::string::npos)break;
                            auto message=it->second.input.substr(0,newline);it->second.input.erase(0,newline+1);
                            if(!message.empty()&&message.back()=='\r')message.pop_back();
                            line(socket,message);
                        }
                    }
                }
                it=peers.find(socket);if(it==peers.end())continue;
                if(FD_ISSET(socket,&writeSet)&&!it->second.output.empty()) {
#ifdef _WIN32
                    const int sent=::send(socket,it->second.output.data(),static_cast<int>(it->second.output.size()),0);
#else
                    const int sent=static_cast<int>(::send(socket,it->second.output.data(),it->second.output.size(),MSG_NOSIGNAL));
#endif
                    if(sent<0&&!would_block()){close_peer(socket);continue;}
                    if(sent>0)it->second.output.erase(0,static_cast<std::size_t>(sent));
                }
                if(it->second.drop)close_peer(socket);
            }
            if(game) {
                const auto previous=game->version();game->tick();
                if(previous!=game->version())broadcast();
            }
        }
        return 0;
    }
};
Server::Server(std::string address,std::uint16_t port,SchedulerConfig config)
    :impl_(std::make_unique<Impl>(std::move(address),port,config)){}
Server::~Server()=default;
int Server::run(){return impl_->run();}
void Server::stop()noexcept{impl_->running.store(false);}
} // namespace cardgame::network
