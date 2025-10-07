#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "test_utils.h"
#include <shmipc/ipc_mmap.h>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <vector>
#include <thread>
#include <atomic>
#include <sys/wait.h>
#include <unistd.h>
#include <cstring>

using namespace std::chrono;

// Простой тест: один процесс пишет, другой читает
TEST_CASE("interprocess - basic communication") {
    const char* segment_name = "test_basic_comm";
    const size_t segment_size = 1024 * 1024; // 1MB
    
    // Создаем shared memory segment
    IpcMemorySegmentResult segment_result = ipc_mmap(segment_name, segment_size);
    REQUIRE(segment_result.ipc_status == IPC_OK);
    
    pid_t child_pid = fork();
    
    if (child_pid == 0) {
        // Дочерний процесс - читатель
        IpcChannelConnectResult connect_result = ipc_channel_connect(
            segment_result.result->memory, test_utils::DEFAULT_CONFIG);
        REQUIRE(connect_result.ipc_status == IPC_OK);
        
        IpcChannel* channel = connect_result.result;
        
        // Читаем 10 сообщений
        for (int i = 0; i < 10; ++i) {
            IpcEntry entry;
            IpcChannelReadResult read_result = ipc_channel_read(channel, &entry);
            REQUIRE(read_result.ipc_status == IPC_OK);
            
            int received_data;
            memcpy(&received_data, entry.payload, entry.size);
            REQUIRE(received_data == i);
            free(entry.payload);
        }
        
        ipc_channel_destroy(channel);
        ipc_unmap(segment_result.result);
        exit(0);
    } else {
        // Родительский процесс - писатель
        IpcChannelResult create_result = ipc_channel_create(
            segment_result.result->memory, segment_size, test_utils::DEFAULT_CONFIG);
        REQUIRE(create_result.ipc_status == IPC_OK);
        
        IpcChannel* channel = create_result.result;
        
        // Пишем 10 сообщений
        for (int i = 0; i < 10; ++i) {
            IpcChannelWriteResult write_result = ipc_channel_write(channel, &i, sizeof(i));
            REQUIRE(write_result.ipc_status == IPC_OK);
        }
        
        // Ждем завершения дочернего процесса
        int status;
        waitpid(child_pid, &status, 0);
        REQUIRE(WEXITSTATUS(status) == 0);
        
        ipc_channel_destroy(channel);
        ipc_unlink(segment_result.result);
        ipc_unmap(segment_result.result);
    }
}

// Тест производительности: измеряем throughput между процессами
TEST_CASE("interprocess - performance throughput") {
    const char* segment_name = "test_performance";
    const size_t segment_size = 2 * 1024 * 1024; // 2MB
    const int iterations = 10000;
    
    IpcMemorySegmentResult segment_result = ipc_mmap(segment_name, segment_size);
    REQUIRE(segment_result.ipc_status == IPC_OK);
    
    pid_t child_pid = fork();
    
    if (child_pid == 0) {
        // Дочерний процесс - читатель
        IpcChannelConnectResult connect_result = ipc_channel_connect(
            segment_result.result->memory, test_utils::DEFAULT_CONFIG);
        REQUIRE(connect_result.ipc_status == IPC_OK);
        
        IpcChannel* channel = connect_result.result;
        
        auto start = high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            IpcEntry entry;
            IpcChannelReadResult read_result = ipc_channel_read(channel, &entry);
            REQUIRE(read_result.ipc_status == IPC_OK);
            free(entry.payload);
        }
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);
        
        double throughput = (double)iterations / duration.count() * 1000000;
        std::cout << "Interprocess read throughput: " << std::fixed << std::setprecision(0) 
                  << throughput << " ops/sec" << std::endl;
        
        ipc_channel_destroy(channel);
        ipc_unmap(segment_result.result);
        exit(0);
    } else {
        // Родительский процесс - писатель
        IpcChannelResult create_result = ipc_channel_create(
            segment_result.result->memory, segment_size, test_utils::DEFAULT_CONFIG);
        REQUIRE(create_result.ipc_status == IPC_OK);
        
        IpcChannel* channel = create_result.result;
        
        auto start = high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            IpcChannelWriteResult write_result = ipc_channel_write(channel, &i, sizeof(i));
            REQUIRE(write_result.ipc_status == IPC_OK);
        }
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);
        
        double throughput = (double)iterations / duration.count() * 1000000;
        std::cout << "Interprocess write throughput: " << std::fixed << std::setprecision(0) 
                  << throughput << " ops/sec" << std::endl;
        
        // Ждем завершения дочернего процесса
        int status;
        waitpid(child_pid, &status, 0);
        REQUIRE(WEXITSTATUS(status) == 0);
        
        ipc_channel_destroy(channel);
        ipc_unlink(segment_result.result);
        ipc_unmap(segment_result.result);
    }
}

