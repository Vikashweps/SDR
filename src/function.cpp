#include "SDR_include.h"
using namespace std;

// Генерация тестового сигнала для передачи
void generate_tx_signal(int16_t *tx_buff, size_t tx_mtu, long long tx_time) {
    // Синхропоследовательности
    for(size_t i = 0; i < 2; i++) {
        tx_buff[0 + i] = 0xffff;
        tx_buff[10 + i] = 0xffff;
    }
 //заполнение tx_buff значениями сэмплов первые 16 бит - I, вторые 16 бит - Q.
    /*for (size_t i = 2; i < 2 * tx_mtu; i += 2) {
        // ЗДЕСЬ БУДУТ ВАШИ СЭМПЛЫ
        tx_buff[i] = 1500 << 4;   // I
        tx_buff[i+1] = 1500 << 4; // Q
    }*/
    // Генерация I/Q samples
    float x = -96;
    for (size_t i = 2; i < 2 * tx_mtu; i += 2) {
        tx_buff[i] = (x*x);   // I компонента
        tx_buff[i+1] = (x*x); // Q компонента
        x += 0.1;
    }

   

    // Встраивание временной метки
    for(size_t i = 0; i < 8; i++) {
        uint8_t tx_time_byte = (tx_time >> (i * 8)) & 0xff;
        tx_buff[2 + i] = tx_time_byte << 4;
    }
}

// Сохранение samples в файл
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

