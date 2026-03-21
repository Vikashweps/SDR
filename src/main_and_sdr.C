#include "test_rx_samples_bpsk_barker13.h"
#include "SDR_include.h"
using namespace std;


int main() {
    printf("Запуск SDR\n");
    srand((unsigned int)time(NULL));

    // Инициализация SDR 
    sdr_device_t *sdr = sdr_init(1);
    if (sdr == NULL) {
        printf("ОШИБКА: Не удалось создать SDR устройство!\n");
        return 1;
    }
    if (sdr_configure(sdr) != 0) { 
        sdr_cleanup(sdr); 
        return 1; 
    }

    printf("Устройство успешно инициализировано\n");
    printf("TX MTU: %zu сэмплов\n", sdr->tx_mtu);
    printf("RX MTU: %zu сэмплов\n", sdr->rx_mtu);

    // Параметры сигнала
    int sample_rate = 4e6;
    vector<int> Barker = {1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1};
    const int samples_per_symbol = 16;  
    const int num_bits = sdr->tx_mtu / samples_per_symbol - 12 - Barker.size();  
    
    // 1. Генерация данных (биты) 
    vector<int> bits(num_bits);
    for (size_t i = 0; i < bits.size(); i++) {
        bits[i] = rand() % 2;  
    }
    printf("Сгенерировано %zu бит\n", bits.size());
   
    
    //  2. BPSK маппер (биты → символы) 
    vector<complex<float>> data_symbols = bpsk_mapper(bits);
    vector<complex<float>> Barker_symbols = bpsk_mapper(Barker);

    // 3. Объединяем: Баркер + Данные 
    vector<complex<float>> sample = Barker_symbols;
    sample.insert(sample.end(), data_symbols.begin(), data_symbols.end());
    
    printf("Всего преобразовано в символы: %zu (Баркер: %zu + Данные: %zu)\n", sample.size(), Barker.size(), data_symbols.size());

    vector<complex<float>> upsample = upsampling(sample, samples_per_symbol);
    vector<complex<float>> convolved = convolve(upsample, samples_per_symbol);
    int16_t *tx_buff = (int16_t*)malloc(2 * sdr->tx_mtu * sizeof(int16_t));

    complex_to_sdr(convolved, tx_buff + 12, convolved.size());

    for (size_t i = 0; i < 2; i++)
    {
        tx_buff[0 + i] = 0xffff;
        // 8 x timestamp words
        tx_buff[10 + i] = 0xffff;
    }
    printf("\nБуфер передачи готов: %zu сэмплов\n", convolved.size());

    // Буферы и НАКОПИТЕЛЬ RX данных
    int16_t *rx_buffer = (int16_t*)malloc(2 * sdr->rx_mtu * sizeof(int16_t));
    long long timeNs = 0;
    const long timeoutUs = 400000;
    
    // Файлы
    FILE *rx_file = fopen("rx_samples.pcm", "wb");
    FILE *tx_file = fopen("tx_samples.pcm", "wb");
    printf("Буферы и файлы готовы\n");

    //Основной цикл RX/TX 
    size_t iteration_count = 100;

    vector<int16_t> rx_raw_all;
    rx_raw_all.reserve(iteration_count * 2 * sdr->rx_mtu); 

    for (size_t i = 0; i < iteration_count; i++)  {
         
        // ПРИЁМ
        void *rx_buffs[] = {rx_buffer};
        int flags;
        int sr = SoapySDRDevice_readStream(sdr->device, sdr->rxStream, rx_buffs, sdr->rx_mtu, &flags, &timeNs, timeoutUs);
        
        if (sr > 0) {
            printf("Приём %zu: %d сэмплов\n", i, sr);
            
            if (rx_file != NULL) {
                fwrite(rx_buffer, sizeof(int16_t), 2 * sr, rx_file);
            }
             rx_raw_all.insert(rx_raw_all.end(), rx_buffer, rx_buffer + 2*sr);
    

             
                // // Конвертация и НАКОПЛЕНИЕ
                // vector<complex<float>> rx_complex;
                // sdr_to_complex(rx_buffer, rx_complex, sr);
                // rx_signal.insert(rx_signal.end(), rx_complex.begin(), rx_complex.end());
                
        } 

    // ПЕРЕДАЧА (на 3-й итерации)
        if (i > 2) {
            
            // if (tx_file != NULL) {
            //     fwrite(tx_buff, sizeof(int16_t), 2 * sdr->tx_mtu, tx_file);
            // }
            
            long long tx_time = timeNs + (4 * 1000 * 1000);
            for(size_t j = 0; j < 8; j++) {
                uint8_t tx_time_byte = (tx_time >> (j * 8)) & 0xff;
                tx_buff[2 + j] = tx_time_byte << 4;
            }
            
            void *tx_buffs[] = {tx_buff};
            int tx_flags = SOAPY_SDR_HAS_TIME;
            int st = SoapySDRDevice_writeStream(sdr->device, sdr->txStream, (const void * const*)tx_buffs, sdr->tx_mtu, &tx_flags, tx_time, timeoutUs);
            
            if (st != (int)sdr->tx_mtu) {
                printf("    ОШИБКА передачи: %d\n", st);
            } else {
                printf(" Успешно отправлено в эфир!\n");
            }
        }
    
    
    }
    // Закрытие файлов
    if (rx_file != NULL) fclose(rx_file);
    if (tx_file != NULL) fclose(tx_file);
    printf("\nФайлы сохранены: rx_samples.pcm, tx_samples.pcm\n");

        //  ОБРАБОТКА ВСЕХ НАКОПЛЕННЫХ ДАННЫХ
if (test_rx_sampless_bpsk_barker13.size() > 0) {
    printf("\n=== Обработка принятых данных ===\n");
    printf("Всего получено сэмплов: %zu\n", test_rx_sampless_bpsk_barker13.size());
    
    // === Вырезаем сегмент для обработки ===
    const size_t start_idx = 600;  // Пропускаем переходные процессы
    const size_t segment_len = 1920;
    const size_t end_idx = std::min(start_idx + segment_len, test_rx_sampless_bpsk_barker13.size());
    
    // Проверка: хватает ли данных?
    if (end_idx <= start_idx) {
        printf("❌ Ошибка: недостаточно данных для обработки (need %zu, have %zu)\n", 
               start_idx + segment_len, test_rx_sampless_bpsk_barker13.size());
        return; // или обработать ошибку
    }
    
    // Создаём сегмент с нормализацией
    // Деление на 2048 = 2^11: предполагаем, что входные данные в формате Q11 (11 бит дробной части)
    vector<complex<float>> rx_segment;
    rx_segment.reserve(end_idx - start_idx);
    
    printf("Копирование сегмента [%zu .. %zu] с нормализацией /2048...\n", start_idx, end_idx);
    for (size_t i = start_idx; i < end_idx; ++i) {
        // Нормализуем: переводим из фиксированной точки в float [-1.0; 1.0]
        float norm_factor = 2048.0f; // 2^11
        rx_segment.push_back(test_rx_sampless_bpsk_barker13[i] / norm_factor);
    }
    
    // Отладка: статистика сегмента
    float max_amp = 0.0f;
    float avg_power = 0.0f;
    for (const auto& s : rx_segment) {
        float amp = std::abs(s);
        if (amp > max_amp) max_amp = amp;
        avg_power += std::norm(s); // |s|^2
    }
    avg_power /= rx_segment.size();
    printf("✓ Сегмент: %zu сэмплов | Max amp: %.3f | Avg power: %.3f (%.2f dB)\n", 
           rx_segment.size(), max_amp, avg_power, 10*log10(avg_power + 1e-12));
    
    // === ЭТАП 1: Согласованный фильтр ===
    printf("\n[1/5] Применение согласованного фильтра (SRRC, sps=16)...\n");
    vector<complex<float>> matched = pulse_shaping(rx_segment, 16);
    printf("✓ После фильтра: %zu сэмплов (ожидалось ~%zu)\n", 
           matched.size(), rx_segment.size() + 16*5*2); // примерная оценка
    
    // Быстрая проверка: не пустой ли результат?
    if (matched.empty()) {
        printf("❌ ОШИБКА: фильтр вернул пустой вектор!\n");
        return;
    }
    
    // Проверка на NaN/Inf
    bool has_nan = false;
    for (size_t i = 0; i < std::min(matched.size(), (size_t)100); ++i) {
        if (std::isnan(matched[i].real()) || std::isinf(matched[i].real())) {
            has_nan = true;
            printf("⚠ NaN/Inf обнаружен на индексе %zu: %f + %fi\n", 
                   i, matched[i].real(), matched[i].imag());
            break;
        }
    }
    if (has_nan) {
        printf("❌ Прерываем обработку из-за некорректных значений\n");
        return;
    }
        //  ЭТАП 2: Грубая частотная синхронизация 
        printf("Поиск частотного сдвига...\n");
        float coarse_freq = coarse_max_freq_calculation(matched, sample_rate);
        printf("Найдено смещение: %.1f Hz\n", coarse_freq);
        
        if (fabsf(coarse_freq) > 5.0f) {
            matched = coarse_freq_sync(matched, coarse_freq, sample_rate);
            printf(" Частота скомпенсирована\n");
        } else {
            printf("ℹ Сдвиг мал (<5 Hz), пропускаем коррекцию\n");
        }

        //  ЭТАП 3: Детектор тактовой ошибки (TED) 
        // Используем сигнал ПОСЛЕ частотной синхронизации
        vector<float> ted = offset(matched, 16);

        //  ЭТАП 4: Даунсемплинг 
        vector<complex<float>> downsampled = dounsampling(matched, ted);

        //  ЭТАП 5: Costas Loop (фазовая синхронизация) 
        printf("Запуск Costas Loop (BPSK)...\n");
        vector<complex<float>> costas_out = costas_loop_bpsk(downsampled);

        // ЭТАП 6: Демодуляция
        vector<int> decoded_bits = bpsk_demapper(downsampled);
        printf(" Декодировано бит: %zu\n", decoded_bits.size());

        //  ЗАПУСК GUI 
        // Порядок параметров ВАЖЕН! Сверьтесь с сигнатурой функции.
        printf("\nОткрытие GUI...\n");
        run_gui(
            sample,                     
            upsample,                   
            convolved,                  
            matched,      
            ted,                        
            samples_per_symbol,
            downsampled,                          
            costas_out,                 
            test_rx_sampless_bpsk_barker13
        );
        printf(" Окно закрыто\n");
    }

    // Очистка ресурсов
    free(tx_buff);
    free(rx_buffer);
    sdr_cleanup(sdr);
    printf("Ресурсы освобождены\n");
    
    printf("\n Работа SDR завершена\n");
    return 0;
}





