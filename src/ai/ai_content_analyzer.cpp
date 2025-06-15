#include "ai_engine.h"
#include <iostream>
#include <algorithm>
#include <numeric>
#include <unordered_set>
#include <cmath>

namespace work_assistant {

// AIContentAnalyzer implementation
class AIContentAnalyzer::Impl {
public:
    Impl() 
        : m_initialized(false)
        , m_min_focused_ratio(0.6f)
        , m_max_distraction_level(3.0f)
        , m_learning_enabled(false)
        , m_statistics{} {
    }

    bool Initialize(const std::string& model_path, AIEngineFactory::EngineType engine_type) {
        if (m_initialized) {
            return true;
        }

        // Create AI engine
        m_engine = AIEngineFactory::Create(engine_type);
        if (!m_engine) {
            std::cerr << "Failed to create AI engine" << std::endl;
            return false;
        }

        // Initialize engine with default config
        AIPromptConfig config;
        if (!m_engine->Initialize(config)) {
            std::cerr << "Failed to initialize AI engine" << std::endl;
            m_engine.reset();
            return false;
        }

        // Load model if path provided
        if (!model_path.empty()) {
            std::cout << "Attempting to load model: " << model_path << std::endl;
            if (!m_engine->LoadModel(model_path)) {
                std::cerr << "❌ CRITICAL: Failed to load model: " << model_path << std::endl;
                std::cerr << "❌ AI analysis will not work properly without a loaded model!" << std::endl;
                // Continue without model for now
            } else {
                std::cout << "✅ Model loaded successfully!" << std::endl;
            }
        } else {
            std::cout << "⚠️  No model path provided - AI will use fallback classification" << std::endl;
        }

        m_initialized = true;
        std::cout << "AI Content Analyzer initialized with " << m_engine->GetEngineInfo() << std::endl;
        return true;
    }

    void Shutdown() {
        if (!m_initialized) {
            return;
        }

        if (m_engine) {
            m_engine->Shutdown();
            m_engine.reset();
        }

        m_initialized = false;
        std::cout << "AI Content Analyzer shut down" << std::endl;
    }

    bool IsReady() const {
        return m_initialized && m_engine && m_engine->IsInitialized();
    }

    ContentAnalysis AnalyzeWindow(const OCRDocument& ocr_result,
                                 const std::string& window_title,
                                 const std::string& app_name) {
        if (!IsReady()) {
            return ContentAnalysis();
        }

        auto start_time = std::chrono::high_resolution_clock::now();

        ContentAnalysis analysis = m_engine->AnalyzeContent(ocr_result, window_title, app_name);

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        // Update statistics
        UpdateStatistics(analysis, duration.count());

        // Post-process analysis
        PostProcessAnalysis(analysis);

        return analysis;
    }

    std::future<ContentAnalysis> AnalyzeWindowAsync(const OCRDocument& ocr_result,
                                                    const std::string& window_title,
                                                    const std::string& app_name) {
        if (!IsReady()) {
            std::promise<ContentAnalysis> promise;
            promise.set_value(ContentAnalysis());
            return promise.get_future();
        }

        return m_engine->AnalyzeContentAsync(ocr_result, window_title, app_name);
    }

    bool IsProductiveActivity(const ContentAnalysis& analysis) const {
        // Multiple criteria for productivity assessment
        bool is_productive_type = ai_utils::IsProductiveContentType(analysis.content_type);
        bool is_focused_category = ai_utils::IsFocusedWorkCategory(analysis.work_category);
        bool low_distraction = analysis.distraction_level <= m_max_distraction_level;
        bool high_priority = analysis.priority >= ActivityPriority::MEDIUM;

        // Weighted decision
        float productivity_score = 0.0f;
        if (is_productive_type) productivity_score += 0.4f;
        if (is_focused_category) productivity_score += 0.3f;
        if (low_distraction) productivity_score += 0.2f;
        if (high_priority) productivity_score += 0.1f;

        return productivity_score >= m_min_focused_ratio;
    }

    int CalculateProductivityScore(const std::vector<ContentAnalysis>& recent_activities) const {
        if (recent_activities.empty()) {
            return 50; // Neutral score
        }

        float total_score = 0.0f;
        float total_weight = 0.0f;

        // Calculate weighted productivity score
        for (size_t i = 0; i < recent_activities.size(); ++i) {
            const auto& activity = recent_activities[i];
            
            // More recent activities have higher weight
            float weight = 1.0f + (static_cast<float>(i) / recent_activities.size()) * 0.5f;
            
            float activity_score = ai_utils::CalculateProductivityScore(activity);
            
            total_score += activity_score * weight;
            total_weight += weight;
        }

        // Convert to 0-100 scale
        float normalized_score = (total_score / total_weight) * 100.0f;
        return static_cast<int>(std::round(std::clamp(normalized_score, 0.0f, 100.0f)));
    }

