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
        std::string messageBuffer;  // Buffer for incomplete messages

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
            if (inputLen < 2) {
                DEBUG("Frame too short (%zu bytes), waiting for more data", inputLen);
                return false;
            }
            
            unsigned char first_byte = input[0];
            bool fin = (first_byte & 0x80) != 0;
            unsigned char opcode = first_byte & 0x0F;
            
            unsigned char second_byte = input[1];
            bool masked = (second_byte & 0x80) != 0;
            uint64_t payload_length = second_byte & 0x7F;
            size_t pos = 2;
            
            // Handle extended payload length
            if (payload_length == 126) {
                if (inputLen < 4) return false;
                payload_length = ((unsigned char)input[2] << 8) | (unsigned char)input[3];
                pos = 4;
            }
            else if (payload_length == 127) {
                if (inputLen < 10) return false;
                payload_length = 0;
                for (int i = 0; i < 8; i++) {
                    payload_length = (payload_length << 8) | (unsigned char)input[2+i];
                }
                pos = 10;
            }
            
            DEBUG("Frame header: fin=%d, opcode=%d, masked=%d, payload_length=%lu, pos=%zu, inputLen=%zu", 
                  fin, opcode, masked, payload_length, pos, inputLen);
            
            // Get masking key if present
            uint8_t mask[4] = {0, 0, 0, 0};
            if (masked) {
                if (inputLen < pos + 4) return false;
                memcpy(mask, input + pos, 4);
                pos += 4;
            }
            
            // Check if we have the full payload
            if (inputLen < pos + payload_length) {
                DEBUG("Waiting for more data: have %zu bytes, need %zu", inputLen, pos + payload_length);
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
            
            // Use a large temporary buffer for receiving data
            char tempBuffer[131072];  // 128KB buffer
            ssize_t bytes = recv(sockfd, tempBuffer, sizeof(tempBuffer), 0);
            
            if (bytes > 0) {
                DEBUG("Received %zd raw bytes", bytes);
                
                // Add new data to existing buffer
                messageBuffer.insert(messageBuffer.end(), tempBuffer, tempBuffer + bytes);
                
                // Try to decode a frame from the buffer
                std::vector<char> decoded;
                bool frameDecoded = decodeFrame(messageBuffer.data(), messageBuffer.size(), decoded);
                
                if (frameDecoded) {
                    // Copy as much as we can to the output buffer
                    size_t copySize = std::min(decoded.size(), bufferSize - 1);
                    memcpy(buffer, decoded.data(), copySize);
                    buffer[copySize] = '\0';
                    *bytesRead = copySize;
                    
                    // Clear the message buffer since we successfully decoded a frame
                    messageBuffer.clear();
                    
                    return true;
                } else {
                    // Keep the partial data in messageBuffer
                    DEBUG("Incomplete frame, buffered %zu bytes", messageBuffer.size());
                }
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
        EEG1_OUTPUT,
        EEG2_OUTPUT,
        EEG3_OUTPUT,
        EEG4_OUTPUT,
        EEG5_OUTPUT,
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
    float eeg_values[5] = {0.f, 0.f, 0.f, 0.f, 0.f};
    float brain_waves[5] = {0.f, 0.f, 0.f, 0.f, 0.f}; // delta, theta, alpha, beta, gamma


    std::string messageBuffer;  // Buffer for incomplete messages

    MuseHeadband() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        // Configure outputs
        configOutput(EEG1_OUTPUT, "EEG Channel 1");
        configOutput(EEG2_OUTPUT, "EEG Channel 2");
        configOutput(EEG3_OUTPUT, "EEG Channel 3");
        configOutput(EEG4_OUTPUT, "EEG Channel 4");
        configOutput(EEG5_OUTPUT, "EEG Channel 5");
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
                    } else {
                        WARN("Failed to connect to Muse Headband server");
                    }
                }
                
                if (connected) {
                    char buffer[131072];  // 128KB buffer
                    size_t bytesRead;
                    if (ws->receive(buffer, sizeof(buffer), &bytesRead)) {
                        buffer[bytesRead] = '\0';
                        std::string message(buffer, bytesRead);
                        
                        // Add to message buffer
                        messageBuffer += message;
                        
                        // Check if we have a complete JSON message
                        if (!messageBuffer.empty() && 
                            messageBuffer[0] == '{' && 
                            messageBuffer[messageBuffer.length()-1] == '}') {
                            parseMuseData(messageBuffer.c_str());
                            messageBuffer.clear();
                        }
                    }
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

    void parseMuseData(const char* jsonStr) {
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
                INFO("Received EEG buffer with %zu num per sample", json_array_size(first_sample));
                for (size_t i = 0; i < 5 && i < json_array_size(first_sample); i++) {
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
        INFO("Parsed EEG values: %.2f, %.2f, %.2f, %.2f, %.2f", 
              eeg_values[0], eeg_values[1], eeg_values[2], eeg_values[3], eeg_values[4]);
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
        // Update connected status based on WebSocket state
        connected = (ws && ws->getReadyState() == easywsclient::OPEN);

        lights[CONNECTION_LIGHT].setBrightness(connected ? 1.f : 0.f);

        std::lock_guard<std::mutex> lock(dataMutex);
        
        // Output EEG values
        for (int i = 0; i < 5; i++) {
            outputs[EEG1_OUTPUT + i].setVoltage(eeg_values[i]);
        }

        // Output brain wave bands
        for (int i = 0; i < 5; i++) {
            outputs[DELTA_OUTPUT + i].setVoltage(brain_waves[i]);
        }
    }

};

struct MuseHeadbandWidget : ModuleWidget {
    struct ThemedLabel : Widget {
        std::string text;
        bool bold;
        
        ThemedLabel(Vec pos, const std::string& text, bool bold = false) {
            this->box.pos = pos;
            this->text = text;
            this->bold = bold;
        }

        void draw(const DrawArgs& args) override {
            nvgFontSize(args.vg, bold ? 13.0 : 11.0);
            nvgFontFaceId(args.vg, APP->window->uiFont->handle);
            nvgTextLetterSpacing(args.vg, 0.0);
            nvgTextAlign(args.vg, NVG_ALIGN_CENTER);
            nvgFillColor(args.vg, nvgRGB(0, 0, 0));
            nvgText(args.vg, 0, 0, text.c_str(), NULL);
        }
    };

    MuseHeadbandWidget(MuseHeadband* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/MuseHeadband.svg")));

        float col_a_center = 10;
        float col_b_center = 40;

        float row_start = 40;

        // Module title
        addChild(new ThemedLabel(mm2px(Vec(col_a_center, 10)), "MUSE", true));

        // Connection LED and label
        addChild(new ThemedLabel(mm2px(Vec(col_a_center, 20)), "CONNECTION"));
        addChild(createLightCentered<MediumLight<GreenRedLight>>(
            mm2px(Vec(col_a_center, 27)), 
            module, 
            MuseHeadband::CONNECTION_LIGHT
        ));

        // EEG Section
        addChild(new ThemedLabel(mm2px(Vec(col_a_center, row_start)), "EEG", true));
        
        // EEG Channel outputs
        const float eegStart = row_start + 15;
        const float eegSpacing = 15;
        for (int i = 0; i < 5; i++) {
            addChild(new ThemedLabel(
                mm2px(Vec(col_a_center, eegStart + i * eegSpacing - 5)),
                string::f("CH %d", i + 1)
            ));
            addOutput(createOutputCentered<PJ301MPort>(
                mm2px(Vec(col_a_center, eegStart + i * eegSpacing)), 
                module, 
                MuseHeadband::EEG1_OUTPUT + i
            ));
        }

        // Brainwave Section
        addChild(new ThemedLabel(mm2px(Vec(col_b_center, row_start)), "WAVES", true));

        // Brainwave outputs
        const float waveStart = row_start + 15;
        const float waveSpacing = 15;
        const char* waveLabels[] = {
            "δ DELTA", "θ THETA", "α ALPHA", "β BETA", "γ GAMMA"
        };
        
        for (int i = 0; i < 5; i++) {
            addChild(new ThemedLabel(
                mm2px(Vec(col_b_center, waveStart + i * waveSpacing - 5)),
                waveLabels[i]
            ));
            addOutput(createOutputCentered<PJ301MPort>(
                mm2px(Vec(col_b_center, waveStart + i * waveSpacing)), 
                module, 
                MuseHeadband::DELTA_OUTPUT + i
            ));
        }
    }
};

Model* modelMuseHeadband = createModel<MuseHeadband, MuseHeadbandWidget>("MuseHeadband");

