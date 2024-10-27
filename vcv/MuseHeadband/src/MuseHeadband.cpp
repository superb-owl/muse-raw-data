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

// Data structures for tracking statistics
struct ChannelStats {
    std::vector<float> samples;
    float max = -std::numeric_limits<float>::infinity();
    float min = std::numeric_limits<float>::infinity();
};

// Function to normalize a value
float normalizeValue(float value, const ChannelStats& stats) {
    float range = stats.max - stats.min;
    if (range == 0) return stats.min;
    float normalized = (value - stats.min) / range;
    return (normalized - 0.5f) * 10.f;
}


void printChannelStats(const ChannelStats& stats, float sample) {
    float norm = normalizeValue(sample, stats);
    INFO("Samples: %d, Max: %f, Min: %f, Current: %f, Normalized: %f", 
        stats.samples.size(), stats.max, stats.min, sample, norm);
}

// Function to update channel statistics
void updateChannelStats(ChannelStats& stats, float sample, float sampleRate) {
    stats.samples.push_back(sample);
    float secondsToKeep = 1.f;
    while (stats.samples.size() > sampleRate * secondsToKeep) {
        stats.samples.erase(stats.samples.begin());
    }
    stats.max = -std::numeric_limits<float>::infinity();
    stats.min = std::numeric_limits<float>::infinity();
    for (float s : stats.samples) {
        stats.max = std::max(stats.max, s);
        stats.min = std::min(stats.min, s);
    }
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
        PPG1_OUTPUT,
        PPG2_OUTPUT,
        PPG3_OUTPUT,
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
    std::vector<std::vector<float>> ppg_samples;
    std::vector<ChannelStats> eegStats;
    std::vector<ChannelStats> ppgStats;
    int sample_rate = 256;
    float sample_time = 0.f; // Accumulator for sample timing

    float last_sample_time = 0.f;

    MuseHeadband() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        // Initialize eegStats and ppgStats
        eegStats.resize(5); // 5 EEG channels
        ppgStats.resize(3); // 3 PPG channels

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
        configOutput(PPG1_OUTPUT, "PPG Channel 1");
        configOutput(PPG2_OUTPUT, "PPG Channel 2");
        configOutput(PPG3_OUTPUT, "PPG Channel 3");
        INFO("MuseHeadband loaded");

        // Start WebSocket connection thread
        wsThread = std::thread([this]() {
            while (running) {
                if (!ws || ws->getReadyState() != easywsclient::OPEN) {
                    ws.reset(easywsclient::WebSocket::create_connection("ws://localhost:8765"));
                    connected = (ws != nullptr);
                    if (connected) {
                        INFO("Connected to Muse Headband server");
                    } else {
                        WARN("Failed to connect to Muse Headband server");
                    }
                }
                
                if (connected) {
                    char buffer[2048];
                    size_t bytesRead;
                    if (ws->receive(buffer, sizeof(buffer), &bytesRead)) {
                        buffer[bytesRead] = '\0';
                        std::string message(buffer, bytesRead);
                        
                        // Check if we have a complete JSON message
                        if (!message.empty() && 
                            message[0] == '{' && 
                            message[message.length()-1] == '}') {
                            parseMuseData(message.c_str());
                        } else {
                            WARN("Invalid JSON message: %s", message.c_str());
                        }
                    } else {
                        WARN("Failed to receive message from Muse Headband server");
                    }
                } else {
                    WARN("Not connected to Muse Headband server");
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
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

        json_t* timestamp = json_object_get(root, "timestamp");
        if (!json_is_number(timestamp)) {
            WARN("Invalid timestamp in JSON");
            return;
        }
        float timestamp_value = json_number_value(timestamp);
        INFO("timestamp: %f", timestamp_value);

        json_t* eeg_channels = json_object_get(root, "eeg_channels");
        if (!json_is_array(eeg_channels)) {
            WARN("No EEG channels found in JSON");
            return;
        }
        size_t num_eeg_channels = json_array_size(eeg_channels);
        std::vector<float> eeg_channel_values;
        for (size_t i = 0; i < num_eeg_channels; i++) {
            json_t* value = json_array_get(eeg_channels, i);
            if (!json_is_number(value)) {
                WARN("Invalid EEG sample value");
                return;
            }
            eeg_channel_values.push_back(json_number_value(value));
        }
        INFO("Received EEG sample: %f, %f, %f, %f, %f", 
            eeg_channel_values[0], eeg_channel_values[1], eeg_channel_values[2], eeg_channel_values[3], eeg_channel_values[4]);
        eeg_samples.push_back(std::move(eeg_channel_values));

        json_t* ppg_channels = json_object_get(root, "ppg_channels");
        if (!json_is_array(ppg_channels)) {
            WARN("No PPG channels found in JSON");
            return;
        }
        size_t num_ppg_channels = json_array_size(ppg_channels);
        std::vector<float> ppg_channel_values;
        for (size_t i = 0; i < num_ppg_channels; i++) {
            json_t* value = json_array_get(ppg_channels, i);
            if (!json_is_number(value)) {
                WARN("Invalid PPG sample value");
                return;
            }
            ppg_channel_values.push_back(json_number_value(value));
        }
        INFO("Received PPG sample: %f, %f, %f", 
            ppg_channel_values[0], ppg_channel_values[1], ppg_channel_values[2]);
        ppg_samples.push_back(std::move(ppg_channel_values));

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

        sample_time += args.sampleTime;

        float sample_period = 1.f / sample_rate;

        if (sample_time - last_sample_time >= sample_period) {
            last_sample_time = sample_time;
            if (!eeg_samples.empty()) {
                std::vector<float> current_sample = std::move(eeg_samples.front());
                eeg_samples.erase(eeg_samples.begin());

                for (int i = 0; i < 5 && i < current_sample.size(); i++) {
                    updateChannelStats(eegStats[i], current_sample[i], sample_rate);
                    outputs[EEG1_OUTPUT + i].setVoltage(normalizeValue(current_sample[i], eegStats[i]));
                }
            }
            if (!ppg_samples.empty()) {
                std::vector<float> current_sample = std::move(ppg_samples.front());
                ppg_samples.erase(ppg_samples.begin());

                for (int i = 0; i < 3 && i < current_sample.size(); i++) {
                    updateChannelStats(ppgStats[i], current_sample[i], sample_rate);
                    outputs[PPG1_OUTPUT + i].setVoltage(normalizeValue(current_sample[i], ppgStats[i]));
                }
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
        float col_c_center = 70;

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

        // PPG Section
        addChild(new ThemedLabel(mm2px(Vec(col_c_center, row_start)), "PPG", true));
        const float ppgStart = row_start + 15;
        const float ppgSpacing = 15;
        for (int i = 0; i < 3; i++) {
            addChild(new ThemedLabel(
                mm2px(Vec(col_c_center, ppgStart + i * ppgSpacing - 5)),
                string::f("CH %d", i + 1)
            ));
            addOutput(createOutputCentered<PJ301MPort>(
                mm2px(Vec(col_c_center, ppgStart + i * ppgSpacing)), 
                module, 
                MuseHeadband::PPG1_OUTPUT + i
            ));
        }
    }
};

Model* modelMuseHeadband = createModel<MuseHeadband, MuseHeadbandWidget>("MuseHeadband");