// Тест с большими данными между процессами
TEST_CASE("interprocess - large data transfer") {
    const char* segment_name = "test_large_data";
    const size_t segment_size = 4 * 1024 * 1024; // 4MB
    const int data_size = 1024; // 1KB блоки
    const int iterations = 100;
    
    IpcMemorySegmentResult segment_result = ipc_mmap(segment_name, segment_size);
    REQUIRE(segment_result.ipc_status == IPC_OK);
    
    pid_t child_pid = fork();
    
    if (child_pid == 0) {
        // Дочерний процесс - читатель
        IpcChannelConnectResult connect_result = ipc_channel_connect(
            segment_result.result->memory, test_utils::DEFAULT_CONFIG);
        REQUIRE(connect_result.ipc_status == IPC_OK);
        
        IpcChannel* channel = connect_result.result;
        
        auto start = high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            IpcEntry entry;
            IpcChannelReadResult read_result = ipc_channel_read(channel, &entry);
            REQUIRE(read_result.ipc_status == IPC_OK);
            REQUIRE(entry.size == data_size);
            free(entry.payload);
        }
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);
        
        double throughput = (double)iterations / duration.count() * 1000000;
        double data_throughput = (double)iterations * data_size / (duration.count() / 1000000.0) / (1024 * 1024);
        std::cout << "Interprocess large data read: " << std::fixed << std::setprecision(0) 
                  << throughput << " ops/sec, " << std::fixed << std::setprecision(2) 
                  << data_throughput << " MB/sec" << std::endl;
        
        ipc_channel_destroy(channel);
        ipc_unmap(segment_result.result);
        exit(0);
    } else {
        // Родительский процесс - писатель
        IpcChannelResult create_result = ipc_channel_create(
            segment_result.result->memory, segment_size, test_utils::DEFAULT_CONFIG);
        REQUIRE(create_result.ipc_status == IPC_OK);
        
        IpcChannel* channel = create_result.result;
        
        std::vector<uint8_t> large_data(data_size, 0xAB);
        
        auto start = high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            IpcChannelWriteResult write_result = ipc_channel_write(channel, large_data.data(), data_size);
            REQUIRE(write_result.ipc_status == IPC_OK);
        }
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);
        
        double throughput = (double)iterations / duration.count() * 1000000;
        double data_throughput = (double)iterations * data_size / (duration.count() / 1000000.0) / (1024 * 1024);
        std::cout << "Interprocess large data write: " << std::fixed << std::setprecision(0) 
                  << throughput << " ops/sec, " << std::fixed << std::setprecision(2) 
                  << data_throughput << " MB/sec" << std::endl;
        
        // Ждем завершения дочернего процесса
        int status;
        waitpid(child_pid, &status, 0);
        REQUIRE(WEXITSTATUS(status) == 0);
        
        ipc_channel_destroy(channel);
        ipc_unlink(segment_result.result);
        ipc_unmap(segment_result.result);
    }
}

