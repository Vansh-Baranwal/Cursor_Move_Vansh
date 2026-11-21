#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include "../src/shake_detector.h"

void AssertTrue(bool condition, const std::string& test_name) {
    if (condition) {
        std::cout << "[PASS] " << test_name << std::endl;
    } else {
        std::cerr << "[FAIL] " << test_name << std::endl;
        exit(1);
    }
}

void AssertFalse(bool condition, const std::string& test_name) {
    AssertTrue(!condition, test_name);
}

// Test 1: Realistic horizontal mouse shake (4-5 rapid back-and-forth strokes)
void TestHorizontalShake() {
    ShakeDetector detector(ShakeDetector::Sensitivity::Medium);
    DWORD t = 1000;
    int base_x = 500;
    int y = 500;
    bool triggered = false;

    // Simulate 5 strokes: right, left, right, left, right
    // Each stroke moves 40 pixels over ~60ms
    int directions[] = { 1, -1, 1, -1, 1, -1 };
    int current_x = base_x;

    for (int dir : directions) {
        for (int step = 0; step < 5; ++step) {
            current_x += dir * 8; // 40 pixels total
            t += 12; // 60ms per stroke
            if (detector.OnMouseMove(current_x, y, t)) {
                triggered = true;
            }
        }
    }

    AssertTrue(triggered, "TestHorizontalShake: Rapid back-and-forth shake should trigger");
}

// Test 2: High-speed linear flick (moving 1000 pixels across dual displays)
void TestLinearSwipeFast() {
    ShakeDetector detector(ShakeDetector::Sensitivity::Medium);
    DWORD t = 1000;
    int x = 100;
    int y = 500;
    bool triggered = false;

    // Move 1000 pixels to the right at high speed (2000 px/sec)
    for (int i = 0; i < 20; ++i) {
        x += 50;
        t += 15; // 15ms intervals
        if (detector.OnMouseMove(x, y, t)) {
            triggered = true;
        }
    }

    AssertFalse(triggered, "TestLinearSwipeFast: Linear high-speed swipe must NOT trigger");
}

// Test 3: Sensor noise / micro jitter (resting hand on high DPI gaming mouse)
void TestSensorJitter() {
    ShakeDetector detector(ShakeDetector::Sensitivity::Medium);
    DWORD t = 1000;
    int x = 500;
    int y = 500;
    bool triggered = false;

    // Tiny 1-2 pixel fluctuations
    int jitters[] = { 1, -2, 2, -1, 1, -1, 2, -2, 1, -1, 2, -2 };
    for (int j : jitters) {
        x += j;
        t += 10;
        if (detector.OnMouseMove(x, y, t)) {
            triggered = true;
        }
    }

    AssertFalse(triggered, "TestSensorJitter: Sensor micro-jitter must NOT trigger");
}

// Test 4: Vertical mouse shake (rapid up and down)
void TestVerticalShake() {
    ShakeDetector detector(ShakeDetector::Sensitivity::Medium);
    DWORD t = 1000;
    int x = 500;
    int current_y = 500;
    bool triggered = false;

    int directions[] = { 1, -1, 1, -1, 1, -1 };
    for (int dir : directions) {
        for (int step = 0; step < 5; ++step) {
            current_y += dir * 8;
            t += 12;
            if (detector.OnMouseMove(x, current_y, t)) {
                triggered = true;
            }
        }
    }

    AssertTrue(triggered, "TestVerticalShake: Rapid vertical shake should trigger");
}

// Test 5: Slow zigzag navigation (reading or browsing)
void TestSlowZigzag() {
    ShakeDetector detector(ShakeDetector::Sensitivity::Medium);
    DWORD t = 1000;
    int x = 500;
    int y = 100;
    bool triggered = false;

    // Slow direction changes spread over 2 seconds (reading text)
    int directions[] = { 1, -1, 1, -1, 1, -1 };
    for (int dir : directions) {
        for (int step = 0; step < 5; ++step) {
            x += dir * 6;
            y += 10;
            t += 80; // 400ms per stroke -> too slow
            if (detector.OnMouseMove(x, y, t)) {
                triggered = true;
            }
        }
    }

    AssertFalse(triggered, "TestSlowZigzag: Slow deliberate zigzag must NOT trigger");
}

// Test 6: Sensitivity comparison (Low requires more effort than High)
void TestSensitivitySettings() {
    ShakeDetector detector_high(ShakeDetector::Sensitivity::High);
    ShakeDetector detector_low(ShakeDetector::Sensitivity::Low);

    DWORD t = 1000;
    int x = 500;
    int y = 500;
    bool triggered_high = false;
    bool triggered_low = false;

    // A lighter shake with 3 short reversals:
    int directions[] = { 1, -1, 1, -1 };
    for (int dir : directions) {
        for (int step = 0; step < 4; ++step) {
            x += dir * 8;
            t += 15;
            if (detector_high.OnMouseMove(x, y, t)) triggered_high = true;
            if (detector_low.OnMouseMove(x, y, t)) triggered_low = true;
        }
    }

    AssertTrue(triggered_high, "TestSensitivity: High sensitivity should trigger on lighter shake");
    AssertFalse(triggered_low, "TestSensitivity: Low sensitivity should NOT trigger on lighter shake");
}

int main() {
    std::cout << "Running ShakeDetector Kinematics Test Suite..." << std::endl;
    TestHorizontalShake();
    TestLinearSwipeFast();
    TestSensorJitter();
    TestVerticalShake();
    TestSlowZigzag();
    TestSensitivitySettings();
    std::cout << "All 6 Kinematics Tests Passed Successfully!" << std::endl;
    return 0;
}
