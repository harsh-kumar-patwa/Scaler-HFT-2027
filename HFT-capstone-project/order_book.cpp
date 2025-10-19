#include "order_book.h"
#include <algorithm>
#include <limits>

// ============================================================================
// Constructor & Destructor
// ============================================================================
OrderBook::OrderBook() 
    : total_orders_added_(0)
    , total_orders_cancelled_(0)
    , total_orders_matched_(0) {
}

OrderBook::~OrderBook() {
    clear();
}

// ============================================================================
// Add Order
// ============================================================================
void OrderBook::add_order(const Order& order) {
    // Allocate order from memory pool
    Order* new_order = order_pool_.allocate();
    *new_order = order;

    total_orders_added_++;

    if (order.is_buy) {
        // Add to bids
        auto it = bids_.find(order.price);
        if (it == bids_.end()) {
            // Create new price level
            auto result = bids_.emplace(std::piecewise_construct,
                                       std::forward_as_tuple(order.price),
                                       std::forward_as_tuple(order.price));
            it = result.first;
        }

        // Add order to the end of the list (FIFO)
        it->second.orders.push_back(new_order);
        it->second.total_quantity += order.quantity;

        // Store location for fast lookup
        OrderLocation loc;
        loc.order = new_order;
        loc.list_iter = --it->second.orders.end();
        loc.is_bid = true;
        loc.price = order.price;
        order_lookup_[order.order_id] = loc;

    } else {
        // Add to asks
        auto it = asks_.find(order.price);
        if (it == asks_.end()) {
            // Create new price level
            auto result = asks_.emplace(std::piecewise_construct,
                                       std::forward_as_tuple(order.price),
                                       std::forward_as_tuple(order.price));
            it = result.first;
        }

        // Add order to the end of the list (FIFO)
        it->second.orders.push_back(new_order);
        it->second.total_quantity += order.quantity;

        // Store location for fast lookup
        OrderLocation loc;
        loc.order = new_order;
        loc.list_iter = --it->second.orders.end();
        loc.is_bid = false;
        loc.price = order.price;
        order_lookup_[order.order_id] = loc;
    }

    // Attempt to match orders
    match_orders();
}

// ============================================================================
// Cancel Order
// ============================================================================
bool OrderBook::cancel_order(uint64_t order_id) {
    auto lookup_it = order_lookup_.find(order_id);
    if (lookup_it == order_lookup_.end()) {
        return false;  // Order not found
    }

    const OrderLocation& loc = lookup_it->second;
    remove_order_from_book(loc);
    
    order_lookup_.erase(lookup_it);
    total_orders_cancelled_++;

    return true;
}

// ============================================================================
// Amend Order
// ============================================================================
bool OrderBook::amend_order(uint64_t order_id, double new_price, uint64_t new_quantity) {
    auto lookup_it = order_lookup_.find(order_id);
    if (lookup_it == order_lookup_.end()) {
        return false;  // Order not found
    }

    Order* order = lookup_it->second.order;
    double old_price = order->price;
    uint64_t old_quantity = order->quantity;

    // If price changes, treat as cancel + add
    if (new_price != old_price) {
        // Save order details
        Order new_order = *order;
        new_order.price = new_price;
        new_order.quantity = new_quantity;

        // Cancel old order
        cancel_order(order_id);

        // Add new order
        add_order(new_order);
        return true;
    }

    // Only quantity changes - update in place
    if (new_quantity != old_quantity) {
        const OrderLocation& loc = lookup_it->second;

        // Update quantity in the order
        order->quantity = new_quantity;

        // Update total quantity at price level
        if (loc.is_bid) {
            auto it = bids_.find(loc.price);
            if (it != bids_.end()) {
                it->second.total_quantity = it->second.total_quantity - old_quantity + new_quantity;
            }
        } else {
            auto it = asks_.find(loc.price);
            if (it != asks_.end()) {
                it->second.total_quantity = it->second.total_quantity - old_quantity + new_quantity;
            }
        }

        // Attempt to match orders (in case quantity increased)
        match_orders();
    }

    return true;
}

