#include "SDR_include.h"
using namespace std;

// Генерация сигнала для передачи
void generate_tx_signal(int16_t *tx_buff, size_t tx_mtu, long long tx_time) {
    // Синхропоследовательности
    for(size_t i = 0; i < 2; i++) {
        tx_buff[0 + i] = 0xffff;
        tx_buff[10 + i] = 0xffff;
    }
 //заполнение tx_buff значениями сэмплов первые 16 бит - I, вторые 16 бит - Q.
    for (size_t i = 2; i < 2 * tx_mtu; i += 2) {
        tx_buff[i] = 1500 << 4;   // I
        tx_buff[i+1] = 1500 << 4; // Q
    }
    // Генерация I/Q samples
    /*float x = -96;
    for (size_t i = 2; i < 2 * tx_mtu; i += 2) {
        tx_buff[i] = (x*x);   // I компонента
        tx_buff[i+1] = (x*x); // Q компонента
        x += 0.1;
    }*/

    // Встраивание временной метки
    for(size_t i = 0; i < 8; i++) {
        uint8_t tx_time_byte = (tx_time >> (i * 8)) & 0xff;
        tx_buff[2 + i] = tx_time_byte << 4;
    }
}

void save_to_file(const char *filename, const int16_t *samples, size_t num_samples) {
    FILE *file = fopen(filename, "wb");
    if (file != NULL) {
        fwrite(samples, sizeof(int16_t), num_samples, file);
        fclose(file);
        printf("Saved %zu samples to %s\n", num_samples / 2, filename);
    }
}

vector<complex<float>> bpsk_mapper(const vector<int>& array){
    vector <complex <float>> samples;
    for (int i = 0; i < array.size(); i++) {
        complex <float> val;
        if (array[i] == 1) {
            val = 1.0 + 0.0f;
        } else {
            val = -1.0 + 0.0f;
        }
        samples.push_back(val);
    }
    return samples;
}


vector<complex<float>> upsampling(vector<complex<float>>& samples, int samples_per_symbol) {
    vector<complex<float>> upsampled;
    upsampled.resize(samples.size() * samples_per_symbol);
    int j = 0;
    for (int i = 0; i < upsampled.size(); i += samples_per_symbol) 
    {
        upsampled[i] = samples[j];
        j++;

    }
    return upsampled;
}


vector<complex<float>> clock_recovery_mueller_muller(const vector<complex<float>>& samples, int sps) {
    float mu = 0.01f;
    std::vector<std::complex<float>> out(samples.size() + 10);
    std::vector<std::complex<float>> out_rail(samples.size() + 10);
    size_t i_in = 0;
    size_t i_out = 2;
    while (i_out < samples.size() && i_in + 16 < samples.size()) {
        out[i_out] = samples[i_in];

        out_rail[i_out] = std::complex<float>(
            std::real(out[i_out]) > 0 ? 1.0f : 0.0f,
            std::imag(out[i_out]) > 0 ? 1.0f : 0.0f
        );

        std::complex<float> x = (out_rail[i_out] - out_rail[i_out - 2]) * std::conj(out[i_out - 1]);
        std::complex<float> y = (out[i_out] - out[i_out - 2]) * std::conj(out_rail[i_out - 1]);
        float mm_val = std::real(y - x);

        mu += sps + 0.01f * mm_val;
        i_in += static_cast<size_t>(std::floor(mu));
        mu = mu - std::floor(mu);
        i_out += 1;
    }
    return std::vector<std::complex<float>>(out.begin() + 2, out.begin() + i_out);
}


void sdr_to_complex(const int16_t* sdr_buf, vector<complex<float>>& out, size_t num_samples) {
    out.resize(num_samples);
    for (size_t i = 0; i < num_samples; ++i) {
        out[i] = {
            (float)sdr_buf[2*i] / 20.0f,
            (float)sdr_buf[2*i + 1] / 20.0f
        };
    }
}

