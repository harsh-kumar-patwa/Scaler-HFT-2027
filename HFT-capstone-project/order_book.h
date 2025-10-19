#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include <list>
#include <unordered_map>
#include <memory>
#include <iostream>
#include <iomanip>
#include <chrono>

// ============================================================================
// Order Structure
// ============================================================================
struct Order {
    uint64_t order_id;     
    bool is_buy;          
    double price;
    uint64_t quantity;
    uint64_t timestamp_ns;

    Order() = default;
    Order(uint64_t id, bool buy, double p, uint64_t qty, uint64_t ts)
        : order_id(id), is_buy(buy), price(p), quantity(qty), timestamp_ns(ts) {}
};

// ============================================================================
// PriceLevel Structure
// ============================================================================
struct PriceLevel {
    double price;
    uint64_t total_quantity;

    PriceLevel() = default;
    PriceLevel(double p, uint64_t qty) : price(p), total_quantity(qty) {}
};

// ============================================================================
// Memory Pool for Order Allocation (Cache-Friendly)
// ============================================================================
template<typename T, size_t BlockSize = 4096>
class MemoryPool {
private:
    struct Block {
        uint8_t data[BlockSize * sizeof(T)];
        Block* next;
        
        Block() : next(nullptr) {}
    };

    Block* blocks_;
    T* free_list_;
    size_t block_count_;

    void allocate_block() {
        Block* new_block = new Block();
        new_block->next = blocks_;
        blocks_ = new_block;
        block_count_++;

        // Add all elements in the block to the free list
        T* block_start = reinterpret_cast<T*>(new_block->data);
        for (size_t i = 0; i < BlockSize; ++i) {
            T* element = block_start + i;
            *reinterpret_cast<T**>(element) = free_list_;
            free_list_ = element;
        }
    }

public:
    MemoryPool() : blocks_(nullptr), free_list_(nullptr), block_count_(0) {
        allocate_block();
    }

    ~MemoryPool() {
        while (blocks_) {
            Block* next = blocks_->next;
            delete blocks_;
            blocks_ = next;
        }
    }

    T* allocate() {
        if (!free_list_) {
            allocate_block();
        }
        T* element = free_list_;
        free_list_ = *reinterpret_cast<T**>(element);
        return new (element) T(); 
    }

    void deallocate(T* element) {
        if (!element) return;
        element->~T();  // Call destructor
        *reinterpret_cast<T**>(element) = free_list_;
        free_list_ = element;
    }

    size_t block_count() const { return block_count_; }

    // Prevent copying
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;
};

// ============================================================================
// Order Book Class
// ============================================================================
class OrderBook {
private:
    // Price level data structure: list of orders at each price
    // Using std::list for O(1) insertion/deletion 
    using OrderList = std::list<Order*>;
    
    struct PriceLevelData {
        double price;
        OrderList orders;
        uint64_t total_quantity;

        PriceLevelData(double p) : price(p), total_quantity(0) {}
    };

    // Bids: sorted descending (highest price first)
    // Asks: sorted ascending (lowest price first)
    std::map<double, PriceLevelData, std::greater<double>> bids_;  // Descending
    std::map<double, PriceLevelData, std::less<double>> asks_;     // Ascending

    // Fast O(1) order lookup: order_id -> (Order*, iterator in price level)
    struct OrderLocation {
        Order* order;
        OrderList::iterator list_iter;
        bool is_bid;
        double price;
    };
    std::unordered_map<uint64_t, OrderLocation> order_lookup_;

    // Memory pool for efficient order allocation
    MemoryPool<Order, 4096> order_pool_;

    // Statistics
    uint64_t total_orders_added_;
    uint64_t total_orders_cancelled_;
    uint64_t total_orders_matched_;

    // Helper methods
    void match_orders();
    void execute_trade(Order* buy_order, Order* sell_order, uint64_t trade_qty);
    void remove_order_from_book(const OrderLocation& loc);

public:
    OrderBook();
    ~OrderBook();

    // Core operations
    void add_order(const Order& order);
    bool cancel_order(uint64_t order_id);
    bool amend_order(uint64_t order_id, double new_price, uint64_t new_quantity);

    // Query operations
    void get_snapshot(size_t depth, std::vector<PriceLevel>& bids, 
                      std::vector<PriceLevel>& asks) const;
    void print_book(size_t depth = 10) const;

    // Utility methods
    size_t bid_levels() const { return bids_.size(); }
    size_t ask_levels() const { return asks_.size(); }
    uint64_t total_orders_added() const { return total_orders_added_; }
    uint64_t total_orders_cancelled() const { return total_orders_cancelled_; }
    uint64_t total_orders_matched() const { return total_orders_matched_; }

    // Get best bid/ask
    bool get_best_bid(double& price, uint64_t& quantity) const;
    bool get_best_ask(double& price, uint64_t& quantity) const;

    // Clear the order book
    void clear();
};

