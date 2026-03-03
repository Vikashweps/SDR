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

    // Параметры сигнала
    vector<int> Barker = {1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1};
    const int samples_per_symbol = 10;  
    const int num_bits = sdr->tx_mtu / samples_per_symbol - Barker.size();  
    
    // 1. Генерация данных (биты) 
    vector<int> bits(num_bits);
    for (size_t i = 0; i < bits.size(); i++) {
        bits[i] = rand() % 2;  
    }
    printf("Сгенерировано %zu бит\n", bits.size());
    printf("Первые 30 бит: ");
    for (size_t i = 0; i < 30 && i < bits.size(); i++) {
        printf("%d", bits[i]);
    }
    printf("\n");
    
    //  2. BPSK маппер (биты → символы) 
    vector<complex<float>> data_symbols = bpsk_mapper(bits);
    vector<complex<float>> Barker_symbols = bpsk_mapper(Barker);

    // 3. Объединяем: Баркер + Данные 
    vector<complex<float>> sample = Barker_symbols;
    sample.insert(sample.end(), data_symbols.begin(), data_symbols.end());
    
    printf("Всего символов: %zu (Баркер: %zu + Данные: %zu)\n", 
           sample.size(), Barker.size(), data_symbols.size());

    vector<complex<float>> upsample = upsampling(sample, samples_per_symbol);
    vector<complex<float>> convolved = convolve(upsample, samples_per_symbol);
    
    int16_t *tx_buff = (int16_t*)malloc(2 * sdr->tx_mtu * sizeof(int16_t));

    complex_to_sdr(convolved, tx_buff, convolved.size());

    // for (size_t i = 0; i < sdr->tx_mtu; i++) {
    //     tx_buff[2*i] = 2000<<4;
    //     tx_buff[2*i + 1] = 2000<<4;
    // }

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
    
    vector<complex<float>> rx_signal;
    
    // Файлы
    FILE *rx_file = fopen("rx_samples.pcm", "wb");
    FILE *tx_file = fopen("tx_samples.pcm", "wb");

    vector<complex<float>> rx_complex(1920);
    vector<complex<float>> rx_complex_full;

    if (rx_file == NULL) printf("Предупреждение: не удалось создать rx_samples.pcm\n");
    if (tx_file == NULL) printf("Предупреждение: не удалось создать tx_samples.pcm\n");
    
    printf("Буферы и файлы готовы\n");

    //Основной цикл RX/TX 
    size_t iteration_count = 100;

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
             for (int i = 0; i < 1920; i++){
                rx_complex[i] = complex<float>(rx_buffer[i*2], rx_buffer[i*2 + 1]);
             }
             rx_complex_full.insert(rx_complex_full.end(), rx_complex.begin(), rx_complex.end());
    

             
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
    if (rx_complex_full.size() > 0) {
        printf("\nОбработка принятых данных n");
        printf("Всего получено сэмплов: %zu\n", rx_complex_full.size());
        
        vector<complex<float>> matched = convolve(rx_complex_full, samples_per_symbol);

        printf("Запуск синхронизации символов...\n");
        vector<float> errof = offset(matched, samples_per_symbol);
        
        printf("Даунсемплинг...\n");
        vector<complex<float>> dounsample = dounsampling(matched, errof);
        printf("Восстановлено символов: %zu\n", dounsample.size());
        
        // Демодуляция: символы → биты
        vector<int> decoded_bits = bpsk_demapper(dounsample);
        printf("Декодировано бит: %zu\n", decoded_bits.size());
        
        printf("Первые 30 декодированных бит: ");
        for (size_t i = 0; i < 30 && i < decoded_bits.size(); i++) {
            printf("%d", decoded_bits[i]);
        }


        printf("\nОткрытие GUI\n");
        run_gui(sample, upsample, convolved, matched, errof, samples_per_symbol, dounsample);
        printf("Окно закрыто\n");
        
    }

    // Очистка 
    free(tx_buff);
    free(rx_buffer);
    sdr_cleanup(sdr);
    printf("Ресурсы освобождены\n");
    
    printf("\nРабота SDR завершена\n");
    return 0;
}
