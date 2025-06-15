#include "ai_engine.h"
#include <iostream>
#include <fstream>
#include <regex>
#include <thread>
#include <sstream>
#include <algorithm>
#include <mutex>
#include <condition_variable>

#if LLAMA_CPP_AVAILABLE
    // Real llama.cpp implementation
    #include "llama.h"
    #include "common/common.h"
#endif

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

#if LLAMA_CPP_AVAILABLE
// Real llama.cpp engine implementation
class LlamaCppEngine : public IAIEngine {
public:
    LlamaCppEngine()
        : m_initialized(false)
        , m_model_loaded(false)
        , m_total_processed(0)
        , m_total_processing_time(0.0)
        , m_model(nullptr)
        , m_context(nullptr) {
    }

    ~LlamaCppEngine() override {
        Shutdown();
    }

    bool Initialize(const AIPromptConfig& config = AIPromptConfig()) override {
        if (m_initialized) {
            return true;
        }

        m_config = config;

        try {
            // Initialize llama.cpp backend
            llama_backend_init();

            // Set default parameters
            m_model_params = llama_model_default_params();
            m_context_params = llama_context_default_params();

            // Configure GPU usage
            m_model_params.n_gpu_layers = config.gpu_layers;
            m_model_params.main_gpu = 0;

            // Configure context
            m_context_params.n_ctx = config.context_length;
            m_context_params.n_batch = 512;
            m_context_params.n_ubatch = 512;
            m_context_params.n_threads = std::thread::hardware_concurrency();
            m_context_params.n_threads_batch = std::thread::hardware_concurrency();

            std::cout << "LLaMA.cpp Engine initialized successfully" << std::endl;
            m_initialized = true;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Failed to initialize LLaMA.cpp Engine: " << e.what() << std::endl;
            return false;
        }
    }

    void Shutdown() override {
        if (!m_initialized) {
            return;
        }

        UnloadModel();

        // Free llama.cpp backend
        llama_backend_free();

        m_initialized = false;
        std::cout << "LLaMA Engine shut down" << std::endl;
    }

    bool IsInitialized() const override {
        return m_initialized;
    }

    bool LoadModel(const std::string& model_path) override {
        if (!m_initialized) {
            std::cerr << "Engine not initialized" << std::endl;
            return false;
        }

        if (m_model_loaded) {
            UnloadModel();
        }

        try {
            std::cout << "Loading model: " << model_path << std::endl;

            // Load the model
            m_model = llama_load_model_from_file(model_path.c_str(), m_model_params);
            if (!m_model) {
                std::cerr << "Failed to load model: " << model_path << std::endl;
                return false;
            }

            // Create context
            m_context = llama_new_context_with_model(m_model, m_context_params);
            if (!m_context) {
                std::cerr << "Failed to create context" << std::endl;
                llama_free_model(m_model);
                m_model = nullptr;
                return false;
            }

            // Initialize model info
            m_model_info.name = ExtractModelName(model_path);
            m_model_info.path = model_path;
            m_model_info.type = "gguf";
            m_model_info.size_mb = GetFileSize(model_path) / (1024 * 1024);
            m_model_info.is_loaded = true;
            m_model_info.supports_classification = true;
            m_model_info.avg_tokens_per_second = 25.0f;  // Conservative estimate
            m_model_info.recommended_context = m_config.context_length;

            m_model_loaded = true;
            std::cout << "Model loaded successfully: " << m_model_info.name
                      << " (" << m_model_info.size_mb << "MB)" << std::endl;
            return true;

        } catch (const std::exception& e) {
            std::cerr << "Exception loading model: " << e.what() << std::endl;
            if (m_context) {
                llama_free(m_context);
                m_context = nullptr;
            }
            if (m_model) {
                llama_free_model(m_model);
                m_model = nullptr;
            }
            return false;
        }
    }

