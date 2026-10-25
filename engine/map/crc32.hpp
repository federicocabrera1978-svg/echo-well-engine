#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ew::map {
// CRC-32 (IEEE 802.3, polinomio 0xEDB88320)
uint32_t crc32(std::span<const std::byte> data);
}