// Упрощенный тест: множественные процессы (1 писатель, 1 читатель)
TEST_CASE("interprocess - single writer single reader") {
    const char* segment_name = "test_single_comm";
    const size_t segment_size = 1024 * 1024; // 1MB
    const int messages_count = 1000;
    
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
        
        for (int i = 0; i < messages_count; ++i) {
            IpcEntry entry;
            IpcChannelReadResult read_result = ipc_channel_read(reader_channel, &entry);
            REQUIRE(read_result.ipc_status == IPC_OK);
            
            int received_data;
            memcpy(&received_data, entry.payload, entry.size);
            REQUIRE(received_data == i);
            free(entry.payload);
        }
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);
        
        double throughput = (double)messages_count / duration.count() * 1000000;
        std::cout << "Reader throughput: " << std::fixed << std::setprecision(0) 
                  << throughput << " ops/sec" << std::endl;
        
        ipc_channel_destroy(reader_channel);
        ipc_unmap(segment_result.result);
        exit(0);
    } else {
        // Родительский процесс - писатель
        IpcChannelResult create_result = ipc_channel_create(
            segment_result.result->memory, segment_size, test_utils::DEFAULT_CONFIG);
        REQUIRE(create_result.ipc_status == IPC_OK);
        
        IpcChannel* channel = create_result.result;
        
        auto start = high_resolution_clock::now();
        
        for (int i = 0; i < messages_count; ++i) {
            IpcChannelWriteResult write_result = ipc_channel_write(channel, &i, sizeof(i));
            REQUIRE(write_result.ipc_status == IPC_OK);
        }
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);
        
        double throughput = (double)messages_count / duration.count() * 1000000;
        std::cout << "Writer throughput: " << std::fixed << std::setprecision(0) 
                  << throughput << " ops/sec" << std::endl;
        
        // Ждем завершения дочернего процесса
        int status;
        waitpid(child_pid, &status, 0);
        REQUIRE(WEXITSTATUS(status) == 0);
        
        ipc_channel_destroy(channel);
        ipc_unlink(segment_result.result);
        ipc_unmap(segment_result.result);
    }
}

// Упрощенный стресс-тест: 1 писатель, 1 читатель, много сообщений
TEST_CASE("interprocess - stress test") {
    const char* segment_name = "test_stress_final";
    const size_t segment_size = 4 * 1024 * 1024; // 4MB
    const int total_messages = 10000;
    
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
                // Нет данных, продолжаем ждать
                continue;
            } else {
                REQUIRE(false); // Неожиданная ошибка
            }
        }
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);
        
        double throughput = (double)messages_read / duration.count() * 1000000;
        std::cout << "Stress test reader throughput: " << std::fixed << std::setprecision(0) 
                  << throughput << " ops/sec" << std::endl;
        
        ipc_channel_destroy(reader_channel);
        ipc_unmap(segment_result.result);
        exit(0);
    } else {
        // Родительский процесс - писатель
        IpcChannelResult create_result = ipc_channel_create(
            segment_result.result->memory, segment_size, test_utils::DEFAULT_CONFIG);
        REQUIRE(create_result.ipc_status == IPC_OK);
        
        IpcChannel* channel = create_result.result;
        
        auto start = high_resolution_clock::now();
        
        for (int i = 0; i < total_messages; ++i) {
            IpcChannelWriteResult write_result = ipc_channel_write(channel, &i, sizeof(i));
            REQUIRE(write_result.ipc_status == IPC_OK);
        }
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);
        
        double throughput = (double)total_messages / duration.count() * 1000000;
        std::cout << "Stress test writer throughput: " << std::fixed << std::setprecision(0) 
                  << throughput << " ops/sec" << std::endl;
        
        // Ждем завершения дочернего процесса
        int status;
        waitpid(child_pid, &status, 0);
        REQUIRE(WEXITSTATUS(status) == 0);
        
        ipc_channel_destroy(channel);
        ipc_unlink(segment_result.result);
        ipc_unmap(segment_result.result);
    }
}