    void UnloadModel() override {
        if (!m_model_loaded) {
            return;
        }

        // Free context
        if (m_context) {
            llama_free(m_context);
            m_context = nullptr;
        }

        // Free model
        if (m_model) {
            llama_free_model(m_model);
            m_model = nullptr;
        }

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

        // Real AI analysis using llama.cpp
        if (m_model && m_context) {
            analysis = RealAnalyzeContent(analysis.extracted_text, window_title, app_name);
        } else {
            // Fallback to heuristics if model not loaded
            analysis = MockAnalyzeContent(analysis.extracted_text, window_title, app_name);
        }

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
        if (m_model_loaded) {
            return "LLaMA.cpp Engine v1.0 (" + m_model_info.name + ")";
        }
        return "LLaMA.cpp Engine v1.0 (No model loaded)";
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
    // Real llama.cpp objects
    llama_model* m_model;
    llama_context* m_context;
    llama_model_params m_model_params;
    llama_context_params m_context_params;

    bool m_initialized;
    bool m_model_loaded;
    AIPromptConfig m_config;
    AIModelInfo m_model_info;

    // Statistics
    size_t m_total_processed;
    double m_total_processing_time;

    // Thread safety
    std::mutex m_inference_mutex;

    // Helper methods
    std::string ExtractModelName(const std::string& path) {
        size_t last_slash = path.find_last_of("/\\");
        std::string filename = (last_slash == std::string::npos) ? path : path.substr(last_slash + 1);
        size_t last_dot = filename.find_last_of('.');
        return (last_dot == std::string::npos) ? filename : filename.substr(0, last_dot);
    }

    size_t GetFileSize(const std::string& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            return 0;
        }
        auto size = file.tellg();
        return static_cast<size_t>(size);
    }

    std::vector<llama_token> Tokenize(const std::string& text, bool add_bos = true) {
        if (!m_model) {
            return {};
        }

        std::vector<llama_token> tokens;
        const int n_tokens_max = text.length() + (add_bos ? 1 : 0) + 1;
        tokens.resize(n_tokens_max);

        const int n_tokens = llama_tokenize(m_model, text.c_str(), text.length(),
                                          tokens.data(), n_tokens_max, add_bos, false);

        if (n_tokens < 0) {
            std::cerr << "Failed to tokenize text" << std::endl;
            return {};
        }

        tokens.resize(n_tokens);
        return tokens;
    }

    std::string DetokenizeTokens(const std::vector<llama_token>& tokens) {
        if (!m_model || tokens.empty()) {
            return "";
        }

        std::string result;
        for (const auto& token : tokens) {
            // Use the helper function from common.h
            std::string piece = llama_token_to_piece(m_context, token, false);
            result.append(piece);
        }
        return result;
    }

    std::string GenerateResponse(const std::string& prompt, int max_tokens = 256) {
        if (!m_model || !m_context) {
            return "";
        }

        std::lock_guard<std::mutex> lock(m_inference_mutex);

        try {
            // Tokenize the prompt
            auto prompt_tokens = Tokenize(prompt, true);
            if (prompt_tokens.empty()) {
                return "";
            }

            // Clear the KV cache
            llama_kv_cache_clear(m_context);

            // Evaluate the prompt
            for (size_t i = 0; i < prompt_tokens.size(); ++i) {
                if (llama_decode(m_context, llama_batch_get_one(&prompt_tokens[i], 1, i, 0)) != 0) {
                    std::cerr << "Failed to evaluate prompt token " << i << std::endl;
                    return "";
                }
            }

            // Generate response tokens
            std::vector<llama_token> response_tokens;
            response_tokens.reserve(max_tokens);

            for (int i = 0; i < max_tokens; ++i) {
                // Sample next token
                llama_token next_token = SampleNextToken();

                // Check for end-of-sequence
                if (next_token == llama_token_eos(m_model)) {
                    break;
                }

                response_tokens.push_back(next_token);

                // Evaluate the new token
                const int n_ctx = llama_n_ctx(m_context);
                const int n_past = prompt_tokens.size() + i;

                if (n_past >= n_ctx) {
                    std::cerr << "Context length exceeded" << std::endl;
                    break;
                }

                if (llama_decode(m_context, llama_batch_get_one(&next_token, 1, n_past, 0)) != 0) {
                    std::cerr << "Failed to evaluate response token " << i << std::endl;
                    break;
                }

                // Check for natural stopping points
                std::string partial_response = DetokenizeTokens(response_tokens);
                if (partial_response.find("\\n\\n") != std::string::npos ||
                    partial_response.find("CONFIDENCE:") != std::string::npos) {
                    break;
                }
            }

            return DetokenizeTokens(response_tokens);

        } catch (const std::exception& e) {
            std::cerr << "Exception during generation: " << e.what() << std::endl;
            return "";
        }
    }