// BPSK Demapper: complex symbols → bits (0/1)
vector<int> bpsk_demapper(const vector<complex<float>>& symbols) {
    vector<int> bits;
    bits.reserve(symbols.size());
    
    for (size_t i = 0; i < symbols.size(); i++) {
        // Если реальная часть > 0 → бит 1, иначе → бит 0
        if (symbols[i].real() > 0.0f) {
            bits.push_back(1);
        } else {
            bits.push_back(0);
        }
    }
    
    return bits;
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

vector<complex<float>> dounsampling(const vector<complex<float>>& matched, const vector<float>& errof) {
    vector<complex<float>> dounsampled;
    dounsampled.reserve(errof.size());  
    
    for (size_t i = 0; i < errof.size(); ++i) {  
        int idx = (errof[i]);    
        if (idx >= 0 && idx <(matched.size())) {
            dounsampled.push_back(matched[idx]);  
        }
    }
    
    return dounsampled;
}

vector<complex<float>> convolve(vector<complex<float>> upsampled, int samples_per_symbol) {
    vector <complex<float>> convolved;
    vector <float> b(samples_per_symbol, 1.0f);
    for (int n = 0; n < upsampled.size(); ++n) 
        {
            complex<float> sum(0.0f, 0.0f);
            for (int k = 0; k < b.size(); ++k) 
            {
                int idx = n - k;
                if (idx >= 0 && idx < upsampled.size()) { 
                sum += upsampled[idx] * b[k];
            }
        }
        convolved.push_back(sum);
        }
    return convolved;
}

vector<float> offset(vector<complex<float>> matched, int samples_per_symbol) 
{
    int K1, K2, p1, p2 = 0;
    float BnTs = 0.0001;
    float Kp = 0.0002;
    float zeta = sqrt(2) / 2;
    float theta = (BnTs / samples_per_symbol) / (zeta + (0.25 / zeta));
    K1 = -4 * zeta * theta / ( (1 + 2 * zeta * theta + pow(theta,2)) * Kp);
    K2 = -4 * pow(theta,2) / ( (1 + 2 * zeta * theta + pow(theta,2))* Kp);
    int tau = 0;
    float err;
    vector<float> errof;


    for (int i = 0; i < matched.size(); i += samples_per_symbol)
    {
        err = (matched[i + samples_per_symbol + tau].real() - matched[i + tau]).real() * matched[i + (samples_per_symbol / 2) + tau].real() + (matched[i + samples_per_symbol + tau].imag() - matched[i + tau]).imag() * matched[i + (samples_per_symbol / 2) + tau].imag(); 
        p1 = err * K1;
        p2 =  p2 + p1 + err * K2;

        if (p2 > 1)
        {
            p2 = p2 - 1;
        }

        if (p2 < -1)
        {
            p2 = p2 + 1;
        }
        tau = ceil(p2 * samples_per_symbol);
        errof.push_back(i + samples_per_symbol + tau);
        }
        return errof;
    }

void sdr_to_complex(const int16_t* sdr_buf, vector<complex<float>>& out, size_t num_samples) {
    out.resize(num_samples);
    for (size_t i = 0; i < num_samples; ++i) {
        out[i] = {
            (float)sdr_buf[2*i] / 32768.0f,
            (float)sdr_buf[2*i + 1] / 32768.0f
        };
    }
}

void complex_to_sdr(const vector<complex<float>>& in, int16_t* sdr_buf, size_t num_samples) {
    for (size_t i = 0; i < num_samples; ++i) {
        sdr_buf[2*i]     = (int16_t)(in[i].real() * 20000.0f);
        sdr_buf[2*i + 1] = (int16_t)(in[i].imag() * 20000.0f);
    }
}

void run_gui(const vector<complex<float>>& sample, const vector<complex<float>>& upsample, const vector<complex<float>>& convolved, const vector<complex<float>>& matched, const vector<float>& errof, int samples_per_symbol, const vector<complex<float>> dounsample)
{

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
    SDL_DisplayMode current;
    SDL_GetCurrentDisplayMode(0, &current);
    
    SDL_Window* window = SDL_CreateWindow("SDR Signal Analyzer", 0, 0, current.w, current.h, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESKTOP);
    
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGui::StyleColorsDark();
    
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = NULL;
    
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 330");

    vector<float> t_sample, i_sample, q_sample;
    for (size_t j = 0; j < sample.size(); j++) {
        t_sample.push_back((float)j);
        i_sample.push_back(sample[j].real());
        q_sample.push_back(sample[j].imag());
    }

    vector<float> t_up, amp_up;
    for (size_t j = 0; j < upsample.size(); j++) {
        t_up.push_back((float)j);
        amp_up.push_back(upsample[j].real());
    }

    vector<float> t_conv, i_conv, q_conv;
    for (size_t j = 0; j < convolved.size(); j++) {
        t_conv.push_back((float)j);
        i_conv.push_back(convolved[j].real());
        q_conv.push_back(convolved[j].imag());
    }

    vector<float> t_match, re_match, im_match;
    for (size_t j = 0; j < matched.size(); j++) {
        t_match.push_back((float)j);
        re_match.push_back(matched[j].real());
        im_match.push_back(matched[j].imag());
    }

    vector<float> i_rx, q_rx;
    for (size_t j = 0; j < dounsample.size(); j++) {
        i_rx.push_back(dounsample[j].real());
        q_rx.push_back(dounsample[j].imag());
    }

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) running = false;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // ВЕРХНЯЯ ПАНЕЛЬ 
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(current.w, current.h * 0.5f));
        ImGui::Begin("##top_tabs", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | 
                     ImGuiWindowFlags_NoSavedSettings);
        
        if (ImGui::BeginTabBar("##top_tabbar")) {
            
            //  1
            if (ImGui::BeginTabItem("TX BPSK")) {
                if (ImPlot::BeginPlot("##const_tx", ImVec2(-1, -1))) {
                    ImPlot::SetupAxes("I", "Q");
                    ImPlot::SetupAxesLimits(-1.25, 1.25, -1.36, 1.36);
                    ImPlot::PlotScatter("BPSK", i_sample.data(), q_sample.data(), (int)i_sample.size());
                    ImPlot::EndPlot();
                }
                ImGui::EndTabItem();
            }
            
            // 2
            if (ImGui::BeginTabItem("Symbol")) {
                if (ImPlot::BeginPlot("##time_tx", ImVec2(-1, -1))) {
                    ImPlot::SetupAxes("Sample", "Amplitude");
                    ImPlot::SetupAxesLimits(-3, (float)sample.size() * 1.01, -1.5, 1.5);
                    ImPlot::PlotLine("I", t_sample.data(), i_sample.data(), (int)i_sample.size());
                    ImPlot::PlotScatter("I", t_sample.data(), i_sample.data(), (int)i_sample.size());
                    ImPlot::EndPlot();
                }
                ImGui::EndTabItem();
            }
            
            //  3:
            if (ImGui::BeginTabItem(" Upsampled")) {
                if (ImPlot::BeginPlot("##upsample", ImVec2(-1, -1))) {
                    ImPlot::SetupAxes("Sample", "Amplitude");
                    ImPlot::SetupAxesLimits(-10, (float)upsample.size() * 1.01, -1.5, 1.5);
                    ImPlot::PlotLine("Upsampled", t_up.data(), amp_up.data(), (int)amp_up.size());
                    ImPlot::PlotScatter("Upsampled", t_up.data(), amp_up.data(), (int)amp_up.size());
                    ImPlot::EndPlot();
                }
                ImGui::EndTabItem();
            }

            // 4: Pulse Shaping
            if (ImGui::BeginTabItem(" Pulse Shaping")) {
                if (ImPlot::BeginPlot("##convolved", ImVec2(-1, -1))) {
                    ImPlot::SetupAxes("Sample", "Amplitude");
                    
                    ImPlot::SetupAxesLimits(-20, (float)convolved.size() * 1.01, -1.5, 1.5);
                    ImPlot::PlotLine("I", t_conv.data(), i_conv.data(), (int)i_conv.size());
                    ImPlot::PlotLine("Q", t_conv.data(), q_conv.data(), (int)q_conv.size());
                    ImPlot::EndPlot();
                }
                ImGui::EndTabItem();
            }
            
            ImGui::EndTabBar();
        }
        ImGui::End();

        // НИЖНЯЯ
        ImGui::SetNextWindowPos(ImVec2(0, current.h * 0.5f));
        ImGui::SetNextWindowSize(ImVec2(current.w, current.h * 0.5f));
        ImGui::Begin("##bottom_tabs", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | 
                     ImGuiWindowFlags_NoSavedSettings);
        
        if (ImGui::BeginTabBar("##bottom_tabbar")) {
            // 5: Symbol Timing 
            if (ImGui::BeginTabItem("Symbol Timing and Mathed")) {
                if (ImPlot::BeginPlot("##sync", ImVec2(-1, -1))) {
                    ImPlot::SetupAxes("Sample", "Amplitude");
                    
                    ImPlot::SetupAxesLimits(-20, (float)matched.size() * 1.01, -11.5, 11.5);
                    
                    ImPlot::PlotLine("I", t_match.data(), re_match.data(), (int)re_match.size());
                    ImPlot::PlotLine("Q", t_match.data(), im_match.data(), (int)im_match.size());

                    for (size_t j = 0; j < errof.size(); j++) {
                        float x = errof[j];
                        if (x >= 0 && x < (float)matched.size()) {
                            float x_data[2] = {x, x};
                            float y_data[2] = {-10.0f, 10.0f};  // ← высота линий синхронизации
                            ImPlot::PlotScatter("TED", x_data, y_data, 2);
                        }
                    }
                    ImPlot::EndPlot();
                }
                ImGui::EndTabItem();
            }
            
            // Вкладка 6: RX Constellation
            if (ImGui::BeginTabItem(" RX PSK")) {
                if (ImPlot::BeginPlot("##const_rx", ImVec2(-1, -1))) {
                    ImPlot::SetupAxes("I", "Q");
                    ImPlot::SetupAxesLimits(-13.0, 13.0, -3.0, 3.0);
                    ImPlot::PlotScatter("BPSK RX", i_rx.data(), q_rx.data(), (int)i_rx.size());
                    ImPlot::EndPlot();
                }
                ImGui::EndTabItem();
            }
            
            ImGui::EndTabBar();
        }
        ImGui::End();

        ImGui::Render();
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}