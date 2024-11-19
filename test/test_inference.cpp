#include <thread>
#include <stdint.h>
#include <chrono>

#include "gtest/gtest.h"
#include <anira/anira.h>
#include <anira/utils/helperFunctions.h>

#include "../extras/desktop/models/hybrid-nn/HybridNNConfig.h"
#include "../extras/desktop/models/hybrid-nn/HybridNNPrePostProcessor.h"
#include "../../../extras/desktop/models/hybrid-nn/HybridNNBypassProcessor.h" // Only needed for round trip test
#include "WavReader.h"


using namespace anira;

static void fill_buffer(AudioBufferF &buffer){
    for (size_t i = 0; i < buffer.get_num_samples(); i++){
        double new_val = anira::random_sample();
        buffer.set_sample(0, i, new_val);
    }
}

static void push_buffer_to_ringbuffer(AudioBufferF const &buffer, RingBuffer &ringbuffer){
    for (size_t i = 0; i < buffer.get_num_samples(); i++){
        ringbuffer.push_sample(0, buffer.get_sample(0, i));
    }
}

union Float_t{
    Float_t(float num = 0.0f) : f(num) {}

    int32_t i;
    float f;
};

TEST(Test_Inference, passthrough){

    size_t bufferSize = 2048;
    double sampleRate = 48000;

    InferenceConfig inferenceConfig = hybridnn_config;
    anira::ContextConfig anira_context_config;

    // Create a pre- and post-processor instance
    HybridNNPrePostProcessor myPrePostProcessor;
    HybridNNBypassProcessor bypass_processor(inferenceConfig);
    // Create an InferenceHandler instance
    anira::InferenceHandler inferenceHandler(myPrePostProcessor, inferenceConfig, bypass_processor, anira_context_config);

    // Create a HostAudioConfig instance containing the host config infos
    anira::HostAudioConfig audioConfig {
        bufferSize,
        sampleRate
    };  



    // Allocate memory for audio processing
    inferenceHandler.prepare(audioConfig);
    // Select the inference backend
    inferenceHandler.set_inference_backend(anira::CUSTOM);

    int latency_offset = inferenceHandler.get_latency();

    RingBuffer ring_buffer;
    ring_buffer.initialize_with_positions(1, latency_offset+bufferSize);

    //fill the buffer with zeroes to compensate for the latency
    for (size_t i = 0; i < latency_offset; i++){
        ring_buffer.push_sample(0, 0);
    }    

    AudioBufferF test_buffer(1, bufferSize);

    std::cout << "starting test" << std::endl;
    for (size_t repeat = 0; repeat < 50; repeat++)
    {
        fill_buffer(test_buffer);
        push_buffer_to_ringbuffer(test_buffer, ring_buffer);
        
        inferenceHandler.process(test_buffer.get_array_of_write_pointers(), bufferSize);

        for (size_t i = 0; i < bufferSize; i++){
            EXPECT_FLOAT_EQ(
                ring_buffer.pop_sample(0),
                test_buffer.get_sample(0, i )
            );
        }
    }
}


TEST(Test_Inference, inference_libtorch){

    size_t buffer_size = 1024;
    double sample_rate = 44100;

    // because of the method used for inference in the jupyter notebook, an additional offset of 149 samples has to be applied to the reference data
    size_t reference_offset = 149;

    // read reference data
    std::vector<float> data_input;
    std::vector<float> data_predicted;

    read_wav(string(GUITARLSTM_MODELS_PATH_PYTORCH) + "/model_0/x_test.wav", data_input);
    read_wav(string(GUITARLSTM_MODELS_PATH_PYTORCH) + "/model_0/y_pred.wav", data_predicted);

    ASSERT_TRUE(data_input.size() > 0);
    ASSERT_TRUE(data_predicted.size() > 0);

    // setup inference
    InferenceConfig inference_config = hybridnn_config;
    anira::ContextConfig anira_context_config;

    // Create a pre- and post-processor instance
    HybridNNPrePostProcessor pp_processor;
    HybridNNBypassProcessor bypass_processor(inference_config);
    // Create an InferenceHandler instance
    anira::InferenceHandler inference_handler(pp_processor, inference_config, bypass_processor, anira_context_config);

    // Create a HostAudioConfig instance containing the host config infos
    anira::HostAudioConfig audio_config {
        buffer_size,
        sample_rate
    };  

    // Allocate memory for audio processing
    inference_handler.prepare(audio_config);
    // Select the inference backend
    inference_handler.set_inference_backend(anira::LIBTORCH);

    int latency_offset = inference_handler.get_latency();

    RingBuffer ring_buffer;
    ring_buffer.initialize_with_positions(1, latency_offset + buffer_size + reference_offset);
    
    //fill the buffer with zeroes to compensate for the latency
    for (size_t i = 0; i < latency_offset + reference_offset; i++){
        ring_buffer.push_sample(0, 0);
    }    

    AudioBufferF test_buffer(1, buffer_size);

    std::cout << "starting test" << std::endl;

    int ulp_max = 0;
    for (size_t repeat = 0; repeat < 150; repeat++)
    {
        for (size_t i = 0; i < buffer_size; i++)
        {
            test_buffer.set_sample(0, i, data_input.at((repeat*buffer_size)+i));
            ring_buffer.push_sample(0, data_predicted.at((repeat*buffer_size)+i));
        }
        
        size_t prev_samples = inference_handler.get_inference_manager().get_num_received_samples();

        inference_handler.process(test_buffer.get_array_of_write_pointers(), buffer_size);
        
        // wait until the block was properly processed
        while (!(inference_handler.get_inference_manager().get_num_received_samples() >= prev_samples)){
            std::this_thread::sleep_for(std::chrono::nanoseconds (10));
        }        

        for (size_t i = 0; i < buffer_size; i++){
            float reference = ring_buffer.pop_sample(0);
            float processed = test_buffer.get_sample(0, i);
                        
            if (repeat*buffer_size + i < latency_offset + reference_offset){
                ASSERT_FLOAT_EQ(reference, 0);
            } else {
                // TODO find a better epsilon!
                float epsilon = max(abs(reference), abs(processed)) * 1e-6f + 1e-7f; 
                int ulp_diff = abs(Float_t(reference).i - Float_t(processed).i);
                ulp_max = max(ulp_max, ulp_diff);
                ASSERT_NEAR(reference, processed, epsilon) << "repeat=" << repeat << ", i=" << i << ", total sample nr: " << repeat*buffer_size + i  << ", ULP diff: " << ulp_diff << std::endl;

            }
        }
    }
}


