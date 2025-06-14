#include "ai_engine.h"
#include <iostream>
#include <fstream>
#include <regex>
#include <thread>
#include <sstream>
#include <algorithm>

// Note: This is a mock implementation since llama.cpp is not installed
// In a real implementation, you would include llama.cpp headers:
// #include "llama.h"
// #include "common.h"

namespace work_assistant {

// Default AI prompts configuration
AIPromptConfig::AIPromptConfig() {
    LoadDefaultPrompts();
}

void AIPromptConfig::LoadDefaultPrompts() {
    system_prompt = R"(You are an AI assistant that analyzes computer screen content to classify work activities. 
Your task is to categorize content based on productivity, work type, and priority level.
Respond with structured classifications only, no explanations.)";

    classification_prompt = R"(Analyze this content and classify it:

Content Text: "{text}"
Window Title: "{title}"
Application: "{app}"

Classify as one of: DOCUMENT, CODE, EMAIL, WEB_BROWSING, SOCIAL_MEDIA, PRODUCTIVITY, ENTERTAINMENT, COMMUNICATION, DEVELOPMENT, DESIGN, EDUCATION, FINANCE, SETTINGS

Priority (1-5): How urgent/important is this activity?
Work Category: FOCUSED_WORK, COMMUNICATION, RESEARCH, LEARNING, PLANNING, BREAK_TIME, ADMINISTRATIVE, CREATIVE, ANALYSIS, COLLABORATION

Respond in format:
TYPE: [classification]
PRIORITY: [1-5]
CATEGORY: [work_category]
PRODUCTIVE: [true/false]
CONFIDENCE: [0.0-1.0])";

    priority_prompt = R"(Rate the priority of this work activity from 1-5:
1 = Very Low (breaks, casual browsing)
2 = Low (routine tasks, organization)
3 = Medium (regular work, communication)
4 = High (important tasks, deadlines)
5 = Very High (critical work, urgent issues)

Content: "{text}"
Window: "{title}"
App: "{app}")";

    category_prompt = R"(Categorize this work activity:
- FOCUSED_WORK: Deep work, coding, writing, analysis
- COMMUNICATION: Meetings, emails, messaging
- RESEARCH: Information gathering, reading documentation
- LEARNING: Tutorials, courses, training
- PLANNING: Project management, scheduling
- BREAK_TIME: Social media, entertainment, personal browsing
- ADMINISTRATIVE: File management, settings, routine tasks
- CREATIVE: Design, content creation, brainstorming
- ANALYSIS: Data analysis, reporting, metrics
- COLLABORATION: Shared work, reviews, team activities

Content: "{text}"
Context: "{title}" in "{app}")";
}

// Mock llama.cpp engine implementation
class LlamaCppEngine : public IAIEngine {
public:
    LlamaCppEngine() 
        : m_initialized(false)
        , m_model_loaded(false)
        , m_total_processed(0)
        , m_total_processing_time(0.0) {
    }

    ~LlamaCppEngine() override {
        Shutdown();
    }

    bool Initialize(const AIPromptConfig& config = AIPromptConfig()) override {
        if (m_initialized) {
            return true;
        }

        m_config = config;
        
        // In real implementation, initialize llama.cpp:
        // llama_backend_init(false);
        // m_model_params = llama_model_default_params();
        // m_context_params = llama_context_default_params();
        
        std::cout << "Mock LLaMA Engine initialized" << std::endl;
        m_initialized = true;
        return true;
    }

    void Shutdown() override {
        if (!m_initialized) {
            return;
        }

        UnloadModel();
        
        // In real implementation:
        // llama_backend_free();
        
        m_initialized = false;
        std::cout << "LLaMA Engine shut down" << std::endl;
    }

    bool IsInitialized() const override {
        return m_initialized;
    }