// void run_gui(const std::vector<std::complex<float>>& sample,
//              const std::vector<std::complex<float>>& upsample,
//              const std::vector<std::complex<float>>& convolved,
//              const std::vector<std::complex<float>>& matched,
//              const std::vector<float>& errof,
//              int samples_per_symbol,
//              const std::vector<std::complex<float>>& dounsample,
//              const std::vector<std::complex<float>>& costas_out,
//              const std::vector<std::complex<float>>& rx_raw)
// {
//     SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
//     SDL_DisplayMode current;
//     SDL_GetCurrentDisplayMode(0, &current);
    
//     SDL_Window* window = SDL_CreateWindow("SDR Signal Analyzer", 0, 0, current.w, current.h, 
//                                           SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESKTOP);
//     if (!window) { printf("SDL_CreateWindow failed\n"); return; }
    
//     SDL_GLContext gl_context = SDL_GL_CreateContext(window);
//     if (!gl_context) { printf("SDL_GL_CreateContext failed\n"); return; }
    
//     ImGui::CreateContext();
//     ImPlot::CreateContext();
//     ImGui::StyleColorsDark();
    
//     ImGuiIO& io = ImGui::GetIO();
//     io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
//     io.IniFilename = NULL;
    
//     ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
//     ImGui_ImplOpenGL3_Init("#version 330");

