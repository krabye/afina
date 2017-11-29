#include "Worker.h"

#include <iostream>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <netdb.h>
#include <afina/Storage.h>
#include <afina/execute/Command.h>
#include <protocol/Parser.h>

#include "Utils.h"

#define MAXEVENTS 64

namespace Afina {
namespace Network {
namespace NonBlocking {

// See Worker.h
Worker::Worker(std::shared_ptr<Afina::Storage> ps) {
	pStorage = ps;
	fifo_fd = -1;
    // TODO: implementation here
}

// See Worker.h
Worker::~Worker() {
    // TODO: implementation here
}

// See Worker.h
void Worker::Start(int server_socket) {

    this->server_socket = server_socket;
    running = true;
    if (pthread_create(&thread, NULL, Worker::OnRun, this) < 0) {
        throw std::runtime_error("Could not create server thread");
    }
    // TODO: implementation here
}

void Worker::Start(int server_socket, int fifo_fd) {
	this->fifo_fd = fifo_fd;
	this->Start(server_socket);
}

// See Worker.h
void Worker::Stop() {
    running = false;
    shutdown(server_socket, SHUT_RDWR);
}

// See Worker.h
void Worker::Join() {
    pthread_join(thread, 0);
}

// See Worker.h
void* Worker::OnRun(void *args) {
    Worker* w = ((Worker*)args);
    w->Run();
    // int server_socket = ((Worker*)args)->server_socket;
}

void Worker::Run() {
	int efd;
    int s;
    struct epoll_event event;
    struct epoll_event *events;

    efd = epoll_create1(0);
    if (efd == -1) {
        throw std::runtime_error("epoll_create");
    }

    event.data.fd = server_socket;
    event.events = EPOLLIN | EPOLLET | EPOLLEXCLUSIVE;
    s = epoll_ctl(efd, EPOLL_CTL_ADD, server_socket, &event);
    if (s == -1) {
        throw std::runtime_error("epoll_ctl");
    }

    if (fifo_fd != -1) {
	    event.data.fd = fifo_fd;
	    event.events = EPOLLIN | EPOLLET | EPOLLEXCLUSIVE;
	    s = epoll_ctl(efd, EPOLL_CTL_ADD, fifo_fd, &event);
	    if (s == -1) {
	        throw std::runtime_error("epoll_ctl");
	    }
	}

    events = (epoll_event*)calloc(MAXEVENTS, sizeof event);

    /* The event loop */
    while (running) {
        int n, i;

        n = epoll_wait(efd, events, MAXEVENTS, -1);
        for (i = 0; i < n; i++) {
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN))) {
                /* An error has occured on this fd, or the socket is not
                   ready for reading (why were we notified then?) */
                fprintf(stderr, "epoll error\n");
                close(events[i].data.fd);
                continue;
            }

