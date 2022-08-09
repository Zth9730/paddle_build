// Copyright (c) 2020 Mobvoi Inc (Binbin Zhang)
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


#pragma once

#include <vector>

namespace ppspeech {

enum SearchType {
    kPrefixBeamSearch = 0,
    kWfstBeamSearch = 1,
};

class SearchInterface {
public:
    virtual ~SearchInterface(){}
    virtual void Search(const std::vector<std::vector<float>>& logp) = 0;
    virtual void Reset() = 0;
    virtual void FinalizeSearch() = 0;

    virtual SearchType Type() const = 0;
    // n-best inputs id
    virtual const std::vector<std::vector<int>>& Inputs() const = 0;
    // n-best outputs id
    virtual const std::vector<std::vector<int>>& Outputs() const = 0;
    // n-best likelihood
    virtual const std::vector<float>& Likelihood() const = 0;
    // n-best timestamp
    virtual const std::vector<std::vector<int>>& Times() const = 0;
};

} // namespace ppspeech