//     // Лямбда для подготовки данных
//     auto prep_plot = [](const std::vector<std::complex<float>>& data, 
//                         std::vector<float>& t, std::vector<float>& i, std::vector<float>& q) {
//         t.resize(data.size()); i.resize(data.size()); q.resize(data.size());
//         for (size_t j = 0; j < data.size(); j++) {
//             t[j] = (float)j;
//             i[j] = data[j].real();
//             q[j] = data[j].imag();
//         }
//     };

//     // Подготовка векторов
//     std::vector<float> t_sample, i_sample, q_sample; prep_plot(sample, t_sample, i_sample, q_sample);
//     std::vector<float> t_up, i_up, q_up;             prep_plot(upsample, t_up, i_up, q_up);
//     std::vector<float> t_conv, i_conv, q_conv;       prep_plot(convolved, t_conv, i_conv, q_conv);
//     std::vector<float> t_match, i_match, q_match;    prep_plot(matched, t_match, i_match, q_match);
//     std::vector<float> t_down, i_down, q_down;       prep_plot(dounsample, t_down, i_down, q_down);
//     std::vector<float> t_costas, i_costas, q_costas; prep_plot(costas_out, t_costas, i_costas, q_costas);
    
//     // RX Raw
//     std::vector<float> t_raw, i_raw, q_raw;
//     size_t raw_limit = std::min(rx_raw.size(), (size_t)5000);
//     if (raw_limit > 0) {
//         t_raw.resize(raw_limit); i_raw.resize(raw_limit); q_raw.resize(raw_limit);
//         for (size_t j = 0; j < raw_limit; j++) {
//             t_raw[j] = (float)j;
//             i_raw[j] = rx_raw[j].real();
//             q_raw[j] = rx_raw[j].imag();
//         }
//     }