    bool LoadModel(const std::string& model_path) override {
        if (!m_initialized) {
            return false;
        }

        if (m_model_loaded) {
            UnloadModel();
        }

        // In real implementation:
        // m_model = llama_load_model_from_file(model_path.c_str(), m_model_params);
        // if (!m_model) {
        //     std::cerr << "Failed to load model: " << model_path << std::endl;
        //     return false;
        // }
        // m_context = llama_new_context_with_model(m_model, m_context_params);

        // Mock implementation
        m_model_info.name = "Qwen2.5-1.5B-Instruct";
        m_model_info.path = model_path;
        m_model_info.type = "gguf";
        m_model_info.size_mb = 1024;  // 1GB
        m_model_info.is_loaded = true;
        m_model_info.supports_classification = true;
        m_model_info.avg_tokens_per_second = 50.0f;
        m_model_info.recommended_context = 2048;

        m_model_loaded = true;
        std::cout << "Mock model loaded: " << model_path << std::endl;
        return true;
    }

    void UnloadModel() override {
        if (!m_model_loaded) {
            return;
        }

        // In real implementation:
        // if (m_context) {
        //     llama_free(m_context);
        //     m_context = nullptr;
        // }
        // if (m_model) {
        //     llama_free_model(m_model);
        //     m_model = nullptr;
        // }

        m_model_info = AIModelInfo();
        m_model_loaded = false;
        std::cout << "Model unloaded" << std::endl;
    }

    AIModelInfo GetModelInfo() const override {
        return m_model_info;
    }

    std::vector<std::string> GetSupportedFormats() const override {
        return {"gguf", "ggml", "bin"};
    }

    ContentAnalysis AnalyzeContent(const OCRDocument& ocr_result, 
                                  const std::string& window_title,
                                  const std::string& app_name) override {
        if (!m_initialized || !m_model_loaded) {
            return ContentAnalysis();
        }

        auto start_time = std::chrono::high_resolution_clock::now();
        
        ContentAnalysis analysis;
        analysis.timestamp = std::chrono::system_clock::now();
        analysis.title = window_title;
        analysis.application = app_name;
        analysis.extracted_text = ocr_result.GetOrderedText();

        // Mock AI analysis based on simple heuristics
        analysis = MockAnalyzeContent(analysis.extracted_text, window_title, app_name);

        auto end_time = std::chrono::high_resolution_clock::now();
        analysis.processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        // Update statistics
        m_total_processed++;
        m_total_processing_time += analysis.processing_time.count();

        return analysis;
    }

    std::future<ContentAnalysis> AnalyzeContentAsync(const OCRDocument& ocr_result,
                                                    const std::string& window_title,
                                                    const std::string& app_name) override {
        return std::async(std::launch::async, [this, ocr_result, window_title, app_name]() {
            return AnalyzeContent(ocr_result, window_title, app_name);
        });
    }

    std::vector<ContentAnalysis> AnalyzeBatch(const std::vector<OCRDocument>& documents) override {
        std::vector<ContentAnalysis> results;
        results.reserve(documents.size());

        for (const auto& doc : documents) {
            results.push_back(AnalyzeContent(doc, "Batch Analysis", "Unknown"));
        }

        return results;
    }

    ContentAnalysis AnalyzeText(const std::string& text, const std::string& context) override {
        OCRDocument mock_doc;
        mock_doc.full_text = text;
        return AnalyzeContent(mock_doc, context, "Unknown");
    }

    void UpdateConfig(const AIPromptConfig& config) override {
        m_config = config;
    }

    AIPromptConfig GetConfig() const override {
        return m_config;
    }

    float GetAverageProcessingTime() const override {
        if (m_total_processed == 0) {
            return 0.0f;
        }
        return static_cast<float>(m_total_processing_time) / m_total_processed;
    }

    size_t GetTotalProcessedItems() const override {
        return m_total_processed;
    }

