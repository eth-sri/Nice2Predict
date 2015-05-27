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

#include <stdio.h>
#include <unordered_map>
#include <vector>

#include "gflags/gflags.h"
#include "glog/logging.h"

#include "stringset.h"
#include "base_online_classifier.h"
#include "hash_classifier.h"


DEFINE_string(lm_file, "/home/veselin/ws/unsup/NNAI/bigdata/train_10", "Location of words to train LM on.");

class WordFrequency {
public:
  WordFrequency() : most_freq_word(-1), sum_freq(0.0) {}

  typedef std::unordered_map<int, double> WordFreq;
  WordFreq freq;

  int most_freq_word;
  double sum_freq;

  void FindMostFreqWord() {
    double max_freq = 0.0;
    sum_freq = 0.0;
    for (auto it = freq.begin(); it != freq.end(); ++it) {
      sum_freq += it->second;
      if (it->second > max_freq) {
        most_freq_word = it->first;
        max_freq = it->second;
      }
    }

  }
};



typedef std::unordered_map<int, WordFrequency> BiGramFreq;
WordFrequency unigrams;
BiGramFreq bigrams;
typedef std::unordered_map<int, BiGramFreq> TriGramFreq;
TriGramFreq trigrams;
std::vector<int> train_words, eval_words;
int eol;
StringSet ss;

const char* ToStrSS(int num) {
  if (num < 0) return "-1";
  return ss.getString(num);
}

void ReadInput() {
  FILE* f = fopen(FLAGS_lm_file.c_str(), "rt");
  eol = ss.addString("</s>");
  std::vector<int> words;
  while (!feof(f)) {
    char word_str[256];
    char c = ' ';
    if (fscanf(f, "%256s%c", word_str, &c) < 1) break;
    int word = ss.addString(word_str);
    words.push_back(word);
    if (c == '\n') words.push_back(eol);
  }
  int mid = words.size() / 2;
  train_words.assign(words.begin(), words.begin() + mid);
  eval_words.assign(words.begin() + mid, words.end());
  LOG(INFO) << "Loaded " << train_words.size() << " training words and " << eval_words.size() << " eval words.";
}

void TrainLM() {
  LOG(INFO) << "Computing LM...";
  int last_word = -1, last_last_word = -1;
  for (int word : train_words) {
    unigrams.freq[word]++;
    bigrams[last_word].freq[word]++;
    trigrams[last_last_word][last_word].freq[word]++;
    last_last_word = last_word;
    last_word = word;
  }
  unigrams.FindMostFreqWord();
  for (auto it = bigrams.begin(); it != bigrams.end(); ++it) {
    it->second.most_freq_word = unigrams.most_freq_word;
    it->second.FindMostFreqWord();
  }
  for (auto it1 = trigrams.begin(); it1 != trigrams.end(); ++it1) {
    for (auto it = it1->second.begin(); it != it1->second.end(); ++it) {
      it->second.most_freq_word = unigrams.most_freq_word;
      it->second.FindMostFreqWord();
    }
  }
}

void EvalLM() {
  LOG(INFO) << "Errors of LM:";
  int last_word = -1, last_last_word = -1;
  int num_incorrect = 0, num_correct = 0;
  for (int word : eval_words) {
    int mfw = bigrams[last_word].most_freq_word;
    if (mfw == -1) mfw = unigrams.most_freq_word;
    if (trigrams[last_last_word].count(last_word)) {
      auto& f = trigrams[last_last_word][last_word];
      mfw = f.most_freq_word;
    }

    bool correct = (mfw == word);
    if (correct) ++num_correct; else ++num_incorrect;
    last_last_word = last_word;
    last_word = word;
  }
  LOG(INFO) << "Correct: " << num_correct << ", incorrect: " << num_incorrect;
}

BaseOnlineClassifier cls(1.0, 0.1, 0.1, BaseOnlineClassifier::RegularizerType::L_INF);

void TrainCLS(int pass) {
  LOG(INFO) << "Computing BaseOnlineClassifier, pass..." << pass;
  int last_word = -1, last_last_word = -1;
  for (int word : train_words) {
    ClassifyInstance instance;
    instance.AddGenericFeature(last_word, 1);
    instance.AddGenericFeature(last_last_word * 5643647, 1);
    instance.AddGenericFeature(last_last_word * 4354357 ^ last_word, 1);
    if (pass == 0)
      cls.AddQuery(word, instance);
    else
      cls.Train(word, instance);

    last_last_word = last_word;
    last_word = word;
  }
  if (pass == 0)
    cls.AllQueriesAdded();
  LOG(INFO) << cls.DebugString();
}


void EvalCLS() {
  LOG(INFO) << "Evaluating BaseOnlineClassifier...";
  int last_word = -1, last_last_word = -1;
  int num_incorrect = 0, num_correct = 0;
  int sample = 0;
  for (int word : eval_words) {
    ClassifyInstance instance;
    instance.AddGenericFeature(last_word, 1);
    instance.AddGenericFeature(last_last_word * 5643647, 1);
    instance.AddGenericFeature(last_last_word * 4354357 ^ last_word, 1);
    int predicted_word = std::get<0>(cls.Classify(instance));
    if (predicted_word == BaseOnlineClassifier::NO_LABEL) predicted_word = unigrams.most_freq_word;

    if (word == predicted_word) {
      ++num_correct;
    } else {
      ++num_incorrect;
    }
/*
    if (sample < 3) {
      LOG(INFO) << "Guessing " << ToStrSS(word) << ", predicted " << ToStrSS(predicted_word);
      for (auto p : instance.GetGenericFeatures()) {
        LOG(INFO) << "Feature: " << cls.DebugBestLabelsForFeature(&ss, p.first, 3);
      }
      LOG(INFO) << "";
    }*/
    ++sample;

    last_last_word = last_word;
    last_word = word;
  }
  LOG(INFO) << "Correct: " << num_correct << ", incorrect: " << num_incorrect;
}


int main(int argc, char** argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  ReadInput();
  TrainLM();
  EvalLM();

  for (int pass = 0; pass < 12; ++pass) {
    TrainCLS(pass);
    EvalCLS();
  }

  return 0;
}


