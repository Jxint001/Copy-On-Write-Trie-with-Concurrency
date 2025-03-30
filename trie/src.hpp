/*
 * Acknowledgement : Yuxuan Wang for Modifying the prototype of TrieStore class
 */

#ifndef SJTU_TRIE_HPP
#define SJTU_TRIE_HPP

#include <algorithm>
#include <cstddef>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sjtu {

// A TrieNode is a node in a Trie.
class TrieNode {
   public:
    // Create a TrieNode with no children.
    TrieNode() = default;

    // Create a TrieNode with some children.
    explicit TrieNode(std::map<char, std::shared_ptr<const TrieNode>> children)
        : children_(std::move(children)) {}

    virtual ~TrieNode() = default;

    // Clone returns a copy of this TrieNode. If the TrieNode has a value, the
    // value is copied (because TrieNodeWithValue has overrided Clone(), virtual funtion ensures that u call the corresponding Clone() in run time). The return type of this function is a unique_ptr to a
    // TrieNode.
    //
    // You cannot use the copy constructor to clone the node because it doesn't
    // know whether a `TrieNode` contains a value or not.
    //
    // Note: if you want to convert `unique_ptr` into `shared_ptr`, you can use
    // `std::shared_ptr<T>(std::move(ptr))`.
    virtual auto Clone() const -> std::unique_ptr<TrieNode> {  // trailing return type
        return std::make_unique<TrieNode>(children_);
    }

    // A map of children, where the key is the next character in the key, and
    // the value is the next TrieNode.
    std::map<char, std::shared_ptr<const TrieNode>> children_;

    // Indicates if the node is the terminal node.
    bool is_value_node_{false};

    // You can add additional fields and methods here. But in general, you don't
    // need to add extra fields to complete this project.
};

// A TrieNodeWithValue is a TrieNode that also has a value of type T associated
// with it.
template <class T>
class TrieNodeWithValue : public TrieNode {
   public:
    // Create a trie node with no children and a value.
    explicit TrieNodeWithValue(std::shared_ptr<T> value)
        : value_(std::move(value)) {
        this->is_value_node_ = true;
    }

    // Create a trie node with children and a value.
    TrieNodeWithValue(std::map<char, std::shared_ptr<const TrieNode>> children,
                      std::shared_ptr<T> value)
        : TrieNode(std::move(children)), value_(std::move(value)) {
        this->is_value_node_ = true;
    }

    // Override the Clone method to also clone the value.
    //
    // Note: if you want to convert `unique_ptr` into `shared_ptr`, you can use
    // `std::shared_ptr<T>(std::move(ptr))`.
    auto Clone() const -> std::unique_ptr<TrieNode> override {
        return std::make_unique<TrieNodeWithValue<T>>(children_, value_);
    }

    // The value associated with this trie node.
    std::shared_ptr<T> value_;
};

// A Trie is a data structure that maps strings to values of type T. All
// operations on a Trie should not modify the trie itself. It should reuse the
// existing nodes as much as possible, and create new nodes to represent the new
// trie.
class Trie {
   private:
    // The root of the trie.
    std::shared_ptr<const TrieNode> root_{nullptr};

    // Create a new trie with the given root.
    explicit Trie(std::shared_ptr<const TrieNode> root)
        : root_(std::move(root)) {}

   public:
    // Create an empty trie.
    Trie() = default;

    bool operator==(const Trie& other) const {
        return root_ == other.root_;
    }
    // by TA: if you don't need this, just comment out.

    // Get the value associated with the given key.
    // 1. If the key is not in the trie, return nullptr.
    // 2. If the key is in the trie but the type is mismatched, return nullptr.
    // 3. Otherwise, return the value.
    // std::string_view: non-owning, immutable and light-weight(usually a pointer and an integer for string length)
    template <class T>
    auto Get(std::string_view key) const -> const T* {
        //std::cout << "in TrieGet" << std::endl;
        if (root_ == nullptr)  return nullptr;
        size_t len = key.size();
        auto cur = root_;
        
        // std::cout << (root_ == nullptr) << std::endl;
        for (size_t i = 0; i < len; ++i) {
            auto it = cur->children_.find(key[i]);
            if (it == cur->children_.end()) return nullptr;
            cur = it->second;
        }
        auto val_node = dynamic_cast<const TrieNodeWithValue<T>*>(cur.get());
        return val_node ? val_node->value_.get() : nullptr;
    }

    // Put a new key-value pair into the trie. If the key already exists,
    // overwrite the value. Return the new trie.
    template <class T>
    auto Put(std::string_view key, T value) const -> Trie {
        size_t len = key.size();
        auto new_root = root_ ? root_->Clone() : std::make_unique<TrieNode>();
        TrieNode* cur = new_root.get();

        // Clone TrieNode along the key path
        TrieNode* copy{nullptr};
        for (size_t i = 0; i < len; ++i) {
            auto it = cur->children_.find(key[i]);
            if (it == cur->children_.end()) {
                cur->children_[key[i]] = std::make_shared<TrieNode>();
            } else {
                auto clone_node = it->second->Clone();
                cur->children_[key[i]] = std::move(clone_node);
            }
            if (i == len - 1)  copy = cur;
            cur = const_cast<TrieNode*>(cur->children_[key[i]].get());
        }

        // update/assign value
        auto val_node = std::make_shared<TrieNodeWithValue<T>>(cur->children_, std::make_shared<T>(std::move(value)));
        copy->children_[key[len-1]] = val_node;

        return Trie(std::move(new_root));  // call std::shared_ptr<...>(std::move(unique_ptr))
    }

