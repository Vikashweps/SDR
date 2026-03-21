#include "test_rx_samples_bpsk_barker13.h"
#include "SDR_include.h"
using namespace std;

int main() {

    // Параметры приема
    const int sample_rate = 4000000;
    const int samples_per_symbol = 16;
    int syms = 5;
    const float beta = 0.75f;
    const float barker_threshold = 8.0f;

    // Формируем Баркера
    vector<int> barker_real = {1, 1, 1, 1, 1, -1, -1, 1, 1, -1, 1, -1, 1};
    vector<int> barker_imag = {1, 1, 1, 1, 1, -1, -1, 1, 1, -1, 1, -1, 1};
    vector<complex<float>> barker_complex;
    for(int i = 0; i < (int)barker_real.size(); i++){
        barker_complex.push_back(complex(barker_real[i] * 1.1f, barker_imag[i] * 1.1f));
    }

    // Исходная последовательность  0X00Hello from user10X0 - декодировано 0X00Hello from user1 
    vector<int> hello_sibguti = {0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 
        1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 
        0, 1, 1, 0, 1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 1, 1, 0, 1, 
        1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 1, 1, 0, 1, 
        1, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 
        0, 0, 0, 1, 1, 0, 0, 0, 0};

    
    const size_t start_idx = 8210;
    const size_t end_idx = min(start_idx + 3000, test_rx_sampless_bpsk_barker13.size()); 
    vector<complex<float>> rx_segment;
    rx_segment.reserve(end_idx - start_idx);
    for (size_t i = start_idx; i < end_idx; ++i) {
        rx_segment.push_back(test_rx_sampless_bpsk_barker13[i] / 2048.0f);
    }
    vector<complex<float>> matched = matched_filter(rx_segment, samples_per_symbol, beta);
    vector<complex<float>> downsampled = clock_recovery_mueller_muller(matched, samples_per_symbol);
    vector<complex<float>> costas_out = costas_loop_bpsk(downsampled);
    
    // Нормализация к единице
    float max = 0.0f;
    vector<complex<float>> costas_norm;
    costas_norm.resize(costas_out.size());
    
    for (int i = 0; i < costas_out.size(); i++) {
        if(costas_out[i].real() > max){
            max = costas_out[i].real();
        }
    }
    if (max > 1e-6f) {  // защита от деления на ноль
        for (size_t i = 0; i < costas_out.size(); i++) {
            costas_norm[i] = costas_out[i] / max;
        }
    }


    // КОРРЕЛЯЦИЯ ДЛЯ ПОИСКА БАРКЕРА
    vector<complex<float>> corr = correlate_barker(costas_norm, barker_complex);
    // Поиск пика
    float max_val = -1e9f;
    int max_idx = -1;
    for (size_t i = 0; i < corr.size(); ++i) {
        if (corr[i].real() > max_val) {
            max_val = corr[i].real();
            max_idx = static_cast<int>(i);
        }
    }

    if (max_idx == -1 ) {
        printf("ERROR: Barker not found!\n");
        return 1;
    }
    printf("Barker found at symbol index: %d, peak: %.4f\n", max_idx, max_val);

    //  ОБРЕЗКА СИГНАЛА 
    const int BARKER_LEN = 13;
    int data_start_idx = max_idx + BARKER_LEN; 
    
     // Извлекаем символы данных из нормированного сигнала
    vector<complex<float>> data_only(costas_norm.begin() + data_start_idx, costas_norm.end());

    // Демодуляция
    vector<int> decoded;
    for (const auto& sym : data_only) {
        decoded.push_back(sym.real() > 0.0f ? 1 : 0);
    }

    printf("Декодировано бит: %zu\n", decoded.size());
    printf("Принятые биты: ");
    for (size_t i = 0; i < decoded.size(); i++) {
        if (i % 8 == 0) printf(" ");
        printf("%d", decoded[i]);
    }
    printf("\n");

    // 8. Побитовое сравнение с эталонной последовательностью
    int success_counter = 0;
    int error_counter = 0;
    size_t compare_len = min(hello_sibguti.size(), decoded.size());
    
    printf("\n Побитовое сравнение \n");
    printf("Ожидаемо бит: %zu\n", hello_sibguti.size());
    printf("Принято бит:   %zu\n", decoded.size());
    printf("Сравниваем первых %zu бит:\n", compare_len);
    
    for (size_t i = 0; i < compare_len; i++) {
        if (hello_sibguti[i] == decoded[i]) {
            success_counter++;
        } else {
            error_counter++;
        }
    }
    
    // Расчёт статистики
    float success_rate = (compare_len > 0) ? 
        (100.0f * success_counter / static_cast<float>(compare_len)) : 0.0f;

    printf("Совпадений:  %d / %zu\n", success_counter, compare_len);
    printf("Ошибок:      %d / %zu\n", error_counter, compare_len);
    printf("Успешность:  %.2f%%\n", success_rate);
    
 

    
    run_gui(
        rx_segment,        
        matched,            
        downsampled,        
        corr,              
        data_only,         
        costas_out                
);

     return 0;
}