void complex_to_sdr(const vector<complex<float>>& in, int16_t* sdr_buf, size_t num_samples) {
    for (size_t i = 0; i < num_samples; ++i) {
        sdr_buf[2*i] = (int16_t)(in[i].real() * 2000.0f) << 4;
        sdr_buf[2*i+1] = (int16_t)(in[i].imag() * 2000.0f) << 4;
    }
}



vector<float> srrc(int syms, float beta, int L) {
    if (beta < 0.0f) beta = 0.0f;
    if (beta > 1.0f) beta = 1.0f;
    
    int span = 2 * syms * L + 1;  
    vector<float> h(span);
    
    for (int i = 0; i < span; i++) {
        float t = (float)(i - syms * L) / (float)L;  
        float result = 0.0f;

        if (beta < EPS) {
            if (fabsf(t) < EPS) {
                result = 1.0f;
            } else {
                float pi_t = PI * t;
                result = sinf(pi_t) / pi_t;
            }
        } else {
            float four_beta_t = 4.0f * beta * t;
            float pi_t = PI * t;
            
            if (fabsf(t) < EPS) {
                result = 1.0f + beta * (4.0f / PI - 1.0f);
            }
            else if (fabsf(fabsf(four_beta_t) - 1.0f) < EPS) {
                float arg = PI * (1.0f + beta) / (4.0f * beta);
                result = (beta / sqrtf(2.0f)) * 
                         ((1.0f + 2.0f/PI) * sinf(arg) + 
                          (1.0f - 2.0f/PI) * cosf(arg));
            }
            else {
                float num = sinf(pi_t * (1.0f - beta)) + 
                            four_beta_t * cosf(pi_t * (1.0f + beta));
                float denom = pi_t * (1.0f - four_beta_t * four_beta_t);
                result = num / denom;
            }
        }
        
        h[i] = isfinite(result) ? result : 0.0f;
    }
    
    float energy = 0.0f;
    for (float v : h) energy += v * v;
    if (energy > 1e-10f) {
        float scale = 1.0f / sqrtf(energy);
        for (float& v : h) v *= scale;
    }
    
    return h;
}

vector<complex<float>> matched_filter(const vector<complex<float>>& input, int L, float beta) {
    const int syms = 5; 
    vector<float> filter = srrc(syms, beta, L); 
    
    int filter_len = filter.size();
    int half_len = filter_len / 2;
    
    vector<complex<float>> output(input.size());
    
    for (size_t i = 0; i < input.size(); i++) {
        complex<float> sum(0.0f, 0.0f);
        for (int m = 0; m < filter_len; m++) {
            int idx = (int)i - half_len + m;
            if (idx >= 0 && idx < (int)input.size()) {
                sum += input[idx] * filter[m];
            }
        }
        output[i] = sum;
    }
    return output;
}

vector<complex<float>> costas_loop_bpsk(const vector<complex<float>>& samples) {
    int N = static_cast<int>(samples.size());
    vector<complex<float>> out(N);
    
    float phase = 0.0f;
    float freq = 0.0f;
    const float alpha = 8.0f;
    const float beta = 0.02f;
    const float TWO_PI = 2.0f * PI;
    
    for (int i = 0; i < N; i++) {
        complex<float> rotator(0.0f, -phase);
        out[i] = samples[i] * exp(rotator);
        
        float error = out[i].real() * out[i].imag();
        freq += beta * error;
        phase += freq + alpha * error;
        
        while (phase >= TWO_PI) phase -= TWO_PI;
        while (phase < 0.0f) phase += TWO_PI;
    }
    return out;
}

vector<complex<float>>correlate_barker(const vector<complex<float>>& signal,  vector<complex<float>>& barker_code) {
    int N = signal.size();
    int L = barker_code.size();
    vector<complex<float>> corr;
    corr.reserve(N - L + 1);
    for (int i = 0; i <= N - L; ++i) {
        complex<float> sum = 0;
        for (int j = 0; j < L; ++j) {
            sum += signal[i + j] * conj(barker_code[j]); 
        }
        
        corr.push_back(sum);
    }
    return corr;
}

