#include <gtest/gtest.h>
#include "../include/util.h"
#include <vector>
#include <chrono>
#include <thread>

class UtilTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up test data
        test_vector = {5, 2, 8, 2, 1, 9, 5};
        duplicate_vector = {1, 3, 2, 3, 1, 4, 2};
    }

    std::vector<int> test_vector;
    std::vector<int> duplicate_vector;
};

TEST_F(UtilTest, MakeSetRemovesDuplicates) {
    std::vector<int> vec = duplicate_vector;
    util::make_set(vec);
    
    // Check that duplicates are removed and vector is sorted
    EXPECT_EQ(vec.size(), 4);  // Should have 1, 2, 3, 4
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[1], 2);
    EXPECT_EQ(vec[2], 3);
    EXPECT_EQ(vec[3], 4);
}

TEST_F(UtilTest, MakeSetEmptyVector) {
    std::vector<int> empty_vec;
    util::make_set(empty_vec);
    EXPECT_TRUE(empty_vec.empty());
}

TEST_F(UtilTest, RemoveSetFunction) {
    std::vector<int> vec = {1, 2, 3, 4, 5, 6, 7};
    std::vector<int> to_remove = {2, 4, 6};
    
    util::remove_set(vec, to_remove);
    
    // Should have 1, 3, 5, 7 remaining
    std::vector<int> expected = {1, 3, 5, 7};
    EXPECT_EQ(vec, expected);
}

TEST_F(UtilTest, SummarizeFunction) {
    std::vector<int> vec = {1, 2, 3, 4, 5};
    
    util::Summary summary = util::summarize(vec, [](int x) { return (double)x; });
    
    EXPECT_EQ(summary.min, 1.0);
    EXPECT_EQ(summary.max, 5.0);
    EXPECT_EQ(summary.avg, 3.0);
}

TEST_F(UtilTest, SizeSumFunction) {
    std::vector<std::vector<int>> vec_of_vecs = {
        {1, 2, 3},      // size 3
        {4, 5},         // size 2
        {6, 7, 8, 9}    // size 4
    };
    
    size_t total = util::size_sum(vec_of_vecs);
    EXPECT_EQ(total, 9);  // 3 + 2 + 4 = 9
}

TEST_F(UtilTest, SizesFunction) {
    std::vector<std::vector<int>> vec_of_vecs = {
        {1, 2, 3},      // size 3
        {4, 5},         // size 2
        {6, 7, 8, 9}    // size 4
    };
    
    std::vector<size_t> sizes = util::sizes(vec_of_vecs);
    std::vector<size_t> expected = {3, 2, 4};
    EXPECT_EQ(sizes, expected);
}

TEST_F(UtilTest, RandomFunction) {
    std::vector<int> vec = {10, 20, 30, 40, 50};
    
    // Test multiple random selections to ensure they're within bounds
    for (int i = 0; i < 10; i++) {
        int random_val = util::random(vec);
        bool found = std::find(vec.begin(), vec.end(), random_val) != vec.end();
        EXPECT_TRUE(found);
    }
}

TEST_F(UtilTest, TimerFunctionality) {
    util::start_timer();
    
    // Sleep for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    double elapsed = util::stop_timer();
    
    // Should be at least 10ms (0.01 seconds), but allow for some variance
    EXPECT_GE(elapsed, 0.008);  // Allow for some timing variance
    EXPECT_LT(elapsed, 0.1);    // Should be much less than 100ms
}

TEST_F(UtilTest, MinBucketQueue) {
    util::min_bucket_queue<int> queue;
    
    // Add items to different buckets
    queue.push(10, 2);
    queue.push(20, 1);
    queue.push(30, 3);
    queue.push(40, 1);
    
    EXPECT_FALSE(queue.empty());
    
    // Should pop from lowest bucket first (bucket 1)
    int val1 = queue.pop();
    EXPECT_TRUE(val1 == 20 || val1 == 40);  // Either item from bucket 1
    
    int val2 = queue.pop();
    EXPECT_TRUE(val2 == 20 || val2 == 40);  // The other item from bucket 1
    EXPECT_TRUE(val1 != val2);  // Should be different values
    
    // Next should be from bucket 2
    EXPECT_EQ(queue.pop(), 10);
    
    // Finally from bucket 3
    EXPECT_EQ(queue.pop(), 30);
    
    EXPECT_TRUE(queue.empty());
}