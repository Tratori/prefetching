#include "hashmap.hpp"

template<typename K, typename V>
size_t HashMap<K, V>::hash(const K& key) {
    return std::hash<K>{}(key) % capacity;
}

template<typename K, typename V>
HashMap<K, V>::HashMap(size_t capacity) : capacity(capacity), size(0) {
    table.resize(capacity);
}

template<typename K, typename V>
HashMap<K, V>::~HashMap() {}

template<typename K, typename V>
void HashMap<K, V>::insert(const K& key, const V& value) {
    size_t index = hash(key);
    for (auto& node : table[index]) {
        if (node.key == key) {
            node.value = value;
            return;
        }
    }
    table[index].emplace_back(key, value);
    size++;
}

template<typename K, typename V>
V& HashMap<K, V>::get(const K& key) {
    size_t index = hash(key);
    for (auto& node : table[index]) {
        if (node.key == key) {
            return node.value;
        }
    }
    throw out_of_range("Key not found");
}

template<typename K, typename V>
void HashMap<K, V>::vectorized_get(const std::vector<K>& keys, std::vector<V>& results) {
    int i = 0;
    for(auto &key : keys){
        size_t index = hash(key);
        bool found = false;
        for (auto& node : table[index]) {
            if (node.key == key) {
                results.at(i) = node.value;
                found = true;
                break;
            }
        }
        if(!found){
            throw out_of_range("Key not found");
        }
        i++;
    }
}

template<typename K, typename V>
void HashMap<K, V>::vectorized_get_gp(const std::vector<K>& keys, std::vector<V>& results) {
    // Vector to keep track of states for each key
    std::vector<int> states(keys.size(), -1);
    // states:
    // -1: Get first node
    //  0: Prefetch next list node
    //  1: Finished, element found

    std::vector<typename std::list<Node<K, V>>::iterator> nodes;

    for (auto& key : keys) {
        _mm_prefetch(&table[hash(key)], _MM_HINT_NTA);
    }

    int finished = 0;
    while (finished < keys.size()) {
        for (int i = 0; i < keys.size(); i++) {
            int& state = states[i];
            if (state == -1) {
                size_t index = hash(keys[i]);
                nodes.emplace_back(table[index].begin());
                _mm_prefetch(&(*nodes.back()), _MM_HINT_NTA);
                state = 0;
            } else if (state == 0) {
                auto& node = nodes[i];
                if (node->key == keys[i]) {
                    results[i] = node->value;
                    state = 1;
                    ++finished;
                    continue;
                }
                ++node;
                if (node != table[hash(keys[i])].end()) {
                    _mm_prefetch(reinterpret_cast<char*>(&(*node)), _MM_HINT_NTA);
                } else {
                    throw out_of_range("Key not found.");
                }
            } else {
                // Finished processing this key
                continue;
            }
        }
    }
}



template<typename K, typename V>
void HashMap<K, V>::vectorized_get_amac(const std::vector<K>& keys, std::vector<V>& results, int group_size) {
    CircularBuffer<AMAC_state> buff(group_size);

    // Initialize variables
    int num_finished = 0;
    int i = 0;
    int j = 0;
    while (num_finished < keys.size()) {
        AMAC_state& state = buff.next_state();

        if (state.stage == 0) {
            if(i >= keys.size()){
                continue;
            }
            state.i = i;
            state.key = keys[i++];
            state.node = table[hash(state.key)].begin();
            state.end = table[hash(state.key)].end();
            state.stage = 1;
            _mm_prefetch(&(*state.node), _MM_HINT_NTA);
        } else if (state.stage == 1) {
            if (state.key == state.node->key) {
                state.stage = 0;
                results[state.i] = state.node->value;
                num_finished++;
            } else {
                ++state.node;
                if (state.node == state.end) {
                    throw out_of_range("Key not found.");
                }
               _mm_prefetch(&(*state.node), _MM_HINT_NTA);
            }
        }
    }
}


template<typename K, typename V>
coroutine HashMap<K, V>::get_co(const K& key, std::vector<V>& results, const int i){
    size_t index = hash(key);

    // prefetch bucket (list head)
    _mm_prefetch(&table[index], _MM_HINT_NTA);
    co_await std::suspend_always{};


    auto node = table[index].begin();
    auto end = table[index].end();
    while (node != end) {
        _mm_prefetch(&(*node), _MM_HINT_NTA);
        co_await std::suspend_always{};
        if (node->key == key) {
            results.at(i) = node->value;
            co_return;
        }
        ++node;
    }
    throw out_of_range("Key not found");
}


template<typename K, typename V>
void HashMap<K, V>::vectorized_get_coroutine(const std::vector<K>& keys, std::vector<V>& results, int group_size) {
    CircularBuffer<coroutine_handle<promise>> buff(min(group_size,  static_cast<int>(keys.size())));

    int num_finished = 0;
    int i = 0;

    while (num_finished < keys.size())
    {
        coroutine_handle<promise>& handle = buff.next_state();
        if (!handle)
        {
            if (i < min(group_size, static_cast<int>(keys.size())))
            {
                handle = get_co(keys[i], results, i);
                i++;
            }
            continue;
        }

        if (handle.done()) {
            num_finished++;
            if (i < keys.size()) {
                handle = get_co(keys[i], results, i);
                ++i;
            } else {
                handle = nullptr;
                continue;
            }
        }

        handle.resume();
    }
}

template<typename K, typename V>
void HashMap<K, V>::remove(const K& key) {
    size_t index = hash(key);
    for (auto it = table[index].begin(); it != table[index].end(); ++it) {
        if ((*it).key == key) {
            table[index].erase(it);
            size--;
            return;
        }
    }
    throw out_of_range("Key not found");
}

template<typename K, typename V>
bool HashMap<K, V>::contains(const K& key) {

    size_t index = hash(key);
    for (auto& node : table[index]) {
        if (node.key == key) {
            return true;
        }
    }
    return false;
}

template<typename K, typename V>
size_t HashMap<K, V>::getSize() const {
    return size;
}

template<typename K, typename V>
bool HashMap<K, V>::isEmpty() const {
    return size == 0;
}


template class HashMap<unsigned int, unsigned int>;