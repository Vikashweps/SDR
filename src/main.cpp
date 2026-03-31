#include "test_rx_samples_bpsk_barker13.h"
#include "SDR_include.h"
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <atomic>
#include <unistd.h>

using namespace std;

int main() {
     // Инициализация двух SDR
    sdr_device_t *rx = sdr_init_usb_index(0);
    sdr_device_t *tx = sdr_init_usb_index(1);
    if (!rx || !tx) return 1;
    
    sdr_configure(rx);
    sdr_configure(tx);
    
    printf(" Запуск SDR (BPSK + Barker-13)\n");
    srand(static_cast<unsigned int>(time(NULL)));

    //  ПАРАМЕТРЫ СИСТЕМЫ 
    const int samples_per_symbol = 16;
    //const float beta = 0.75f;
    const float barker_threshold = 8.0f;
    const int min_peak_distance = 20;
    const int BARKER_LEN = 13;
    const float NORM_FACTOR = 2048.0f;  // Нормализация int16 → float

    //  ЭТАЛОННЫЙ БАРКЕР 
    vector<int> barker_real = {1, 1, 1, 1, 1, -1, -1, 1, 1, -1, 1, -1, 1};
    vector<int> barker_imag = {1, 1, 1, 1, 1, -1, -1, 1, 1, -1, 1, -1, 1};
    vector<complex<float>> barker_complex;
    barker_complex.reserve(barker_real.size());
    for(size_t i = 0; i < barker_real.size(); i++){
        barker_complex.emplace_back(barker_real[i] * 1.1f, barker_imag[i] * 1.1f);
    }

    //  ЭТАЛОННЫЕ ДАННЫЕ 
    vector<int> hello_sibguti = {
        0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 
        1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1, 1, 0, 1, 1, 
        0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 
        1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 0, 
        1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 1, 
        1, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 
        1, 1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0
    };

    //  ИНИЦИАЛИЗАЦИЯ SDR 
    sdr_device_t *sdr = sdr_init(1);
    if (sdr == NULL) {
        printf(" ОШИБКА: Не удалось создать SDR устройство!\n");
        return 1;
    }
    if (sdr_configure(sdr) != 0) { 
        sdr_cleanup(sdr); 
        return 1; 
    }
    printf("✓ Устройство успешно инициализировано\n");
    printf("TX MTU: %zu сэмплов | RX MTU: %zu сэмплов\n", sdr->tx_mtu, sdr->rx_mtu);

    //  ПОДГОТОВКА ДАННЫХ ДЛЯ ПЕРЕДАЧИ 
    vector<complex<float>> data_symbols = bpsk_mapper(hello_sibguti);
    vector<complex<float>> tx_frame = barker_complex;
    tx_frame.insert(tx_frame.end(), data_symbols.begin(), data_symbols.end());
    
    printf(" Сформирован кадр: %zu символов (Баркер: %zu + Данные: %zu)\n", 
           tx_frame.size(), barker_complex.size(), data_symbols.size());
    
    // здесь будет офдм
    vector<complex<float>> upsampled = OFDM(tx_frame, samples_per_symbol);
    
    int16_t *tx_buff = (int16_t*)malloc(2 * sdr->tx_mtu * sizeof(int16_t));
    if (!tx_buff) { printf(" ERROR: malloc tx_buff\n"); sdr_cleanup(sdr); return 1; }
    
    for (size_t i = 0; i < 2; i++) {
        tx_buff[i] = 0xffff;
        tx_buff[10 + i] = 0xffff;
    }

    //  БУФЕРЫ ПРИЁМА 
    int16_t *rx_buffer = (int16_t*)malloc(2 * sdr->rx_mtu * sizeof(int16_t));
    if (!rx_buffer) { printf(" ERROR: malloc rx_buffer\n"); free(tx_buff); sdr_cleanup(sdr); return 1; }
    
    vector<int16_t> rx_raw_all;
    rx_raw_all.reserve(200000);

    FILE *rx_file = fopen("rx_samples.pcm", "wb");
    FILE *tx_file = fopen("tx_samples.pcm", "wb");
    printf("✓ Буферы и файлы готовы\n");

    //  ОСНОВНОЙ ЦИКЛ RX/TX 
    const size_t MAX_ITER = 100;
    const long TIMEOUT_US = 400000;
    long long timeNs = 0;

    printf("\n Запуск цикла (%zu итераций)...\n", MAX_ITER);
    
    for (size_t iter = 0; iter < MAX_ITER; ++iter) {
        //  ПРИЁМ 
        void *rx_buffs[] = {rx_buffer};
        int flags;
        int sr = SoapySDRDevice_readStream(sdr->device, sdr->rxStream, rx_buffs, sdr->rx_mtu, &flags, &timeNs, TIMEOUT_US);
        
        if (sr > 0) {
            if (rx_file) fwrite(rx_buffer, sizeof(int16_t), 2*sr, rx_file);
            rx_raw_all.insert(rx_raw_all.end(), rx_buffer, rx_buffer + 2*sr);
            if (iter % 10 == 0) printf("📥 Приём #%zu: %d сэмплов (всего: %zu)\n", iter, sr, rx_raw_all.size()/2);
        }

        //  ПЕРЕДАЧА (начиная с 3-й итерации) 
        if (iter >= 3 && !upsampled.empty()) {
            complex_to_sdr(upsampled, tx_buff + 12, min(upsampled.size(), sdr->tx_mtu - 12));
            
            long long tx_time = timeNs + (4LL * 1000 * 1000);
            for(size_t j = 0; j < 8; j++) {
                uint8_t byte = (tx_time >> (j * 8)) & 0xff;
                tx_buff[2 + j] = byte << 4;
            }
            
            void *tx_buffs[] = {tx_buff};
            int tx_flags = SOAPY_SDR_HAS_TIME;
            int st = SoapySDRDevice_writeStream(sdr->device, sdr->txStream, 
                                               (const void * const*)tx_buffs, 
                                               sdr->tx_mtu, &tx_flags, tx_time, TIMEOUT_US);
            if (st != (int)sdr->tx_mtu) {
                printf("⚠ Ошибка передачи: %d\n", st);
            } else if (iter == 3) {
                printf("Передача начата! (%zu сэмплов)\n", upsampled.size());
            }
            if (tx_file) fwrite(tx_buff, sizeof(int16_t), 2*sdr->tx_mtu, tx_file);
        }
    }

    //  ЗАВЕРШЕНИЕ ЦИКЛА 
    if (rx_file) fclose(rx_file);
    if (tx_file) fclose(tx_file);
    printf("\n Файлы сохранены: rx_samples.pcm, tx_samples.pcm\n");

    //  ОБРАБОТКА ПРИНЯТЫХ ДАННЫХ (ВСЁ В MAIN) 
    printf("\n ОБРАБОТКА ПРИНЯТЫХ ДАННЫХ n");
    
    if (rx_raw_all.empty()) {
        printf(" Нет данных для обработки!\n");
    } else {
        printf("Получено сырых сэмплов (I16Q16): %zu → %zu комплексных\n", 
               rx_raw_all.size(), rx_raw_all.size() / 2);

        // 1. Конвертация int16 → complex<float> с нормализацией
        vector<complex<float>> rx_segment;
        rx_segment.reserve(rx_raw_all.size() / 2);
        
        for (size_t i = 0; i + 1 < rx_raw_all.size(); i += 2) {
            float I = static_cast<float>(rx_raw_all[i]) / NORM_FACTOR;
            float Q = static_cast<float>(rx_raw_all[i+1]) / NORM_FACTOR;
            rx_segment.emplace_back(I, Q);
        }

        // Статистика сигнала
        float max_amp = 1e-6f, avg_power = 0;
        for (const auto& s : rx_segment) {
            float amp = abs(s);
            if (amp > max_amp) max_amp = amp;
            avg_power += norm(s);
        }
        avg_power /= rx_segment.size();
        printf("✓ Сигнал: Max amp=%.3f, Avg power=%.3f (%.2f dB)\n", 
               max_amp, avg_power, 10*log10(avg_power + 1e-12));

        // 2. Цепочка обработки сигнала
        printf("\n[1/4] ОФДМ-демодулятор...\n");
        vector<complex<float>> matched = OFDM_Demodulate(rx_segment, samples_per_symbol);
        if (matched.empty()) { printf(" ERROR: пустой вектор!\n"); }
        else { printf("✓ ОФДМ-демодулятор: %zu сэмплов\n", matched.size()); }

        printf("[2/4] Восстановление тактовой частоты (Mueller-Muller)...\n");
        vector<complex<float>> downsampled = clock_recovery_mueller_muller(matched, samples_per_symbol);
        if (downsampled.empty()) { printf(" ERROR: clock_recovery вернул пустой вектор!\n"); }
        else { printf("✓ clock_recovery: %zu символов\n", downsampled.size()); }

        printf("[3/4] Фазовая синхронизация (Costas Loop BPSK)...\n");
        vector<complex<float>> costas_out = costas_loop_bpsk(downsampled);
        if (costas_out.empty()) { printf(" ERROR: costas_loop вернул пустой вектор!\n"); }
        else { printf("✓ costas_loop: %zu символов\n", costas_out.size()); }

        // Нормализация
        float max_costas = 1e-6f;
        for (const auto& s : costas_out) {
            float amp = abs(s);
            if (amp > max_costas) max_costas = amp;
        }
        vector<complex<float>> costas_norm(costas_out.size());
        for (size_t i = 0; i < costas_out.size(); ++i) {
            costas_norm[i] = costas_out[i] / max_costas;
        }

        // 3. Корреляция и поиск Баркера
        printf("[4/4] Поиск фреймов по корреляции с Баркером...\n");
        vector<complex<float>> corr = correlate_barker(costas_norm, barker_complex);
        
        vector<int> barker_positions;
        float local_max = -1e9f;
        int local_max_idx = -1;

        for (size_t i = 0; i < corr.size(); ++i) {
            if (corr[i].real() > local_max) {
                local_max = corr[i].real();
                local_max_idx = static_cast<int>(i);
            }
            if (i >= static_cast<size_t>(min_peak_distance) && local_max_idx >= 0) {
                if (local_max > barker_threshold) {
                    bool too_close = false;
                    for (int prev : barker_positions) {
                        if (abs(local_max_idx - prev) < min_peak_distance) { too_close = true; break; }
                    }
                    if (!too_close) {
                        barker_positions.push_back(local_max_idx);
                        printf("✓ Barker #%zu @ symbol %d, peak=%.3f\n", 
                               barker_positions.size(), local_max_idx, local_max);
                    }
                }
                local_max = -1e9f; local_max_idx = -1;
            }
        }
        // Последний кандидат
        if (local_max > barker_threshold && local_max_idx >= 0) {
            bool too_close = false;
            for (int prev : barker_positions) {
                if (abs(local_max_idx - prev) < min_peak_distance) { too_close = true; break; }
            }
            if (!too_close) {
                barker_positions.push_back(local_max_idx);
                printf("✓ Barker #%zu @ symbol %d, peak=%.3f\n", 
                       barker_positions.size(), local_max_idx, local_max);
            }
        }

        if (barker_positions.empty()) {
            printf("ERROR: Баркер не найден (порог=%.2f)\n", barker_threshold);
        } else {
            printf("\nНайдено фреймов: %zu\n", barker_positions.size());

            // 4. Извлечение и демодуляция данных
            printf("\n Извлечение данных из фреймов...\n");
            vector<int> all_decoded_bits;
            all_decoded_bits.reserve(costas_norm.size());

            for (size_t f = 0; f < barker_positions.size(); ++f) {
                int start = barker_positions[f] + BARKER_LEN;
                int end = (f + 1 < barker_positions.size()) ? barker_positions[f+1] : static_cast<int>(costas_norm.size());
                if (start >= end) continue;
                
                for (int i = start; i < end; ++i) {
                    all_decoded_bits.push_back(costas_norm[i].real() > 0.0f ? 1 : 0);
                }
                printf("  Frame %zu: [%d..%d) → %d бит\n", f, start, end, end - start);
            }

            // 5. Сравнение с эталоном
            printf("\n СТАТИСТИКА n");
            printf("Декодировано бит: %zu | Ожидалось: %zu\n", 
                   all_decoded_bits.size(), hello_sibguti.size());

            if (!hello_sibguti.empty() && !all_decoded_bits.empty()) {
                size_t cmp_len = min(hello_sibguti.size(), all_decoded_bits.size());
                int ok = 0, err = 0;
                
                printf("\n Первые 180 бит:\nОжидаемо: ");
                for (size_t i = 0; i < min(cmp_len, size_t(180)); ++i) {
                    if (i % 8 == 0 && i > 0) printf(" ");
                    printf("%d", hello_sibguti[i]);
                }
                printf("\nПринято:  ");
                for (size_t i = 0; i < min(cmp_len, size_t(180)); ++i) {
                    if (i % 8 == 0 && i > 0) printf(" ");
                    printf("%d", all_decoded_bits[i]);
                }
                printf("\n");

                for (size_t i = 0; i < cmp_len; ++i) {
                    if (hello_sibguti[i] == all_decoded_bits[i]) ok++; else err++;
                }
                if (all_decoded_bits.size() > hello_sibguti.size()) 
                    err += static_cast<int>(all_decoded_bits.size() - hello_sibguti.size());
                else if (hello_sibguti.size() > all_decoded_bits.size())
                    err += static_cast<int>(hello_sibguti.size() - all_decoded_bits.size());
                
                size_t total = max(hello_sibguti.size(), all_decoded_bits.size());
                printf("\n Совпадений: %d / %zu\n Ошибок: %d / %zu\n Успешность: %.2f%%\n BER: %.4f\n",
                       ok, total, err, total, 100.0f * ok / total, static_cast<float>(err) / total);
            }
        }
    }

    //  ОЧИСТКА РЕСУРСОВ 
    free(tx_buff);
    free(rx_buffer);
    sdr_cleanup(sdr);
    
    printf("\nРабота завершена, ресурсы освобождены.\n");
    return 0;
}