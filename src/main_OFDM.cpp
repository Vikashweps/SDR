#include "test_rx_samples_bpsk_barker13.h"
#include "SDR_include.h"
using namespace std;

int main() {
    int Nc = 46; //поднесущие
    vector <int> hello_sibguti = {
        0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 
        1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1, 1, 0, 1, 1, 
        0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 
        1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 0, 
        1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 1, 
        1, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 
        1, 1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0
    };

    vector<complex<float>> symbols= bpsk_mapper( hello_sibguti );
    vector<complex<float>> Ofdm = OFDM (symbols, Nc);

    // Цепочка обработки сигнала
    vector<complex<float>> OFDM_Dem = OFDM_Demodulate(Ofdm, Nc);
    //vector<complex<float>> downsampled = clock_recovery_mueller_muller(OFDM_Dem, samples_per_symbol);
    //vector<complex<float>> costas_out = costas_loop_bpsk(downsampled);
    
    vector <int> decoded_bits  = bpsk_demapper(OFDM_Dem);
   

    printf("Исходное количество бит: %zu\n", hello_sibguti.size());
    printf("Декодировано бит: %zu\n", decoded_bits.size());

    if (!hello_sibguti.empty()) {
        int success_counter = 0;
        int error_counter = 0;
        size_t compare_len = 180;
        
        printf("\n Побитовое сравнение с эталоном (первые %zu бит):\n", compare_len);
        
        
        if (compare_len > 0) {
            printf("Ожидаемо: ");
            printf("\n");
            for (size_t i = 0; i < min(compare_len, size_t(180)); ++i) {
                if (i % 8 == 0 && i > 0) printf(" ");
                printf("%d", hello_sibguti[i]);
            }
            printf("%s\n", compare_len > 180 ? "..." : "");
            printf("\n");
            printf("Принято:  ");
            printf("\n");
            for (size_t i = 0; i < min(compare_len, size_t(180)); ++i) {
                if (i % 8 == 0 && i > 0) printf(" ");
                printf("%d", decoded_bits[i]);
            }
            printf("%s\n", compare_len > 180 ? "..." : "");
        }
        
        // Подсчёт ошибок
        for (size_t i = 0; i < compare_len; ++i) {
            if (hello_sibguti[i] == decoded_bits[i]) {
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