    // Remove the key from the trie. If the key does not exist, return the
    // original trie. Otherwise, returns the new trie.
    auto Remove(std::string_view key) const -> Trie {
        size_t len = key.size();
        if (!root_) return *this;

        auto new_root = root_->Clone();
        TrieNode* cur = new_root.get();
        std::vector<TrieNode*> path;  // A stack to track the key path

        for (size_t i = 0; i < len; ++i) {
            auto it = cur->children_.find(key[i]);
            if (it == cur->children_.end()) return *this;
            path.push_back(cur);  // store father
            auto clone_node = it->second->Clone();
            cur->children_[key[i]] = std::move(clone_node);
            cur = const_cast<TrieNode*>(cur->children_[key[i]].get());
        }

        // Remove the value node
        if (!cur->is_value_node_)  return *this;
        if (!cur->children_.empty()) {
            //cur->is_value_node_ = false;
            auto normal_node = std::make_shared<TrieNode>(cur->children_);
            path.back()->children_[key[len-1]] = normal_node;
            return Trie(std::move(new_root));
        } else {
            cur->is_value_node_ = false;
            path.back()->children_.erase(key[len - 1]);
        }
        
        // Clean up empty nodes along the path
        for (int i = path.size() - 1; i >= 0; --i) {
            if (path[i]->children_.empty() && !path[i]->is_value_node_) {
                if (i > 0) {
                    path[i - 1]->children_.erase(key[i - 1]);
                }
            } else {
                break;
            }
        }

        return Trie(std::move(new_root));
    }
};

// This class is used to guard the value returned by the trie. It holds a
// reference to the root so that the reference to the value will not be
// invalidated.
template <class T>
class ValueGuard {
   public:
    ValueGuard(Trie root, const T& value)
        : root_(std::move(root)), value_(value) {}
    auto operator*() const -> const T& { return value_; }

   private:
    Trie root_;
    const T& value_;
};

// This class is a thread-safe wrapper around the Trie class. It provides a
// simple interface for accessing the trie. It should allow concurrent reads and
// a single write operation at the same time.
class TrieStore {
   public:
    // This function returns a ValueGuard object that holds a reference to the
    // value in the trie of the given version (default: newest version). If the
    // key does not exist in the trie, it will return std::nullopt.
    template <class T>
    auto Get(std::string_view key, size_t version = -1) -> std::optional<ValueGuard<T>> {
        //std::cout << "in storeGet" << std::endl;
        std::shared_lock<std::shared_mutex> lock(snapshots_lock_);
        if (version >= snapshots_.size())  version = snapshots_.size() - 1;
        const Trie& trie = snapshots_[version];
        const T* val = trie.Get<T>(key);

        if (val == nullptr)  return std::nullopt;
        return ValueGuard<T>(trie, *val);
    }

    // This function will insert the key-value pair into the trie. If the key
    // already exists in the trie, it will overwrite the value return the
    // version number after operation Hint: new version should only be visible
    // after the operation is committed(completed)
    template <class T>
    size_t Put(std::string_view key, T value) {
        std::lock_guard<std::mutex> writeGuard(write_lock_);
        std::unique_lock<std::shared_mutex> lock(snapshots_lock_);
        Trie new_trie = snapshots_.back().Put(key, std::move(value));
        snapshots_.push_back(std::move(new_trie));
        return snapshots_.size() - 1;
    }

    // This function will remove the key-value pair from the trie.
    // return the version number after operation
    // if the key does not exist, version number should not be increased
    size_t Remove(std::string_view key) {
        std::lock_guard<std::mutex> writeGuard(write_lock_);
        std::unique_lock<std::shared_mutex> lock(snapshots_lock_);
        Trie new_trie = snapshots_.back().Remove(key);

        if (new_trie == snapshots_.back())  return snapshots_.size() - 1;
        snapshots_.push_back(std::move(new_trie));
        return snapshots_.size() - 1;
    }

    // This function return the newest version number
    size_t get_version() {
        std::shared_lock<std::shared_mutex> lock(snapshots_lock_);
        return snapshots_.size() - 1; 
    }

   private:
    // This mutex sequences all writes operations and allows only one write
    // operation at a time. Concurrent modifications should have the effect of
    // applying them in some sequential order
    std::mutex write_lock_;
    std::shared_mutex snapshots_lock_;

    // Stores all historical versions of trie
    // version number ranges from [0, snapshots_.size())
    std::vector<Trie> snapshots_{1};
};

}  // namespace sjtu

#endif  // SJTU_TRIE_HPP