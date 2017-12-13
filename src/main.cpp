#include <chrono>
#include <iostream>
#include <memory>
#include <uv.h>
#include <unistd.h>
#include <fstream>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <time.h>
#include <signal.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <cxxopts.hpp>

#include <afina/Storage.h>
#include <afina/Version.h>
#include <afina/network/Server.h>

#include "network/blocking/ServerImpl.h"
#include "network/nonblocking/ServerImpl.h"
#include "network/uv/ServerImpl.h"

#include "storage/MapBasedGlobalLockImpl.h"
#include "storage/MapBasedRWLockImpl.h"
#include "storage/MapBasedStripeLockImpl.h"

#define MAXEVENTS 8

typedef struct {
	std::shared_ptr<Afina::Storage> storage;
	std::shared_ptr<Afina::Network::Server> server;
} Application;

// Handle all signals catched
void signal_handler(uv_signal_t *handle, int signum) {
	Application *pApp = static_cast<Application *>(handle->data);

	std::cout << "Receive stop signal" << std::endl;
	uv_stop(handle->loop);
}

// Called when it is time to collect passive metrics from services
void timer_handler(uv_timer_t *handle) {
	Application *pApp = static_cast<Application *>(handle->data);
	// std::cout << "Start passive metrics collection" << std::endl;
}

void run_loop(int efd, int signal_fd, int timer_fd) {
	struct epoll_event *events;
	struct epoll_event event;
	events = (epoll_event*)calloc(MAXEVENTS, sizeof event);
	bool running = true;
	unsigned long long tmp;
	ssize_t res;
	while(running) {
		int n, i;

		n = epoll_wait(efd, events, MAXEVENTS, -1);
		for (i = 0; i < n; i++) {
			if (events[i].data.fd == signal_fd) {
				running = false;
				std::cout << "Receive stop signal" << std::endl;
				break;
			}
			if (events[i].data.fd == timer_fd) {
				while (1) {
					res = read(timer_fd, &tmp, sizeof tmp);
					if (res == -1) {
						std::cout << "Start passive metrics collection" << std::endl;
						break;
					}
				}
			}
		}
	}
}

int makeTimer(int t)
{
	struct itimerspec timeout;
	int timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
	if (timer_fd <= 0) {
		throw std::runtime_error("Failed to create timer\n");
	}

	if (fcntl(timer_fd, F_SETFL, O_NONBLOCK)) {
		throw std::runtime_error("Failed to set to non blocking mode\n");
	}

	timeout.it_value.tv_sec = t;
	timeout.it_value.tv_nsec = 0;
	timeout.it_interval.tv_sec = t; /* recurring */
	timeout.it_interval.tv_nsec = 0;

	if (timerfd_settime(timer_fd, 0, &timeout, NULL)) {
		throw std::runtime_error("Failed to set timer duration\n");
	}

	return timer_fd;
}

