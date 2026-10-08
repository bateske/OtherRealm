#pragma once
#include <stdint.h>
namespace device {
enum Button { A=1, B=2, Up=4, Down=8, Left=16, Right=32, Start=64, Select=128 };
void buttonsBegin();
uint8_t buttonsRead();
}
