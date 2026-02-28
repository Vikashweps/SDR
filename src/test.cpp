#include "SDR_include.h"
#include "third_party/implot/implot.h"
#include "third_party/imgui/backends/imgui_impl_sdl2.h"
#include "third_party/imgui/backends/imgui_impl_opengl3.h"
#include "third_party/imgui/imgui.h"
using namespace std;

vector<complex<float>> bpsk_mapper(const vector<int>& array){
    vector <complex <float>> samples;
    for (size_t i = 0; i < array.size(); i++) {
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

vector <complex <float>> upsampling(vector<complex <float>> & samples, int samples_per_symbol) {
    vector<complex<float>> upsampled;
    upsampled.resize(samples.size() * samples_per_symbol);
    int j = 0;
    for (size_t i = 0; i < upsampled.size(); i += samples_per_symbol) 
    {
        upsampled[i] = samples[j];
        j++;

    }
    return upsampled;
}

vector <complex <float>> convolve(vector<complex <float>> upsampled, int samples_per_symbol) {
    vector <complex<float>> convolved;
    vector <float> b(samples_per_symbol, 1.0f);
    for (size_t n = 0; n < upsampled.size(); ++n) 
        {
            complex<float> sum(0.0f, 0.0f);
            for (size_t k = 0; k < b.size(); ++k) 
            {
                size_t idx = n - k;
                if (idx < upsampled.size()) { 
                sum += upsampled[idx] * b[k];
            }
        }
        convolved.push_back(sum);
        }
    return convolved;
}

vector<float> offset(vector<complex <float>> matched, int samples_per_symbol)
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


    for (size_t i = 0; i < matched.size(); i += samples_per_symbol)
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



    void run_gui(const vector<complex<float>>& sample, const vector<complex<float>>& upsample, const vector<complex<float>>& convolved, const vector<complex<float>>& matched, const vector<float>& errof, int samples_per_symbol,const vector<complex<float>> dounsample)
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
        size_t idx = (errof[i]);    
        if (idx <(matched.size())) {
            dounsampled.push_back(matched[idx]);  
        }
    }
    
    return dounsampled;
}




int main(){
    
    int size = 10;
    vector<int>array  = {1, 0, 1, 0, 1, 0, 1, 0, 1, 0};
    
    vector<complex<float>> sample = bpsk_mapper(array);
    cout << "samples" << endl;
    for(size_t i = 0; i < sample.size(); i++) 
    {
        cout << sample[i] << " " ;
    }
    cout << endl;

    cout << "upsampling" << endl;
    vector<complex<float>> upsample = upsampling(sample, size);
     for(size_t i = 0; i < upsample.size(); i++) 
    {
        cout << upsample[i] << " "  ;
    }
    cout << endl;

    cout << "convolve" << endl;
    vector<complex<float>> convolved = convolve(upsample, size);
     for(size_t i = 0; i < convolved.size(); i++) 
    {
        cout << convolved[i] << " "  ;
    }
    cout << endl;
    
    cout << "matched filter" << endl;
    vector<complex<float>> matched = convolve(convolved, size);
     for(size_t i = 0; i < matched.size(); i++) 
    {
        cout << matched[i] << " "  ;
    }
    cout << endl;

    cout << "errof" << endl;
    vector<float> errof = offset(matched, size);
     for(int i = 0; i < size; i++) 
    {
        cout << errof[i] << " "  ;
    }
    cout << endl;

    cout << "dounsampling" << endl;
    vector<complex<float>> dounsample = dounsampling(matched, errof );
     for(size_t i = 0; i < dounsample.size(); i++) 
    {
        cout << dounsample[i] << " "  ;
    }
    cout << endl;

    run_gui(sample, upsample, convolved, matched, errof, size, dounsample);
    return 0;
}