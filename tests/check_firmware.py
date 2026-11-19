#!/usr/bin/env python3
"""Check ARM vectors, static task stacks and DMA placement in a linked ELF32."""
import argparse
import struct
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("elf", type=Path)
args = parser.parse_args()
data = args.elf.read_bytes()
assert data[:6] == b"\x7fELF\x01\x01", "Expected little-endian ELF32"
header = struct.unpack_from("<16sHHIIIIIHHHHHH", data)
assert header[2] == 40, "Expected ARM machine"
section_offset, section_size, section_count, strings_index = header[6], header[11], header[12], header[13]
sections = [struct.unpack_from("<IIIIIIIIII", data, section_offset + i * section_size)
            for i in range(section_count)]


def contents(section):
    return data[section[4]:section[4] + section[5]]


def name(strings, offset):
    return strings[offset:strings.index(b"\0", offset)].decode()


strings = contents(sections[strings_index])
by_name = {name(strings, s[0]): s for s in sections}
symbols = {}
for section in sections:
    if section[1] != 2:  # SHT_SYMTAB
        continue
    symbol_strings = contents(sections[section[6]])
    for offset in range(section[4], section[4] + section[5], section[9]):
        symbol = struct.unpack_from("<IIIBBH", data, offset)
        symbols[name(symbol_strings, symbol[0])] = symbol


def matching(fragment):
    matches = [symbol for key, symbol in symbols.items() if fragment in key]
    assert len(matches) == 1, f"Expected exactly one {fragment}: {len(matches)}"
    return matches[0]


def within(symbol, low, high):
    assert low <= symbol[1] and symbol[1] + symbol[2] <= high, hex(symbol[1])


for fragment in ("wire_tx", "wire_rx", "Charger6status", "Charger7tx_data"):
    symbol = matching(fragment)
    within(symbol, 0x38000000, 0x38004000)
    if fragment.startswith("wire_"):
        assert symbol[1] % 32 == 0, "BDMA wire frames must stay aligned"
for fragment in ("rxDataHall", "raw_adc_val"):
    within(matching(fragment), 0x24000000, 0x24020000)
for fragment in ("control_stack", "acquisition_stack", "imu_stack", "tof_stack", "services_stack"):
    within(matching(fragment), 0x20000000, 0x20020000)
for section_name in (".RAM", ".RAM_D3"):
    assert by_name[section_name][1] == 8, f"{section_name} must remain NOLOAD/NOBITS"

vectors = contents(by_name[".isr_vector"])
for vector, handler in ((11, "SVC_Handler"), (14, "PendSV_Handler"),
                        (15, "SysTick_Handler"), (16 + 55, "TIM7_IRQHandler"),
                        (16 + 84, "SPI4_IRQHandler"), (16 + 95, "I2C4_EV_IRQHandler"),
                        (16 + 96, "I2C4_ER_IRQHandler")):
    pointer, = struct.unpack_from("<I", vectors, vector * 4)
    assert pointer & 1 and pointer == symbols[handler][1], f"Incorrect vector for {handler}"
print("PASS: ARM vectors, task stacks, Hall/ADC DMA buffers, and D3 BDMA frames")
