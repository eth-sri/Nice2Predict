/*
   Copyright 2015 Software Reliability Lab, ETH Zurich

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
 */

#ifndef BASE_NBEST_H_
#define BASE_NBEST_H_

#include <algorithm>
#include <unordered_map>
#include <utility>
#include <vector>

// A helper class to keep the N-best value of certain type.
template<class Item, class Score>
class NBest {
public:
  void AddScoreToItem(const Item& item, const Score& added_score) {
    item_map_[item] += added_score;
  }

  typedef std::vector<std::pair<Score, Item> > SortedAllBest;

  struct SortedNBest {
    explicit SortedNBest(
        typename NBest<Item, Score>::SortedAllBest::const_iterator b,
        typename NBest<Item, Score>::SortedAllBest::const_iterator e) : begin_(b), end_(e) {
    }

    typename NBest<Item, Score>::SortedAllBest::const_iterator begin() { return begin_; }
    typename NBest<Item, Score>::SortedAllBest::const_iterator end() { return end_; }

    typename NBest<Item, Score>::SortedAllBest::const_iterator begin_, end_;
  };

  SortedNBest produce_nbest(unsigned n) {
    sorted_nbest_.resize(item_map_.size());
    int c = 0;
    for (auto it = item_map_.begin(); it != item_map_.end(); ++it) {
      sorted_nbest_[c].first = it->second;
      sorted_nbest_[c].second = it->first;
      ++c;
    }
    std::sort(sorted_nbest_.begin(), sorted_nbest_.end(), std::greater<std::pair<Score, Item> >());
    return SortedNBest(
        sorted_nbest_.begin(),
        (sorted_nbest_.size() > n) ? (sorted_nbest_.begin() + n) : sorted_nbest_.end());
  }

private:
  std::unordered_map<Item, Score> item_map_;
  SortedAllBest sorted_nbest_;
};



#endif /* BASE_NBEST_H_ */
