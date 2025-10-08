#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "test_utils.h"
#include <shmipc/ipc_mmap.h>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>
#include <cstring>
#include <random>

using namespace std::chrono;

// Простой стресс-тест: 1 писатель, 1 читатель, много сообщений
TEST_CASE("interprocess stress - simple stress test") {
    const char* segment_name = "test_simple_stress";
    const size_t segment_size = 2 * 1024 * 1024; // 2MB
    const int total_messages = 5000; // Уменьшаем количество
    
    IpcMemorySegmentResult segment_result = ipc_mmap(segment_name, segment_size);
    REQUIRE(segment_result.ipc_status == IPC_OK);
    
    pid_t child_pid = fork();
    
    if (child_pid == 0) {
        // Дочерний процесс - читатель
        IpcChannelConnectResult connect_result = ipc_channel_connect(
            segment_result.result->memory, test_utils::DEFAULT_CONFIG);
        REQUIRE(connect_result.ipc_status == IPC_OK);
        
        IpcChannel* reader_channel = connect_result.result;
        
        auto start = high_resolution_clock::now();
        
        int messages_read = 0;
        while (messages_read < total_messages) {
            IpcEntry entry;
            IpcChannelReadResult read_result = ipc_channel_read(reader_channel, &entry);
            if (read_result.ipc_status == IPC_OK) {
                free(entry.payload);
                messages_read++;
            } else if (read_result.ipc_status == IPC_EMPTY) {
                continue;
            } else {
                REQUIRE(false);
            }
        }
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);
        
        double throughput = (double)messages_read / duration.count() * 1000000;
        std::cout << "Simple stress reader throughput: " << std::fixed << std::setprecision(0) 
                  << throughput << " ops/sec" << std::endl;
        
        ipc_channel_destroy(reader_channel);
        ipc_unmap(segment_result.result);
        exit(0);
    } else {
        // Родительский процесс - писатель
        IpcChannelResult create_result = ipc_channel_create(
            segment_result.result->memory, segment_size, test_utils::DEFAULT_CONFIG);
        REQUIRE(create_result.ipc_status == IPC_OK);
        
        IpcChannel* writer_channel = create_result.result;
        
        auto start = high_resolution_clock::now();
        
        for (int i = 0; i < total_messages; ++i) {
            IpcChannelWriteResult write_result = ipc_channel_write(writer_channel, &i, sizeof(i));
            REQUIRE(write_result.ipc_status == IPC_OK);
        }
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);
        
        double throughput = (double)total_messages / duration.count() * 1000000;
        std::cout << "Simple stress writer throughput: " << std::fixed << std::setprecision(0) 
                  << throughput << " ops/sec" << std::endl;
        
        // Ждем завершения дочернего процесса
        int status;
        waitpid(child_pid, &status, 0);
        REQUIRE(WEXITSTATUS(status) == 0);
        
        ipc_channel_destroy(writer_channel);
        ipc_unlink(segment_result.result);
        ipc_unmap(segment_result.result);
    }
}

