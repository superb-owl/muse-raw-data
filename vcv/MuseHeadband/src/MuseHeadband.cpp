#include "plugin.hpp"
#include <thread>
#include <mutex>
#include <string>
#include <cstring>
#include <cstdio>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

// Websocket implementation - single header, no dependencies
namespace easywsclient {
    enum ReadyState { CLOSING, CLOSED, CONNECTING, OPEN };

    class WebSocket {
    protected:
        int sockfd = -1;
        ReadyState state = CLOSED;

    public:
        WebSocket() {}
        virtual ~WebSocket() { close(); }
        
        static WebSocket* create_connection(const std::string& url) {
            char host[128];
            int port;
            sscanf(url.c_str(), "ws://%[^:/]:%d", host, &port);
            
            struct addrinfo hints;
            struct addrinfo *result;
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;
            
            char sport[16];
            snprintf(sport, 16, "%d", port);
            if (getaddrinfo(host, sport, &hints, &result) != 0) {
                return nullptr;
            }
            
            int sockfd = -1;
            for(struct addrinfo *p = result; p != nullptr; p = p->ai_next) {
                sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
                if (sockfd == -1) continue;
                if (connect(sockfd, p->ai_addr, p->ai_addrlen) != -1) {
                    break;
                }
                ::close(sockfd);  // Fixed: Using global close
                sockfd = -1;
            }
            freeaddrinfo(result);
            
            if (sockfd != -1) {
                WebSocket* ws = new WebSocket();
                ws->sockfd = sockfd;
                ws->state = OPEN;
                return ws;
            }
            return nullptr;
        }

        void send(const std::string& message) {
            if (state == OPEN) {
                ::send(sockfd, message.c_str(), message.size(), 0);
            }
        }

        bool receive(char* buffer, size_t bufferSize, size_t* bytesRead) {
            if (state != OPEN) return false;
            ssize_t bytes = recv(sockfd, buffer, bufferSize - 1, 0);
            if (bytes > 0) {
                *bytesRead = bytes;
                return true;
            }
            return false;
        }

        void close() {
            if (sockfd != -1) {
                ::close(sockfd);  // Fixed: Using global close
                sockfd = -1;
                state = CLOSED;
            }
        }

        ReadyState getReadyState() const { 
            return state;
        }
    };
}

struct MuseHeadband : Module {
    enum ParamId {
        PARAMS_LEN
    };
    enum InputId {
        INPUTS_LEN
    };
    enum OutputId {
        STREAM_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        CONNECTION_LIGHT,
        LIGHTS_LEN
    };

    std::unique_ptr<easywsclient::WebSocket> ws;
    std::thread wsThread;
    std::mutex dataMutex;
    bool connected = false;
    float currentValue = 0.f;
    bool running = true;

    MuseHeadband() {
        INFO("MuseHeadband loaded");
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configOutput(STREAM_OUTPUT, "WebSocket Stream");

        // Start WebSocket connection in separate thread
        wsThread = std::thread([this]() {
            INFO("MuseHeadband thread started");
            while (running) {
                if (!ws || ws->getReadyState() != easywsclient::OPEN) {
                    INFO("Connecting to WebSocket server...");
                    ws.reset(easywsclient::WebSocket::create_connection("ws://localhost:8080"));
                    connected = (ws != nullptr);
                    if (connected) {
                        INFO("Connected to WebSocket server");
                        ws->send("Hello from VCV Rack!");
                    }
                }
                
                if (connected) {
                    char buffer[1024];
                    size_t bytesRead;
                    if (ws->receive(buffer, sizeof(buffer), &bytesRead)) {
                        buffer[bytesRead] = '\0';
                        try {
                            float value = std::stof(buffer);
                            std::lock_guard<std::mutex> lock(dataMutex);
                            currentValue = value;
                        } catch (...) {
                            // Handle parsing errors
                        }
                    }
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

    ~MuseHeadband() {
        running = false;
        if (wsThread.joinable()) {
            wsThread.join();
        }
        if (ws) {
            ws->close();
        }
    }

    void process(const ProcessArgs& args) override {
        // Update connection status light
        lights[CONNECTION_LIGHT].setBrightness(connected ? 1.f : 0.f);

        // Output the current value
        std::lock_guard<std::mutex> lock(dataMutex);
        outputs[STREAM_OUTPUT].setVoltage(currentValue * 10.f); // Scale to ±10V range
    }
};

struct MuseHeadbandWidget : ModuleWidget {
    MuseHeadbandWidget(MuseHeadband* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/MuseHeadband.svg")));

        // Add connection status light
        addChild(createLightCentered<MediumLight<GreenLight>>(
            mm2px(Vec(7.62, 10.16)), 
            module, 
            MuseHeadband::CONNECTION_LIGHT
        ));

        // Add output port
        addOutput(createOutputCentered<PJ301MPort>(
            mm2px(Vec(7.62, 116.84)), 
            module, 
            MuseHeadband::STREAM_OUTPUT
        ));
    }
};

Model* modelMuseHeadband = createModel<MuseHeadband, MuseHeadbandWidget>("MuseHeadband");