// OFDM МОДУЛЯТОР (GI = 1/8 от символа)
vector<complex<float>> OFDM_Modulate(
    const vector<complex<float>>& symbols,   // Входные символы
    int Nc = 64)                              // Количество поднесущих
{
    // Защитный интервал = 1/8 от полезной длины символа
    int guard_length = Nc / 8;                
    int useful_len = Nc;
    int total_len = useful_len + guard_length;
    
    vector<complex<float>> out;
    int num_blocks = symbols.size() / Nc;
    
    for (int b = 0; b < num_blocks; b++) {
        // 1. Заполнение частотной области
        vector<complex<float>> freq(Nc);
        for (int k = 0; k < Nc; k++) 
            freq[k] = symbols[b * Nc + k];
        
        // 2. IFFT: частота → время
        vector<complex<float>> time(Nc);
        for (int n = 0; n < Nc; n++) {
            time[n] = {0, 0};
            for (int k = 0; k < Nc; k++) {
                float angle = 2.0f * PI * k * n / Nc;
                time[n] += freq[k] * complex<float>(cosf(angle), sinf(angle));
            }
            time[n] /= Nc;  // Нормировка мощности
        }
        
        // 3. Добавление циклического префикса (копия конца в начало)
        for (int i = 0; i < guard_length; i++) {
            out.push_back(time[Nc - guard_length + i]);
        }
        // Полезная часть
        for (int i = 0; i < Nc; i++) {
            out.push_back(time[i]);
        }
    }
    return out;
}

// OFDM ДЕМОДУЛЯТОР 

vector<complex<float>> OFDM_Demodulate(
    const vector<complex<float>>& rx_signal,  // Принятый сигнал
    int Nc = 64)                               // Количество поднесущих
{
    // Защитный интервал = 1/8 от полезной длины символа
    int guard_length = Nc / 8;
    int useful_len = Nc;
    int total_len = useful_len + guard_length;
    
    vector<complex<float>> out;
    int num_blocks = rx_signal.size() / total_len;
    
    for (int b = 0; b < num_blocks; b++) {
        // 1. Пропускаем защитный интервал
        int start = b * total_len + guard_length;
        vector<complex<float>> time(Nc);
        for (int i = 0; i < Nc; i++) 
            time[i] = rx_signal[start + i];
        
        // 2. FFT: время → частота
        vector<complex<float>> freq(Nc);
        for (int k = 0; k < Nc; k++) {
            freq[k] = {0, 0};
            for (int n = 0; n < Nc; n++) {
                float angle = -2.0f * PI * k * n / Nc;
                freq[k] += time[n] * complex<float>(cosf(angle), sinf(angle));
            }
            // Нормировка не нужна: деление на N в TX компенсируется FFT
        }
        
        // 3. Сохраняем восстановленные символы
        for (int k = 0; k < Nc; k++) 
            out.push_back(freq[k]);
    }
    return out;
}


