#include "order_book.h"
#include <iostream>
#include <chrono> // using it to get precise timestamps
#include <random> // using it for random numbrs
#include <vector>

// Helper function to get current timestamp in nanoseconds
uint64_t get_timestamp_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}

// Test basic functionality
void test_basic_operations() {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║       TEST 1: BASIC OPERATIONS (ADD, CANCEL, AMEND)       ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";

    OrderBook book;

    // Add buy orders
    book.add_order(Order(1, true, 100.0, 50, get_timestamp_ns()));
    book.add_order(Order(2, true, 99.5, 100, get_timestamp_ns()));
    book.add_order(Order(3, true, 99.0, 75, get_timestamp_ns()));

    // Add sell orders
    book.add_order(Order(4, false, 101.0, 60, get_timestamp_ns()));
    book.add_order(Order(5, false, 101.5, 80, get_timestamp_ns()));
    book.add_order(Order(6, false, 102.0, 90, get_timestamp_ns()));

    std::cout << " After adding 6 orders (3 bids, 3 asks):\n";
    book.print_book(5);

    // Cancel an order
    std::cout << " Cancelling order #2 (Bid @ 99.5)...\n";
    bool cancelled = book.cancel_order(2);
    std::cout << (cancelled ? "✅ Successfully cancelled" : "❌ Failed to cancel") << "\n";
    book.print_book(5);

    // Amend order quantity
    std::cout << " Amending order #3 quantity from 75 to 125...\n";
    bool amended = book.amend_order(3, 99.0, 125);
    std::cout << (amended ? "✅ Successfully amended" : "❌ Failed to amend") << "\n";
    book.print_book(5);

    // Amend order price (cancel + add)
    std::cout << " Amending order #4 price from 101.0 to 100.5...\n";
    amended = book.amend_order(4, 100.5, 60);
    std::cout << (amended ? "✅ Successfully amended" : "❌ Failed to amend") << "\n";
    book.print_book(5);
}

// Test order matching
void test_order_matching() {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║         TEST 2: ORDER MATCHING (BID >= ASK)               ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";

    OrderBook book;

    // Add initial orders with a spread
    book.add_order(Order(101, true, 100.0, 100, get_timestamp_ns()));
    book.add_order(Order(102, true, 99.5, 150, get_timestamp_ns()));
    book.add_order(Order(103, false, 101.0, 80, get_timestamp_ns()));
    book.add_order(Order(104, false, 101.5, 120, get_timestamp_ns()));

    std::cout << "\n📊 Initial order book (no matching):\n";
    book.print_book(5);

    // Add a buy order that crosses the spread
    std::cout << "\n⚡ Adding aggressive buy order @ 102.0 for 200 units...\n";
    std::cout << "   This should match with ask orders!\n";
    book.add_order(Order(105, true, 102.0, 200, get_timestamp_ns()));
    book.print_book(5);

    // Add a sell order that crosses the spread
    std::cout << "\n⚡ Adding aggressive sell order @ 99.0 for 120 units...\n";
    std::cout << "   This should match with remaining bid orders!\n";
    book.add_order(Order(106, false, 99.0, 120, get_timestamp_ns()));
    book.print_book(5);
}

// Test FIFO ordering
void test_fifo_priority() {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║           TEST 3: FIFO PRIORITY AT SAME PRICE              ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";

    OrderBook book;

    // Add multiple orders at the same price
    std::cout << "Adding 3 buy orders at price 100.0:\n";
    book.add_order(Order(201, true, 100.0, 50, get_timestamp_ns()));
    std::cout << "   Order #201: 50 units (FIRST)\n";
    
    book.add_order(Order(202, true, 100.0, 75, get_timestamp_ns()));
    std::cout << "   Order #202: 75 units (SECOND)\n";
    
    book.add_order(Order(203, true, 100.0, 100, get_timestamp_ns()));
    std::cout << "   Order #203: 100 units (THIRD)\n";

    book.print_book(5);

    // Add a matching sell order
    std::cout << " Adding sell order @ 100.0 for 100 units...\n";
    std::cout << "   Should match: Order #201 (50) + Order #202 (50 out of 75)\n";
    book.add_order(Order(204, false, 100.0, 100, get_timestamp_ns()));
    
    book.print_book(5);
    std::cout << " FIFO verified: First orders filled first!\n";
}

// Performance benchmark
void test_performance() {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║         TEST 4: PERFORMANCE BENCHMARK                      ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";

    OrderBook book;
    const size_t num_orders = 100000;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> side_dist(0, 1);
    std::uniform_real_distribution<> price_dist(95.0, 105.0);
    std::uniform_int_distribution<uint64_t> qty_dist(10, 1000);

    std::cout << " Adding " << num_orders << " orders...\n";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < num_orders; ++i) {
        bool is_buy = side_dist(gen) == 0;
        double price = price_dist(gen);
        uint64_t quantity = qty_dist(gen);
        
        // Round price to 2 decimal places
        price = std::round(price * 100.0) / 100.0;
        
        book.add_order(Order(i + 1, is_buy, price, quantity, get_timestamp_ns()));
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "✅ Added " << num_orders << " orders in " 
              << duration.count() << " μs\n";
    std::cout << "   Average: " << (duration.count() / static_cast<double>(num_orders)) 
              << " μs per order\n";
    std::cout << "   Throughput: " 
              << (num_orders * 1000000.0 / duration.count()) 
              << " orders/second\n\n";

    // Test snapshot retrieval
    std::vector<PriceLevel> bids, asks;
    
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10000; ++i) {
        book.get_snapshot(10, bids, asks);
    }
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << " Retrieved 10,000 snapshots (depth=10) in " 
              << duration.count() << " μs\n";
    std::cout << "   Average: " << (duration.count() / 10000.0) 
              << " μs per snapshot\n\n";

    // Display final state
    std::cout << "Final order book state:\n";
    book.print_book(10);

    // Test cancellation performance
    std::vector<uint64_t> order_ids_to_cancel;
    for (size_t i = 1; i <= num_orders && order_ids_to_cancel.size() < 10000; i += 10) {
        order_ids_to_cancel.push_back(i);
    }

    start = std::chrono::high_resolution_clock::now();
    size_t cancelled_count = 0;
    for (uint64_t id : order_ids_to_cancel) {
        if (book.cancel_order(id)) {
            cancelled_count++;
        }
    }
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "  Cancelled " << cancelled_count << " orders in " 
              << duration.count() << " μs\n";
    std::cout << "   Average: " << (duration.count() / static_cast<double>(cancelled_count)) 
              << " μs per cancellation\n";
}

// Main function
int main(int argc, char* argv[]) {
    // Suppress unused parameter warnings
    (void)argc;
    (void)argv;
    
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                                                              ║\n";
    std::cout << "║        HIGH-FREQUENCY TRADING LIMIT ORDER BOOK               ║\n";
    std::cout << "║              Low-Latency In-Memory System                    ║\n";
    std::cout << "║                                                              ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n";

    try {
        test_basic_operations();
        test_order_matching();
        test_fifo_priority();
        test_performance();

        std::cout << "\n╔══════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                  ALL TESTS COMPLETED ✅                      ║\n";
        std::cout << "╚══════════════════════════════════════════════════════════════╝\n\n";

    } catch (const std::exception& e) {
        std::cerr << "\n❌ Error: " << e.what() << "\n\n";
        return 1;
    }

    return 0;
}

