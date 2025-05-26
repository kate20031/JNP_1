#ifndef KVFIFO_H
#define KVFIFO_H

#include <cstddef>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>
#include <iterator>

template<typename K, typename V>
class kvfifo {
private:
    using k_v_queue_t = std::list<std::pair<K const, V>>;
    using k_v_queue_iterator_t = typename k_v_queue_t::iterator;
    using k_v_map_t = std::map<K, std::list<k_v_queue_iterator_t>>;
    using k_v_map_const_iterator_t = typename k_v_map_t::const_iterator;

public:
    kvfifo() : dataPtr(std::make_shared<container_t>()) {}

    kvfifo(kvfifo const &other) {
        if (!other.unshareable) {
            dataPtr = other.dataPtr;
        } else {
            dataPtr = std::make_shared<container_t>(*other.dataPtr);
        }
    }

    kvfifo(kvfifo &&other) noexcept = default;

    ~kvfifo() noexcept = default;

    kvfifo &operator=(kvfifo other) {
        if (!other.unshareable) {
            dataPtr = other.dataPtr;
        } else {
            dataPtr = std::make_shared<container_t>(*other.dataPtr);
        }

        unshareable = false;

        return *this;
    }

    void push(K const &k, V const &v) {
        if (dataPtr == nullptr) {
            dataPtr = std::make_shared<container_t>();
        }

        copy_guard_t guard(this);
        aboutToModify();

        auto it = dataPtr->iterator_list_map.find(k);

        dataPtr->pair_list.push_back({k, v});

        try {
            if (it == dataPtr->iterator_list_map.end()) {
                std::list<k_v_queue_iterator_t> new_list;
                new_list.push_back(std::prev(dataPtr->pair_list.end()));
                dataPtr->iterator_list_map.insert({k, new_list});
            } else {
                it->second.push_back(std::prev(dataPtr->pair_list.end()));
            }
        } catch (...) {
            dataPtr->pair_list.pop_back();
            throw;
        }

        guard.no_rollback();
    }

    void pop() {
        if (empty()) {
            throw std::invalid_argument("Empty queue");
        }

        copy_guard_t guard(this);
        aboutToModify();

        auto it = dataPtr->iterator_list_map.find(dataPtr->pair_list.front().first);
        it->second.pop_front();

        if (it->second.empty()) {
            dataPtr->iterator_list_map.erase(it);
        }

        dataPtr->pair_list.pop_front();

        guard.no_rollback();
    }

    void pop(K const &k) {
        if (empty()) {
            throw std::invalid_argument("Key not found");
        }

        copy_guard_t guard(this);
        aboutToModify();

        auto it = dataPtr->iterator_list_map.find(k);

        if (it == dataPtr->iterator_list_map.end()) {
            throw std::invalid_argument("Key not found");
        } else {
            dataPtr->pair_list.erase(it->second.front());
            it->second.pop_front();

            if (it->second.empty()) {
                dataPtr->iterator_list_map.erase(it);
            }
        }

        guard.no_rollback();
    }

    void move_to_back(K const &k) {
        if (empty()) {
            throw std::invalid_argument("Key not found");
        }

        copy_guard_t guard(this);
        aboutToModify();

        auto it = dataPtr->iterator_list_map.find(k);

        if (it == dataPtr->iterator_list_map.end()) {
            throw std::invalid_argument("Key not found");
        }

        for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
            dataPtr->pair_list.splice(dataPtr->pair_list.end(), dataPtr->pair_list, *it2);
        }

