#ifndef ANIRA_STATEFULRNNCONFIG_2048_H
#define ANIRA_STATEFULRNNCONFIG_2048_H

#include <anira/anira.h>

static anira::InferenceConfig statefulRNNConfig_2048(
#ifdef USE_LIBTORCH
        STATEFULLSTM_MODELS_PATH_PYTORCH + std::string("/model_0/stateful-lstm-dynamic.pt"),
        {{2048, 1, 1}},
        {{2048, 1, 1}},
#endif
#ifdef USE_ONNXRUNTIME
        STATEFULLSTM_MODELS_PATH_PYTORCH + std::string("/model_0/stateful-lstm-libtorch.onnx"),
        {{2048, 1, 1}},
        {{2048, 1, 1}},
#endif
#ifdef USE_TFLITE
        STATEFULLSTM_MODELS_PATH_TENSORFLOW + std::string("/model_0/stateful-lstm-dynamic.tflite"),
        {{1, 2048, 1}},
        {{1, 2048, 1}},
#endif

        42.66f,
        0,
        false,
        true
);

#endif //ANIRA_STATEFULRNNCONFIG_2048_H