TEST(Test_Inference, inference_onnx){

    size_t buffer_size = 1024;
    double sample_rate = 44100;

    // because of the method used for inference in the jupyter notebook, an additional offset of 149 samples has to be applied to the reference data
    size_t reference_offset = 149;

    // read reference data
    std::vector<float> data_input;
    std::vector<float> data_predicted;

    read_wav(string(GUITARLSTM_MODELS_PATH_PYTORCH) + "/model_0/x_test.wav", data_input);
    read_wav(string(GUITARLSTM_MODELS_PATH_PYTORCH) + "/model_0/y_pred.wav", data_predicted);

    ASSERT_TRUE(data_input.size() > 0);
    ASSERT_TRUE(data_predicted.size() > 0);

    // setup inference
    InferenceConfig inference_config = hybridnn_config;
    anira::ContextConfig anira_context_config;

    // Create a pre- and post-processor instance
    HybridNNPrePostProcessor pp_processor;
    HybridNNBypassProcessor bypass_processor(inference_config);
    // Create an InferenceHandler instance
    anira::InferenceHandler inference_handler(pp_processor, inference_config, bypass_processor, anira_context_config);

    // Create a HostAudioConfig instance containing the host config infos
    anira::HostAudioConfig audio_config {
        buffer_size,
        sample_rate
    };  

    // Allocate memory for audio processing
    inference_handler.prepare(audio_config);
    // Select the inference backend
    inference_handler.set_inference_backend(anira::LIBTORCH);

    int latency_offset = inference_handler.get_latency();

    RingBuffer ring_buffer;
    ring_buffer.initialize_with_positions(1, latency_offset + buffer_size + reference_offset);
    
    //fill the buffer with zeroes to compensate for the latency
    for (size_t i = 0; i < latency_offset + reference_offset; i++){
        ring_buffer.push_sample(0, 0);
    }    

    AudioBufferF test_buffer(1, buffer_size);

    std::cout << "starting test" << std::endl;

    int ulp_max = 0;
    for (size_t repeat = 0; repeat < 150; repeat++)
    {
        for (size_t i = 0; i < buffer_size; i++)
        {
            test_buffer.set_sample(0, i, data_input.at((repeat*buffer_size)+i));
            ring_buffer.push_sample(0, data_predicted.at((repeat*buffer_size)+i));
        }
        
        size_t prev_samples = inference_handler.get_inference_manager().get_num_received_samples();

        inference_handler.process(test_buffer.get_array_of_write_pointers(), buffer_size);
        
        // wait until the block was properly processed
        while (!(inference_handler.get_inference_manager().get_num_received_samples() >= prev_samples)){
            std::this_thread::sleep_for(std::chrono::nanoseconds (10));
        }        

        for (size_t i = 0; i < buffer_size; i++){
            float reference = ring_buffer.pop_sample(0);
            float processed = test_buffer.get_sample(0, i);
                        
            if (repeat*buffer_size + i < latency_offset + reference_offset){
                ASSERT_FLOAT_EQ(reference, 0);
            } else {
                // TODO find a better epsilon!
                float epsilon = max(abs(reference), abs(processed)) * 1e-6f + 2e-7f; 
                int ulp_diff = abs(Float_t(reference).i - Float_t(processed).i);
                ulp_max = max(ulp_max, ulp_diff);
                ASSERT_NEAR(reference, processed, epsilon) << "repeat=" << repeat << ", i=" << i << ", total sample nr: " << repeat*buffer_size + i  << ", ULP diff: " << ulp_diff << std::endl;

            }
        }
    }
}