#include "ServerImpl.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>

#include <pthread.h>
#include <signal.h>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>

#include <afina/Storage.h>
#include <afina/execute/Command.h>
#include <protocol/Parser.h>

namespace Afina {
namespace Network {
namespace Blocking {

void *ServerImpl::RunAcceptorProxy(void *p) {
    ServerImpl *srv = reinterpret_cast<ServerImpl *>(p);
    try {
        srv->RunAcceptor();
    } catch (std::runtime_error &ex) {
        std::cerr << "Server fails: " << ex.what() << std::endl;
    }
    return 0;
}

// See Server.h
ServerImpl::ServerImpl(std::shared_ptr<Afina::Storage> ps) : Server(ps) {}

// See Server.h
ServerImpl::~ServerImpl() {}

// See Server.h
void ServerImpl::Start(uint32_t port, uint16_t n_workers) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

    // If a client closes a connection, this will generally produce a SIGPIPE
    // signal that will kill the process. We want to ignore this signal, so send()
    // just returns -1 when this happens.
    sigset_t sig_mask;
    sigemptyset(&sig_mask);
    sigaddset(&sig_mask, SIGPIPE);
    try {
        if (pthread_sigmask(SIG_BLOCK, &sig_mask, NULL) != 0) {
            throw std::runtime_error("Unable to mask SIGPIPE");
        }
    
        // Setup server parameters BEFORE thread created, that will guarantee
        // variable value visibility
        max_workers = n_workers;
        listen_port = port;
    
        // The pthread_create function creates a new thread.
        //
        // The first parameter is a pointer to a pthread_t variable, which we can use
        // in the remainder of the program to manage this thread.
        //
        // The second parameter is used to specify the attributes of this new thread
        // (e.g., its stack size). We can leave it NULL here.
        //
        // The third parameter is the function this thread will run. This function *must*
        // have the following prototype:
        //    void *f(void *args);
        //
        // Note how the function expects a single parameter of type void*. We are using it to
        // pass this pointer in order to proxy call to the class member function. The fourth
        // parameter to pthread_create is used to specify this parameter value.
        //
        // The thread we are creating here is the "server thread", which will be
        // responsible for listening on port 23300 for incoming connections. This thread,
        // in turn, will spawn threads to service each incoming connection, allowing
        // multiple clients to connect simultaneously.
        // Note that, in this particular example, creating a "server thread" is redundant,
        // since there will only be one server thread, and the program's main thread (the
        // one running main()) could fulfill this purpose.
        running.store(true);
        if (pthread_create(&accept_thread, NULL, ServerImpl::RunAcceptorProxy, this) < 0) {
            throw std::runtime_error("Could not create server thread");
        } 
    } catch (std::runtime_error &ex) {
        std::cerr << "Server fails: " << ex.what() << std::endl;
    }
}

// See Server.h
void ServerImpl::Stop() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    running.store(false);
    shutdown(server_socket, SHUT_RDWR);
}

// See Server.h
void ServerImpl::Join() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    pthread_join(accept_thread, 0);
}