    std::string GetEngineInfo() const override {
        return "Mock LLaMA.cpp Engine v1.0 (Qwen2.5-1.5B compatible)";
    }

private:
    ContentAnalysis MockAnalyzeContent(const std::string& text, 
                                      const std::string& window_title,
                                      const std::string& app_name) {
        ContentAnalysis analysis;
        
        // Mock analysis based on keywords and patterns
        std::string combined_text = text + " " + window_title + " " + app_name;
        std::transform(combined_text.begin(), combined_text.end(), combined_text.begin(), ::tolower);

        // Content type classification
        if (combined_text.find("code") != std::string::npos || 
            combined_text.find("vscode") != std::string::npos ||
            combined_text.find("vim") != std::string::npos ||
            combined_text.find("github") != std::string::npos ||
            app_name.find("Code") != std::string::npos) {
            analysis.content_type = ContentType::CODE;
            analysis.work_category = WorkCategory::FOCUSED_WORK;
            analysis.priority = ActivityPriority::HIGH;
            analysis.is_productive = true;
            analysis.is_focused_work = true;
        }
        else if (combined_text.find("email") != std::string::npos ||
                 combined_text.find("outlook") != std::string::npos ||
                 combined_text.find("gmail") != std::string::npos ||
                 app_name.find("Mail") != std::string::npos) {
            analysis.content_type = ContentType::EMAIL;
            analysis.work_category = WorkCategory::COMMUNICATION;
            analysis.priority = ActivityPriority::MEDIUM;
            analysis.is_productive = true;
        }
        else if (combined_text.find("facebook") != std::string::npos ||
                 combined_text.find("twitter") != std::string::npos ||
                 combined_text.find("instagram") != std::string::npos ||
                 combined_text.find("reddit") != std::string::npos) {
            analysis.content_type = ContentType::SOCIAL_MEDIA;
            analysis.work_category = WorkCategory::BREAK_TIME;
            analysis.priority = ActivityPriority::LOW;
            analysis.is_productive = false;
            analysis.distraction_level = 7;
        }
        else if (combined_text.find("youtube") != std::string::npos ||
                 combined_text.find("netflix") != std::string::npos ||
                 combined_text.find("video") != std::string::npos) {
            analysis.content_type = ContentType::ENTERTAINMENT;
            analysis.work_category = WorkCategory::BREAK_TIME;
            analysis.priority = ActivityPriority::VERY_LOW;
            analysis.is_productive = false;
            analysis.distraction_level = 8;
        }
        else if (combined_text.find("document") != std::string::npos ||
                 combined_text.find("word") != std::string::npos ||
                 combined_text.find("excel") != std::string::npos ||
                 app_name.find("Office") != std::string::npos) {
            analysis.content_type = ContentType::PRODUCTIVITY;
            analysis.work_category = WorkCategory::FOCUSED_WORK;
            analysis.priority = ActivityPriority::MEDIUM;
            analysis.is_productive = true;
            analysis.is_focused_work = true;
        }
        else if (combined_text.find("browser") != std::string::npos ||
                 combined_text.find("chrome") != std::string::npos ||
                 combined_text.find("firefox") != std::string::npos) {
            analysis.content_type = ContentType::WEB_BROWSING;
            analysis.work_category = WorkCategory::RESEARCH;
            analysis.priority = ActivityPriority::MEDIUM;
            analysis.is_productive = true;
        }
        else {
            analysis.content_type = ContentType::UNKNOWN;
            analysis.work_category = WorkCategory::UNKNOWN;
            analysis.priority = ActivityPriority::MEDIUM;
        }

        // Set confidence scores
        analysis.classification_confidence = 0.75f + (static_cast<float>(rand()) / RAND_MAX) * 0.2f;
        analysis.priority_confidence = 0.70f + (static_cast<float>(rand()) / RAND_MAX) * 0.25f;
        analysis.category_confidence = 0.80f + (static_cast<float>(rand()) / RAND_MAX) * 0.15f;

        // Extract mock keywords
        analysis.keywords = ai_utils::ExtractEntities(text);
        if (analysis.keywords.empty()) {
            // Generate some mock keywords based on content type
            switch (analysis.content_type) {
                case ContentType::CODE:
                    analysis.keywords = {"programming", "development", "coding"};
                    break;
                case ContentType::EMAIL:
                    analysis.keywords = {"communication", "email", "message"};
                    break;
                case ContentType::SOCIAL_MEDIA:
                    analysis.keywords = {"social", "networking", "posts"};
                    break;
                default:
                    analysis.keywords = {"work", "activity", "content"};
                    break;
            }
        }

        // Set attention level
        analysis.requires_attention = (analysis.priority >= ActivityPriority::HIGH);

        return analysis;
    }

private:
    bool m_initialized;
    bool m_model_loaded;
    AIPromptConfig m_config;
    AIModelInfo m_model_info;
    
