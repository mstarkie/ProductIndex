#include "ProductIndex.h"
#include <iostream>
#include <chrono>
#include <random>
#include <iomanip>
#include <climits>

using std::cout;
using std::cerr;
using std::endl;

// Performance test with millions of records

std::string generateProductId(size_t index) {
    // Generate IDs like "ID00000001", "ID00000002", etc.
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "ID%08zu", index);
    return std::string(buffer);
}

int basicTest() {
    const std::string indexPath = "product_index.dat";

    // ===========================================
    // PHASE 1: Building the index (during document load)
    // ===========================================
    {
        ProductIndex builder;

        cout << "Building index with sample data..." << endl;

        // Simulate loading product structure from Vis Mockup
        // In reality, you'd traverse the product structure tree here
        // and call addRecord() for each node

        builder.addRecord("ID345345", 1001);
        builder.addRecord("ID000001", 1002);
        builder.addRecord("ID999999", 1003);
        builder.addRecord("PART-A-001", 2001);
        builder.addRecord("PART-Z-999", 2002);
        builder.addRecord("ASSEMBLY-TOP", 3001);

        // For a real submarine with millions of parts:
        // while (moreNodes) {
        //     std::string productId = getProductIdFromVisMockup(node);
        //     int64_t nodeKey = getNodeKeyFromVisMockup(node);
        //     builder.addRecord(productId, nodeKey);
        //     node = getNextNode();
        // }

        // Sort and write to disk
        if (builder.buildIndex(indexPath)) {
            cout << "Index built successfully." << endl;
        }
        else {
            std::cerr << "Failed to build index!" << endl;
            return 1;
        }

        // builder goes out of scope here, memory is freed
    }

    // ===========================================
    // PHASE 2: Using the index (during user interaction)
    // ===========================================
    {
        ProductIndex index;

        if (!index.openIndex(indexPath)) {
            std::cerr << "Failed to open index!" << endl;
            return 1;
        }

        cout << "Index opened. Record count: " << index.getRecordCount() << endl;

        // Simulate user selections
        std::vector<std::string> testIds = {
            "ID345345",      // Should find
            "ID000001",      // Should find
            "PART-Z-999",    // Should find
            "NONEXISTENT",   // Should not find
        };

        for (const auto& productId : testIds) {
            int64_t nodeKey = 0;

            auto start = std::chrono::high_resolution_clock::now();
            bool found = index.lookup(productId, nodeKey);
            auto end = std::chrono::high_resolution_clock::now();

            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

            if (found) {
                cout << "Found: " << productId
                    << " -> NodeKey: " << nodeKey
                    << " (" << duration.count() << " us)" << endl;

                // Here you would call Vis Mockup to select the node:
                // visMockup->SetNodeSelected(nodeKey, true);
            }
            else {
                cout << "Not found: " << productId
                    << " (" << duration.count() << " us)" << endl;
            }
        }
    }

    cout << "Done." << endl;
    return 0;
}

