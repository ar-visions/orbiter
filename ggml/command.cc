#include "common-sdl.h"
#include "common.h"
#include "whisper.h"
#include "grammar-parser.h"

/*
add to import:
whisper.cpp	https://github.com/ggerganov/whisper.cpp	d682e15
	-DGGML_VULKAN=ON
	> ../models/download-ggml-model.sh base.en
	> mkdir -p $BUILD_DIR/models
	> cp ../models/ggml-base.en.bin $BUILD_DIR/models/

add libs
whisper ggml-vulkan SDL2

*/
#include <sstream>
#include <iostream>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <regex>
#include <string>
#include <thread>
#include <vector>
#include <map>
#include <alsa/asoundlib.h>
#include <sys/time.h>

// command-line parameters
struct whisper_params {
    int32_t n_threads  = std::min(4, (int32_t) std::thread::hardware_concurrency());
    int32_t prompt_ms  = 5000;
    int32_t command_ms = 8000;
    int32_t capture_id = -1;
    int32_t max_tokens = 32;
    int32_t audio_ctx  = 0;

    float vad_thold  = 0.6f;
    float freq_thold = 100.0f;

    float grammar_penalty = 100.0f;

    grammar_parser::parse_state grammar_parsed;

    bool translate     = false;
    bool print_special = false;
    bool print_energy  = false;
    bool no_timestamps = true;
    bool use_gpu       = true;
    bool flash_attn    = false;

    std::string language  = "en";
    std::string model     = "models/ggml-base.en.bin";
    std::string fname_out;
    std::string commands;
    std::string prompt;
    std::string context;
    std::string grammar;

    // A regular expression that matches tokens to suppress
    std::string suppress_regex;
};

static std::string transcribe(
                 whisper_context * ctx,
            const whisper_params & params,
        const std::vector<float> & pcmf32,
               const std::string & grammar_rule,
                           float & logprob_min,
                           float & logprob_sum,
                             int & n_tokens,
                         int64_t & t_ms) {
    const auto t_start = std::chrono::high_resolution_clock::now();

    logprob_min = 0.0f;
    logprob_sum = 0.0f;
    n_tokens    = 0;
    t_ms        = 0;

    //whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_BEAM_SEARCH);

    wparams.print_progress   = false;
    wparams.print_special    = params.print_special;
    wparams.print_realtime   = false;
    wparams.print_timestamps = !params.no_timestamps;
    wparams.translate        = params.translate;
    wparams.no_context       = true;
    wparams.no_timestamps    = params.no_timestamps;
    wparams.single_segment   = true;
    wparams.max_tokens       = params.max_tokens;
    wparams.language         = params.language.c_str();
    wparams.n_threads        = params.n_threads;

    wparams.audio_ctx = params.audio_ctx;

    wparams.temperature     = 0.4f;
    wparams.temperature_inc = 1.0f;
    wparams.greedy.best_of  = 5;

    wparams.beam_search.beam_size = 5;

    wparams.initial_prompt = params.context.data();

    wparams.suppress_regex = params.suppress_regex.c_str();

    const auto & grammar_parsed = params.grammar_parsed;
    auto grammar_rules = grammar_parsed.c_rules();

    if (!params.grammar_parsed.rules.empty() && !grammar_rule.empty()) {
        if (grammar_parsed.symbol_ids.find(grammar_rule) == grammar_parsed.symbol_ids.end()) {
            fprintf(stderr, "%s: warning: grammar rule '%s' not found - skipping grammar sampling\n", __func__, grammar_rule.c_str());
        } else {
            wparams.grammar_rules   = grammar_rules.data();
            wparams.n_grammar_rules = grammar_rules.size();
            wparams.i_start_rule    = grammar_parsed.symbol_ids.at(grammar_rule);
            wparams.grammar_penalty = params.grammar_penalty;
        }
    }

    if (whisper_full(ctx, wparams, pcmf32.data(), pcmf32.size()) != 0) {
        return "";
    }

    std::string result;

    const int n_segments = whisper_full_n_segments(ctx);
    for (int i = 0; i < n_segments; ++i) {
        const char * text = whisper_full_get_segment_text(ctx, i);

        result += text;

        const int n = whisper_full_n_tokens(ctx, i);
        for (int j = 0; j < n; ++j) {
            const auto token = whisper_full_get_token_data(ctx, i, j);

            if(token.plog > 0.0f) exit(0);
            logprob_min = std::min(logprob_min, token.plog);
            logprob_sum += token.plog;
            ++n_tokens;
        }
    }

    const auto t_end = std::chrono::high_resolution_clock::now();
    t_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();

    return result;
}

int main(int argc, char ** argv) {
    /// lets request asound at card 2 device 0
    whisper_params params;
    params.capture_id = 1;

    whisper_lang_id(params.language.c_str());

    // whisper init
    struct whisper_context_params cparams = whisper_context_default_params();

    cparams.use_gpu    = params.use_gpu;
    cparams.flash_attn = params.flash_attn;

    struct whisper_context * ctx = whisper_init_from_file_with_params(params.model.c_str(), cparams);

    // print some info about the processing
    fprintf(stderr, "\n");
    if (!whisper_is_multilingual(ctx)) {
        if (params.language != "en" || params.translate) {
            params.language = "en";
            params.translate = false;
            fprintf(stderr, "%s: WARNING: model is not multilingual, ignoring language and translation options\n", __func__);
        }
    }

    // init audio
    audio_async audio(30*1000);
    if (!audio.init(params.capture_id, WHISPER_SAMPLE_RATE)) {
        fprintf(stderr, "%s: audio.init() failed!\n", __func__);
        return 1;
    }

    audio.resume();

    // wait for 1 second to avoid any buffered noise
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    audio.clear();

    bool is_running  = true;
    bool have_prompt = false;
    bool ask_prompt  = true;

    float logprob_min0 = 0.0f;
    float logprob_min  = 0.0f;

    float logprob_sum0 = 0.0f;
    float logprob_sum  = 0.0f;

    int n_tokens0 = 0;
    int n_tokens  = 0;

    std::vector<float> pcmf32_cur;
    std::vector<float> pcmf32_prompt;

    std::string k_prompt = params.prompt.empty() ? "Engage." : params.prompt;

    fprintf(stderr, "\n");
    fprintf(stderr, "%s: general-purpose mode\n", __func__);

    // main loop
    while (is_running) {
        // handle Ctrl + C
        is_running = sdl_poll_events();

        // delay
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        audio.get(2000, pcmf32_cur);

        if (::vad_simple(pcmf32_cur, WHISPER_SAMPLE_RATE, 1000, params.vad_thold, params.freq_thold, params.print_energy)) {
            fprintf(stdout, "speech detected...\n");

            int64_t t_ms = 0;
            const auto txt = ::trim(::transcribe(ctx, params, pcmf32_cur, "root", logprob_min, logprob_sum, n_tokens, t_ms));

            //const float p = 100.0f * std::exp((logprob - logprob0) / (n_tokens - n_tokens0));
            const float p = 100.0f * std::exp(logprob_min);

            // cut the prompt from the decoded text
            fprintf(stdout, "%s: Command '%s%s%s', (t = %d ms, p = %.2f)\n", __func__, "\033[1m", txt.c_str(), "\033[0m", (int) t_ms, p);

            audio.clear();
        }
    }

    audio.pause();
    whisper_print_timings(ctx);
    whisper_free(ctx);

    return 0;
}
