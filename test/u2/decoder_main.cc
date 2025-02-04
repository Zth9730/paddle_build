// Copyright (c) 2020 Mobvoi Inc (Binbin Zhang, Di Wu)
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <iomanip>
#include <thread>
#include <utility>

#include "decoder/params.h"
#include "frontend/wav.h"
#include "utils/flags.h"
#include "utils/string.h"
#include "utils/thread_pool.h"
#include "utils/timer.h"
#include "utils/utils.h"

// profiler
#ifdef USE_PROFILING
#include "paddle/fluid/platform/profiler.h"
#include "paddle/fluid/platform/profiler/profiler.h"

using paddle::platform::EnableHostEventRecorder;
using paddle::platform::Profiler;
using paddle::platform::ProfilerOptions;
using paddle::platform::ProfilerResult;
using paddle::platform::RecordEvent;
using paddle::platform::RecordInstantEvent;
using paddle::platform::TracerEventType;
#endif

DEFINE_bool(simulate_streaming, false, "simulate streaming input");
DEFINE_bool(output_nbest, false, "output n-best of decode result");
DEFINE_string(wav_path, "", "single wave path");
DEFINE_string(wav_scp, "", "input wav scp");
DEFINE_string(result, "", "result output file");
DEFINE_bool(continuous_decoding, false, "continuous decoding mode");
DEFINE_int32(thread_num, 1, "num of decode thread");

std::shared_ptr<ppspeech::DecodeOptions> g_decode_config;
std::shared_ptr<ppspeech::FeaturePipelineConfig> g_feature_config;
std::shared_ptr<ppspeech::DecodeResource> g_decode_resource;

std::ofstream g_result;
std::mutex g_mutex;
int g_total_waves_dur = 0;
int g_total_decode_time = 0;

void decode(std::pair<std::string, std::string> wav) {
  ppspeech::WavReader wav_reader(wav.second);
  int num_samples = wav_reader.num_samples();
  CHECK_EQ(wav_reader.sample_rate(), FLAGS_sample_rate);

  auto feature_pipeline =
      std::make_shared<ppspeech::FeaturePipeline>(*g_feature_config);
  feature_pipeline->AcceptWaveform(wav_reader.data(), num_samples);
  feature_pipeline->SetInputFinished();
  LOG(INFO) << "num frames " << feature_pipeline->num_frames();

  ppspeech::AsrDecoder decoder(
      feature_pipeline, g_decode_resource, *g_decode_config);

  int wave_dur = static_cast<int>(static_cast<float>(num_samples) /
                                  wav_reader.sample_rate() * 1000);
  int decode_time = 0;
  std::string final_result;
  while (true) {
    ppspeech::Timer timer;
    ppspeech::DecodeState state = decoder.Decode();

    if (state == ppspeech::DecodeState::kEndFeats) {
      decoder.Rescoring();
    }

    int chunk_decode_time = timer.Elapsed();
    decode_time += chunk_decode_time;
    if (decoder.DecodedSomething()) {
      LOG(INFO) << "Partial result: " << decoder.result()[0].sentence;
    }

    if (FLAGS_continuous_decoding &&
        state == ppspeech::DecodeState::kEndpoint) {
      if (decoder.DecodedSomething()) {
        decoder.Rescoring();
        LOG(INFO) << "Final result (continuous decoding): "
                  << decoder.result()[0].sentence;
        final_result.append(decoder.result()[0].sentence);
      }
      decoder.ResetContinuousDecoding();
    }

    if (state == ppspeech::DecodeState::kEndFeats) {
      break;
    } else if (FLAGS_chunk_size > 0 && FLAGS_simulate_streaming) {
      float frame_shift_in_ms =
          static_cast<float>(g_feature_config->frame_shift) /
          wav_reader.sample_rate() * 1000;
      auto wait_time =
          decoder.num_frames_in_current_chunk() * frame_shift_in_ms -
          chunk_decode_time;

      if (wait_time > 0) {
        LOG(INFO) << "Simulate streaming, waiting for " << wait_time << "ms";
        std::this_thread::sleep_for(
            std::chrono::milliseconds(static_cast<int>(wait_time)));
      }
    }
  }

  if (decoder.DecodedSomething()) {
    final_result.append(decoder.result()[0].sentence);
  }
  LOG(INFO) << wav.first << ": Final result: " << final_result << std::endl;
  LOG(INFO) << "Decoded " << wave_dur << "ms audio taken " << decode_time
            << "ms.";

  g_mutex.lock();
  std::ostream& buffer = FLAGS_result.empty() ? std::cout : g_result;
  if (!FLAGS_output_nbest) {
    buffer << wav.first << " " << final_result << std::endl;
  } else {
    buffer << "wav " << wav.first << std::endl;
    auto& results = decoder.result();
    for (auto& r : results) {
      if (r.sentence.empty()) continue;
      buffer << "candidate " << r.score << " " << r.sentence << std::endl;
    }
  }

  g_total_waves_dur += wave_dur;
  g_total_decode_time += decode_time;
  g_mutex.unlock();
}

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();
  FLAGS_logtostderr = 1;

  // profiler
#ifdef USE_PROFILING
  EnableHostEventRecorder();
  ProfilerOptions options;
  options.trace_level = 2;
  options.trace_switch = 3;
  auto profiler = Profiler::Create(options);
  profiler->Prepare();
  profiler->Start();
#endif

  g_decode_config = ppspeech::InitDecodeOptionsFromFlags();
  g_feature_config = ppspeech::InitFeaturePipelineConfigFromFlags();
  g_decode_resource = ppspeech::InitDecodeResourceFromFlags();

  if (FLAGS_wav_path.empty() && FLAGS_wav_scp.empty()) {
    LOG(FATAL) << "Please provide the wave path or the wav scp.";
  }

  std::vector<std::pair<std::string, std::string>> waves;  // utt, wav
  if (!FLAGS_wav_path.empty()) {
    waves.emplace_back(make_pair("test", FLAGS_wav_path));
  } else {
    std::ifstream wav_scp(FLAGS_wav_scp);
    std::string line;
    while (getline(wav_scp, line)) {
      std::vector<std::string> strs;
      ppspeech::SplitString(line, &strs);
      CHECK_GE(strs.size(), 2);
      waves.emplace_back(make_pair(strs[0], strs[1]));
    }
  }

  if (!FLAGS_result.empty()) {
    g_result.open(FLAGS_result, std::ios::out);
  }

  {
    ThreadPool pool(FLAGS_thread_num);
    for (auto& wav : waves) {
      pool.enqueue(decode, wav);
    }
  }
  // decode(waves[0]);

  LOG(INFO) << "Total: decoded " << g_total_waves_dur << "ms audio taken "
            << g_total_decode_time << "ms.";
  LOG(INFO) << "RTF: " << std::setprecision(4)
            << static_cast<float>(g_total_decode_time) / g_total_waves_dur;

  // profiler
#ifdef USE_PROFILING
  auto profiler_result = profiler->Stop();
  profiler_result->Save("decoder.main.prof");
#endif
  return 0;
}