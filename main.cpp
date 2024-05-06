#include "hashmap.hpp"

#include <random>
#include <chrono>
#include <assert.h>

const int NUM_KEYS = 10'000'000;
const int TOTAL_QUERIES = 25'000'000;
const int GROUP_SIZE = 32;
const int AMAC_REQUESTS_SIZE = 1024;

void measure_vectorized_get(HashMap<uint32_t, uint32_t>& openMap, auto gen, auto dis){
    auto start = std::chrono::high_resolution_clock::now();
    auto end = std::chrono::high_resolution_clock::now();
    double total_time = 0;
    std::vector<uint32_t> requests (GROUP_SIZE);
    std::vector<uint32_t> results (GROUP_SIZE);
    for(int i = 0; i < TOTAL_QUERIES; i+=GROUP_SIZE){
        for(int j = 0; j < GROUP_SIZE; j++){
            int random_number = dis(gen); // will generate duplicates, we dont care
            requests.at(j) = random_number;
        }
        start = std::chrono::high_resolution_clock::now();
        openMap.vectorized_get(requests, results);
        end = std::chrono::high_resolution_clock::now();
        total_time += std::chrono::duration<double>(end - start).count();

        for(int j = 0; j < GROUP_SIZE; j++){
            assert(results.at(j) == requests.at(j) +1);
        }

    }

    double throughput = TOTAL_QUERIES / total_time;

    std::cout << "Vectorized_get()" << std::endl;
    std::cout << "Total time taken: " << total_time << " seconds" << std::endl;
    std::cout << "Throughput: " << throughput << " queries/second" << std::endl;
}


void measure_vectorized_get_gp(HashMap<uint32_t, uint32_t>& openMap, auto gen, auto dis){
    auto start = std::chrono::high_resolution_clock::now();
    auto end = std::chrono::high_resolution_clock::now();
    double total_time = 0;
    std::vector<uint32_t> requests (GROUP_SIZE);
    std::vector<uint32_t> results (GROUP_SIZE);
    for(int i = 0; i < TOTAL_QUERIES; i+=GROUP_SIZE){
        for(int j = 0; j < GROUP_SIZE; j++){
            int random_number = dis(gen); // will generate duplicates, we dont care
            requests.at(j) = random_number;
        }

        start = std::chrono::high_resolution_clock::now();
        openMap.vectorized_get_gp(requests, results);
        end = std::chrono::high_resolution_clock::now();
        total_time += std::chrono::duration<double>(end - start).count();

        for(int j = 0; j < GROUP_SIZE; j++){
            assert(results.at(j) == requests.at(j) +1);
        }

    }

    double throughput = TOTAL_QUERIES / total_time;

    std::cout << "Vectorized_get_gp()" << std::endl;
    std::cout << "Total time taken: " << total_time << " seconds" << std::endl;
    std::cout << "Throughput: " << throughput << " queries/second" << std::endl;
}

void measure_vectorized_amac(HashMap<uint32_t, uint32_t>& openMap, auto gen, auto dis){
    auto start = std::chrono::high_resolution_clock::now();
    auto end = std::chrono::high_resolution_clock::now();
    double total_time = 0;
    std::vector<uint32_t> requests (AMAC_REQUESTS_SIZE);
    std::vector<uint32_t> results (AMAC_REQUESTS_SIZE);
    for(int i = 0; i < TOTAL_QUERIES; i+=AMAC_REQUESTS_SIZE){
        for(int j = 0; j < AMAC_REQUESTS_SIZE; j++){
            int random_number = dis(gen); // will generate duplicates, we dont care
            requests.at(j) = random_number;
        }

        start = std::chrono::high_resolution_clock::now();
        openMap.vectorized_get_amac(requests, results, GROUP_SIZE);
        end = std::chrono::high_resolution_clock::now();
        total_time += std::chrono::duration<double>(end - start).count();

        for(int j = 0; j < AMAC_REQUESTS_SIZE; j++){
            assert(results.at(j) == requests.at(j) +1);
        }

    }

    double throughput = TOTAL_QUERIES / total_time;

    std::cout << "Vectorized_get_amac()" << std::endl;
    std::cout << "Total time taken: " << total_time << " seconds" << std::endl;
    std::cout << "Throughput: " << throughput << " queries/second" << std::endl;
}


void measure_vectorized_co(HashMap<uint32_t, uint32_t>& openMap, auto gen, auto dis){
    auto start = std::chrono::high_resolution_clock::now();
    auto end = std::chrono::high_resolution_clock::now();
    double total_time = 0;
    std::vector<uint32_t> requests (AMAC_REQUESTS_SIZE);
    std::vector<uint32_t> results (AMAC_REQUESTS_SIZE);
    for(int i = 0; i < TOTAL_QUERIES; i+=AMAC_REQUESTS_SIZE){
        for(int j = 0; j < AMAC_REQUESTS_SIZE; j++){
            int random_number = dis(gen); // will generate duplicates, we dont care
            requests.at(j) = random_number;
        }

        start = std::chrono::high_resolution_clock::now();
        openMap.vectorized_get_coroutine(requests, results, GROUP_SIZE);
        end = std::chrono::high_resolution_clock::now();
        total_time += std::chrono::duration<double>(end - start).count();

        for(int j = 0; j < AMAC_REQUESTS_SIZE; j++){
            assert(results.at(j) == requests.at(j) +1);
        }

    }

    double throughput = TOTAL_QUERIES / total_time;

    std::cout << "Vectorized_get_co()" << std::endl;
    std::cout << "Total time taken: " << total_time << " seconds" << std::endl;
    std::cout << "Throughput: " << throughput << " queries/second" << std::endl;
}


int main() {
    HashMap<uint32_t, uint32_t> openMap {500'000};


    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_int_distribution<> dis(0, NUM_KEYS-1);

    for(uint32_t i = 0; i < NUM_KEYS; i++){
        openMap.insert(i, i+1);
    }

    measure_vectorized_amac(openMap, gen, dis);
    measure_vectorized_co(openMap, gen, dis);
    measure_vectorized_get_gp(openMap, gen, dis);
    measure_vectorized_get(openMap, gen, dis);

    return 0;
}
