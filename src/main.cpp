#include "SDR_include.h"
using namespace std;

int main() {
    printf("Запуск SDR\n");
    srand((unsigned int)time(NULL));
    
    printf("\n[1] Инициализация устройства...\n");
    sdr_device_t *sdr = sdr_init(1);
    if (sdr == NULL) {
        printf("ОШИБКА: Не удалось создать SDR устройство!\n");
        return 1;
    }
    if (sdr_configure(sdr) != 0) { sdr_cleanup(sdr); return 1; }

    printf("✓ Устройство успешно инициализировано\n");
    printf("    TX MTU: %zu сэмплов\n", sdr->tx_mtu);

    //  Параметры сигнала
    const int samples_per_symbol = 10;  
    const int num_bits = sdr->tx_mtu / samples_per_symbol;  
    
    printf("\n[2] Параметры сигнала:\n");
    printf("    Бит: %d\n", num_bits);
    printf("    Сэмплов на символ (SPS): %d\n", samples_per_symbol);
    printf("    Всего сэмплов: %d\n", num_bits * samples_per_symbol);
    
    printf("\n[3] Генерация случайных битов...\n");
    vector<int> bits(num_bits);
    for (int i = 0; i < num_bits; i++) {
        bits[i] = rand() % 2;  
    }
    printf("Сгенерировано %zu бит\n", bits.size());
    
    printf("   BPSK модуляция...\n");
    vector<complex<float>> sample = bpsk_mapper(bits);
    printf("      Получено %zu символов\n", sample.size());
    
    printf("   Апсемплинг (×%d)...\n", samples_per_symbol);
    vector<complex<float>> upsample = upsampling(sample, samples_per_symbol);
    printf("      Получено %zu сэмплов\n", upsample.size());
    
    printf("   Формирующий фильтр...\n");
    vector<complex<float>> convolved = convolve(upsample, samples_per_symbol);
    printf("      Получено %zu сэмплов\n", convolved.size());
    printf(" обработка завершена\n");

    // 5. Конвертация в буфер SDR (complex → int16_t) 
    int16_t *tx_buff = (int16_t*)malloc(2 * sdr->tx_mtu * sizeof(int16_t));
    complex_to_sdr(convolved, tx_buff, convolved.size());

    for (size_t i = convolved.size(); i < sdr->tx_mtu; i++) {
        tx_buff[2*i] = 0;
        tx_buff[2*i + 1] = 0;
    }
    printf(" Буфер передачи готов: %zu сэмплов\n", convolved.size());

    printf("\n[6] Подготовка к приёму/передаче...\n");
    int16_t *rx_buffer = (int16_t*)malloc(2 * sdr->rx_mtu * sizeof(int16_t));
    long long timeNs = 0;
    const long timeoutUs = 400000;
    
    FILE *rx_file = fopen("rx_samples.pcm", "wb");
    FILE *tx_file = fopen("tx_samples.pcm", "wb");
    
    if (rx_file == NULL) printf(" Предупреждение: не удалось создать rx_samples.pcm\n");
    if (tx_file == NULL) printf(" Предупреждение: не удалось создать tx_samples.pcm\n");
    
    vector<complex<float>> rx_signal;
    printf("Буферы и файлы готовы\n");

    size_t iteration_count = 20;
    printf("\n[7] Запуск цикла приёма/передачи (%zu итераций)...\n\n", iteration_count);
    
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
            
            vector<complex<float>> rx_complex;
            sdr_to_complex(rx_buffer, rx_complex, sr);
            rx_signal.insert(rx_signal.end(), rx_complex.begin(), rx_complex.end());
        } else {
            printf("Приём %zu: ОШИБКА (%d)\n", i, sr);
        }

        // ПЕРЕДАЧА (на 3-й итерации)
        if (i == 2) {
            printf("\n    >>> ОТПРАВКА СИГНАЛА (%zu сэмплов) <<<\n", convolved.size());
            
            if (tx_file != NULL) {
                fwrite(tx_buff, sizeof(int16_t), 2 * sdr->tx_mtu, tx_file);
            }
            
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
        
        this_thread::sleep_for(chrono::milliseconds(50));
    }

    if (rx_file != NULL) fclose(rx_file);
    if (tx_file != NULL) fclose(tx_file);
    printf("\n Файлы сохранены: rx_samples.pcm, tx_samples.pcm\n");

    printf("\n[8] Обработка принятых данных (DSP)...\n");
    
    if (rx_signal.size() > 0) {
        printf("    Всего получено сэмплов: %zu\n", rx_signal.size());
        
        vector<complex<float>> mathed = convolve(convolved, samples_per_symbol);
        vector<float> errof = offset(mathed, samples_per_symbol);
        vector<complex<float>> dounsample = dounsampling(mathed, errof);
        
        printf("    Точек синхронизации: %zu\n", errof.size());
        printf("    Восстановлено символов: %zu\n", dounsample.size());
        printf(" Обработка завершена\n");
        
        //  9. Визуализация 
        printf("\n[9] Открытие графического интерфейса...\n");
        printf("    (Закройте окно для продолжения)\n");
        run_gui(sample, upsample, convolved, mathed, errof, samples_per_symbol, dounsample);
        printf(" Окно закрыто\n");
    } else {
        printf(" ПРЕДУПРЕЖДЕНИЕ: Данные приёма пусты!\n");
    }

    
    printf("\n[10] Очистка ресурсов...\n");
    free(tx_buff);
    free(rx_buffer);
    sdr_cleanup(sdr);
    printf("Ресурсы освобождены\n");
    
    printf("\nРабота SDR завершена \n");
    return 0;
}