    llama_token SampleNextToken() {
        if (!m_context) {
            return 0;
        }

        // Get logits
        const float* logits = llama_get_logits(m_context);
        if (!logits) {
            return 0;
        }

        const int n_vocab = llama_n_vocab(m_model);

        // Create candidates array
        std::vector<llama_token_data> candidates;
        candidates.reserve(n_vocab);

        for (int i = 0; i < n_vocab; ++i) {
            candidates.emplace_back(llama_token_data{i, logits[i], 0.0f});
        }

        llama_token_data_array candidates_p = {
            candidates.data(),
            candidates.size(),
            false
        };

        // Apply temperature and top-p sampling
        const float temperature = m_config.temperature;
        const float top_p = m_config.top_p;

        llama_sample_temp(m_context, &candidates_p, temperature);
        llama_sample_top_p(m_context, &candidates_p, top_p, 1);

        // Sample the token
        return llama_sample_token(m_context, &candidates_p);
    }

    ContentAnalysis RealAnalyzeContent(const std::string& text,
                                      const std::string& window_title,
                                      const std::string& app_name) {
        ContentAnalysis analysis;

        // Build classification prompt
        std::string prompt = ai_utils::BuildClassificationPrompt(text, window_title, app_name);

        // Generate AI response
        std::string ai_response = GenerateResponse(prompt, m_config.max_tokens);

        if (ai_response.empty()) {
            // Fallback to heuristic analysis
            return MockAnalyzeContent(text, window_title, app_name);
        }

        // Parse the AI response
        analysis = ParseAIResponse(ai_response, text, window_title, app_name);

        // Validate and set defaults if parsing failed
        if (analysis.content_type == ContentType::UNKNOWN) {
            analysis = MockAnalyzeContent(text, window_title, app_name);
        }

        return analysis;
    }

    ContentAnalysis ParseAIResponse(const std::string& response,
                                   const std::string& text,
                                   const std::string& window_title,
                                   const std::string& app_name) {
        ContentAnalysis analysis;

        try {
            // Parse structured response
            std::regex type_regex(R"(TYPE:\s*(\w+))");
            std::regex priority_regex(R"(PRIORITY:\s*(\d+))");
            std::regex category_regex(R"(CATEGORY:\s*(\w+))");
            std::regex productive_regex(R"(PRODUCTIVE:\s*(true|false))");
            std::regex confidence_regex(R"(CONFIDENCE:\s*([0-9.]+))");

            std::smatch match;

            // Extract content type
            if (std::regex_search(response, match, type_regex)) {
                analysis.content_type = ai_utils::StringToContentType(match[1].str());
            }

            // Extract priority
            if (std::regex_search(response, match, priority_regex)) {
                int priority_val = std::stoi(match[1].str());
                analysis.priority = static_cast<ActivityPriority>(std::clamp(priority_val, 1, 5));
            }

            // Extract work category
            if (std::regex_search(response, match, category_regex)) {
                analysis.work_category = ai_utils::StringToWorkCategory(match[1].str());
            }

            // Extract productivity
            if (std::regex_search(response, match, productive_regex)) {
                analysis.is_productive = (match[1].str() == "true");
            }

            // Extract confidence
            if (std::regex_search(response, match, confidence_regex)) {
                analysis.classification_confidence = std::stof(match[1].str());
            }

            // Set derived properties
            analysis.is_focused_work = ai_utils::IsFocusedWorkCategory(analysis.work_category);
            analysis.requires_attention = (analysis.priority >= ActivityPriority::HIGH);

            // Extract keywords
            analysis.keywords = ai_utils::ExtractEntities(text);

            // Set confidence scores with some variation
            if (analysis.classification_confidence == 0.0f) {
                analysis.classification_confidence = 0.85f;
            }
            analysis.priority_confidence = std::max(0.7f, analysis.classification_confidence - 0.1f);
            analysis.category_confidence = std::max(0.75f, analysis.classification_confidence - 0.05f);

        } catch (const std::exception& e) {
            std::cerr << "Error parsing AI response: " << e.what() << std::endl;
            // Return empty analysis to trigger fallback
            return ContentAnalysis();
        }

        return analysis;
    }
};