            else if (server_socket == events[i].data.fd) {
                /* We have a notification on the listening socket, which
                   means one or more incoming connections. */
                while (1) {
                    struct sockaddr in_addr;
                    socklen_t in_len;
                    int infd;
                    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

                    in_len = sizeof in_addr;
                    infd = accept(server_socket, &in_addr, &in_len);
                    if (infd == -1) {
                        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                            /* We have processed all incoming
                               connections. */
                            break;
                        } else {
                            perror("accept");
                            break;
                        }
                    }

                    s = getnameinfo(&in_addr, in_len, hbuf, sizeof hbuf, sbuf, sizeof sbuf,
                                    NI_NUMERICHOST | NI_NUMERICSERV);
                    if (s == 0) {
                        printf("Accepted connection on descriptor %d "
                               "(host=%s, port=%s)\n",
                               infd, hbuf, sbuf);
                    }

                    /* Make the incoming socket non-blocking and add it to the
                       list of fds to monitor. */
                    try {
                    	make_socket_non_blocking(infd);
                    } catch (std::runtime_error &ex) {
                    	std::cerr << "Thread error: " << ex.what() << std::endl;
                    	break;
                    }
                    if (s == -1)
                        abort();

                    event.data.fd = infd;
                    event.events = EPOLLIN | EPOLLET | EPOLLEXCLUSIVE;
                    s = epoll_ctl(efd, EPOLL_CTL_ADD, infd, &event);
                    if (s == -1) {
                        perror("epoll_ctl");
                        abort();
                    }
                }
                continue;
            } else {
                /* We have data on the fd waiting to be read. Read and
                   display it. We must read whatever data is available
                   completely, as we are running in edge-triggered mode
                   and won't get a notification again for the same
                   data. */
                int done = 0;
                char _buf[512];
                char* begin = _buf;
                char* buf = _buf;
                ssize_t res, res1;
                size_t parsed = 0;
                std::string args;
                std::string out;
                int client_socket = events[i].data.fd;

                State st = States[client_socket];
                Protocol::Parser parser = st.parser;
                uint32_t body_size = st.body_size;
                bool state = st.state;

                std::unique_ptr<Execute::Command> cmd;
                if (state) {
                    cmd = parser.Build(body_size);
                    body_size = st.body_size;
                    args = st.args;
                }

                while (1) {
                    res = read(client_socket, buf, sizeof buf);
                    if (res == -1) {
                        /* If errno == EAGAIN, that means we have read all
                           data. So go back to the main loop. */
                        if (errno != EAGAIN) {
                            perror("read");
                            done = 1;
                        }
                        break;
                    } else if (res == 0) {
                        /* End of file. The remote has closed the
                           connection. */
                        done = 1;
                        break;
                    }


                    try {
                        while (res > 0) {
                        	if(!state){
                        		parsed = 0;
	                            state = parser.Parse(buf, res, parsed);
	                            
	                            buf = buf+parsed;
	                            res -= parsed;

	                            if (state){
	                                cmd = parser.Build(body_size);

	                                //Process operations without args here
	                                if (body_size == 0) {
		                                cmd->Execute(*pStorage, args, out);
		                                if (events[i].data.fd != fifo_fd){
		                                	if ( (res1 = write(client_socket, &out[0], out.size())) <= 0 ){
				                                // perror("Socket error");
				                                done = 1;
				                                break;
				                            }
				                        }

	                                	parser.Reset();
		                                parsed = 0;
		                                body_size = 0;
		                                args = "";
		                                out = "";
		                                state = false;
		                            }
	                            }

	                        } else {
	                        	//Here we process operations that have args (and have been parsed)
                                args.append(buf, res < body_size ? res : body_size);
                                if (body_size > res) {
                                    body_size -= res;
                                    buf = buf + res;
                                    res = 0; //No more data in buf to proces
                                    //Reset buf to begin then
                                    buf = begin;
                                } else {
                                    buf = buf + body_size;
                                    res -= body_size;
                                    body_size = 0;

                                    if (buf[0] == '\r' && buf[1] == '\n') {
                                        buf += 2;
                                        res -= 2;

                                        if (res == 0)
                                            buf = begin;
                                    }

                                    cmd->Execute(*pStorage, args, out);
                                    if (events[i].data.fd != fifo_fd){
	                                    if ( (res1 = write(client_socket, &out[0], out.size())) <= 0 ){
			                                // perror("Socket error");
			                                done = 1;
			                                break;
			                            }
			                        }

                                    parser.Reset();
	                                parsed = 0;
	                                body_size = 0;
	                                args = "";
	                                out="";
	                                state = false;
                                }
	                        }
                        }
                    } catch (std::runtime_error &ex) {
                        std::cerr << "Server fails: " << ex.what() << std::endl;
                        out = std::string("ERROR\r\n");
                        if (events[i].data.fd != fifo_fd){
	                        if ( (res1 = write(client_socket, &out[0], out.size())) <= 0 ){
	                            // perror("Socket error");
	                            done = 1;
	                            break;
	                        }
	                    }

                        //This block needed to read remaining data
                        //from client socket
                        while (1) {
		                    res = read(client_socket, buf, sizeof buf);
		                    if (res == -1) {
		                        /* If errno == EAGAIN, that means we have read all
		                           data. So go back to the main loop. */
		                        if (errno != EAGAIN) {
		                            perror("read");
		                            done = 1;
		                        }
		                        break;
		                    } else if (res == 0) {
		                        /* End of file. The remote has closed the
		                           connection. */
		                        done = 1;
		                        break;
		                    }
		                }
		                //end of block

                        parser.Reset();
                        state = false;
                        buf = begin;
                        args = "";
                        out = "";
                        parsed = 0;
                        break;
                    }
                }

                States[client_socket].parser = parser;
                States[client_socket].body_size = body_size;
                States[client_socket].state = state;
                States[client_socket].args = args;

                if (done) {
                    printf("Closed connection on descriptor %d\n", events[i].data.fd);
                    States.erase(client_socket);
                    /* Closing the descriptor will make epoll remove it
                       from the set of descriptors which are monitored. */
                    close(events[i].data.fd);
                }
            }
        }
    }

    free(events);
}

} // namespace NonBlocking
} // namespace Network
} // namespace Afina