    // Statistics
    size_t m_total_processed;
    double m_total_processing_time;

    // In real implementation, these would be llama.cpp objects:
    // llama_model* m_model = nullptr;
    // llama_context* m_context = nullptr;
    // llama_model_params m_model_params;
    // llama_context_params m_context_params;
};

// AI Engine Factory implementation
std::unique_ptr<IAIEngine> AIEngineFactory::Create(EngineType type) {
    switch (type) {
        case EngineType::LLAMA_CPP:
            return std::make_unique<LlamaCppEngine>();
        
        case EngineType::OLLAMA:
            std::cerr << "Ollama engine not implemented yet" << std::endl;
            return nullptr;
        
        case EngineType::OPENAI_API:
            std::cerr << "OpenAI API engine not implemented yet" << std::endl;
            return nullptr;
        
        case EngineType::LOCAL_TRANSFORMER:
            std::cerr << "Local transformer engine not implemented yet" << std::endl;
            return nullptr;
        
        default:
            return nullptr;
    }
}

std::vector<AIEngineFactory::EngineType> AIEngineFactory::GetAvailableEngines() {
    return {EngineType::LLAMA_CPP};
}

std::vector<AIModelInfo> AIEngineFactory::FindAvailableModels(const std::string& models_dir) {
    std::vector<AIModelInfo> models;
    
    // Mock available models
    models.emplace_back("Qwen2.5-1.5B-Instruct", models_dir + "/qwen2.5-1.5b-instruct.gguf");
    models.emplace_back("Llama-2-7B-Chat", models_dir + "/llama-2-7b-chat.gguf");
    models.emplace_back("Mistral-7B-Instruct", models_dir + "/mistral-7b-instruct.gguf");
    models.emplace_back("Phi-3-Mini", models_dir + "/phi-3-mini.gguf");
    
    // In real implementation, scan directory for .gguf files:
    // for (const auto& entry : std::filesystem::directory_iterator(models_dir)) {
    //     if (entry.path().extension() == ".gguf") {
    //         models.emplace_back(entry.path().stem().string(), entry.path().string());
    //     }
    // }
    
    return models;
}

