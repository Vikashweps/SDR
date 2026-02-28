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
    for (size_t i = 2; i < 2 * tx_mtu; i += 2) {
        // ЗДЕСЬ БУДУТ ВАШИ СЭМПЛЫ
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



    void run_gui(const vector<complex<float>>& sample, const vector<complex<float>>& upsample, const vector<complex<float>>& convolved, const vector<complex<float>>& matched, const vector<float>& errof, int samples_per_symbol, const vector<complex<float>> dounsample)
    {
        
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
    SDL_Window* window = SDL_CreateWindow( "Testing functions", 0, 0, 1200, 800, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);

    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Включить Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Включить Docking

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 330");

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) running = false;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("1. Symbols", nullptr, ImGuiWindowFlags_NoCollapse);
        if (ImPlot::BeginPlot("I/Q")) {
            static vector<float> i;
            static vector<float> Q; 
            i.clear(); Q.clear();
            for (auto& s : sample) { i.push_back(s.real()); Q.push_back(s.imag()); }
            ImPlot::SetupAxes("I", "Q");
            ImPlot::SetupAxesLimits(-1.5, 1.5, -1.5, 1.5);
            ImPlot::PlotScatter("BPSK", i.data(), Q.data(), static_cast<int>(i.size()));
            ImPlot::EndPlot();
        }
        ImGui::End();

        ImGui::Begin("2. BPSK", nullptr, ImGuiWindowFlags_NoCollapse);
        if (ImPlot::BeginPlot("I vs Sample")) {
            static vector<float> vals; vals.clear();
            for (auto& s : sample) vals.push_back(s.real());
            ImPlot::SetupAxes("Sample", "Amplitude");
            ImPlot::SetupAxesLimits(-1, 10, -1.5, 1.5);
            ImPlot::PlotScatter("I", vals.data(), static_cast<int>(vals.size()));
            ImPlot::EndPlot();
        }
        ImGui::End();

        ImGui::Begin("3. Upsampling", nullptr, ImGuiWindowFlags_NoCollapse);
        if (ImPlot::BeginPlot("Upsampled")) {
            static vector<float> vals; vals.clear();
            for (auto& s : upsample) vals.push_back(s.real());
            ImPlot::SetupAxes("Sample", "Amplitude");
            ImPlot::SetupAxesLimits(-5, 105, -1.5, 1.5);
            ImPlot::PlotScatter("Upsampled", vals.data(), static_cast<int>(vals.size()));
            ImPlot::EndPlot();
        }
        ImGui::End();

        ImGui::Begin("4. Convolution", nullptr, ImGuiWindowFlags_NoCollapse);
        if (ImPlot::BeginPlot("Shaped Signal")) {
            static vector<float> i, Q; i.clear(); Q.clear();
            for (auto& c : convolved) { i.push_back(c.real()); Q.push_back(c.imag()); }
            ImPlot::SetupAxes("Sample", "Amplitude");
            ImPlot::SetupAxesLimits(-5, 105, -1.5, 1.5);
            ImPlot::PlotLine("I", i.data(), static_cast<int>(i.size()));
            ImPlot::PlotLine("Q", Q.data(), static_cast<int>(Q.size()));
            ImPlot::EndPlot();
        }
        ImGui::End();
    
        
        ImGui::Begin("Matched Filter + Error");
        vector<float> time_c, re_c, im_c;
        int N = matched.size();
        time_c.reserve(N); re_c.reserve(N); im_c.reserve(N);
        for (int i = 0; i < N; ++i) 
        {
            time_c.push_back(static_cast<float>(i));
            re_c.push_back(matched[i].real());
            im_c.push_back(matched[i].imag());
        }

        float max_amp = 0.0f;
        for (float r : re_c) max_amp = fmaxf(max_amp, fabsf(r));
        for (float im : im_c) max_amp = fmaxf(max_amp, fabsf(im));
        float y_limit = max_amp * 1.2f;  
        
        if (ImPlot::BeginPlot("##Plot", ImVec2(-1, 300))) 
        {
            ImPlot::SetupAxes("Sample index", "Amplitude");
            ImPlot::SetupAxesLimits(-5, 105, -15.5, 15.5);
            ImPlot::PlotLine("Real", time_c.data(), re_c.data(), N);
            ImPlot::PlotLine("Imag", time_c.data(), im_c.data(), N);
            
            for (size_t i = 0; i < errof.size(); ++i) 
            {
                int idx = (int)errof[i];
                float x_data[] = {(float)idx, (float)idx};
                float y_data[] = {-y_limit, y_limit};
                ImPlot::PlotLine("##Sync", x_data, y_data, 2);
            }
            ImPlot::EndPlot();
        }
        ImGui::End();

        ImGui::Begin("6. Downsampled");
        if (ImPlot::BeginPlot("Downsampled")) {
            static vector<float> vals; vals.clear();
            for (auto& s : dounsample) vals.push_back(s.real());
            ImPlot::SetupAxes("Sample", "Amplitude");
            ImPlot::SetupAxesLimits(-5, 5, -15.5, 15.5);
            ImPlot::PlotScatter("Downsampled", vals.data(), static_cast<int>(vals.size()));
            ImPlot::EndPlot();
        }
        ImGui::End();

        ImGui::Begin("7. Decoded Symbols", nullptr, ImGuiWindowFlags_NoCollapse);
        if (ImPlot::BeginPlot("I/Q")) {
            
            static vector<float> i, Q; 
            i.clear(); Q.clear();

            for (auto& s : dounsample) {
                i.push_back(s.real());
                Q.push_back(s.imag());
            }
            
            ImPlot::SetupAxes("I", "Q");
            ImPlot::SetupAxesLimits(-10.5, 10.5, -1.5, 1.5);
            ImPlot::PlotScatter("Decoded", i.data(), Q.data(), static_cast<int>(i.size()));
            
            ImPlot::EndPlot();
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

void sdr_to_complex(const int16_t* sdr_buf, vector<complex<float>>& out, size_t num_samples) {
    out.resize(num_samples);
    for (size_t i = 0; i < num_samples; ++i) {
        out[i] = {
            (float)sdr_buf[2*i] / 32768.0f,
            (float)sdr_buf[2*i + 1] / 32768.0f
        };
    }
}

// Передача: complex<float> от DSP -> SDR буфер (int16_t I/Q interleaved)
void complex_to_sdr(const vector<complex<float>>& in, int16_t* sdr_buf, size_t num_samples) {
    for (size_t i = 0; i < num_samples; ++i) {
        sdr_buf[2*i]     = (int16_t)(in[i].real() * 32767.0f);
        sdr_buf[2*i + 1] = (int16_t)(in[i].imag() * 32767.0f);
    }
}