    std::vector<std::string> DetectWorkPatterns(const std::vector<ContentAnalysis>& activities) const {
        std::vector<std::string> patterns;

        if (activities.size() < 3) {
            return patterns;
        }

        // Pattern 1: Consistent work type
        std::unordered_map<ContentType, int> type_counts;
        for (const auto& activity : activities) {
            type_counts[activity.content_type]++;
        }

        auto max_type = std::max_element(type_counts.begin(), type_counts.end(),
                                       [](const auto& a, const auto& b) {
                                           return a.second < b.second;
                                       });

        if (max_type != type_counts.end() && 
            max_type->second >= static_cast<int>(activities.size() * 0.6f)) {
            patterns.push_back("Focused on " + ai_utils::ContentTypeToString(max_type->first));
        }

        // Pattern 2: High productivity period
        int productive_count = 0;
        for (const auto& activity : activities) {
            if (IsProductiveActivity(activity)) {
                productive_count++;
            }
        }

        float productivity_ratio = static_cast<float>(productive_count) / activities.size();
        if (productivity_ratio >= 0.8f) {
            patterns.push_back("High productivity period");
        } else if (productivity_ratio <= 0.3f) {
            patterns.push_back("Low productivity period");
        }

        // Pattern 3: Frequent context switching
        int context_switches = 0;
        for (size_t i = 1; i < activities.size(); ++i) {
            if (activities[i].content_type != activities[i-1].content_type) {
                context_switches++;
            }
        }

        if (context_switches >= static_cast<int>(activities.size() * 0.7f)) {
            patterns.push_back("Frequent task switching");
        }

        // Pattern 4: Deep work session
        int focused_streak = 0;
        int max_focused_streak = 0;
        for (const auto& activity : activities) {
            if (activity.is_focused_work) {
                focused_streak++;
                max_focused_streak = std::max(max_focused_streak, focused_streak);
            } else {
                focused_streak = 0;
            }
        }

        if (max_focused_streak >= 5) {
            patterns.push_back("Deep work session detected");
        }

        // Pattern 5: Break pattern
        bool has_breaks = false;
        for (const auto& activity : activities) {
            if (activity.work_category == WorkCategory::BREAK_TIME) {
                has_breaks = true;
                break;
            }
        }

        if (has_breaks) {
            patterns.push_back("Regular break intervals");
        } else if (activities.size() >= 10) {
            patterns.push_back("No breaks detected - consider taking breaks");
        }

        return patterns;
    }

    ContentType PredictNextActivity(const std::vector<ContentAnalysis>& recent_activities) const {
        if (recent_activities.empty()) {
            return ContentType::UNKNOWN;
        }

        // Simple prediction based on recent patterns
        std::unordered_map<ContentType, float> type_weights;
        
        // Weight recent activities more heavily
        for (size_t i = 0; i < recent_activities.size(); ++i) {
            float weight = 1.0f + static_cast<float>(i) / recent_activities.size();
            type_weights[recent_activities[i].content_type] += weight;
        }

        // Find most likely next activity
        auto max_type = std::max_element(type_weights.begin(), type_weights.end(),
                                       [](const auto& a, const auto& b) {
                                           return a.second < b.second;
                                       });

        return (max_type != type_weights.end()) ? max_type->first : ContentType::UNKNOWN;
    }

    void SetProductivityThresholds(float min_focused_ratio, float max_distraction_level) {
        m_min_focused_ratio = std::clamp(min_focused_ratio, 0.0f, 1.0f);
        m_max_distraction_level = std::clamp(max_distraction_level, 0.0f, 10.0f);
    }

    void UpdatePrompts(const AIPromptConfig& config) {
        if (m_engine) {
            m_engine->UpdateConfig(config);
        }
    }

    void EnableLearning(bool enable) {
        m_learning_enabled = enable;
        // Future: implement user feedback learning
    }

    AIContentAnalyzer::Statistics GetStatistics() const {
        return m_statistics;
    }

    void ResetStatistics() {
        m_statistics = Statistics{};
    }

private:
    void UpdateStatistics(const ContentAnalysis& analysis, double processing_time_ms) {
        m_statistics.total_analyzed++;

        if (analysis.content_type != ContentType::UNKNOWN) {
            m_statistics.successful_classifications++;
            m_statistics.type_counts[analysis.content_type]++;
        }

        if (analysis.work_category != WorkCategory::UNKNOWN) {
            m_statistics.category_counts[analysis.work_category]++;
        }

        // Update averages
        double total_time = m_statistics.average_processing_time_ms * (m_statistics.total_analyzed - 1);
        total_time += processing_time_ms;
        m_statistics.average_processing_time_ms = total_time / m_statistics.total_analyzed;

        double total_confidence = m_statistics.average_confidence * (m_statistics.successful_classifications - 1);
        total_confidence += analysis.classification_confidence;
        m_statistics.average_confidence = total_confidence / m_statistics.successful_classifications;
    }