//     bool running = true;
//     while (running) {
//         SDL_Event event;
//         while (SDL_PollEvent(&event)) {
//             ImGui_ImplSDL2_ProcessEvent(&event);
//             if (event.type == SDL_QUIT) running = false;
//             if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) running = false;
//         }

//         ImGui_ImplOpenGL3_NewFrame();
//         ImGui_ImplSDL2_NewFrame();
//         ImGui::NewFrame();

//         // ВЕРХНЯЯ ПАНЕЛЬ (TX)
//         ImGui::SetNextWindowPos(ImVec2(0, 0));
//         ImGui::SetNextWindowSize(ImVec2(current.w, current.h * 0.5f));
//         ImGui::Begin("##top", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        
//         if (ImGui::BeginTabBar("##top_tabs")) {
//             if (ImGui::BeginTabItem("TX Constellation")) {
//                 if (ImPlot::BeginPlot("##tx_const", ImVec2(-1, -1))) {
//                     ImPlot::SetupAxes("I", "Q");
//                     ImPlot::SetupAxesLimits(-1.5, 1.5, -1.5, 1.5);
//                     if (!i_sample.empty()) ImPlot::PlotScatter("TX", i_sample.data(), q_sample.data(), (int)i_sample.size());
//                     ImPlot::EndPlot();
//                 }
//                 ImGui::EndTabItem();
//             }
//             if (ImGui::BeginTabItem("TX Symbol")) {
//                 if (ImPlot::BeginPlot("##tx_sym", ImVec2(-1, -1))) {
//                     ImPlot::SetupAxes("Sample", "Amplitude");
//                     if (!i_sample.empty()) ImPlot::PlotLine("I", t_sample.data(), i_sample.data(), (int)i_sample.size());
//                     ImPlot::EndPlot();
//                 }
//                 ImGui::EndTabItem();
//             }
//             if (ImGui::BeginTabItem("TX Upsampled")) {
//                 if (ImPlot::BeginPlot("##tx_up", ImVec2(-1, -1))) {
//                     ImPlot::SetupAxes("Sample", "Amplitude");
//                     if (!i_up.empty()) ImPlot::PlotLine("I", t_up.data(), i_up.data(), (int)i_up.size());
//                     ImPlot::EndPlot();
//                 }
//                 ImGui::EndTabItem();
//             }
//             if (ImGui::BeginTabItem("TX Filtered")) {
//                 if (ImPlot::BeginPlot("##tx_filt", ImVec2(-1, -1))) {
//                     ImPlot::SetupAxes("Sample", "Amplitude");
//                     if (!i_conv.empty()) {
//                         ImPlot::PlotLine("I", t_conv.data(), i_conv.data(), (int)i_conv.size());
//                         ImPlot::PlotLine("Q", t_conv.data(), q_conv.data(), (int)q_conv.size());
//                     }
//                     ImPlot::EndPlot();
//                 }
//                 ImGui::EndTabItem();
//             }
//             ImGui::EndTabBar();
//         }
//         ImGui::End();

//         // НИЖНЯЯ ПАНЕЛЬ (RX)
//         ImGui::SetNextWindowPos(ImVec2(0, current.h * 0.5f));
//         ImGui::SetNextWindowSize(ImVec2(current.w, current.h * 0.5f));
//         ImGui::Begin("##bottom", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

//         if (ImGui::BeginTabBar("##bottom_tabs")) {
            
//             if (ImGui::BeginTabItem("RX Raw")) {
//                 if (ImPlot::BeginPlot("##rx_raw", ImVec2(-1, -1))) {
//                     ImPlot::SetupAxes("Sample", "Amplitude");
//                     ImPlot::SetupAxesLimits(-10, (float)t_raw.size() * 1.01f, -2.0f, 2.0f);
//                     if (!i_raw.empty()) {
//                         ImPlot::PlotLine("I", t_raw.data(), i_raw.data(), (int)i_raw.size());
//                         ImPlot::PlotLine("Q", t_raw.data(), q_raw.data(), (int)q_raw.size());
//                     }
//                     ImPlot::EndPlot();
//                 }
//                 ImGui::Text("Принято: %zu сэмплов", rx_raw.size());
//                 ImGui::EndTabItem();
//             }
            
