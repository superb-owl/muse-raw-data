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
#include <jansson.h>


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
                ::close(sockfd);
                sockfd = -1;
            }
            freeaddrinfo(result);
            
            if (sockfd != -1) {
                // Send WebSocket handshake
                std::string handshake = 
                    "GET / HTTP/1.1\r\n"
                    "Host: " + std::string(host) + ":" + sport + "\r\n"
                    "Upgrade: websocket\r\n"
                    "Connection: Upgrade\r\n"
                    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"  // This is a static key for simplicity
                    "Sec-WebSocket-Version: 13\r\n"
                    "\r\n";

                INFO("Sending WebSocket handshake: %s", handshake.c_str());
                if (::send(sockfd, handshake.c_str(), handshake.length(), 0) < 0) {
                    WARN("Failed to send handshake");
                    ::close(sockfd);
                    return nullptr;
                }

                // Receive handshake response
                char buffer[1024];
                ssize_t bytes = recv(sockfd, buffer, sizeof(buffer)-1, 0);
                if (bytes > 0) {
                    buffer[bytes] = '\0';
                    INFO("Received handshake response: %s", buffer);

                    // Check if response contains "101 Switching Protocols"
                    if (strstr(buffer, "101 Switching Protocols") == nullptr) {
                        WARN("Invalid handshake response");
                        ::close(sockfd);
                        return nullptr;
                    }

                    WebSocket* ws = new WebSocket();
                    ws->sockfd = sockfd;
                    ws->state = OPEN;
                    return ws;
                }

                WARN("No handshake response received");
                ::close(sockfd);
                return nullptr;
            }
            return nullptr;
        }

        bool decodeFrame(const char* input, size_t inputLen, std::vector<char>& output) {
            if (inputLen < 2) return false;

            // Check if this is a final frame (first bit)
            bool fin = (input[0] & 0x80) != 0;

            // Get opcode (last 4 bits of first byte)
            uint8_t opcode = input[0] & 0x0F;

            // Check if masked (first bit of second byte)
            bool masked = (input[1] & 0x80) != 0;

            // Get payload length
            uint64_t payload_length = input[1] & 0x7F;
            size_t pos = 2;

            if (payload_length == 126) {
                if (inputLen < 4) {
                    WARN("Payload length is 126 but input length is %d", inputLen);
                    return false;
                }
                payload_length = (input[2] << 8) | input[3];
                pos += 2;
            } else if (payload_length == 127) {
                if (inputLen < 10) {
                    WARN("Payload length is 127 but input length is %d", inputLen);
                    return false;
                }
                payload_length = 0;
                for (int i = 0; i < 8; i++) {
                    payload_length = (payload_length << 8) | (unsigned char)input[2+i];
                }
                pos += 8;
            }

            // Get masking key if masked
            uint8_t mask[4] = {0, 0, 0, 0};
            if (masked) {
                if (inputLen < pos + 4) {
                    WARN("Masked but input length is %d", inputLen);
                    return false;
                }
                memcpy(mask, input + pos, 4);
                pos += 4;
            }

            // Check if we have enough data
            if (inputLen < pos + payload_length) {
                WARN("Not enough data for payload of length %d", payload_length);
                return false;
            }

            // Decode payload
            output.resize(payload_length);
            for (size_t i = 0; i < payload_length; i++) {
                if (masked) {
                    output[i] = input[pos + i] ^ mask[i % 4];
                } else {
                    output[i] = input[pos + i];
                }
            }

            return true;
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
                // Decode WebSocket frame
                std::vector<char> decoded;
                if (decodeFrame(buffer, bytes, decoded)) {
                    // Copy decoded data to buffer
                    size_t copySize = std::min(decoded.size(), bufferSize - 1);
                    memcpy(buffer, decoded.data(), copySize);
                    buffer[copySize] = '\0';
                    *bytesRead = copySize;
                    return true;
                }
                DEBUG("Failed to decode WebSocket frame");
                return false;
            }
            WARN("Receive failed with error: %s", strerror(errno));
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
        EEG1_OUTPUT,
        EEG2_OUTPUT,
        EEG3_OUTPUT,
        EEG4_OUTPUT,
        DELTA_OUTPUT,
        THETA_OUTPUT,
        ALPHA_OUTPUT,
        BETA_OUTPUT,
        GAMMA_OUTPUT,
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
    bool running = true;

    // EEG data
    float eeg_values[4] = {0.f, 0.f, 0.f, 0.f};
    float brain_waves[5] = {0.f, 0.f, 0.f, 0.f, 0.f}; // delta, theta, alpha, beta, gamma

    std::string messageBuffer;

    MuseHeadband() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        // Configure outputs
        configOutput(EEG1_OUTPUT, "EEG Channel 1");
        configOutput(EEG2_OUTPUT, "EEG Channel 2");
        configOutput(EEG3_OUTPUT, "EEG Channel 3");
        configOutput(EEG4_OUTPUT, "EEG Channel 4");
        configOutput(DELTA_OUTPUT, "Delta waves (1-4 Hz)");
        configOutput(THETA_OUTPUT, "Theta waves (4-8 Hz)");
        configOutput(ALPHA_OUTPUT, "Alpha waves (8-13 Hz)");
        configOutput(BETA_OUTPUT, "Beta waves (13-32 Hz)");
        configOutput(GAMMA_OUTPUT, "Gamma waves (32+ Hz)");
        INFO("MuseHeadband loaded");

        // Start WebSocket connection thread
        wsThread = std::thread([this]() {
            while (running) {
                if (!ws || ws->getReadyState() != easywsclient::OPEN) {
                    ws.reset(easywsclient::WebSocket::create_connection("ws://localhost:8080"));
                    connected = (ws != nullptr);
                    if (connected) {
                        INFO("Connected to Muse Headband server");
                        messageBuffer.clear();
                    }
                }

                if (connected) {
                    char buffer[16384];
                    size_t bytesRead;
                    if (ws->receive(buffer, sizeof(buffer), &bytesRead)) {
                        buffer[bytesRead] = '\0';
                        std::string message(buffer);

                        // Check if this is a complete JSON message
                        if (message[0] == '{' && message[message.length()-1] == '}') {
                            parseMuseData(message.c_str());
                            messageBuffer.clear();
                        } else {
                            // Accumulate partial messages
                            messageBuffer += message;
                            if (messageBuffer[0] == '{' && 
                                messageBuffer[messageBuffer.length()-1] == '}') {
                                parseMuseData(messageBuffer.c_str());
                                messageBuffer.clear();
                            }
                        }
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

    void parseMuseData(const char* jsonStr) {
        INFO("Received JSON: %s", jsonStr);
        json_error_t error;
        json_t* root = json_loads(jsonStr, 0, &error);
        
        if (!root) {
            WARN("Failed to parse JSON: %s", error.text);
            return;
        }

        // Parse EEG buffer
        json_t* eeg_buffer = json_object_get(root, "eeg_buffer");
        if (json_is_array(eeg_buffer) && json_array_size(eeg_buffer) > 0) {
            json_t* first_sample = json_array_get(eeg_buffer, 0);
            if (json_is_array(first_sample)) {
                std::lock_guard<std::mutex> lock(dataMutex);
                for (size_t i = 0; i < 4 && i < json_array_size(first_sample); i++) {
                    json_t* value = json_array_get(first_sample, i);
                    if (json_is_number(value)) {
                        eeg_values[i] = json_number_value(value) / 1000.0f; // Convert to millivolts
                    }
                }
            }
        }

        // Parse brain wave bands
        json_t* eeg_bands = json_object_get(root, "eeg_bands");
        if (json_is_object(eeg_bands)) {
            std::lock_guard<std::mutex> lock(dataMutex);
            const char* band_names[] = {"delta", "theta", "alpha", "beta", "gamma"};
            for (size_t i = 0; i < 5; i++) {
                json_t* band = json_object_get(eeg_bands, band_names[i]);
                if (json_is_array(band) && json_array_size(band) > 0) {
                    json_t* first_value = json_array_get(band, 0);
                    if (json_is_number(first_value)) {
                        brain_waves[i] = json_number_value(first_value) / 100.0f; // Scale to reasonable voltage
                    }
                }
            }
        }

        json_decref(root);
        INFO("Parsed EEG values: %.2f, %.2f, %.2f, %.2f", 
              eeg_values[0], eeg_values[1], eeg_values[2], eeg_values[3]);
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
        lights[CONNECTION_LIGHT].setBrightness(connected ? 1.f : 0.f);

        std::lock_guard<std::mutex> lock(dataMutex);
        
        // Output EEG values
        for (int i = 0; i < 4; i++) {
            outputs[EEG1_OUTPUT + i].setVoltage(eeg_values[i]);
        }

        // Output brain wave bands
        for (int i = 0; i < 5; i++) {
            outputs[DELTA_OUTPUT + i].setVoltage(brain_waves[i]);
        }
    }
};
struct MuseHeadbandWidget : ModuleWidget {
    MuseHeadbandWidget(MuseHeadband* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/MuseHeadband.svg")));

        // Add connection status light at the top
        addChild(createLightCentered<MediumLight<GreenLight>>(
            mm2px(Vec(7.62, 10.16)), 
            module, 
            MuseHeadband::CONNECTION_LIGHT
        ));

        // EEG outputs
        for (int i = 0; i < 4; i++) {
            addOutput(createOutputCentered<PJ301MPort>(
                mm2px(Vec(7.62, 30 + i * 20)), 
                module, 
                MuseHeadband::EEG1_OUTPUT + i
            ));
        }

        // Brain wave band outputs
        const char* labels[] = {"δ", "θ", "α", "β", "γ"};
        for (int i = 0; i < 5; i++) {
            addOutput(createOutputCentered<PJ301MPort>(
                mm2px(Vec(7.62, 110 + i * 20)), 
                module, 
                MuseHeadband::DELTA_OUTPUT + i
            ));
        }
    }
};

Model* modelMuseHeadband = createModel<MuseHeadband, MuseHeadbandWidget>("MuseHeadband");