// Utility functions implementation
namespace ai_utils {

std::string ContentTypeToString(ContentType type) {
    switch (type) {
        case ContentType::DOCUMENT: return "DOCUMENT";
        case ContentType::CODE: return "CODE";
        case ContentType::EMAIL: return "EMAIL";
        case ContentType::WEB_BROWSING: return "WEB_BROWSING";
        case ContentType::SOCIAL_MEDIA: return "SOCIAL_MEDIA";
        case ContentType::PRODUCTIVITY: return "PRODUCTIVITY";
        case ContentType::ENTERTAINMENT: return "ENTERTAINMENT";
        case ContentType::COMMUNICATION: return "COMMUNICATION";
        case ContentType::DEVELOPMENT: return "DEVELOPMENT";
        case ContentType::DESIGN: return "DESIGN";
        case ContentType::EDUCATION: return "EDUCATION";
        case ContentType::FINANCE: return "FINANCE";
        case ContentType::SETTINGS: return "SETTINGS";
        default: return "UNKNOWN";
    }
}

ContentType StringToContentType(const std::string& str) {
    if (str == "DOCUMENT") return ContentType::DOCUMENT;
    if (str == "CODE") return ContentType::CODE;
    if (str == "EMAIL") return ContentType::EMAIL;
    if (str == "WEB_BROWSING") return ContentType::WEB_BROWSING;
    if (str == "SOCIAL_MEDIA") return ContentType::SOCIAL_MEDIA;
    if (str == "PRODUCTIVITY") return ContentType::PRODUCTIVITY;
    if (str == "ENTERTAINMENT") return ContentType::ENTERTAINMENT;
    if (str == "COMMUNICATION") return ContentType::COMMUNICATION;
    if (str == "DEVELOPMENT") return ContentType::DEVELOPMENT;
    if (str == "DESIGN") return ContentType::DESIGN;
    if (str == "EDUCATION") return ContentType::EDUCATION;
    if (str == "FINANCE") return ContentType::FINANCE;
    if (str == "SETTINGS") return ContentType::SETTINGS;
    return ContentType::UNKNOWN;
}

std::string WorkCategoryToString(WorkCategory category) {
    switch (category) {
        case WorkCategory::FOCUSED_WORK: return "FOCUSED_WORK";
        case WorkCategory::COMMUNICATION: return "COMMUNICATION";
        case WorkCategory::RESEARCH: return "RESEARCH";
        case WorkCategory::LEARNING: return "LEARNING";
        case WorkCategory::PLANNING: return "PLANNING";
        case WorkCategory::BREAK_TIME: return "BREAK_TIME";
        case WorkCategory::ADMINISTRATIVE: return "ADMINISTRATIVE";
        case WorkCategory::CREATIVE: return "CREATIVE";
        case WorkCategory::ANALYSIS: return "ANALYSIS";
        case WorkCategory::COLLABORATION: return "COLLABORATION";
        default: return "UNKNOWN";
    }
}

WorkCategory StringToWorkCategory(const std::string& str) {
    if (str == "FOCUSED_WORK") return WorkCategory::FOCUSED_WORK;
    if (str == "COMMUNICATION") return WorkCategory::COMMUNICATION;
    if (str == "RESEARCH") return WorkCategory::RESEARCH;
    if (str == "LEARNING") return WorkCategory::LEARNING;
    if (str == "PLANNING") return WorkCategory::PLANNING;
    if (str == "BREAK_TIME") return WorkCategory::BREAK_TIME;
    if (str == "ADMINISTRATIVE") return WorkCategory::ADMINISTRATIVE;
    if (str == "CREATIVE") return WorkCategory::CREATIVE;
    if (str == "ANALYSIS") return WorkCategory::ANALYSIS;
    if (str == "COLLABORATION") return WorkCategory::COLLABORATION;
    return WorkCategory::UNKNOWN;
}

std::string ActivityPriorityToString(ActivityPriority priority) {
    switch (priority) {
        case ActivityPriority::VERY_LOW: return "VERY_LOW";
        case ActivityPriority::LOW: return "LOW";
        case ActivityPriority::MEDIUM: return "MEDIUM";
        case ActivityPriority::HIGH: return "HIGH";
        case ActivityPriority::VERY_HIGH: return "VERY_HIGH";
        default: return "MEDIUM";
    }
}

bool IsProductiveContentType(ContentType type) {
    switch (type) {
        case ContentType::DOCUMENT:
        case ContentType::CODE:
        case ContentType::EMAIL:
        case ContentType::PRODUCTIVITY:
        case ContentType::DEVELOPMENT:
        case ContentType::DESIGN:
        case ContentType::EDUCATION:
        case ContentType::FINANCE:
            return true;
        default:
            return false;
    }
}

bool IsFocusedWorkCategory(WorkCategory category) {
    switch (category) {
        case WorkCategory::FOCUSED_WORK:
        case WorkCategory::CREATIVE:
        case WorkCategory::ANALYSIS:
            return true;
        default:
            return false;
    }
}

float CalculateProductivityScore(const ContentAnalysis& analysis) {
    float score = 0.0f;
    
    // Base score from content type
    if (IsProductiveContentType(analysis.content_type)) {
        score += 0.4f;
    }
    
    // Work category bonus
    if (IsFocusedWorkCategory(analysis.work_category)) {
        score += 0.3f;
    } else if (analysis.work_category == WorkCategory::COMMUNICATION ||
               analysis.work_category == WorkCategory::LEARNING) {
        score += 0.2f;
    }
    
    // Priority contribution
    score += static_cast<float>(static_cast<int>(analysis.priority)) * 0.1f;
    
    // Distraction penalty
    if (analysis.distraction_level > 5) {
        score -= 0.2f;
    }
    
    // Focus bonus
    if (analysis.is_focused_work) {
        score += 0.2f;
    }
    
    return std::max(0.0f, std::min(1.0f, score));
}

std::vector<std::string> ExtractEntities(const std::string& text) {
    std::vector<std::string> entities;
    
    // Simple entity extraction using regex patterns
    std::regex word_regex(R"(\b[A-Z][a-z]+\b)");  // Capitalized words
    std::sregex_iterator iter(text.begin(), text.end(), word_regex);
    std::sregex_iterator end;
    
    for (; iter != end; ++iter) {
        std::string word = iter->str();
        if (word.length() > 2) {  // Filter short words
            entities.push_back(word);
        }
    }
    
    // Remove duplicates
    std::sort(entities.begin(), entities.end());
    entities.erase(std::unique(entities.begin(), entities.end()), entities.end());
    
    // Limit to top entities
    if (entities.size() > 10) {
        entities.resize(10);
    }
    
    return entities;
}

bool ValidateModelFile(const std::string& model_path) {
    std::ifstream file(model_path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Check file size (should be at least 100MB for a reasonable model)
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.close();
    
    return file_size > 100 * 1024 * 1024;  // 100MB minimum
}

size_t EstimateModelMemoryUsage(const std::string& model_path) {
    std::ifstream file(model_path, std::ios::binary);
    if (!file.is_open()) {
        return 0;
    }
    
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.close();
    
    // Rough estimation: model size + context memory + overhead
    return file_size + (4 * 1024 * 1024) + (100 * 1024 * 1024);  // +4MB context +100MB overhead
}

std::vector<std::string> GetRecommendedModels() {
    return {
        "Qwen2.5-1.5B-Instruct (Recommended for classification)",
        "Llama-2-7B-Chat (High quality, needs more memory)",
        "Mistral-7B-Instruct (Good balance of size/quality)",
        "Phi-3-Mini (Fastest, smaller model)"
    };
}

std::string BuildClassificationPrompt(const std::string& text, 
                                     const std::string& window_title,
                                     const std::string& app_name) {
    std::string prompt = R"(Analyze this screen content and classify the work activity:

TEXT CONTENT:
{text}

WINDOW TITLE: {title}
APPLICATION: {app}

Classify into one of these content types:
- DOCUMENT: Text documents, PDFs, writing
- CODE: Programming, development work  
- EMAIL: Email applications, communication
- WEB_BROWSING: Web pages, research browsing
- SOCIAL_MEDIA: Social platforms, personal networking
- PRODUCTIVITY: Office apps, spreadsheets, presentations
- ENTERTAINMENT: Videos, games, media consumption
- COMMUNICATION: Chat, messaging, meetings
- DEVELOPMENT: IDEs, terminals, dev tools
- DESIGN: Design software, graphics, creative work
- EDUCATION: Learning materials, tutorials, courses
- FINANCE: Financial apps, banking, accounting
- SETTINGS: System settings, configuration

Respond in exact format:
TYPE: [classification]
PRIORITY: [1-5]
CATEGORY: [FOCUSED_WORK|COMMUNICATION|RESEARCH|LEARNING|PLANNING|BREAK_TIME|ADMINISTRATIVE|CREATIVE|ANALYSIS|COLLABORATION]
PRODUCTIVE: [true|false]
CONFIDENCE: [0.0-1.0])";

    // Replace placeholders
    std::string result = prompt;
    size_t pos = 0;
    while ((pos = result.find("{text}", pos)) != std::string::npos) {
        result.replace(pos, 6, text.substr(0, 500));  // Limit text length
        pos += std::min(text.length(), size_t(500));
    }
    while ((pos = result.find("{title}")) != std::string::npos) {
        result.replace(pos, 7, window_title);
    }
    while ((pos = result.find("{app}")) != std::string::npos) {
        result.replace(pos, 5, app_name);
    }
    
    return result;
}

std::string ParseClassificationResponse(const std::string& response) {
    // This would parse the model's response in a real implementation
    // For now, return the response as-is
    return response;
}

} // namespace ai_utils

} // namespace work_assistant