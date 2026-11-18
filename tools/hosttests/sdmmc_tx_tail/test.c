/* SPDX-License-Identifier: Apache-2.0
 * Host-only extracted-function test; register operations are FIFO stubs.
 */
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STM32_SDMMC_STA_OFFSET 0
#define STM32_SDMMC_FIFO_OFFSET 1
#define STM32_SDMMC_STA_TXFIFOF 1
struct stm32_dev_s { uint32_t *buffer; size_t remaining; };
static uint32_t fifo[128];
static size_t written, capacity;
static uint32_t sdmmc_getreg32(struct stm32_dev_s *priv, int offset)
{
  (void)priv;
  assert(offset == STM32_SDMMC_STA_OFFSET);
  return written >= capacity ? STM32_SDMMC_STA_TXFIFOF : 0;
}
static void sdmmc_putreg32(struct stm32_dev_s *priv, uint32_t word, int offset)
{
  (void)priv;
  assert(offset == STM32_SDMMC_FIFO_OFFSET);
  assert(written < capacity && written < 128);
  fifo[written++] = word;
}
#include "sendfifo.inc"

int main(void)
{
  const size_t sizes[] = {1, 2, 3, 4, 5, 511, 512};
  size_t cases = 0;
  /* The target is little-endian; check the host used for word comparison. */
  const uint32_t endian = 1;
  assert(*(const uint8_t *)&endian == 1);
  for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); ++s)
    {
      size_t length = sizes[s], words = (length + 3) / 4;
      for (size_t pause = 0; pause <= words; ++pause)
        {
          /* Exactly length bytes: ASan detects an erroneous full-word tail
           * read. malloc supplies full-word alignment for the driver input.
           */
          uint8_t *source = malloc(length);
          uint8_t *original = malloc(length);
          assert(source != NULL && original != NULL);
          for (size_t i = 0; i < length; ++i)
            source[i] = (uint8_t)(i * 37 + 11);
          memcpy(original, source, length);
          struct stm32_dev_s priv = {(uint32_t *)source, length};
          memset(fifo, 0xa5, sizeof(fifo));
          written = 0;
          capacity = pause;
          stm32_sendfifo(&priv);
          assert(written == pause);
          size_t sent = pause * 4 < length ? pause * 4 : length;
          assert(priv.remaining == length - sent);
          assert(memcmp(source, original, length) == 0);
          uint32_t *saved = priv.buffer;
          stm32_sendfifo(&priv); /* still full: no progress or source change */
          assert(written == pause && priv.buffer == saved);
          assert(priv.remaining == length - sent);
          capacity = words;
          stm32_sendfifo(&priv);
          assert(written == words && priv.remaining == 0);
          assert(memcmp(source, original, length) == 0);
          for (size_t w = 0; w < words; ++w)
            {
              uint32_t expected = 0;
              for (size_t b = 0; b < 4 && w * 4 + b < length; ++b)
                expected |= (uint32_t)original[w * 4 + b] << (8 * b);
              assert(fifo[w] == expected);
            }
          stm32_sendfifo(&priv); /* complete: no duplicate word */
          assert(written == words);
          free(original);
          free(source);
          ++cases;
        }
    }
  printf("PASS %zu cases: exact-size source bounds, immutable bytes, FIFO pause/resume and zero-padded TX tails; host stubs, not hardware.\n", cases);
  return 0;
}