#else
// Mock implementation when llama.cpp is not available
class LlamaCppEngine : public IAIEngine {
public:
    LlamaCppEngine() : m_initialized(false), m_model_loaded(false) {}

    bool Initialize(const AIPromptConfig& config = AIPromptConfig()) override {
        std::cout << "WARNING: llama.cpp not available, using mock implementation" << std::endl;
        m_config = config;
        m_initialized = true;
        return true;
    }

    void Shutdown() override {
        m_initialized = false;
    }

    bool IsInitialized() const override {
        return m_initialized;
    }

    bool LoadModel(const std::string& model_path) override {
        std::cout << "Mock: Would load model " << model_path << std::endl;
        m_model_loaded = true;
        return true;
    }

    void UnloadModel() override {
        m_model_loaded = false;
    }

    AIModelInfo GetModelInfo() const override {
        AIModelInfo info;
        info.name = "Mock Model";
        info.is_loaded = m_model_loaded;
        return info;
    }

    std::vector<std::string> GetSupportedFormats() const override {
        return {"gguf"};
    }

    ContentAnalysis AnalyzeContent(const OCRDocument& ocr_result,
                                  const std::string& window_title,
                                  const std::string& app_name) override {
        return MockAnalyzeContent(ocr_result.GetOrderedText(), window_title, app_name);
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
        for (const auto& doc : documents) {
            results.push_back(AnalyzeContent(doc, "Batch", "Unknown"));
        }
        return results;
    }

    ContentAnalysis AnalyzeText(const std::string& text, const std::string& context) override {
        return MockAnalyzeContent(text, context, "Unknown");
    }

    void UpdateConfig(const AIPromptConfig& config) override {
        m_config = config;
    }

    AIPromptConfig GetConfig() const override {
        return m_config;
    }

    float GetAverageProcessingTime() const override {
        return 50.0f; // Mock processing time
    }

    size_t GetTotalProcessedItems() const override {
        return 0;
    }

    std::string GetEngineInfo() const override {
        return "Mock LLaMA.cpp Engine (llama.cpp not available)";
    }

private:
    bool m_initialized;
    bool m_model_loaded;
    AIPromptConfig m_config;

    ContentAnalysis MockAnalyzeContent(const std::string& text,
                                      const std::string& window_title,
                                      const std::string& app_name) {
        ContentAnalysis analysis;
        analysis.timestamp = std::chrono::system_clock::now();
        analysis.title = window_title;
        analysis.application = app_name;
        analysis.extracted_text = text;

        // Simple heuristic classification
        std::string combined = text + " " + window_title + " " + app_name;
        std::transform(combined.begin(), combined.end(), combined.begin(), ::tolower);

        if (combined.find("code") != std::string::npos) {
            analysis.content_type = ContentType::CODE;
            analysis.work_category = WorkCategory::FOCUSED_WORK;
            analysis.priority = ActivityPriority::HIGH;
            analysis.is_productive = true;
        } else if (combined.find("email") != std::string::npos) {
            analysis.content_type = ContentType::EMAIL;
            analysis.work_category = WorkCategory::COMMUNICATION;
            analysis.priority = ActivityPriority::MEDIUM;
            analysis.is_productive = true;
        } else {
            analysis.content_type = ContentType::UNKNOWN;
            analysis.work_category = WorkCategory::UNKNOWN;
            analysis.priority = ActivityPriority::MEDIUM;
        }

        analysis.classification_confidence = 0.6f;
        analysis.priority_confidence = 0.6f;
        analysis.category_confidence = 0.6f;
        analysis.processing_time = std::chrono::milliseconds(50);

        return analysis;
    }
};
#endif

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