// Copyright (c) 2021 Mobvoi Inc (Zhendong Peng)
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "utils/utils.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <queue>
#include <utility>
#include <vector>

namespace ppspeech {

float LogSumExp(float x, float y) {
  if (x <= -kFloatMax) return y;
  if (y <= -kFloatMax) return x;
  float max = std::max(x, y);
  return max + std::log(std::exp(x - max) + std::exp(y - max));
}

template <typename T>
struct ValueComp {
  bool operator()(const std::pair<T, int32_t>& lhs,
                  const std::pair<T, int32_t>& rhs) const {
    return lhs.first > rhs.first ||
           (lhs.first == rhs.first && lhs.second < rhs.second);
  }
};

template <typename T>
void TopK(const std::vector<T>& data,
          int32_t k,
          std::vector<T>* values,
          std::vector<int>* indices) {
  // k laggest T
  std::vector<std::pair<T, int32_t>> heap_data;  // (val, idx), smallest heap
  int n = data.size();

  for (int32_t i = 0; i < k && i < n; ++i) {
    heap_data.emplace_back(data[i], i);
  }

  std::priority_queue<std::pair<T, int32_t>,
                      std::vector<std::pair<T, int32_t>>,
                      ValueComp<T>>
      pq(ValueComp<T>(), std::move(heap_data));

  for (int32_t i = k; i < n; ++i) {
    if (pq.top().first < data[i]) {
      pq.pop();
      pq.emplace(data[i], i);
    }
  }

  values->resize(std::min(k, n));
  indices->resize(std::min(k, n));
  int32_t cur = values->size() - 1;
  while (!pq.empty()) {
    const auto& item = pq.top();
    (*values)[cur] = item.first;
    (*indices)[cur] = item.second;
    pq.pop();
    cur -= 1;
  }
}

template void TopK<float>(const std::vector<float>& data,
                          int32_t k,
                          std::vector<float>* values,
                          std::vector<int>* indices);

}  // namespace ppspeech