int perfTest() {
    const std::string indexPath = "large_index.dat";
    const size_t RECORD_COUNT = 2'000'000;  // 2 million records
    const size_t LOOKUP_COUNT = 1000;

    // ===========================================
    // PHASE 1: Build a large index
    // ===========================================
    cout << "Building index with " << RECORD_COUNT << " records..." << endl;

    auto buildStart = std::chrono::high_resolution_clock::now();
    {
        ProductIndex builder;

        for (size_t i = 0; i < RECORD_COUNT; ++i) {
            builder.addRecord(generateProductId(i), static_cast<int64_t>(i * 100));

            if ((i + 1) % 500000 == 0) {
                cout << "  Added " << (i + 1) << " records..." << endl;
            }
        }

        cout << "Sorting and writing to disk..." << endl;
        if (!builder.buildIndex(indexPath)) {
            std::cerr << "Failed to build index!" << endl;
            return 1;
        }
    }
    auto buildEnd = std::chrono::high_resolution_clock::now();
    auto buildDuration = std::chrono::duration_cast<std::chrono::milliseconds>(buildEnd - buildStart);

    cout << "Index built in " << buildDuration.count() << " ms" << endl;

    // ===========================================
    // PHASE 2: Performance test lookups
    // ===========================================
    ProductIndex index;
    if (!index.openIndex(indexPath)) {
        std::cerr << "Failed to open index!" << endl;
        return 1;
    }

    cout << "\nIndex opened. Record count: " << index.getRecordCount() << endl;

    // Generate random indices to lookup
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dist(0, RECORD_COUNT - 1);

    cout << "\nPerforming " << LOOKUP_COUNT << " random lookups..." << endl;

    size_t foundCount = 0;
    long long totalMicroseconds = 0;
    long long maxMicroseconds = 0;
    long long minMicroseconds = LLONG_MAX;

    for (size_t i = 0; i < LOOKUP_COUNT; ++i) {
        size_t targetIndex = dist(gen);
        std::string targetId = generateProductId(targetIndex);
        int64_t nodeKey = 0;

        auto start = std::chrono::high_resolution_clock::now();
        bool found = index.lookup(targetId, nodeKey);
        auto end = std::chrono::high_resolution_clock::now();

        long long duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        totalMicroseconds += duration;
        maxMicroseconds = std::max(maxMicroseconds, duration);
        minMicroseconds = std::min(minMicroseconds, duration);

        if (found) {
            foundCount++;
        }
    }

    // Also test some lookups that won't be found
    cout << "Performing " << LOOKUP_COUNT << " lookups for non-existent IDs..." << endl;

    size_t notFoundCount = 0;
    for (size_t i = 0; i < LOOKUP_COUNT; ++i) {
        std::string targetId = "NOTFOUND" + std::to_string(i);
        int64_t nodeKey = 0;

        auto start = std::chrono::high_resolution_clock::now();
        bool found = index.lookup(targetId, nodeKey);
        auto end = std::chrono::high_resolution_clock::now();

        long long duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        totalMicroseconds += duration;
        maxMicroseconds = std::max(maxMicroseconds, duration);
        minMicroseconds = std::min(minMicroseconds, duration);

        if (!found) {
            notFoundCount++;
        }
    }

    // Results
    cout << "\n=== Results ===" << endl;
    cout << "Records in index:    " << RECORD_COUNT << endl;
    cout << "Total lookups:       " << (LOOKUP_COUNT * 2) << endl;
    cout << "Found (expected):    " << foundCount << " / " << LOOKUP_COUNT << endl;
    cout << "Not found (expected):" << notFoundCount << " / " << LOOKUP_COUNT << endl;
    cout << std::fixed << std::setprecision(2);
    cout << "Avg lookup time:     " << (double)totalMicroseconds / (LOOKUP_COUNT * 2) << " us" << endl;
    cout << "Min lookup time:     " << minMicroseconds << " us" << endl;
    cout << "Max lookup time:     " << maxMicroseconds << " us" << endl;

    // Calculate theoretical disk reads
    size_t theoreticalReads = 0;
    size_t n = RECORD_COUNT;
    while (n > 1) {
        theoreticalReads++;
        n /= 2;
    }
    cout << "\nTheoretical max disk reads per lookup: " << theoreticalReads << " (log2 of " << RECORD_COUNT << ")" << endl;

    return 0;
}

void waitForEnter(std::string msg) {
    cout << msg << endl;
    cout << "Press Enter to continue..." << endl;
    std::string dummy;
    std::getline(std::cin, dummy);
}

int main() {
    int retCode = 0;
    waitForEnter("Start Basic Test");
    retCode = basicTest();
    if (retCode != 0) {
        std::cerr << "Basic Test Failed" << endl;
        return retCode;
    }
    cout << endl;
    waitForEnter("Start Performance Test");
    retCode = perfTest();
    if (retCode != 0) {
        std::cerr << "Performance Test Failed" << endl;
        return retCode;
    }
    cout << endl;
    cout << "All tests completed successfully" << endl;
    return retCode;
}