// Стресс-тест с разными размерами данных
TEST_CASE("interprocess stress - variable data sizes") {
    const char* segment_name = "test_variable_sizes";
    const size_t segment_size = 2 * 1024 * 1024; // 2MB
    const int data_sizes[] = {4, 8, 16, 32}; // Небольшие размеры данных
    const int iterations_per_size = 500;
    
    IpcMemorySegmentResult segment_result = ipc_mmap(segment_name, segment_size);
    REQUIRE(segment_result.ipc_status == IPC_OK);
    
    for (int data_size : data_sizes) {
        std::cout << "\n=== Testing data size: " << data_size << " bytes ===" << std::endl;
        
        pid_t child_pid = fork();
        
        if (child_pid == 0) {
            // Дочерний процесс - читатель
            IpcChannelConnectResult connect_result = ipc_channel_connect(
                segment_result.result->memory, test_utils::DEFAULT_CONFIG);
            REQUIRE(connect_result.ipc_status == IPC_OK);
            
            IpcChannel* reader_channel = connect_result.result;
            
            auto start = high_resolution_clock::now();
            
            for (int i = 0; i < iterations_per_size; ++i) {
                IpcEntry entry;
                IpcChannelReadResult read_result = ipc_channel_read(reader_channel, &entry);
                REQUIRE(read_result.ipc_status == IPC_OK);
                REQUIRE(entry.size == data_size);
                free(entry.payload);
            }
            
            auto end = high_resolution_clock::now();
            auto duration = duration_cast<microseconds>(end - start);
            
            double throughput = (double)iterations_per_size / duration.count() * 1000000;
            double data_throughput = (double)iterations_per_size * data_size / 
                                   (duration.count() / 1000000.0) / (1024 * 1024);
            std::cout << "Reader throughput: " << std::fixed << std::setprecision(0) 
                      << throughput << " ops/sec, " << std::fixed << std::setprecision(2) 
                      << data_throughput << " MB/sec" << std::endl;
            
            ipc_channel_destroy(reader_channel);
            ipc_unmap(segment_result.result);
            exit(0);
        } else {
            // Родительский процесс - писатель
            IpcChannelResult create_result = ipc_channel_create(
                segment_result.result->memory, segment_size, test_utils::DEFAULT_CONFIG);
            REQUIRE(create_result.ipc_status == IPC_OK);
            
            IpcChannel* writer_channel = create_result.result;
            
            std::vector<uint8_t> data(data_size, 0xAB);
            
            auto start = high_resolution_clock::now();
            
            for (int i = 0; i < iterations_per_size; ++i) {
                IpcChannelWriteResult write_result = ipc_channel_write(
                    writer_channel, data.data(), data_size);
                REQUIRE(write_result.ipc_status == IPC_OK);
            }
            
            auto end = high_resolution_clock::now();
            auto duration = duration_cast<microseconds>(end - start);
            
            double throughput = (double)iterations_per_size / duration.count() * 1000000;
            double data_throughput = (double)iterations_per_size * data_size / 
                                   (duration.count() / 1000000.0) / (1024 * 1024);
            std::cout << "Writer throughput: " << std::fixed << std::setprecision(0) 
                      << throughput << " ops/sec, " << std::fixed << std::setprecision(2) 
                      << data_throughput << " MB/sec" << std::endl;
            
            // Ждем завершения дочернего процесса
            int status;
            waitpid(child_pid, &status, 0);
            REQUIRE(WEXITSTATUS(status) == 0);
            
            ipc_channel_destroy(writer_channel);
        }
    }
    
    // Очищаем ресурсы
    ipc_unlink(segment_result.result);
    ipc_unmap(segment_result.result);
}

// Стресс-тест с рандомными данными
TEST_CASE("interprocess stress - random data") {
    const char* segment_name = "test_random_data";
    const size_t segment_size = 2 * 1024 * 1024; // 2MB
    const int total_operations = 2000; // Уменьшаем количество операций
    const int max_data_size = 8; // Очень маленький максимальный размер
    
    IpcMemorySegmentResult segment_result = ipc_mmap(segment_name, segment_size);
    REQUIRE(segment_result.ipc_status == IPC_OK);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> size_dist(1, max_data_size);
    
    pid_t child_pid = fork();
    
    if (child_pid == 0) {
        // Дочерний процесс - читатель
        IpcChannelConnectResult connect_result = ipc_channel_connect(
            segment_result.result->memory, test_utils::DEFAULT_CONFIG);
        REQUIRE(connect_result.ipc_status == IPC_OK);
        
        IpcChannel* reader_channel = connect_result.result;
        
        auto start = high_resolution_clock::now();
        
        int operations_performed = 0;
        while (operations_performed < total_operations) {
            IpcEntry entry;
            IpcChannelReadResult read_result = ipc_channel_read(reader_channel, &entry);
            if (read_result.ipc_status == IPC_OK) {
                // Проверяем, что данные корректные
                uint8_t* payload = static_cast<uint8_t*>(entry.payload);
                for (size_t i = 0; i < entry.size; ++i) {
                    REQUIRE(payload[i] == 0xAB);
                }
                free(entry.payload);
                operations_performed++;
            } else if (read_result.ipc_status == IPC_EMPTY) {
                continue;
            } else {
                REQUIRE(false);
            }
        }
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);
        
        double throughput = (double)operations_performed / duration.count() * 1000000;
        std::cout << "Random data reader throughput: " << std::fixed << std::setprecision(0) 
                  << throughput << " ops/sec" << std::endl;
        
        ipc_channel_destroy(reader_channel);
        ipc_unmap(segment_result.result);
        exit(0);
    } else {
        // Родительский процесс - писатель
        IpcChannelResult create_result = ipc_channel_create(
            segment_result.result->memory, segment_size, test_utils::DEFAULT_CONFIG);
        REQUIRE(create_result.ipc_status == IPC_OK);
        
        IpcChannel* writer_channel = create_result.result;
        
        auto start = high_resolution_clock::now();
        
        for (int i = 0; i < total_operations; ++i) {
            int data_size = size_dist(gen);
            std::vector<uint8_t> random_data(data_size, 0xAB);
            
            IpcChannelWriteResult write_result = ipc_channel_write(
                writer_channel, random_data.data(), data_size);
            REQUIRE(write_result.ipc_status == IPC_OK);
        }
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);
        
        double throughput = (double)total_operations / duration.count() * 1000000;
        std::cout << "Random data writer throughput: " << std::fixed << std::setprecision(0) 
                  << throughput << " ops/sec" << std::endl;
        
        // Ждем завершения дочернего процесса
        int status;
        waitpid(child_pid, &status, 0);
        REQUIRE(WEXITSTATUS(status) == 0);
        
        ipc_channel_destroy(writer_channel);
        ipc_unlink(segment_result.result);
        ipc_unmap(segment_result.result);
    }
}