    void PostProcessAnalysis(ContentAnalysis& analysis) {
        // Apply business rules and consistency checks
        
        // Rule 1: Break time activities should have low priority
        if (analysis.work_category == WorkCategory::BREAK_TIME) {
            analysis.priority = std::min(analysis.priority, ActivityPriority::LOW);
            analysis.is_productive = false;
            analysis.is_focused_work = false;
        }

        // Rule 2: High distraction content shouldn't be focused work
        if (analysis.distraction_level > 6) {
            analysis.is_focused_work = false;
            analysis.work_category = WorkCategory::BREAK_TIME;
        }

        // Rule 3: Consistency between content type and work category
        if (analysis.content_type == ContentType::ENTERTAINMENT ||
            analysis.content_type == ContentType::SOCIAL_MEDIA) {
            analysis.work_category = WorkCategory::BREAK_TIME;
            analysis.is_productive = false;
        }

        // Rule 4: Development and code content should be focused work
        if (analysis.content_type == ContentType::CODE ||
            analysis.content_type == ContentType::DEVELOPMENT) {
            analysis.work_category = WorkCategory::FOCUSED_WORK;
            analysis.is_focused_work = true;
            analysis.is_productive = true;
            analysis.priority = std::max(analysis.priority, ActivityPriority::MEDIUM);
        }

        // Rule 5: Communication content priority adjustment
        if (analysis.content_type == ContentType::EMAIL ||
            analysis.content_type == ContentType::COMMUNICATION) {
            analysis.work_category = WorkCategory::COMMUNICATION;
            analysis.is_productive = true;
        }

        // Calculate overall productivity flag
        analysis.is_productive = IsProductiveActivity(analysis);
    }

private:
    bool m_initialized;
    std::unique_ptr<IAIEngine> m_engine;
    
    // Configuration
    float m_min_focused_ratio;
    float m_max_distraction_level;
    bool m_learning_enabled;
    
    // Statistics
    AIContentAnalyzer::Statistics m_statistics;
};

// AIContentAnalyzer public interface implementation
AIContentAnalyzer::AIContentAnalyzer() : m_impl(std::make_unique<Impl>()) {}
AIContentAnalyzer::~AIContentAnalyzer() = default;

bool AIContentAnalyzer::Initialize(const std::string& model_path, 
                                  AIEngineFactory::EngineType engine_type) {
    return m_impl->Initialize(model_path, engine_type);
}

void AIContentAnalyzer::Shutdown() {
    m_impl->Shutdown();
}

bool AIContentAnalyzer::IsReady() const {
    return m_impl->IsReady();
}

ContentAnalysis AIContentAnalyzer::AnalyzeWindow(const OCRDocument& ocr_result,
                                                 const std::string& window_title,
                                                 const std::string& app_name) {
    return m_impl->AnalyzeWindow(ocr_result, window_title, app_name);
}

std::future<ContentAnalysis> AIContentAnalyzer::AnalyzeWindowAsync(const OCRDocument& ocr_result,
                                                                   const std::string& window_title,
                                                                   const std::string& app_name) {
    return m_impl->AnalyzeWindowAsync(ocr_result, window_title, app_name);
}

bool AIContentAnalyzer::IsProductiveActivity(const ContentAnalysis& analysis) const {
    return m_impl->IsProductiveActivity(analysis);
}

int AIContentAnalyzer::CalculateProductivityScore(const std::vector<ContentAnalysis>& recent_activities) const {
    return m_impl->CalculateProductivityScore(recent_activities);
}

std::vector<std::string> AIContentAnalyzer::DetectWorkPatterns(const std::vector<ContentAnalysis>& activities) const {
    return m_impl->DetectWorkPatterns(activities);
}

ContentType AIContentAnalyzer::PredictNextActivity(const std::vector<ContentAnalysis>& recent_activities) const {
    return m_impl->PredictNextActivity(recent_activities);
}

void AIContentAnalyzer::SetProductivityThresholds(float min_focused_ratio, float max_distraction_level) {
    m_impl->SetProductivityThresholds(min_focused_ratio, max_distraction_level);
}

void AIContentAnalyzer::UpdatePrompts(const AIPromptConfig& config) {
    m_impl->UpdatePrompts(config);
}

void AIContentAnalyzer::EnableLearning(bool enable) {
    m_impl->EnableLearning(enable);
}

AIContentAnalyzer::Statistics AIContentAnalyzer::GetStatistics() const {
    return m_impl->GetStatistics();
}

void AIContentAnalyzer::ResetStatistics() {
    m_impl->ResetStatistics();
}

} // namespace work_assistant