// See Server.h
void ServerImpl::RunAcceptor() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

    // For IPv4 we use struct sockaddr_in:
    // struct sockaddr_in {
    //     short int          sin_family;  // Address family, AF_INET
    //     unsigned short int sin_port;    // Port number
    //     struct in_addr     sin_addr;    // Internet address
    //     unsigned char      sin_zero[8]; // Same size as struct sockaddr
    // };
    //
    // Note we need to convert the port to network order

    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;          // IPv4
    server_addr.sin_port = htons(listen_port); // TCP port number
    server_addr.sin_addr.s_addr = INADDR_ANY;  // Bind to any address

    try {
        // Arguments are:
        // - Family: IPv4
        // - Type: Full-duplex stream (reliable)
        // - Protocol: TCP
        server_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (server_socket == -1) {
            throw std::runtime_error("Failed to open socket");
        }

        // when the server closes the socket,the connection must stay in the TIME_WAIT state to
        // make sure the client received the acknowledgement that the connection has been terminated.
        // During this time, this port is unavailable to other processes, unless we specify this option
        //
        // This option let kernel knows that we are OK that multiple threads/processes are listen on the
        // same port. In a such case kernel will balance input traffic between all listeners (except those who
        // are closed already)
        int opts = 1;
        if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opts, sizeof(opts)) == -1) {
            close(server_socket);
            throw std::runtime_error("Socket setsockopt() failed");
        }

        // Bind the socket to the address. In other words let kernel know data for what address we'd
        // like to see in the socket
        if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
            close(server_socket);
            throw std::runtime_error("Socket bind() failed");
        }

        // Start listening. The second parameter is the "backlog", or the maximum number of
        // connections that we'll allow to queue up. Note that listen() doesn't block until
        // incoming connections arrive. It just makesthe OS aware that this process is willing
        // to accept connections on this socket (which is bound to a specific IP and port)
        if (listen(server_socket, 5) == -1) {
            close(server_socket);
            throw std::runtime_error("Socket listen() failed");
        }

        int client_socket;
        struct sockaddr_in client_addr;
        socklen_t sinSize = sizeof(struct sockaddr_in);
        while (running.load()) {
            std::cout << "network debug: waiting for connection..." << std::endl;

            // When an incoming connection arrives, accept it. The call to accept() blocks until
            // the incoming connection arrives
            if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &sinSize)) == -1) {
                close(server_socket);
                throw std::runtime_error("Socket accept() failed");
            }

            con_mutex.lock();
            if (connections.size() >= max_workers) {
                std::cerr << "not enough workers\n";
                close(client_socket);
            } else {
                try {
                    pthread_t cl_connection;
                    pthread_attr_t attr;
                    if (pthread_attr_init(&attr) < 0) {
                        throw std::runtime_error("Could not set init attribute");
                    }
                    if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) < 0) {
                        throw std::runtime_error("Could not set detache attribute");
                    }
                    
                    connections.push_back(cl_connection);
                    Data data = {this, client_socket};
                    if (pthread_create(&connections.back(), &attr, ServerImpl::RunConnectionProxy, &data) < 0) {
                        throw std::runtime_error("Could not create client thread");
                    }
                    
                } catch (std::runtime_error &ex) {
                    std::cerr << "New thread fails: " << ex.what() << std::endl;
                }
            }

            con_mutex.unlock();
        }
    } catch (std::runtime_error &ex) {
        std::cerr << "Server fails: " << ex.what() << std::endl;
    }

    //Waiting until all threads stop working
    while (true) {
        con_mutex.lock();
        if (connections.size() == 0) {
            con_mutex.unlock();
            break;
        }    
        con_mutex.unlock();
    }

    // Cleanup on exit...
    close(server_socket);
}

void *ServerImpl::RunConnectionProxy(void *p){
    Data *data = reinterpret_cast<Data *>(p);
    data->serv->RunConnection(data->socket);
    data->serv->con_mutex.lock();
    for (std::vector<pthread_t>::iterator it = data->serv->connections.begin();
            it != data->serv->connections.end(); ++it) {
        if (*it == pthread_self()) {
            data->serv->connections.erase(it);
            break;
        }
    }
    data->serv->con_mutex.unlock();
    return 0;
}

// See Server.h
void ServerImpl::RunConnection(int client_socket) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    char buf[100];
    ssize_t res;
    size_t parsed = 0;
    bool state = false;
    Protocol::Parser parser;
    uint32_t body_size;
    std::string args;
    std::string out;

    try {
        while (running.load() || !state) {
            if ( (res = recv(client_socket,  buf, 100, 0)) < 0 ) {
                throw std::runtime_error("Socket error");
            } else if (res == 0) {
                throw std::runtime_error("Socket closed by client");
            }

            try {
                state = parser.Parse(buf, res, parsed);
            } catch (std::runtime_error &ex) {
                std::cerr << "Server fails: " << ex.what() << std::endl;
                parser.Reset();
                state = false;
            }
            if (state) {
                auto cmd = parser.Build(body_size);

                if (res - parsed > 0 && body_size > 0) {
                    args = std::string(buf+parsed, res-parsed);
                    body_size = (body_size > res - parsed) ? (body_size + parsed - res) : 0;
                }
                while (body_size > 0) {
                    if ( (res = recv(client_socket, buf, body_size, 0)) < 0 ){
                        throw std::runtime_error("Socket error");
                    } else if (res == 0) {
                        throw std::runtime_error("Socket closed by client");
                    }
                    args.append(buf, res);
                    body_size -= res;
                }

                cmd->Execute(*pStorage, args, out);

                if ( (res = send(client_socket, &out[0], out.size(), 0)) <= 0 ){
                    throw std::runtime_error("Socket error");
                }
                
                parser.Reset();
                parsed = 0;
                state = false;
            }
        }
    } catch (std::runtime_error &ex) {
        std::cerr << "Thread finished: " << ex.what() << std::endl;
    } catch (...) {
        std::cerr << "Thread failed: Another error\n";
    }
    close(client_socket);
}

} // namespace Blocking
} // namespace Network
} // namespace Afina