// Стресс-тест с peek операциями
TEST_CASE("interprocess stress - peek operations") {
    const char* segment_name = "test_peek_ops";
    const size_t segment_size = 1 * 1024 * 1024; // 1MB
    const int total_operations = 1000;
    
    IpcMemorySegmentResult segment_result = ipc_mmap(segment_name, segment_size);
    REQUIRE(segment_result.ipc_status == IPC_OK);
    
    pid_t child_pid = fork();
    
    if (child_pid == 0) {
        // Дочерний процесс - читатель с peek
        IpcChannelConnectResult connect_result = ipc_channel_connect(
            segment_result.result->memory, test_utils::DEFAULT_CONFIG);
        REQUIRE(connect_result.ipc_status == IPC_OK);
        
        IpcChannel* reader_channel = connect_result.result;
        
        auto start = high_resolution_clock::now();
        
        int operations_performed = 0;
        while (operations_performed < total_operations) {
            // Чередуем peek и read операции
            if (operations_performed % 3 == 0) {
                IpcEntry entry;
                IpcChannelPeekResult peek_result = ipc_channel_peek(reader_channel, &entry);
                if (peek_result.ipc_status == IPC_OK) {
                    // Если есть данные, читаем их
                    IpcChannelReadResult read_result = ipc_channel_read(reader_channel, &entry);
                    if (read_result.ipc_status == IPC_OK) {
                        free(entry.payload);
                        operations_performed++;
                    }
                }
            } else {
                IpcEntry entry;
                IpcChannelReadResult read_result = ipc_channel_read(reader_channel, &entry);
                if (read_result.ipc_status == IPC_OK) {
                    free(entry.payload);
                    operations_performed++;
                } else if (read_result.ipc_status == IPC_EMPTY) {
                    continue;
                }
            }
        }
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);
        
        double throughput = (double)operations_performed / duration.count() * 1000000;
        std::cout << "Peek operations reader throughput: " << std::fixed << std::setprecision(0) 
                  << throughput << " ops/sec" << std::endl;
        
        ipc_channel_destroy(reader_channel);
        ipc_unmap(segment_result.result);
        exit(0);
    } else {
        // Родительский процесс - писатель
        IpcChannelResult create_result = ipc_channel_create(
            segment_result.result->memory, segment_size, test_utils::DEFAULT_CONFIG);
        REQUIRE(create_result.ipc_status == IPC_OK);
        
        IpcChannel* writer_channel = create_result.result;
        
        auto start = high_resolution_clock::now();
        
        for (int i = 0; i < total_operations; ++i) {
            IpcChannelWriteResult write_result = ipc_channel_write(writer_channel, &i, sizeof(i));
            REQUIRE(write_result.ipc_status == IPC_OK);
        }
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);
        
        double throughput = (double)total_operations / duration.count() * 1000000;
        std::cout << "Peek operations writer throughput: " << std::fixed << std::setprecision(0) 
                  << throughput << " ops/sec" << std::endl;
        
        // Ждем завершения дочернего процесса
        int status;
        waitpid(child_pid, &status, 0);
        REQUIRE(WEXITSTATUS(status) == 0);
        
        ipc_channel_destroy(writer_channel);
        ipc_unlink(segment_result.result);
        ipc_unmap(segment_result.result);
    }
}