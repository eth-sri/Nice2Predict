/*
 Copyright 2013 Software Reliability Lab, ETH Zurich

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

#include <string.h>
#include <string>

#include <glog/logging.h>

#include "stringset.h"

StringSet::StringSet() :
    m_hashTableLoad(0) {
}

int StringSet::addString(const char* s) {
  return addStringL(s, strlen(s));
}

const char* StringSet::getString(int index) const {
  return m_data.data() + index;
}

bool StringSet::containsString(const char* s) const {
  int len = strlen(s);
  return findStringL(s, len, stringHash(s, len)) != -1;
}

int StringSet::findString(const char* s) const {
  int len = strlen(s);
  return findStringL(s, len, stringHash(s, len));
}

int StringSet::addStringL(const char* s, int slen) {
  int hash = stringHash(s, slen);
  int pos = findStringL(s, slen, hash);
  if (pos == -1) {
    pos = m_data.size();
    addHash(hash, pos);
    m_data.insert(m_data.end(), s, s + slen + 1);
  }
  return pos;
}

int StringSet::findStringL(const char* s, int slen, int hash) const {
  if (m_hashes.size() == 0)
    return -1;
  size_t p = hash % m_hashes.size();
  while (m_hashes[p] != -1) {
    const char* s1 = getString(m_hashes[p]);
    if (strcmp(s, s1) == 0)
      return m_hashes[p];
    ++p;
    if (p == m_hashes.size())
      p = 0;
  }
  return -1;
}

int StringSet::stringHash(const char* s, int slen) const {
  unsigned long hash = 5381;
  for (int i = 0; i < slen; ++i) {
    hash = ((hash << 5) + hash) + static_cast<unsigned int>(s[i]);
  }
  return hash * 13;
}

void StringSet::addHash(int hash, int value) {
  while (static_cast<size_t>(m_hashTableLoad * 2) >= m_hashes.size()) {
    m_hashes.assign(m_hashes.size() * 2 + 3, -1);
    rehashAll();
  }
  addHashNoRehash(hash, value);
}

void StringSet::addHashNoRehash(int hash, int value) {
  ++m_hashTableLoad;
  size_t p = hash % m_hashes.size();
  while (m_hashes[p] != -1) {
    ++p;
    if (p == m_hashes.size())
      p = 0;
  }
  m_hashes[p] = value;
}

void StringSet::rehashAll() {
  m_hashTableLoad = 0;
  size_t pos = 0;
  while (pos < m_data.size()) {
    const char* str = getString(pos);
    int len = strlen(str);
    addHashNoRehash(stringHash(str, len), pos);
    pos += len + 1;
  }
}

void StringSet::getAllStrings(std::vector<int>* strings) const {
  size_t pos = 0;
  while (pos < m_data.size()) {
    const char* str = getString(pos);
    int len = strlen(str);
    strings->push_back(pos);
    pos += len + 1;
  }
}

void StringSet::saveToFile(FILE* f) const {
  int n = m_data.size();
  fwrite(&n, sizeof(int), 1, f);
  fwrite(m_data.data(), sizeof(char), m_data.size(), f);
  n = m_hashes.size();
  fwrite(&n, sizeof(int), 1, f);
}

bool StringSet::loadFromFile(FILE* f) {
  int n = 0;
  if (fread(&n, sizeof(int), 1, f) != 1)
    return false;
  m_data.resize(n, 0);
  if (fread(m_data.data(), sizeof(char), n, f) != m_data.size())
    return false;
  if (fread(&n, sizeof(int), 1, f) != 1)
    return false;  // Get the hash size, but ignore it.
  m_hashes.assign(n, -1);
  rehashAll();
  return true;
}

