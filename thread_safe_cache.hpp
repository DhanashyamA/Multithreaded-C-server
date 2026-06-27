#pragma once
#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <optional>

using namespace std;

class ThreadSafeCache {
private:
    // Maps a filename to its string content
    unordered_map<string, string> cache;
    
    // C++17 Readers-Writer Lock
    mutable shared_mutex rw_lock;

public:
    // FAST PATH: Read Lock
    optional<string> getFile(const string& filename) const {
        // Multiple threads can hold this lock simultaneously
        shared_lock<shared_mutex> read_lock(rw_lock);
        
        auto it = cache.find(filename);
        if (it != cache.end()) {
            return it->second; // Cache Hit!
        }
        
        return nullopt; // Cache Miss!
    }

    // SLOW PATH: Write Lock
    void saveFile(const string& filename, const string& file_content) {
        // Only ONE thread can hold this lock. It blocks all readers.
        unique_lock<shared_mutex> write_lock(rw_lock);
        cache[filename] = file_content;
    }
};