        guard.no_rollback();
    }

    std::pair<K const &, V const &> front() const {
        if (empty()) {
            throw std::invalid_argument("Empty queue");
        }

        return {dataPtr->pair_list.front().first, dataPtr->pair_list.front().second};
    }

    std::pair<K const &, V &> front() {
        if (empty()) {
            throw std::invalid_argument("Empty queue");
        }

        copy_guard_t guard(this);
        aboutToModify(true);

        guard.no_rollback();
        return {dataPtr->pair_list.front().first, dataPtr->pair_list.front().second};
    }

    std::pair<K const &, V const &> back() const {
        if (empty()) {
            throw std::invalid_argument("Empty queue");
        }

        return {dataPtr->pair_list.back().first, dataPtr->pair_list.back().second};
    }

    std::pair<K const &, V &> back() {
        if (empty()) {
            throw std::invalid_argument("Empty queue");
        }

        copy_guard_t guard(this);
        aboutToModify(true);

        guard.no_rollback();
        return {dataPtr->pair_list.back().first,
                dataPtr->pair_list.back().second};
    }

    std::pair<K const &, V const &> first(K const &key) const {
        if (empty()) {
            throw std::invalid_argument("Key not found");
        }

        auto it = dataPtr->iterator_list_map.find(key);

        if (it == dataPtr->iterator_list_map.end()) {
            throw std::invalid_argument("Key not found");
        }

        return {it->second.front()->first, it->second.front()->second};
    }

    std::pair<K const &, V &> first(K const &key) {
        if (empty()) {
            throw std::invalid_argument("Key not found");
        }

        copy_guard_t guard(this);
        aboutToModify(true);

        auto it = dataPtr->iterator_list_map.find(key);

        if (it == dataPtr->iterator_list_map.end()) {
            throw std::invalid_argument("Key not found");
        }

        guard.no_rollback();
        return {it->second.front()->first, it->second.front()->second};
    }

    std::pair<K const &, V const &> last(K const &key) const {
        if (empty()) {
            throw std::invalid_argument("Key not found");
        }

        auto it = dataPtr->iterator_list_map.find(key);

        if (it == dataPtr->iterator_list_map.end()) {
            throw std::invalid_argument("Key not found");
        }

        return {it->second.back()->first, it->second.back()->second};
    }

    std::pair<K const &, V &> last(K const &key) {
        if (empty()) {
            throw std::invalid_argument("Key not found");
        }

        copy_guard_t guard(this);
        aboutToModify(true);

        auto it = dataPtr->iterator_list_map.find(key);

        if (it == dataPtr->iterator_list_map.end()) {
            throw std::invalid_argument("Key not found");
        }

        guard.no_rollback();
        return {it->second.back()->first, it->second.back()->second};
    }

    size_t size() const noexcept {
        if (dataPtr == nullptr) {
            return 0;
        }

        return dataPtr->pair_list.size();
    }

    bool empty() const noexcept {
        if (dataPtr == nullptr) {
            return true;
        }

        return dataPtr->pair_list.empty();
    }

    size_t count(K const &k) const {
        if (dataPtr == nullptr) {
            return 0;
        }

        auto it = dataPtr->iterator_list_map.find(k);

        if (it == dataPtr->iterator_list_map.end()) {
            return 0;
        } else {
            return it->second.size();
        }
    }

    void clear() {
        if (dataPtr == nullptr) {
            return;
        }

        copy_guard_t guard(this);
        aboutToModify();

        dataPtr->pair_list.clear();
        dataPtr->iterator_list_map.clear();
        guard.no_rollback();
    }

    class k_iterator {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = const K;
        using difference_type = std::ptrdiff_t;
        using pointer = const K *;
        using reference = const K &;

        k_iterator() = default;

        explicit k_iterator(k_v_map_const_iterator_t it) : it(it) {}

        K const &operator*() const {
            return it->first;
        }

        K const *operator->() const {
            return &it->first;
        }

        k_iterator &operator++() {
            ++it;
            return *this;
        }

        k_iterator operator++(int) {
            k_iterator tmp(*this);
            operator++();
            return tmp;
        }

        k_iterator &operator--() {
            --it;
            return *this;
        }

        k_iterator operator--(int) {
            k_iterator tmp(*this);
            operator--();
            return tmp;
        }

        bool operator==(k_iterator const &other) const {
            return it == other.it;
        }

        bool operator!=(k_iterator const &other) const {
            return it != other.it;
        }

    private:
        k_v_map_const_iterator_t it;
    };

    k_iterator k_begin() const {
        if (dataPtr == nullptr) {
            return k_iterator();
        }

        return k_iterator(dataPtr->iterator_list_map.cbegin());
    }

    k_iterator k_end() const {
        if (dataPtr == nullptr) {
            return k_iterator();
        }

        return k_iterator(dataPtr->iterator_list_map.cend());
    }

private:
    void aboutToModify(bool markUnshareable = false) {
        /**
         * Sprawdzamy czy use_count() > 2, a nie czy unique(), bo na pewno
         * jeden wskaźnik jest nasz a drugi w copy_guard_t.
         */
        if (dataPtr.use_count() > 2 && !unshareable) {
            dataPtr = std::make_shared<container_t>(*dataPtr);
        }

        if (markUnshareable) {
            unshareable = true;
        } else {
            unshareable = false;
        }
    }

    struct container_t {
        container_t() = default;

        container_t(container_t const &other) {
            k_v_map_t new_map;
            k_v_queue_t new_list;

            for (auto it = other.pair_list.begin(); it != other.pair_list.end(); ++it) {
                new_list.push_back({it->first, it->second});
            }

            for (auto it = new_list.begin(); it != new_list.end(); ++it) {
                auto key_it = new_map.find(it->first);

                if (key_it == new_map.end()) {
                    std::list<k_v_queue_iterator_t> new_it_list;
                    new_it_list.push_back(it);
                    new_map.insert({it->first, new_it_list});
                } else {
                    key_it->second.push_back(it);
                }
            }

            std::swap(iterator_list_map, new_map);
            std::swap(pair_list, new_list);
        }

        container_t(container_t &&other) noexcept = default;

        ~container_t() noexcept = default;

        k_v_map_t iterator_list_map;
        k_v_queue_t pair_list;
    };

    class copy_guard_t {
    public:
        explicit copy_guard_t(kvfifo *to_guard) : guarded(to_guard),
            guarded_data(to_guard->dataPtr),
            guarded_unshareable(to_guard->unshareable) {}

        ~copy_guard_t() noexcept {
            if (rollback) {
                std::swap(guarded->dataPtr, guarded_data);
                guarded->unshareable = guarded_unshareable;
            }
        }

        void no_rollback() {
            rollback = false;
        }

    private:
        kvfifo *guarded;
        std::shared_ptr<container_t> guarded_data;
        bool guarded_unshareable;
        bool rollback = true;
    };

    bool unshareable = false;
    std::shared_ptr<container_t> dataPtr;
};

#endif