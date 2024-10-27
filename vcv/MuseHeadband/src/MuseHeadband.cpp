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
#include <vector>

#include "WebSocket.hpp"

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
    std::vector<std::vector<float>> eeg_samples;
    float eeg_values[5] = {0.f, 0.f, 0.f, 0.f, 0.f};
    float brain_waves[5] = {0.f, 0.f, 0.f, 0.f, 0.f}; // delta, theta, alpha, beta, gamma
    int eeg_sample_rate = 256; // Default value, will be updated from JSON
    float sample_time = 0.f; // Accumulator for sample timing




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

        // Parse EEG sample rate
        json_t* sample_rate = json_object_get(root, "eeg_sample_rate");
        if (json_is_integer(sample_rate)) {
            eeg_sample_rate = json_integer_value(sample_rate);
            INFO("EEG sample rate: %d Hz", eeg_sample_rate);
        }

        json_t* eeg_buffer = json_object_get(root, "eeg_buffer");
        if (json_is_array(eeg_buffer)) {
            std::lock_guard<std::mutex> lock(dataMutex);
            size_t buffer_size = json_array_size(eeg_buffer);
            eeg_samples.clear();
            eeg_samples.reserve(buffer_size);

            for (size_t i = 0; i < buffer_size; i++) {
                json_t* sample = json_array_get(eeg_buffer, i);
                if (json_is_array(sample)) {
                    std::vector<float> sample_values;
                    size_t sample_size = json_array_size(sample);
                    sample_values.reserve(sample_size);

                    for (size_t j = 0; j < sample_size; j++) {
                        json_t* value = json_array_get(sample, j);
                        if (json_is_number(value)) {
                            sample_values.push_back(json_number_value(value) / 1000.0f); // Convert to millivolts
                        }
                    }

                    eeg_samples.push_back(std::move(sample_values));
                }
            }
            INFO("Parsed %zu EEG samples", eeg_samples.size());

            // Update eeg_values with the last sample for backwards compatibility
            if (!eeg_samples.empty()) {
                const auto& last_sample = eeg_samples.back();
                for (size_t i = 0; i < 5 && i < last_sample.size(); i++) {
                    eeg_values[i] = last_sample[i];
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
        INFO("Parsed EEG values (last sample): %.2f, %.2f, %.2f, %.2f, %.2f", 
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
        
        // Calculate time for one sample
        float sample_period = 1.f / eeg_sample_rate;
        
        // Accumulate time
        sample_time += args.sampleTime;
        
        // Check if it's time to output a new sample
        if (sample_time >= sample_period) {
            sample_time -= sample_period;
            
            // Output EEG values
            if (!eeg_samples.empty()) {
                std::vector<float> current_sample = std::move(eeg_samples.front());
                eeg_samples.erase(eeg_samples.begin());

                for (int i = 0; i < 5 && i < current_sample.size(); i++) {
                    outputs[EEG1_OUTPUT + i].setVoltage(current_sample[i] * 10.0);
                }
            }

            // Output brain wave bands
            for (int i = 0; i < 5; i++) {
                outputs[DELTA_OUTPUT + i].setVoltage(brain_waves[i]);
            }
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










