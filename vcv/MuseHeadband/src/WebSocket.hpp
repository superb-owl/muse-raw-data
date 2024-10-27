
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
            if (state != OPEN) {
                WARN("Socket not open");
                return false;
            }
            
            // Use a large temporary buffer for receiving data
            char tempBuffer[131072];  // 128KB buffer
            ssize_t bytes = recv(sockfd, tempBuffer, sizeof(tempBuffer), 0);
            
            if (bytes > 0) {
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
                    // DEBUG("Incomplete frame, buffered %zu bytes", messageBuffer.size());
                }
            }
            
            WARN("Failed to receive data");
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
