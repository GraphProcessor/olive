/**
 * Utils
 *
 * Author: Yichao Cheng (onesuperclark@gmail.com)
 * Created on: 2014-10-20
 * Last Modified: 2014-10-23
 */

#include <sys/time.h>
#include <cuda.h>
#include <ctype.h>
 
#include "util.h"

int getGpuNum(void) {
    int num = 1;
    if (cudaGetDeviceCount(&num) != cudaSuccess) {
        oliveError("fail to get device count");
    }
    return num;
}

void setGpuNum(int num) {
    if (cudaGetDeviceCount(&num) != cudaSuccess) {
        oliveError("fail to set device count: %d", num);
    }
}

void checkAvailableMemory(void) {
    size_t available = 0; size_t total = 0;
    cudaError_t err;
    err = cudaMemGetInfo(&available, &total);
    if (err == cudaSuccess) {
        printf("available memory: %llu / %llu\n", available, total);
    } else {
        oliveError("fail to check available memory");
    }
}

bool isNumeric(char * str) {
  assert(str);
  while ((* str) != '\0') {
    if (!isdigit(* str)) {
        return false;
    } else {
        str++;
    }
  }
  return true;
}

double Timer::getTime(void) {
    cudaThreadSynchronize();
    timeval t;
    gettimeofday(&t, NULL);
    return static_cast<double>(t.tv_sec)+static_cast<double>(t.tv_usec/1000000);
}

void Timer::initialize(void) {
    this->last_time = getTime();
}

double Timer::elapsedTime(void) {
    double new_time = getTime();
    double elapesed = new_time - this->last_time;
    this->last_time = new_time;
    return elapesed;
}