int main(int argc, char **argv) {
	// Build version
	// TODO: move into Version.h as a function
	std::stringstream app_string;
	app_string << "Afina " << Afina::Version_Major << "." << Afina::Version_Minor << "." << Afina::Version_Patch;
	if (Afina::Version_SHA.size() > 0) {
		app_string << "-" << Afina::Version_SHA;
	}

	// Command line arguments parsing
	cxxopts::Options options("afina", "Simple memory caching server");
	try {
		// TODO: use custom cxxopts::value to print options possible values in help message
		// and simplify validation below
		options.add_options()("s,storage", "Type of storage service to use", cxxopts::value<std::string>());
		options.add_options()("n,network", "Type of network service to use", cxxopts::value<std::string>());
		options.add_options()("d,demon", "Demon mode");
		options.add_options()("p,print-pid", "Print pid to file", cxxopts::value<std::string>());
		options.add_options()("r", "Input fifo", cxxopts::value<std::string>());
		options.add_options()("w", "Output fifo", cxxopts::value<std::string>());
		options.add_options()("h,help", "Print usage info");
		options.parse(argc, argv);

		if (options.count("help") > 0) {
			std::cerr << options.help() << std::endl;
			return 0;
		}
	} catch (cxxopts::OptionParseException &ex) {
		std::cerr << "Error: " << ex.what() << std::endl;
		return 1;
	}

	pid_t p = getpid();
	if (options.count("demon") > 0) {
		if ( (p = fork()) == 0 ) {
			p = setsid();
			fclose(stderr);
			fclose(stdout);
			fclose(stdin);
		} else if (p != -1) {
			return 0;
		} else {
			printf("Can not fork\n");
			return 1;
		}
	}

	std::string pid_file = "afina.pid";
	if (options.count("print-pid") > 0) {
		pid_file = options["print-pid"].as<std::string>();
		std::ofstream fout(pid_file);
		fout << p << "\n";
		fout.close();
	}

	// Start boot sequence
	Application app;
	std::cout << "Starting " << app_string.str() << std::endl;

	// Build new storage instance
	std::string storage_type = "map_global";
	if (options.count("storage") > 0) {
		storage_type = options["storage"].as<std::string>();
	}

	int input_fifo = -1;
	try {
		if (storage_type == "map_global") {
			app.storage = std::make_shared<Afina::Backend::MapBasedGlobalLockImpl>();
		} else if (storage_type == "rwlock") {
			app.storage = std::make_shared<Afina::Backend::MapBasedRWLockImpl>();
		} else if (storage_type == "stripe") {
			app.storage = std::make_shared<Afina::Backend::MapBasedStripeLockImpl>(4);
		} else {
			throw std::runtime_error("Unknown storage type");
		}
	
		// Build  & start network layer
		std::string network_type = "uv";
		if (options.count("network") > 0) {
			network_type = options["network"].as<std::string>();
		}
	
		if (network_type == "uv") {
			app.server = std::make_shared<Afina::Network::UV::ServerImpl>(app.storage);
		} else if (network_type == "blocking") {
			app.server = std::make_shared<Afina::Network::Blocking::ServerImpl>(app.storage);
		} else if (network_type == "nonblocking") {
			app.server = std::make_shared<Afina::Network::NonBlocking::ServerImpl>(app.storage);
		} else {
			throw std::runtime_error("Unknown network type");
		}

		if (options.count("r") > 0) {
			std::string input_fifo_name = options["r"].as<std::string>();
			unlink(input_fifo_name.data());
			if (mkfifo(input_fifo_name.data(), S_IFIFO | S_IRUSR | S_IWUSR) < 0) {
				throw std::runtime_error("Input fifo make failed");
			}
			if ( (input_fifo = open(input_fifo_name.data(), O_NONBLOCK|O_RDWR)) < 0) {
				throw std::runtime_error("Input fifo open failed");
			}
			app.server->Set_fifo_id(input_fifo);
		}
	} catch (std::runtime_error &ex) {
		std::cerr << "Server fails: " << ex.what() << std::endl;
		return 1;
	}

	// Init local loop. It will react to signals and performs some metrics collections. Each
	// subsystem is able to push metrics actively, but some metrics could be collected only
	// by polling, so loop here will does that work
	int efd;
	efd = epoll_create1(0);
	sigset_t mask;
	sigset_t orig_mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, SIGKILL);
	int signal_fd, timer_fd;
	if (sigprocmask(SIG_BLOCK, &mask, &orig_mask) < 0) {
		perror ("sigprocmask");
		return 1;
	}
	signal_fd = signalfd(-1, &mask, 0);

	epoll_event event;
	event.data.fd = signal_fd;
	event.events = EPOLLIN;
	epoll_ctl(efd, EPOLL_CTL_ADD, signal_fd, &event);

	timer_fd = makeTimer(5);

	event.data.fd = timer_fd;
	event.events = EPOLLIN;
	epoll_ctl(efd, EPOLL_CTL_ADD, timer_fd, &event);

	// Start services
	try {
		app.storage->Start();
		app.server->Start(8080);

		// Freeze current thread and process events
		std::cout << "Application started with storage " << storage_type << std::endl;
		// uv_run(&loop, UV_RUN_DEFAULT);
		run_loop(efd, signal_fd, timer_fd);

		// Stop services
		app.server->Stop();
		app.server->Join();
		app.storage->Stop();

		std::cout << "Application stopped" << std::endl;
	} catch (std::exception &e) {
		std::cerr << "Fatal error" << e.what() << std::endl;
	}

	return 0;
}
