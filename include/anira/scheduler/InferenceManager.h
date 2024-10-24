#ifndef ANIRA_INFERENCEMANAGER_H
#define ANIRA_INFERENCEMANAGER_H

#include "InferenceThread.h"
#include "AniraContext.h"
#include "../utils/HostAudioConfig.h"
#include "../InferenceConfig.h"
#include "../PrePostProcessor.h"

namespace anira {
    
class ANIRA_API InferenceManager {
public:
    InferenceManager() = delete;
    InferenceManager(PrePostProcessor &pp_processor, InferenceConfig& config, BackendBase* custom_processor);
    ~InferenceManager();

    void prepare(HostAudioConfig config);
    void process(float ** input_buffer, size_t input_samples);

    void set_backend(InferenceBackend new_inference_backend);
    InferenceBackend get_backend() const;

    int get_latency() const;

    // Required for unit test
    size_t get_num_received_samples() const;
    const AniraContext& get_anira_context() const;

    int get_missing_blocks() const;
    int get_session_id() const;

private:
    void process_input(float ** input_buffer, const size_t input_samples);
    void process_output(float ** input_buffer, const size_t input_samples);
    void clear_buffer(float ** input_buffer, const size_t input_samples);
    int calculate_latency();
    int calculate_buffer_adaptation(int m_host_buffer_size, int model_output_size);
    int max_num_inferences(int m_host_buffer_size, int model_output_size);
    int greatest_common_divisor(int a, int b);
    int leat_common_multiple(int a, int b);

private:
    std::shared_ptr<AniraContext> m_anira_context;

    InferenceConfig& m_inference_config;
    SessionElement& m_session;
    HostAudioConfig m_spec;

    size_t m_init_samples = 0;
    std::atomic<int> m_inference_counter {0};
};

} // namespace anira

#endif //ANIRA_INFERENCEMANAGER_H