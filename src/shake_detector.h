#pragma once
#include <windows.h>
#include <cmath>
#include <algorithm>

// Kinematic movement detector inspired by macOS "Shake mouse pointer to locate"
// Designed to be mathematically robust:
// 1. High reversal count in horizontal/vertical axis within tight time window (400ms)
// 2. High ratio of total path travel distance to net spatial bounding displacement (in-place oscillation)
// 3. Rejects linear swipes, flicks across multi-monitors, and sensor micro-jitter
class ShakeDetector {
public:
    enum class Sensitivity {
        Low = 0,     // Requires more vigorous shaking (5 reversals, 240px path)
        Medium = 1,  // Balanced macOS-like default (4 reversals, 160px path)
        High = 2     // Sensitive (3 reversals, 100px path)
    };

    struct Sample {
        int x;
        int y;
        DWORD time_ms;
    };

    static constexpr int MAX_SAMPLES = 64;
    static constexpr DWORD WINDOW_DURATION_MS = 420; // 420ms sliding window

    explicit ShakeDetector(Sensitivity sensitivity = Sensitivity::Medium)
        : sensitivity_(sensitivity)
        , sample_count_(0)
        , head_index_(0)
        , last_trigger_time_(0) {
        UpdateThresholds();
    }

    void SetSensitivity(Sensitivity s) {
        sensitivity_ = s;
        UpdateThresholds();
    }

    Sensitivity GetSensitivity() const {
        return sensitivity_;
    }

    void Reset() {
        sample_count_ = 0;
        head_index_ = 0;
    }

    // Called on every mouse move. Returns true at the exact moment a shake gesture is confirmed.
    bool OnMouseMove(int x, int y, DWORD time_ms) {
        if (sample_count_ > 0) {
            const Sample& prev = GetLatestSample();
            if (prev.x == x && prev.y == y) {
                return false; // No actual movement
            }
        }

        // Add to ring buffer
        samples_[head_index_] = { x, y, time_ms };
        head_index_ = (head_index_ + 1) % MAX_SAMPLES;
        if (sample_count_ < MAX_SAMPLES) {
            sample_count_++;
        }

        // Check if we meet the shake criteria
        return EvaluateShake(time_ms);
    }

    // Evaluate current buffer against kinematics rules
    bool EvaluateShake(DWORD current_time) {
        if (sample_count_ < 6) {
            return false;
        }

        // Prune and find valid window start
        int valid_count = 0;
        int indices[MAX_SAMPLES];

        for (int i = 0; i < sample_count_; ++i) {
            int idx = (head_index_ - 1 - i + MAX_SAMPLES) % MAX_SAMPLES;
            DWORD age = current_time - samples_[idx].time_ms;
            if (age <= WINDOW_DURATION_MS) {
                indices[valid_count++] = idx;
            } else {
                break; // Samples are in chronological order
            }
        }

        if (valid_count < 6) {
            return false;
        }

        // Reverse indices so they are in chronological order [oldest ... newest]
        for (int i = 0; i < valid_count / 2; ++i) {
            std::swap(indices[i], indices[valid_count - 1 - i]);
        }

        // 1. Calculate bounding box and total path distance
        int min_x = samples_[indices[0]].x;
        int max_x = min_x;
        int min_y = samples_[indices[0]].y;
        int max_y = min_y;

        double total_path = 0.0;
        for (int i = 1; i < valid_count; ++i) {
            const Sample& p1 = samples_[indices[i - 1]];
            const Sample& p2 = samples_[indices[i]];

            min_x = (std::min)(min_x, p2.x);
            max_x = (std::max)(max_x, p2.x);
            min_y = (std::min)(min_y, p2.y);
            max_y = (std::max)(max_y, p2.y);

            double dx = p2.x - p1.x;
            double dy = p2.y - p1.y;
            total_path += std::sqrt(dx * dx + dy * dy);
        }

        double span_x = max_x - min_x;
        double span_y = max_y - min_y;
        double net_span = std::sqrt(span_x * span_x + span_y * span_y);

        // Linear swipe suppression:
        // A swipe across the screen has net_span close to total_path (ratio ~ 1.0 to 1.3).
        // Shaking in place has total_path much larger than net_span (ratio >= 1.6).
        double confinement_ratio = (net_span > 1.0) ? (total_path / net_span) : total_path;
        if (confinement_ratio < min_confinement_ratio_) {
            return false; // Moved linearly, not an oscillation
        }

        if (total_path < min_total_path_) {
            return false; // Not fast/far enough
        }

        // 2. Count directional reversals along the primary axis
        int x_reversals = CountReversalsAxis(indices, valid_count, true);
        int y_reversals = CountReversalsAxis(indices, valid_count, false);
        int total_reversals = (std::max)(x_reversals, y_reversals) + (std::min)(x_reversals, y_reversals) / 2;

        if (total_reversals >= required_reversals_) {
            // Cooldown between repeated triggers: minimum 150ms
            if (current_time - last_trigger_time_ > 150) {
                last_trigger_time_ = current_time;
                return true;
            }
        }

        return false;
    }

private:
    const Sample& GetLatestSample() const {
        int latest_idx = (head_index_ - 1 + MAX_SAMPLES) % MAX_SAMPLES;
        return samples_[latest_idx];
    }

    int CountReversalsAxis(const int* indices, int count, bool is_x_axis) const {
        int reversals = 0;
        int last_dir = 0;
        int stroke_accum = 0;

        for (int i = 1; i < count; ++i) {
            int d = is_x_axis ? 
                (samples_[indices[i]].x - samples_[indices[i - 1]].x) : 
                (samples_[indices[i]].y - samples_[indices[i - 1]].y);

            // Filter micro-jitter (< 3 pixels)
            if (std::abs(d) < 3) {
                continue;
            }

            int curr_dir = (d > 0) ? 1 : -1;

            if (last_dir == 0) {
                last_dir = curr_dir;
                stroke_accum = std::abs(d);
            } else if (curr_dir == last_dir) {
                stroke_accum += std::abs(d);
            } else {
                // Direction changed! Only count if stroke was significant (>= min_stroke_distance_)
                if (stroke_accum >= min_stroke_distance_) {
                    reversals++;
                }
                last_dir = curr_dir;
                stroke_accum = std::abs(d);
            }
        }

        return reversals;
    }

    void UpdateThresholds() {
        switch (sensitivity_) {
        case Sensitivity::Low:
            required_reversals_ = 5;
            min_stroke_distance_ = 16;
            min_total_path_ = 240.0;
            min_confinement_ratio_ = 1.8;
            break;
        case Sensitivity::High:
            required_reversals_ = 3;
            min_stroke_distance_ = 10;
            min_total_path_ = 100.0;
            min_confinement_ratio_ = 1.5;
            break;
        case Sensitivity::Medium:
        default:
            required_reversals_ = 4;
            min_stroke_distance_ = 12;
            min_total_path_ = 160.0;
            min_confinement_ratio_ = 1.65;
            break;
        }
    }

    Sensitivity sensitivity_;
    Sample samples_[MAX_SAMPLES];
    int sample_count_;
    int head_index_;
    DWORD last_trigger_time_;

    int required_reversals_;
    int min_stroke_distance_;
    double min_total_path_;
    double min_confinement_ratio_;
};