// ============================================================================
// Get Snapshot
// ============================================================================
void OrderBook::get_snapshot(size_t depth, std::vector<PriceLevel>& bids, 
                            std::vector<PriceLevel>& asks) const {
    bids.clear();
    asks.clear();

    // Get top N bids (highest prices)
    size_t count = 0;
    for (const auto& [price, level_data] : bids_) {
        if (count >= depth) break;
        bids.emplace_back(price, level_data.total_quantity);
        count++;
    }

    // Get top N asks (lowest prices)
    count = 0;
    for (const auto& [price, level_data] : asks_) {
        if (count >= depth) break;
        asks.emplace_back(price, level_data.total_quantity);
        count++;
    }
}

// ============================================================================
// Print Book
// ============================================================================
void OrderBook::print_book(size_t depth) const {
    std::vector<PriceLevel> bids, asks;
    get_snapshot(depth, bids, asks);

    std::cout << "\n========================================\n";
    std::cout << "         ORDER BOOK SNAPSHOT\n";
    std::cout << "========================================\n";
    std::cout << std::fixed << std::setprecision(2);

    // Print asks in reverse (highest to lowest)
    std::cout << "\nASKS (Sell Orders):\n";
    std::cout << std::setw(15) << "Price" << " | " 
              << std::setw(15) << "Quantity" << "\n";
    std::cout << "----------------------------------------\n";
    
    for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
        std::cout << std::setw(15) << it->price << " | " 
                  << std::setw(15) << it->total_quantity << "\n";
    }

    std::cout << "========================================\n";
    std::cout << "              SPREAD\n";
    
    if (!asks.empty() && !bids.empty()) {
        double spread = asks.front().price - bids.front().price;
        double mid_price = (asks.front().price + bids.front().price) / 2.0;
        std::cout << "  Bid: " << bids.front().price 
                  << " | Ask: " << asks.front().price << "\n";
        std::cout << "  Spread: " << spread 
                  << " | Mid: " << mid_price << "\n";
    } else {
        std::cout << "  No spread available\n";
    }
    
    std::cout << "========================================\n";

    // Print bids (highest to lowest)
    std::cout << "\nBIDS (Buy Orders):\n";
    std::cout << std::setw(15) << "Price" << " | " 
              << std::setw(15) << "Quantity" << "\n";
    std::cout << "----------------------------------------\n";
    
    for (const auto& bid : bids) {
        std::cout << std::setw(15) << bid.price << " | " 
                  << std::setw(15) << bid.total_quantity << "\n";
    }

    std::cout << "========================================\n";
    std::cout << "Statistics:\n";
    std::cout << "  Total Orders Added: " << total_orders_added_ << "\n";
    std::cout << "  Total Orders Cancelled: " << total_orders_cancelled_ << "\n";
    std::cout << "  Total Orders Matched: " << total_orders_matched_ << "\n";
    std::cout << "  Bid Levels: " << bids_.size() << "\n";
    std::cout << "  Ask Levels: " << asks_.size() << "\n";
    std::cout << "  Memory Pool Blocks: " << order_pool_.block_count() << "\n";
    std::cout << "========================================\n\n";
}

