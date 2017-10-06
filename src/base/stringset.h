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

#ifndef STRINGSET_H_
#define STRINGSET_H_

#include <stdio.h>
#include <vector>

class StringSet {
public:
	StringSet();

	// Returns the index of the added string.
	int addString(const char* s);

	// Returns the string for an index. The returned pointer is guaranteed
	// to be valid only until the next modification of StringSet.
	const char* getString(int index) const;

	// Returns whether the set contains a given string.
	bool containsString(const char* s) const;

	// Returns the index of a string if exists or -1 otherwise.
	int findString(const char* s) const;

	// Saves the string set to a file.
	void saveToFile(FILE* f) const;

	// Loads the string set from a file.
	bool loadFromFile(FILE* f);

	// The number of entries in the string set.
	int numEntries() const { return m_hashTableLoad; }

	// Returns all strings in the StringSet.
	void getAllStrings(std::vector<int>* strings) const;

	// Return the data size. Entries after this number are free for use
	int getSize() const { return m_data.size(); }

private:
	// Returns the index of the added string.
	int addStringL(const char* s, int slen);

	// Returns the index of a string if exists or -1 otherwise.
	int findStringL(const char* s, int slen, int hash) const;

	// Computes hashcode for a string.
	int stringHash(const char* s, int slen) const;

	// Adds a value to the hashtable.
	void addHash(int hash, int value);
	void addHashNoRehash(int hash, int value);

	void rehashAll();

	std::vector<char> m_data;
	std::vector<int> m_hashes;
	int m_hashTableLoad;
};

#endif /* STRINGSET_H_ */