void run_gui(
    const vector<complex<float>>& rx_raw,
    const vector<complex<float>>& matched,
    const vector<complex<float>>& downsampled,
    const vector<complex<float>>& correlation,
    const vector<complex<float>>& payload,
    const vector<complex<float>>& costas_out)
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
    SDL_DisplayMode current;
    SDL_GetCurrentDisplayMode(0, &current);
    
    SDL_Window* window = SDL_CreateWindow("SDR: BPSK Receiver", 0, 0, 
                                          current.w, current.h,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!window) { printf("Error creating SDL window\n"); return; }
    
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    ImGui::CreateContext(); 
    ImPlot::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 330");

    auto prep_complex = [](const vector<complex<float>>& src, 
                          vector<float>& t, vector<float>& i, vector<float>& q, 
                          int limit = -1) {
        int n = (limit > 0 && src.size() > (size_t)limit) ? limit : (int)src.size();
        t.resize(n); i.resize(n); q.resize(n);
        for(int j = 0; j < n; ++j) { 
            t[j] = (float)j; i[j] = src[j].real(); q[j] = src[j].imag(); 
        }
    };
    
    auto prep_real = [](const vector<complex<float>>& src, 
                       vector<float>& t, vector<float>& v, int limit = -1) {
        int n = (limit > 0 && src.size() > (size_t)limit) ? limit : (int)src.size();
        t.resize(n); v.resize(n);
        for(int j = 0; j < n; ++j) { t[j] = (float)j; v[j] = src[j].real(); }
    };

    vector<float> t_raw, i_raw, q_raw; prep_complex(rx_raw, t_raw, i_raw, q_raw, 3000);
    vector<float> t_match, i_match, q_match; prep_complex(matched, t_match, i_match, q_match, 3000);
    vector<float> t_down, i_down, q_down; prep_complex(downsampled, t_down, i_down, q_down, 3000);
    vector<float> t_corr, v_corr; prep_real(correlation, t_corr, v_corr, 3000);
    vector<float> t_pay, i_pay, q_pay; prep_complex(payload, t_pay, i_pay, q_pay, 3000);
    vector<float> t_costas, i_costas, q_costas; prep_complex(costas_out, t_costas, i_costas, q_costas, 3000);

    float corr_max_val = -1e9f; int corr_max_idx = -1;
    for (size_t i = 0; i < correlation.size(); ++i) {
        float val = correlation[i].real();
        if (val > corr_max_val) { corr_max_val = val; corr_max_idx = (int)i; }
    }

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE))
                running = false;
        }

        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplSDL2_NewFrame(); ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(current.w, current.h));
        ImGui::Begin("##Main", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        
        if (ImGui::BeginTabBar("MainTabs")) {

            if (ImGui::BeginTabItem("1. Raw Signal")) {
                ImGui::SetNextWindowPos(ImVec2(0, 30));
                ImGui::SetNextWindowSize(ImVec2(current.w, current.h * 0.45f));
                ImGui::Begin("##RawWave", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
                if (ImPlot::BeginPlot("##RW", ImVec2(-1, -1))) {
                    ImPlot::SetupAxes("Sample", "Amplitude");
                    ImPlot::PlotLine("I", t_raw.data(), i_raw.data(), (int)i_raw.size());
                    ImPlot::PlotLine("Q", t_raw.data(), q_raw.data(), (int)q_raw.size());
                    ImPlot::EndPlot();
                } ImGui::End();
                
                ImGui::SetNextWindowPos(ImVec2(0, current.h * 0.5f + 10));
                ImGui::SetNextWindowSize(ImVec2(current.w, current.h * 0.45f));
                ImGui::Begin("##RawConst", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
                if (ImPlot::BeginPlot("##RC", ImVec2(-1, -1))) {
                    ImPlot::SetupAxes("I", "Q"); ImPlot::SetupAxesLimits(-3, 3, -3, 3);
                    ImPlot::PlotScatter("Points", i_raw.data(), q_raw.data(), (int)i_raw.size());
                    ImPlot::EndPlot();
                } ImGui::End();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("2. Matched Filter")) {
                ImGui::SetNextWindowPos(ImVec2(0, 30));
                ImGui::SetNextWindowSize(ImVec2(current.w, current.h * 0.45f));
                ImGui::Begin("##MatchedWave", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
                if (ImPlot::BeginPlot("##MW", ImVec2(-1, -1))) {
                    ImPlot::SetupAxes("Sample", "Amplitude");
                    ImPlot::PlotLine("I", t_match.data(), i_match.data(), (int)i_match.size());
                    ImPlot::PlotLine("Q", t_match.data(), q_match.data(), (int)q_match.size());
                    ImPlot::EndPlot();
                } ImGui::End();
                
                ImGui::SetNextWindowPos(ImVec2(0, current.h * 0.5f + 10));
                ImGui::SetNextWindowSize(ImVec2(current.w, current.h * 0.45f));
                ImGui::Begin("##MatchedConst", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
                if (ImPlot::BeginPlot("##MC", ImVec2(-1, -1))) {
                    ImPlot::SetupAxes("I", "Q"); ImPlot::SetupAxesLimits(-3, 3, -3, 3);
                    ImPlot::PlotScatter("", i_match.data(), q_match.data(), (int)i_match.size());
                    ImPlot::EndPlot();
                } ImGui::End();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("3. Timing Sync")) {
                ImGui::SetNextWindowPos(ImVec2(0, 30));
                ImGui::SetNextWindowSize(ImVec2(current.w, current.h * 0.45f));
                ImGui::Begin("##DownWave", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
                if (ImPlot::BeginPlot("##DW", ImVec2(-1, -1))) {
                    ImPlot::SetupAxes("Symbol", "Amplitude");
                    ImPlot::PlotLine("I", t_down.data(), i_down.data(), (int)i_down.size());
                    ImPlot::PlotLine("Q", t_down.data(), q_down.data(), (int)q_down.size());
                    ImPlot::EndPlot();
                } ImGui::End();
                
                ImGui::SetNextWindowPos(ImVec2(0, current.h * 0.5f + 10));
                ImGui::SetNextWindowSize(ImVec2(current.w, current.h * 0.45f));
                ImGui::Begin("##DownConst", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
                if (ImPlot::BeginPlot("##DC", ImVec2(-1, -1))) {
                    ImPlot::SetupAxes("I", "Q"); ImPlot::SetupAxesLimits(-2, 2, -2, 2);
                    ImPlot::PlotScatter("", i_down.data(), q_down.data(), (int)i_down.size());
                    ImPlot::EndPlot();
                } ImGui::End();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("4. Costas Loop")) {
                ImGui::SetNextWindowPos(ImVec2(0, 30));
                ImGui::SetNextWindowSize(ImVec2(current.w, current.h * 0.45f));
                ImGui::Begin("##CostasWave", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
                if (ImPlot::BeginPlot("##CW", ImVec2(-1, -1))) {
                    ImPlot::SetupAxes("Symbol", "Amplitude");
                    ImPlot::PlotLine("I", t_costas.data(), i_costas.data(), (int)i_costas.size());
                    ImPlot::PlotLine("Q", t_costas.data(), q_costas.data(), (int)q_costas.size());
                    ImPlot::EndPlot();
                } ImGui::End();
                
                ImGui::SetNextWindowPos(ImVec2(0, current.h * 0.5f + 10));
                ImGui::SetNextWindowSize(ImVec2(current.w, current.h * 0.45f));
                ImGui::Begin("##CostasConst", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
                if (ImPlot::BeginPlot("##CC", ImVec2(-1, -1))) {
                    ImPlot::SetupAxes("I", "Q"); ImPlot::SetupAxesLimits(-2, 2, -2, 2);
                    ImPlot::PlotScatter("", i_costas.data(), q_costas.data(), (int)i_costas.size());
                    ImPlot::EndPlot();
                } ImGui::End();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("5. Correlation")) {
                // Top-left: waveform
                ImGui::SetNextWindowPos(ImVec2(0, 30));
                ImGui::SetNextWindowSize(ImVec2(current.w * 0.6f, current.h * 0.35f));
                ImGui::Begin("##CorrWave", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
                if (ImPlot::BeginPlot("##CorrW", ImVec2(-1, -1))) {
                    ImPlot::SetupAxes("Sample", "Amplitude");
                    ImPlot::PlotLine("I (matched)", t_match.data(), i_match.data(), (int)i_match.size());
                    ImPlot::EndPlot();
                }
                ImGui::End();
                
                // Top-right: constellation (costas_out)
                ImGui::SetNextWindowPos(ImVec2(current.w * 0.62f, 30));
                ImGui::SetNextWindowSize(ImVec2(current.w * 0.36f, current.h * 0.35f));
                ImGui::Begin("##CorrConst", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
                if (ImPlot::BeginPlot("##CorrC", ImVec2(-1, -1))) {
                    ImPlot::SetupAxes("I", "Q");
                    ImPlot::SetupAxesLimits(-2.5, 2.5, -2.5, 2.5);
                    ImPlot::PlotScatter("Constellation", i_costas.data(), q_costas.data(), (int)i_costas.size());
                    ImPlot::EndPlot();
                }
                ImGui::End();
                
                // Bottom: correlation plot
                ImGui::SetNextWindowPos(ImVec2(0, current.h * 0.4f + 10));
                ImGui::SetNextWindowSize(ImVec2(current.w, current.h * 0.55f));
                ImGui::Begin("##CorrPlot", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
                if (ImPlot::BeginPlot("##CorrVal", ImVec2(-1, -1))) {
                    ImPlot::SetupAxes("Position (symbols)", "Correlation");
                    ImPlot::PlotLine("Correlation", t_corr.data(), v_corr.data(), (int)v_corr.size());
                    
                    // Threshold line (50% of max)
                    if (!v_corr.empty()) {
                        float mx = *max_element(v_corr.begin(), v_corr.end());
                        float thr = mx * 0.5f;
                        float xl[2] = {0, (float)v_corr.size()};
                        float yl[2] = {thr, thr};
                        ImPlot::PlotLine("Threshold", xl, yl, 2);
                    }
                    
                    // Peak marker
                    if (corr_max_idx >= 0 && corr_max_idx < (int)correlation.size()) {
                        float px[1] = {(float)corr_max_idx};
                        float py[1] = {corr_max_val};
                        ImPlot::PlotScatter("Peak", px, py, 1);
                    }
                    ImPlot::EndPlot();
                }
                ImGui::End();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("6. Payload")) {
                ImGui::SetNextWindowPos(ImVec2(0, 30));
                ImGui::SetNextWindowSize(ImVec2(current.w, current.h * 0.45f));
                ImGui::Begin("##PayloadWave", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
                if (ImPlot::BeginPlot("##PW", ImVec2(-1, -1))) {
                    ImPlot::SetupAxes("Bit", "Amplitude");
                    ImPlot::PlotStems("I", t_pay.data(), i_pay.data(), (int)i_pay.size(), 0.0f);
                    ImPlot::EndPlot();
                } ImGui::End();
                
                ImGui::SetNextWindowPos(ImVec2(0, current.h * 0.5f + 10));
                ImGui::SetNextWindowSize(ImVec2(current.w, current.h * 0.45f));
                ImGui::Begin("##PayloadConst", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
                if (ImPlot::BeginPlot("##PC", ImVec2(-1, -1))) {
                    ImPlot::SetupAxes("I", "Q"); ImPlot::SetupAxesLimits(-2, 2, -2, 2);
                    ImPlot::PlotScatter("", i_pay.data(), q_pay.data(), (int)i_pay.size());
                    ImPlot::EndPlot();
                } ImGui::End();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        
        ImGui::SetNextWindowPos(ImVec2(10, current.h - 100));
        ImGui::SetNextWindowSize(ImVec2(400, 90));
        ImGui::Begin("##Info", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("RAW: %zu samples | Matched: %zu | Downsampled: %zu", 
                   rx_raw.size(), matched.size(), downsampled.size());
        ImGui::Text("Correlation: %zu values | Payload: %zu symbols", 
                   correlation.size(), payload.size());
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) running = false;
        ImGui::Text("ESC - Exit");
        ImGui::End();
        
        ImGui::End();
        ImGui::Render();
        glClearColor(0.1f, 0.1f, 0.15f, 1.f); glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplSDL2_Shutdown();
    ImPlot::DestroyContext(); ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context); SDL_DestroyWindow(window); SDL_Quit();
}