// ============================================================================
// Match Orders
// ============================================================================
void OrderBook::match_orders() {
    while (!bids_.empty() && !asks_.empty()) {
        auto best_bid_it = bids_.begin();
        auto best_ask_it = asks_.begin();

        double best_bid_price = best_bid_it->first;
        double best_ask_price = best_ask_it->first;

        // Check if orders can match
        if (best_bid_price < best_ask_price) {
            break;  // No match possible
        }

        // Get the first orders in each list (FIFO)
        if (best_bid_it->second.orders.empty() || best_ask_it->second.orders.empty()) {
            break;
        }

        Order* buy_order = best_bid_it->second.orders.front();
        Order* sell_order = best_ask_it->second.orders.front();

        // Calculate trade quantity
        uint64_t trade_qty = std::min(buy_order->quantity, sell_order->quantity);

        // Execute the trade
        execute_trade(buy_order, sell_order, trade_qty);

        // Update quantities
        buy_order->quantity -= trade_qty;
        sell_order->quantity -= trade_qty;

        best_bid_it->second.total_quantity -= trade_qty;
        best_ask_it->second.total_quantity -= trade_qty;

        // Remove fully filled orders
        if (buy_order->quantity == 0) {
            order_lookup_.erase(buy_order->order_id);
            best_bid_it->second.orders.pop_front();
            order_pool_.deallocate(buy_order);

            // Remove price level if empty
            if (best_bid_it->second.orders.empty()) {
                bids_.erase(best_bid_it);
            }
        }

        if (sell_order->quantity == 0) {
            order_lookup_.erase(sell_order->order_id);
            best_ask_it->second.orders.pop_front();
            order_pool_.deallocate(sell_order);

            // Remove price level if empty
            if (best_ask_it->second.orders.empty()) {
                asks_.erase(best_ask_it);
            }
        }
    }
}

// ============================================================================
// Execute Trade
// ============================================================================
void OrderBook::execute_trade(Order* buy_order, Order* sell_order, uint64_t trade_qty) {
    total_orders_matched_++;

    // Dummy execution

    (void)buy_order;
    (void)sell_order;
    (void)trade_qty;

    
    std::cout << "TRADE: Buy Order #" << buy_order->order_id 
              << " x Sell Order #" << sell_order->order_id
              << " | Qty: " << trade_qty 
              << " | Price: " << sell_order->price << "\n";
}

// ============================================================================
// Remove Order from Book (Helper)
// ============================================================================
void OrderBook::remove_order_from_book(const OrderLocation& loc) {
    if (loc.is_bid) {
        auto it = bids_.find(loc.price);
        if (it != bids_.end()) {
            // Update total quantity
            it->second.total_quantity -= loc.order->quantity;

            // Remove order from list
            it->second.orders.erase(loc.list_iter);

            // Deallocate order
            order_pool_.deallocate(loc.order);

            // Remove price level if empty
            if (it->second.orders.empty()) {
                bids_.erase(it);
            }
        }
    } else {
        auto it = asks_.find(loc.price);
        if (it != asks_.end()) {
            // Update total quantity
            it->second.total_quantity -= loc.order->quantity;

            // Remove order from list
            it->second.orders.erase(loc.list_iter);

            // Deallocate order
            order_pool_.deallocate(loc.order);

            // Remove price level if empty
            if (it->second.orders.empty()) {
                asks_.erase(it);
            }
        }
    }
}

// ============================================================================
// Get Best Bid
// ============================================================================
bool OrderBook::get_best_bid(double& price, uint64_t& quantity) const {
    if (bids_.empty()) {
        return false;
    }

    auto it = bids_.begin();
    price = it->first;
    quantity = it->second.total_quantity;
    return true;
}

// ============================================================================
// Get Best Ask
// ============================================================================
bool OrderBook::get_best_ask(double& price, uint64_t& quantity) const {
    if (asks_.empty()) {
        return false;
    }

    auto it = asks_.begin();
    price = it->first;
    quantity = it->second.total_quantity;
    return true;
}

// ============================================================================
// Clear
// ============================================================================
void OrderBook::clear() {
    // Deallocate all orders in bids
    for (auto& [price, level_data] : bids_) {
        for (Order* order : level_data.orders) {
            order_pool_.deallocate(order);
        }
    }

    // Deallocate all orders in asks
    for (auto& [price, level_data] : asks_) {
        for (Order* order : level_data.orders) {
            order_pool_.deallocate(order);
        }
    }

    bids_.clear();
    asks_.clear();
    order_lookup_.clear();

    total_orders_added_ = 0;
    total_orders_cancelled_ = 0;
    total_orders_matched_ = 0;
}