//             if (ImGui::BeginTabItem("RX Matched")) {
//                 if (ImPlot::BeginPlot("##rx_match", ImVec2(-1, -1))) {
//                     ImPlot::SetupAxes("Sample", "Amplitude");
//                     ImPlot::SetupAxesLimits(-20, (float)matched.size() * 1.01f, -2.0f, 2.0f);
//                     if (!i_match.empty()) {
//                         ImPlot::PlotLine("I", t_match.data(), i_match.data(), (int)i_match.size());
//                         ImPlot::PlotLine("Q", t_match.data(), q_match.data(), (int)q_match.size());
//                         // Отрисовка меток TED
//                         for (float x : errof) {
//                             if (x >= 0 && x < (float)matched.size()) {
//                                 float xv[2] = {x, x}, yv[2] = {-2.0f, 2.0f};
//                                 ImPlot::PlotLine("TED", xv, yv, 2);
//                             }
//                         }
//                     }
//                     ImPlot::EndPlot();
//                 }
//                 ImGui::Text("Matched: %zu | TED: %zu", matched.size(), errof.size());
//                 ImGui::EndTabItem();
//             }
            
//             if (ImGui::BeginTabItem("RX Downsampled")) {
//                 if (ImPlot::BeginPlot("##rx_down", ImVec2(-1, -1))) {
//                     ImPlot::SetupAxes("Symbol", "Amplitude");
//                     ImPlot::SetupAxesLimits(-5, (float)dounsample.size() * 1.01f, -2.0f, 2.0f);
//                     if (!i_down.empty()) {
//                         ImPlot::PlotLine("I", t_down.data(), i_down.data(), (int)i_down.size());
//                         ImPlot::PlotLine("Q", t_down.data(), q_down.data(), (int)q_down.size());
//                     }
//                     ImPlot::EndPlot();
//                 }
//                 ImGui::Text("Downsampled: %zu символов", dounsample.size());
//                 ImGui::EndTabItem();
//             }

//             if (ImGui::BeginTabItem("After Costas")) {
//                 if (ImPlot::BeginPlot("##costas", ImVec2(-1, -1))) {
//                     ImPlot::SetupAxes("Symbol", "Amplitude");
//                     ImPlot::SetupAxesLimits(-10, (float)costas_out.size() * 1.01f, -2.0f, 2.0f);
//                     if (!i_costas.empty()) {
//                         ImPlot::PlotLine("I", t_costas.data(), i_costas.data(), (int)i_costas.size());
//                         ImPlot::PlotLine("Q", t_costas.data(), q_costas.data(), (int)q_costas.size());
//                     }
//                     ImPlot::EndPlot();
//                 }
//                 ImGui::Text("Costas: %zu символов", costas_out.size());
//                 ImGui::EndTabItem();
//             }
            
//             if (ImGui::BeginTabItem("Constellation")) {
//                 if (ImPlot::BeginPlot("##const", ImVec2(-1, -1))) {
//                     ImPlot::SetupAxes("I", "Q");
//                     ImPlot::SetupAxesLimits(-2.0, 2.0, -2.0, 2.0);
//                     if (!i_costas.empty()) {
//                         ImPlot::PlotScatter("BPSK", i_costas.data(), q_costas.data(), (int)i_costas.size());
//                         float ref_I[] = {-1.0f, 1.0f}, ref_Q[] = {0.0f, 0.0f};
//                         ImPlot::PlotScatter("Ref", ref_I, ref_Q, 2);
//                     }
//                     ImPlot::EndPlot();
//                 }
//                 ImGui::Text("BPSK: точки на I=±1, Q≈0");
//                 ImGui::EndTabItem();
//             }
            
//             ImGui::EndTabBar();
//         }
//         ImGui::End();

//         ImGui::Render();
//         glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
//         glClear(GL_COLOR_BUFFER_BIT);
//         ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
//         SDL_GL_SwapWindow(window);
//     }

//     ImGui_ImplOpenGL3_Shutdown();
//     ImGui_ImplSDL2_Shutdown();
//     ImPlot::DestroyContext();
//     ImGui::DestroyContext();
//     SDL_GL_DeleteContext(gl_context);
//     SDL_DestroyWindow(window);
//     SDL_Quit();
// }