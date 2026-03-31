#include "test_rx_samples_bpsk_barker13.h"
#include "SDR_include.h"
using namespace std;

int main() {

    const int sample_rate = 1000000;
    const int samples_per_symbol = 16;
    const float beta = 0.75f;
    const float barker_threshold = 8.0f;      // Порог обнаружения Баркера
    const int min_peak_distance = 20;          // Мин. расстояние между пиками (символы)
    const int BARKER_LEN = 13;                 // Длина кода Баркера

    vector<int> barker_real = {1, 1, 1, 1, 1, -1, -1, 1, 1, -1, 1, -1, 1};
    vector<int> barker_imag = {1, 1, 1, 1, 1, -1, -1, 1, 1, -1, 1, -1, 1};
    vector<complex<float>> barker_complex;
    barker_complex.reserve(barker_real.size());
    for(size_t i = 0; i < barker_real.size(); i++){
        barker_complex.emplace_back(barker_real[i] * 1.1f, barker_imag[i] * 1.1f);
    }

    vector<int> hello_sibguti = {
        0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 
        1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1, 1, 0, 1, 1, 
        0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 
        1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 0, 
        1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 1, 
        1, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 
        1, 1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0
    };

    // Обрабатываем ВЕСЬ доступный массив, а не сегмент
    vector<complex<float>> rx_segment;
    rx_segment.reserve(test_rx_sampless_bpsk_barker13.size());
    for (size_t i = 0; i < test_rx_sampless_bpsk_barker13.size(); ++i) {
        rx_segment.push_back(test_rx_sampless_bpsk_barker13[i] / 2048.0f);
    }

    // Цепочка обработки сигнала
    vector<complex<float>> matched = matched_filter(rx_segment, samples_per_symbol, beta);
    vector<complex<float>> downsampled = clock_recovery_mueller_muller(matched, samples_per_symbol);
    vector<complex<float>> costas_out = costas_loop_bpsk(downsampled);
    
    // Нормализация к единице (по модулю)
    float max_amp = 1e-6f;
    for (const auto& s : costas_out) {
        float amp = abs(s);
        if (amp > max_amp) max_amp = amp;
    }
    
    vector<complex<float>> costas_norm(costas_out.size());
    for (size_t i = 0; i < costas_out.size(); ++i) {
        costas_norm[i] = costas_out[i] / max_amp;
    }

    // КОРРЕЛЯЦИЯ ДЛЯ ПОИСКА БАРКЕРА
    vector<complex<float>> corr = correlate_barker(costas_norm, barker_complex);
    vector<int> barker_positions;
    float local_max = -1e9f;
    int local_max_idx = -1;

    for (size_t i = 0; i < corr.size(); ++i) {
        // Поиск локального максимума
        if (corr[i].real() > local_max) {
            local_max = corr[i].real();
            local_max_idx = static_cast<int>(i);
        }
        
        // Проверка кандидата после окна задержки
        if (i >= static_cast<size_t>(min_peak_distance) && local_max_idx >= 0) {
            if (local_max > barker_threshold) {
                // Проверка на дубликаты (слишком близкие пики)
                bool too_close = false;
                for (int prev_pos : barker_positions) {
                    if (abs(local_max_idx - prev_pos) < min_peak_distance) {
                        too_close = true;
                        break;
                    }
                }
                if (!too_close) {
                    barker_positions.push_back(local_max_idx);
                    printf("✓ Barker frame #%zu found at symbol: %d, peak: %.4f\n", 
                           barker_positions.size(), local_max_idx, local_max);
                }
            }
            // Сброс для поиска следующего пика
            local_max = -1e9f;
            local_max_idx = -1;
        }
    }

    // Проверка последнего кандидата (если массив закончился)
    if (local_max > barker_threshold && local_max_idx >= 0) {
        bool too_close = false;
        for (int prev_pos : barker_positions) {
            if (abs(local_max_idx - prev_pos) < min_peak_distance) {
                too_close = true;
                break;
            }
        }
        if (!too_close) {
            barker_positions.push_back(local_max_idx);
            printf("✓ Barker frame #%zu found at symbol: %d, peak: %.4f\n", 
                   barker_positions.size(), local_max_idx, local_max);
        }
    }

    if (barker_positions.empty()) {
        printf(" ERROR: No Barker sequences found above threshold %.2f!\n", barker_threshold);
        printf(" Совет: попробуйте уменьшить barker_threshold или проверить качество сигнала\n");
        return 1;
    }

    printf("\n Найдено фреймов: %zu\n", barker_positions.size());

    vector<int> all_decoded_bits;
    all_decoded_bits.reserve(costas_norm.size()); // предвыделение памяти

    for (size_t frame_idx = 0; frame_idx < barker_positions.size(); ++frame_idx) {
        int frame_start = barker_positions[frame_idx] + BARKER_LEN;
        
        // Конец фрейма: начало следующего Баркера или конец массива
        int frame_end = (frame_idx + 1 < barker_positions.size()) 
                      ? barker_positions[frame_idx + 1]  
                      : static_cast<int>(costas_norm.size());
        
        // Защита от некорректных границ
        if (frame_start >= frame_end) {
            printf("⚠ Frame %zu: invalid bounds [%d, %d), skipped\n", 
                   frame_idx, frame_start, frame_end);
            continue;
        }
        
        // Извлечение символов фрейма
        vector<complex<float>> frame_data(
            costas_norm.begin() + frame_start, 
            costas_norm.begin() + frame_end
        );
        
        // Демодуляция BPSK: real > 0 → 1, иначе 0
        for (const auto& sym : frame_data) {
            all_decoded_bits.push_back(sym.real() > 0.0f ? 1 : 0);
        }
        
        printf("Frame %zu: symbols [%6d, %6d), bits: %4zu\n", 
               frame_idx, frame_start, frame_end, frame_data.size());
    }


    printf("\n" "СТАТИСТИКА\n");
    printf("Всего декодировано бит: %zu\n", all_decoded_bits.size());

    if (!hello_sibguti.empty()) {
        int success_counter = 0;
        int error_counter = 0;
        size_t compare_len = 180;
        
        printf("\n Побитовое сравнение с эталоном (первые %zu бит):\n", compare_len);
        
        
        if (compare_len > 0) {
            printf("Ожидаемо: ");
            for (size_t i = 0; i < min(compare_len, size_t(180)); ++i) {
                if (i % 8 == 0 && i > 0) printf(" ");
                printf("%d", hello_sibguti[i]);
            }
            printf("%s\n", compare_len > 180 ? "..." : "");
            
            printf("Принято:  ");
            for (size_t i = 0; i < min(compare_len, size_t(180)); ++i) {
                if (i % 8 == 0 && i > 0) printf(" ");
                printf("%d", all_decoded_bits[i]);
            }
            printf("%s\n", compare_len > 180 ? "..." : "");
        }
        
        // Подсчёт ошибок
        for (size_t i = 0; i < compare_len; ++i) {
            if (hello_sibguti[i] == all_decoded_bits[i]) {
                success_counter++;
            } else {
                error_counter++;
            }
        }
        
        float success_rate = (compare_len > 0) ? 
            (100.0f * success_counter / static_cast<float>(compare_len)) : 0.0f;
        float ber = (compare_len > 0) ? 
            (static_cast<float>(error_counter) / static_cast<float>(compare_len)) : 1.0f;
        
        printf("\n Совпадений:  %d / %zu\n", success_counter, compare_len);
        printf(" Ошибок:      %d / %zu\n", error_counter, compare_len);
        printf(" Успешность:  %.2f%%\n", success_rate);
    }
//     run_gui(
//         rx_segment,        
//         matched,            
//         downsampled,        
//         corr,              
//         data_only,         
//         costas_out                
// );

     return 0;
}