#include "tokenizer.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

static const uint8_t jsc_hex_value[1 << 8] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static const char *jsc_token_names[] = {
    "NONE",
    "IDENTIFIER", "STRING", "NUMBER", "REGEXP", "TEMPLATE",
    "BREAK", "CASE", "CATCH", "CLASS", "CONST", "CONTINUE", "DEBUGGER", "DEFAULT",
    "DELETE", "DO", "ELSE", "EXPORT", "EXTENDS", "FINALLY", "FOR", "FUNCTION",
    "IF", "IMPORT", "IN", "INSTANCEOF", "NEW", "RETURN", "SUPER", "SWITCH",
    "THIS", "THROW", "TRY", "TYPEOF", "VAR", "VOID", "WHILE", "WITH", "YIELD",
    "AWAIT", "ASYNC", "LET", "STATIC",
    "TRUE", "FALSE", "NULL", "UNDEFINED",
    "=", "+", "-", "*", "/", "%", "++", "--", "==", "!=", "===", "!==", ">", "<",
    ">=", "<=", "&&", "||", "!", "&", "|", "^", "~", "<<", ">>", ">>>", "+=", "-=",
    "*=", "/=", "%=", "<<=", ">>=", ">>>=", "&=", "|=", "^=", "&&=", "||=", "??",
    "??=", "?.", "**", "**=", "=>",
    ";", ":", ",", ".", "?", "(", ")", "[", "]", "{", "}", "...",
    "TEMPLATE_START", "TEMPLATE_MIDDLE", "TEMPLATE_END",
    "EOF", "ERROR"};

static const struct
{
  const char *keyword;
  jsc_token_type type;
} jsc_keywords[] = {
    {"break", JSC_TOKEN_BREAK},
    {"case", JSC_TOKEN_CASE},
    {"catch", JSC_TOKEN_CATCH},
    {"class", JSC_TOKEN_CLASS},
    {"const", JSC_TOKEN_CONST},
    {"continue", JSC_TOKEN_CONTINUE},
    {"debugger", JSC_TOKEN_DEBUGGER},
    {"default", JSC_TOKEN_DEFAULT},
    {"delete", JSC_TOKEN_DELETE},
    {"do", JSC_TOKEN_DO},
    {"else", JSC_TOKEN_ELSE},
    {"export", JSC_TOKEN_EXPORT},
    {"extends", JSC_TOKEN_EXTENDS},
    {"finally", JSC_TOKEN_FINALLY},
    {"for", JSC_TOKEN_FOR},
    {"function", JSC_TOKEN_FUNCTION},
    {"if", JSC_TOKEN_IF},
    {"import", JSC_TOKEN_IMPORT},
    {"in", JSC_TOKEN_IN},
    {"instanceof", JSC_TOKEN_INSTANCEOF},
    {"new", JSC_TOKEN_NEW},
    {"return", JSC_TOKEN_RETURN},
    {"super", JSC_TOKEN_SUPER},
    {"switch", JSC_TOKEN_SWITCH},
    {"this", JSC_TOKEN_THIS},
    {"throw", JSC_TOKEN_THROW},
    {"try", JSC_TOKEN_TRY},
    {"typeof", JSC_TOKEN_TYPEOF},
    {"var", JSC_TOKEN_VAR},
    {"void", JSC_TOKEN_VOID},
    {"while", JSC_TOKEN_WHILE},
    {"with", JSC_TOKEN_WITH},
    {"yield", JSC_TOKEN_YIELD},
    {"await", JSC_TOKEN_AWAIT},
    {"async", JSC_TOKEN_ASYNC},
    {"let", JSC_TOKEN_LET},
    {"static", JSC_TOKEN_STATIC},
    {"true", JSC_TOKEN_TRUE},
    {"false", JSC_TOKEN_FALSE},
    {"null", JSC_TOKEN_NULL},
    {"undefined", JSC_TOKEN_UNDEFINED},
    {NULL, JSC_TOKEN_NONE}};

#define JSC_IS_WHITESPACE(c) ((c) == ' ' || (c) == '\t' || (c) == '\n' || (c) == '\r' || (c) == '\f' || (c) == '\v')
#define JSC_IS_LETTER(c) (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z'))
#define JSC_IS_DIGIT(c) ((c) >= '0' && (c) <= '9')
#define JSC_IS_HEX_DIGIT(c) (JSC_IS_DIGIT(c) || ((c) >= 'a' && (c) <= 'f') || ((c) >= 'A' && (c) <= 'F'))
#define JSC_IS_OCTAL_DIGIT(c) ((c) >= '0' && (c) <= '7')
#define JSC_IS_BINARY_DIGIT(c) ((c) == '0' || (c) == '1')
#define JSC_IS_IDENTIFIER_START(c) (JSC_IS_LETTER(c) || (c) == '_' || (c) == '$')
#define JSC_IS_IDENTIFIER_PART(c) (JSC_IS_IDENTIFIER_START(c) || JSC_IS_DIGIT(c))
#define JSC_IS_LINE_TERMINATOR(c) ((c) == '\n' || (c) == '\r')

static void jsc_vec_scan_identifier(jsc_tokenizer_state *state);
static void jsc_scan_string(jsc_tokenizer_state *state, char quote);
static void jsc_scan_template(jsc_tokenizer_state *state);
static void jsc_scan_number(jsc_tokenizer_state *state);
static void jsc_scan_regexp(jsc_tokenizer_state *state);

jsc_vector_level jsc_get_vector_level(void)
{
#if defined(__AVX512F__)
  return JSC_SIMD_AVX512F;
#elif defined(__AVX2__)
  return JSC_SIMD_AVX2;
#elif defined(__AVX__)
  return JSC_SIMD_AVX;
#elif defined(__SSE4_2__)
  return JSC_SIMD_SSE42;
#elif defined(__SSE4_1__)
  return JSC_SIMD_SSE41;
#elif defined(__SSSE3__)
  return JSC_SIMD_SSSE3;
#elif defined(__SSE3__)
  return JSC_SIMD_SSE3;
#elif defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
  return JSC_SIMD_SSE2;
#else
  return JSC_SIMD_NONE;
#endif
}

static void jsc_set_token_error(jsc_tokenizer_state *state, const char *message)
{
  if (state->error_message)
  {
    free(state->error_message);
  }

  state->error_message = strdup(message);
  state->current.type = JSC_TOKEN_ERROR;
}

static JSC_FORCE_INLINE void jsc_advance_position(jsc_tokenizer_state *state)
{
  if (JSC_UNLIKELY(state->position >= state->source_length))
  {
    state->eof_reached = true;
    return;
  }

  char c = state->source[state->position];
  state->position++;

  if (c == '\n')
  {
    state->line++;
    state->column = 0;
  }
  else if (c == '\r')
  {
    if (state->position < state->source_length && state->source[state->position] == '\n')
    {
      state->position++;
    }

    state->line++;
    state->column = 0;
  }
  else
  {
    state->column++;
  }
}

static JSC_FORCE_INLINE char jsc_peek(jsc_tokenizer_state *state)
{
  if (JSC_UNLIKELY(state->position >= state->source_length))
  {
    return '\0';
  }

  return state->source[state->position];
}

static JSC_FORCE_INLINE char jsc_peek_n(jsc_tokenizer_state *state, size_t n)
{
  if (JSC_UNLIKELY(state->position + n >= state->source_length))
  {
    return '\0';
  }

  return state->source[state->position + n];
}

static JSC_FORCE_INLINE bool jsc_match(jsc_tokenizer_state *state, char expected)
{
  if (jsc_peek(state) != expected)
  {
    return false;
  }

  jsc_advance_position(state);

  return true;
}

static void jsc_vec_skip_whitespace(jsc_tokenizer_state *state)
{
  size_t remaining = state->source_length - state->position;

#if defined(__AVX512F__)
  if (state->vector_level >= JSC_SIMD_AVX512F && remaining >= (1 << 6))
  {
    const __m512i spaces = _mm512_set1_epi8(' ');
    const __m512i tabs = _mm512_set1_epi8('\t');
    const __m512i newlines = _mm512_set1_epi8('\n');
    const __m512i carriage_returns = _mm512_set1_epi8('\r');
    const __m512i form_feeds = _mm512_set1_epi8('\f');
    const __m512i vertical_tabs = _mm512_set1_epi8('\v');

    while (remaining >= (1 << 6))
    {
      _mm_prefetch(state->source + state->position + JSC_PREFETCH_DISTANCE, _MM_HINT_T0);

      __m512i chunk = _mm512_loadu_si512((const __m512i *)(state->source + state->position));

      __mmask64 is_space = _mm512_cmpeq_epi8_mask(chunk, spaces);
      __mmask64 is_tab = _mm512_cmpeq_epi8_mask(chunk, tabs);
      __mmask64 is_newline = _mm512_cmpeq_epi8_mask(chunk, newlines);
      __mmask64 is_cr = _mm512_cmpeq_epi8_mask(chunk, carriage_returns);
      __mmask64 is_ff = _mm512_cmpeq_epi8_mask(chunk, form_feeds);
      __mmask64 is_vt = _mm512_cmpeq_epi8_mask(chunk, vertical_tabs);

      __mmask64 is_whitespace = is_space | is_tab | is_newline | is_cr | is_ff | is_vt;

      if (is_whitespace == 0)
      {
        break;
      }

      if (is_whitespace == UINT64_MAX)
      {
        int newline_count = _mm_popcnt_u64(_mm512_movepi8_mask(newlines));
        int cr_count = _mm_popcnt_u64(_mm512_movepi8_mask(carriage_returns));

        state->line += newline_count + cr_count;

        if (newline_count > 0 || cr_count > 0)
        {
          state->column = 0;
        }
        else
        {
          state->column += (1 << 6);
        }

        state->position += (1 << 6);
        remaining -= (1 << 6);

        continue;
      }

      int trailing_zeros = _tzcnt_u64(~is_whitespace);

      if (trailing_zeros > 0)
      {
        for (int i = 0; i < trailing_zeros; i++)
        {
          jsc_advance_position(state);
        }

        remaining = state->source_length - state->position;
      }
      else
      {
        break;
      }
    }
  }
#elif defined(__AVX2__)
  if (state->vector_level >= JSC_SIMD_AVX2 && remaining >= (1 << 5))
  {
    const __m256i spaces = _mm256_set1_epi8(' ');
    const __m256i tabs = _mm256_set1_epi8('\t');
    const __m256i newlines = _mm256_set1_epi8('\n');
    const __m256i carriage_returns = _mm256_set1_epi8('\r');
    const __m256i form_feeds = _mm256_set1_epi8('\f');
    const __m256i vertical_tabs = _mm256_set1_epi8('\v');

    while (remaining >= (1 << 5))
    {
      _mm_prefetch(state->source + state->position + JSC_PREFETCH_DISTANCE, _MM_HINT_T0);

      __m256i chunk = _mm256_loadu_si256((const __m256i *)(state->source + state->position));

      __m256i is_space = _mm256_cmpeq_epi8(chunk, spaces);
      __m256i is_tab = _mm256_cmpeq_epi8(chunk, tabs);
      __m256i is_newline = _mm256_cmpeq_epi8(chunk, newlines);
      __m256i is_cr = _mm256_cmpeq_epi8(chunk, carriage_returns);
      __m256i is_ff = _mm256_cmpeq_epi8(chunk, form_feeds);
      __m256i is_vt = _mm256_cmpeq_epi8(chunk, vertical_tabs);

      __m256i is_whitespace = _mm256_or_si256(
          _mm256_or_si256(
              _mm256_or_si256(is_space, is_tab),
              _mm256_or_si256(is_newline, is_cr)),
          _mm256_or_si256(is_ff, is_vt));

      uint32_t mask = _mm256_movemask_epi8(is_whitespace);

      if (mask == 0)
      {
        break;
      }

      if (mask == 0xFFFFFFFF)
      {
        int newline_count = _mm_popcnt_u32(_mm256_movemask_epi8(is_newline));
        int cr_count = _mm_popcnt_u32(_mm256_movemask_epi8(is_cr));

        state->line += newline_count + cr_count;

        if (newline_count > 0 || cr_count > 0)
        {
          state->column = 0;
        }
        else
        {
          state->column += (1 << 5);
        }

        state->position += (1 << 5);
        remaining -= (1 << 5);

        continue;
      }

      int trailing_zeros = __builtin_ctz(~mask);

      if (trailing_zeros > 0)
      {
        for (int i = 0; i < trailing_zeros; i++)
        {
          jsc_advance_position(state);
        }

        remaining = state->source_length - state->position;
      }
      else
      {
        break;
      }
    }
  }
#elif defined(__SSE4_2__)
  if (state->vector_level >= JSC_SIMD_SSE42 && remaining >= (1 << 4))
  {
    const __m128i spaces = _mm_set1_epi8(' ');
    const __m128i tabs = _mm_set1_epi8('\t');
    const __m128i newlines = _mm_set1_epi8('\n');
    const __m128i carriage_returns = _mm_set1_epi8('\r');
    const __m128i form_feeds = _mm_set1_epi8('\f');
    const __m128i vertical_tabs = _mm_set1_epi8('\v');

    while (remaining >= (1 << 4))
    {
      _mm_prefetch(state->source + state->position + JSC_PREFETCH_DISTANCE, _MM_HINT_T0);

      __m128i chunk = _mm_loadu_si128((const __m128i *)(state->source + state->position));

      __m128i is_space = _mm_cmpeq_epi8(chunk, spaces);
      __m128i is_tab = _mm_cmpeq_epi8(chunk, tabs);
      __m128i is_newline = _mm_cmpeq_epi8(chunk, newlines);
      __m128i is_cr = _mm_cmpeq_epi8(chunk, carriage_returns);
      __m128i is_ff = _mm_cmpeq_epi8(chunk, form_feeds);
      __m128i is_vt = _mm_cmpeq_epi8(chunk, vertical_tabs);

      __m128i is_whitespace = _mm_or_si128(
          _mm_or_si128(
              _mm_or_si128(is_space, is_tab),
              _mm_or_si128(is_newline, is_cr)),
          _mm_or_si128(is_ff, is_vt));

      uint16_t mask = _mm_movemask_epi8(is_whitespace);

      if (mask == 0)
      {
        break;
      }

      if (mask == 0xFFFF)
      {
        int newline_count = _mm_popcnt_u32(_mm_movemask_epi8(is_newline));
        int cr_count = _mm_popcnt_u32(_mm_movemask_epi8(is_cr));

        state->line += newline_count + cr_count;

        if (newline_count > 0 || cr_count > 0)
        {
          state->column = 0;
        }
        else
        {
          state->column += (1 << 4);
        }

        state->position += (1 << 4);
        remaining -= (1 << 4);

        continue;
      }

      int trailing_zeros = __builtin_ctz(~mask);

      if (trailing_zeros > 0)
      {
        for (int i = 0; i < trailing_zeros; i++)
        {
          jsc_advance_position(state);
        }
        remaining = state->source_length - state->position;
      }
      else
      {
        break;
      }
    }
  }
#elif defined(__SSE2__)
  if (state->vector_level >= JSC_SIMD_SSE2 && remaining >= (1 << 4))
  {
    const __m128i spaces = _mm_set1_epi8(' ');
    const __m128i tabs = _mm_set1_epi8('\t');
    const __m128i newlines = _mm_set1_epi8('\n');
    const __m128i carriage_returns = _mm_set1_epi8('\r');
    const __m128i form_feeds = _mm_set1_epi8('\f');
    const __m128i vertical_tabs = _mm_set1_epi8('\v');

    while (remaining >= (1 << 4))
    {
      __m128i chunk = _mm_loadu_si128((const __m128i *)(state->source + state->position));

      __m128i is_space = _mm_cmpeq_epi8(chunk, spaces);
      __m128i is_tab = _mm_cmpeq_epi8(chunk, tabs);
      __m128i is_newline = _mm_cmpeq_epi8(chunk, newlines);
      __m128i is_cr = _mm_cmpeq_epi8(chunk, carriage_returns);
      __m128i is_ff = _mm_cmpeq_epi8(chunk, form_feeds);
      __m128i is_vt = _mm_cmpeq_epi8(chunk, vertical_tabs);

      __m128i is_whitespace = _mm_or_si128(
          _mm_or_si128(
              _mm_or_si128(is_space, is_tab),
              _mm_or_si128(is_newline, is_cr)),
          _mm_or_si128(is_ff, is_vt));

      uint16_t mask = _mm_movemask_epi8(is_whitespace);

      if (mask == 0)
      {
        break;
      }

      if (mask == 0xFFFF)
      {
        uint16_t newline_mask = _mm_movemask_epi8(is_newline);
        uint16_t cr_mask = _mm_movemask_epi8(is_cr);

        int newline_count = 0;
        int cr_count = 0;

        while (newline_mask)
        {
          newline_count++;
          newline_mask &= newline_mask - 1;
        }

        while (cr_mask)
        {
          cr_count++;
          cr_mask &= cr_mask - 1;
        }

        state->line += newline_count + cr_count;

        if (newline_count > 0 || cr_count > 0)
        {
          state->column = 0;
        }
        else
        {
          state->column += (1 << 4);
        }

        state->position += (1 << 4);
        remaining -= (1 << 4);

        continue;
      }

      int i = 0;

      while ((mask & (1 << i)) && i < (1 << 4))
      {
        jsc_advance_position(state);
        i++;
      }

      remaining = state->source_length - state->position;
    }
  }
#endif

  while (state->position < state->source_length)
  {
    char c = jsc_peek(state);

    if (!JSC_IS_WHITESPACE(c))
    {
      break;
    }

    jsc_advance_position(state);
  }
}

static void jsc_vec_skip_comment(jsc_tokenizer_state *state)
{
  if (jsc_peek(state) == '/' && jsc_peek_n(state, 1) == '/')
  {
    jsc_advance_position(state);
    jsc_advance_position(state);

    size_t remaining = state->source_length - state->position;

#if defined(__AVX512F__)
    if (state->vector_level >= JSC_SIMD_AVX512F && remaining >= (1 << 6))
    {
      const __m512i newlines = _mm512_set1_epi8('\n');
      const __m512i carriage_returns = _mm512_set1_epi8('\r');

      while (remaining >= (1 << 6))
      {
        _mm_prefetch(state->source + state->position + JSC_PREFETCH_DISTANCE, _MM_HINT_T0);

        __m512i chunk = _mm512_loadu_si512((const __m512i *)(state->source + state->position));

        __mmask64 is_newline = _mm512_cmpeq_epi8_mask(chunk, newlines);
        __mmask64 is_cr = _mm512_cmpeq_epi8_mask(chunk, carriage_returns);

        __mmask64 is_line_term = is_newline | is_cr;

        if (is_line_term == 0)
        {
          state->position += (1 << 6);
          state->column += (1 << 6);
          remaining -= (1 << 6);
          continue;
        }

        int trailing_zeros = _tzcnt_u64(is_line_term);
        state->position += trailing_zeros;
        state->column += trailing_zeros;
        break;
      }
    }
#elif defined(__AVX2__)
    if (state->vector_level >= JSC_SIMD_AVX2 && remaining >= (1 << 5))
    {
      const __m256i newlines = _mm256_set1_epi8('\n');
      const __m256i carriage_returns = _mm256_set1_epi8('\r');

      while (remaining >= (1 << 5))
      {
        _mm_prefetch(state->source + state->position + JSC_PREFETCH_DISTANCE, _MM_HINT_T0);

        __m256i chunk = _mm256_loadu_si256((const __m256i *)(state->source + state->position));

        __m256i is_newline = _mm256_cmpeq_epi8(chunk, newlines);
        __m256i is_cr = _mm256_cmpeq_epi8(chunk, carriage_returns);

        __m256i is_line_term = _mm256_or_si256(is_newline, is_cr);

        uint32_t mask = _mm256_movemask_epi8(is_line_term);

        if (mask == 0)
        {
          state->position += (1 << 5);
          state->column += (1 << 5);
          remaining -= (1 << 5);
          continue;
        }

        int trailing_zeros = __builtin_ctz(mask);
        state->position += trailing_zeros;
        state->column += trailing_zeros;
        break;
      }
    }
#elif defined(__SSE4_2__)
    if (state->vector_level >= JSC_SIMD_SSE42 && remaining >= (1 << 4))
    {
      const __m128i newlines = _mm_set1_epi8('\n');
      const __m128i carriage_returns = _mm_set1_epi8('\r');

      while (remaining >= (1 << 4))
      {
        _mm_prefetch(state->source + state->position + JSC_PREFETCH_DISTANCE, _MM_HINT_T0);

        __m128i chunk = _mm_loadu_si128((const __m128i *)(state->source + state->position));

        __m128i is_newline = _mm_cmpeq_epi8(chunk, newlines);
        __m128i is_cr = _mm_cmpeq_epi8(chunk, carriage_returns);

        __m128i is_line_term = _mm_or_si128(is_newline, is_cr);

        uint16_t mask = _mm_movemask_epi8(is_line_term);

        if (mask == 0)
        {
          state->position += (1 << 4);
          state->column += (1 << 4);
          remaining -= (1 << 4);
          continue;
        }

        int trailing_zeros = __builtin_ctz(mask);
        state->position += trailing_zeros;
        state->column += trailing_zeros;
        break;
      }
    }
#elif defined(__SSE2__)
    if (state->vector_level >= JSC_SIMD_SSE2 && remaining >= (1 << 4))
    {
      const __m128i newlines = _mm_set1_epi8('\n');
      const __m128i carriage_returns = _mm_set1_epi8('\r');

      while (remaining >= (1 << 4))
      {
        __m128i chunk = _mm_loadu_si128((const __m128i *)(state->source + state->position));

        __m128i is_newline = _mm_cmpeq_epi8(chunk, newlines);
        __m128i is_cr = _mm_cmpeq_epi8(chunk, carriage_returns);

        __m128i is_line_term = _mm_or_si128(is_newline, is_cr);

        uint16_t mask = _mm_movemask_epi8(is_line_term);

        if (mask == 0)
        {
          state->position += (1 << 4);
          state->column += (1 << 4);
          remaining -= (1 << 4);
          continue;
        }

        int i = 0;

        while (i < (1 << 4) && !(mask & (1 << i)))
        {
          i++;
        }

        state->position += i;
        state->column += i;
        break;
      }
    }
#endif

    while (state->position < state->source_length)
    {
      char c = jsc_peek(state);

      if (JSC_IS_LINE_TERMINATOR(c))
      {
        break;
      }

      jsc_advance_position(state);
    }
  }
  else if (jsc_peek(state) == '/' && jsc_peek_n(state, 1) == '*')
  {
    jsc_advance_position(state);
    jsc_advance_position(state);

    size_t remaining = state->source_length - state->position;
    char last_char = '\0';

#if defined(__AVX512F__)
    if (state->vector_level >= JSC_SIMD_AVX512F && remaining >= (1 << 6))
    {
      const __m512i stars = _mm512_set1_epi8('*');
      const __m512i slashes = _mm512_set1_epi8('/');

      while (remaining >= (1 << 6))
      {
        _mm_prefetch(state->source + state->position + JSC_PREFETCH_DISTANCE, _MM_HINT_T0);

        __m512i chunk = _mm512_loadu_si512((const __m512i *)(state->source + state->position));

        __mmask64 is_star = _mm512_cmpeq_epi8_mask(chunk, stars);

        if (is_star == 0)
        {
          last_char = '\0';
          uint32_t newline_count = _mm512_movepi8_mask(_mm512_cmpeq_epi8(chunk, _mm512_set1_epi8('\n')));
          uint32_t cr_count = _mm512_movepi8_mask(_mm512_cmpeq_epi8(chunk, _mm512_set1_epi8('\r')));

          state->line += _mm_popcnt_u32(newline_count) + _mm_popcnt_u32(cr_count);
          state->position += (1 << 6);
          remaining -= (1 << 6);
          continue;
        }

        for (int i = 0; i < (1 << 6); i++)
        {
          char c = state->source[state->position];
          jsc_advance_position(state);

          if (c == '*' && jsc_peek(state) == '/')
          {
            jsc_advance_position(state);
            return;
          }
        }

        remaining = state->source_length - state->position;
      }
    }
#elif defined(__AVX2__)
    if (state->vector_level >= JSC_SIMD_AVX2 && remaining >= (1 << 5))
    {
      const __m256i stars = _mm256_set1_epi8('*');
      const __m256i slashes = _mm256_set1_epi8('/');

      while (remaining >= (1 << 5))
      {
        _mm_prefetch(state->source + state->position + JSC_PREFETCH_DISTANCE, _MM_HINT_T0);

        __m256i chunk = _mm256_loadu_si256((const __m256i *)(state->source + state->position));

        __m256i is_star = _mm256_cmpeq_epi8(chunk, stars);

        uint32_t star_mask = _mm256_movemask_epi8(is_star);

        if (star_mask == 0)
        {
          last_char = '\0';

          __m256i newlines = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('\n'));
          __m256i crs = _mm256_cmpeq_epi8(chunk, _mm256_set1_epi8('\r'));

          uint32_t newline_mask = _mm256_movemask_epi8(newlines);
          uint32_t cr_mask = _mm256_movemask_epi8(crs);

          int newline_count = _mm_popcnt_u32(newline_mask);
          int cr_count = _mm_popcnt_u32(cr_mask);

          state->line += newline_count + cr_count;
          state->position += (1 << 5);
          remaining -= (1 << 5);
          continue;
        }

        for (int i = 0; i < (1 << 5); i++)
        {
          char c = state->source[state->position];
          jsc_advance_position(state);

          if (c == '*' && jsc_peek(state) == '/')
          {
            jsc_advance_position(state);
            return;
          }
        }

        remaining = state->source_length - state->position;
      }
    }
#elif defined(__SSE4_2__)
    if (state->vector_level >= JSC_SIMD_SSE42 && remaining >= (1 << 4))
    {
      const __m128i stars = _mm_set1_epi8('*');
      const __m128i slashes = _mm_set1_epi8('/');

      while (remaining >= (1 << 4))
      {
        _mm_prefetch(state->source + state->position + JSC_PREFETCH_DISTANCE, _MM_HINT_T0);

        __m128i chunk = _mm_loadu_si128((const __m128i *)(state->source + state->position));

        __m128i is_star = _mm_cmpeq_epi8(chunk, stars);

        uint16_t star_mask = _mm_movemask_epi8(is_star);

        if (star_mask == 0)
        {
          last_char = '\0';

          __m128i newlines = _mm_cmpeq_epi8(chunk, _mm_set1_epi8('\n'));
          __m128i crs = _mm_cmpeq_epi8(chunk, _mm_set1_epi8('\r'));

          uint16_t newline_mask = _mm_movemask_epi8(newlines);
          uint16_t cr_mask = _mm_movemask_epi8(crs);

          int newline_count = _mm_popcnt_u32(newline_mask);
          int cr_count = _mm_popcnt_u32(cr_mask);

          state->line += newline_count + cr_count;
          state->position += (1 << 4);
          remaining -= (1 << 4);
          continue;
        }

        for (int i = 0; i < (1 << 4); i++)
        {
          char c = state->source[state->position];
          jsc_advance_position(state);

          if (c == '*' && jsc_peek(state) == '/')
          {
            jsc_advance_position(state);
            return;
          }
        }

        remaining = state->source_length - state->position;
      }
    }
#endif

    while (state->position < state->source_length)
    {
      char c = jsc_peek(state);
      jsc_advance_position(state);

      if (last_char == '*' && c == '/')
      {
        break;
      }

      last_char = c;
    }
  }
}

static void jsc_skip_whitespace_and_comments(jsc_tokenizer_state *state)
{
  bool skipped_something;

  do
  {
    size_t old_position = state->position;

    jsc_vec_skip_whitespace(state);

    if (jsc_peek(state) == '/' && (jsc_peek_n(state, 1) == '/' || jsc_peek_n(state, 1) == '*'))
    {
      jsc_vec_skip_comment(state);
    }

    skipped_something = old_position != state->position;
  } while (skipped_something);
}

static bool jsc_check_keyword(const char *str, size_t len, jsc_token *token)
{
  for (int i = 0; jsc_keywords[i].keyword != NULL; i++)
  {
    if (len == strlen(jsc_keywords[i].keyword) &&
        memcmp(str, jsc_keywords[i].keyword, len) == 0)
    {
      token->type = jsc_keywords[i].type;
      return true;
    }
  }

  return false;
}

static void jsc_scan_template(jsc_tokenizer_state *state)
{
  jsc_token *token = &state->current;
  token->start = state->source + state->position - 1;
  token->line = state->line;
  token->column = state->column - 1;

  size_t string_length = 0;

  if (state->in_template)
  {
    token->type = JSC_TOKEN_TEMPLATE_MIDDLE;
  }
  else
  {
    token->type = JSC_TOKEN_TEMPLATE_START;
    state->in_template = true;
    state->template_depth = 0;
  }

  while (state->position < state->source_length)
  {
    char c = jsc_peek(state);

    if (c == '\0')
    {
      jsc_set_token_error(state, "Unterminated template literal");
      return;
    }

    jsc_advance_position(state);

    if (c == '`')
    {
      if (token->type == JSC_TOKEN_TEMPLATE_START)
      {
        state->string_buffer[string_length] = '\0';
        token->string_value.data = malloc(string_length + 1);
        memcpy(token->string_value.data, state->string_buffer, string_length + 1);
        token->string_value.length = string_length;

        token->type = JSC_TOKEN_STRING;
        state->in_template = false;
      }
      else
      {
        state->string_buffer[string_length] = '\0';
        token->string_value.data = malloc(string_length + 1);
        memcpy(token->string_value.data, state->string_buffer, string_length + 1);
        token->string_value.length = string_length;

        token->type = JSC_TOKEN_TEMPLATE_END;
        state->in_template = false;
      }
      break;
    }

    if (c == '$' && jsc_peek(state) == '{')
    {
      jsc_advance_position(state);

      state->string_buffer[string_length] = '\0';
      token->string_value.data = malloc(string_length + 1);
      memcpy(token->string_value.data, state->string_buffer, string_length + 1);
      token->string_value.length = string_length;

      state->template_depth++;
      state->template_brace_depth++;
      break;
    }

    if (c == '\\')
    {
      if (state->position >= state->source_length)
      {
        jsc_set_token_error(state, "Unterminated template literal");
        return;
      }

      char escape = jsc_peek(state);
      jsc_advance_position(state);

      switch (escape)
      {
      case '\'':
      case '"':
      case '\\':
      case '`':
      case '$':
        state->string_buffer[string_length++] = escape;
        break;
      case 'b':
        state->string_buffer[string_length++] = '\b';
        break;
      case 'f':
        state->string_buffer[string_length++] = '\f';
        break;
      case 'n':
        state->string_buffer[string_length++] = '\n';
        break;
      case 'r':
        state->string_buffer[string_length++] = '\r';
        break;
      case 't':
        state->string_buffer[string_length++] = '\t';
        break;
      case 'v':
        state->string_buffer[string_length++] = '\v';
        break;
      case 'u':
      {
        if (state->position + 3 >= state->source_length)
        {
          jsc_set_token_error(state, "Invalid Unicode escape sequence");
          return;
        }

        uint32_t hex_value = 0;

        for (int i = 0; i < 4; i++)
        {
          char hex = jsc_peek(state);
          jsc_advance_position(state);

          if (!JSC_IS_HEX_DIGIT(hex))
          {
            jsc_set_token_error(state, "Invalid Unicode escape sequence");
            return;
          }

          hex_value = (hex_value << 4) | jsc_hex_value[(unsigned char)hex];
        }

        if (hex_value < 0x80)
        {
          state->string_buffer[string_length++] = (char)hex_value;
        }
        else if (hex_value < 0x800)
        {
          state->string_buffer[string_length++] = (char)(0xC0 | (hex_value >> 6));
          state->string_buffer[string_length++] = (char)(0x80 | (hex_value & 0x3F));
        }
        else
        {
          state->string_buffer[string_length++] = (char)(0xE0 | (hex_value >> 12));
          state->string_buffer[string_length++] = (char)(0x80 | ((hex_value >> 6) & 0x3F));
          state->string_buffer[string_length++] = (char)(0x80 | (hex_value & 0x3F));
        }
        break;
      }
      case 'x':
      {
        if (state->position + 1 >= state->source_length)
        {
          jsc_set_token_error(state, "Invalid hex escape sequence");
          return;
        }

        char hex1 = jsc_peek(state);
        jsc_advance_position(state);
        char hex2 = jsc_peek(state);
        jsc_advance_position(state);

        if (!JSC_IS_HEX_DIGIT(hex1) || !JSC_IS_HEX_DIGIT(hex2))
        {
          jsc_set_token_error(state, "Invalid hex escape sequence");
          return;
        }

        uint8_t hex_value = (jsc_hex_value[(unsigned char)hex1] << 4) | jsc_hex_value[(unsigned char)hex2];
        state->string_buffer[string_length++] = (char)hex_value;
        break;
      }
      default:
        state->string_buffer[string_length++] = escape;
        break;
      }
    }
    else
    {
      state->string_buffer[string_length++] = c;
    }

    if (string_length >= JSC_MAX_STRING_LENGTH - 4)
    {
      jsc_set_token_error(state, "Template literal too long");
      return;
    }
  }

  token->length = state->position - (token->start - state->source);
}

static void jsc_scan_regexp(jsc_tokenizer_state *state)
{
  jsc_token *token = &state->current;
  token->type = JSC_TOKEN_REGEXP;
  token->start = state->source + state->position - 1;
  token->line = state->line;
  token->column = state->column - 1;

  size_t pattern_length = 0;
  size_t flags_length = 0;

  while (state->position < state->source_length)
  {
    char c = jsc_peek(state);

    if (c == '\0' || JSC_IS_LINE_TERMINATOR(c))
    {
      jsc_set_token_error(state, "Unterminated regular expression literal");
      return;
    }

    jsc_advance_position(state);

    if (c == '/')
    {
      break;
    }

    if (c == '\\')
    {
      if (state->position >= state->source_length)
      {
        jsc_set_token_error(state, "Unterminated regular expression literal");
        return;
      }

      state->regexp_buffer[pattern_length++] = c;
      c = jsc_peek(state);
      jsc_advance_position(state);
    }

    if (pattern_length >= JSC_MAX_REGEXP_LENGTH - 2)
    {
      jsc_set_token_error(state, "Regular expression literal too long");
      return;
    }

    state->regexp_buffer[pattern_length++] = c;
  }

  while (state->position < state->source_length)
  {
    char c = jsc_peek(state);

    if (!JSC_IS_IDENTIFIER_PART(c))
    {
      break;
    }

    if (flags_length >= 63)
    {
      jsc_set_token_error(state, "Regular expression flags too long");
      return;
    }

    jsc_advance_position(state);
    state->regexp_flags_buffer[flags_length++] = c;
  }

  token->length = state->position - (token->start - state->source);

  state->regexp_buffer[pattern_length] = '\0';
  token->regexp_value.data = malloc(pattern_length + 1);
  memcpy(token->regexp_value.data, state->regexp_buffer, pattern_length + 1);
  token->regexp_value.length = pattern_length;

  state->regexp_flags_buffer[flags_length] = '\0';
  token->regexp_value.flags = malloc(flags_length + 1);
  memcpy(token->regexp_value.flags, state->regexp_flags_buffer, flags_length + 1);
  token->regexp_value.flags_length = flags_length;
}

static void jsc_scan_number(jsc_tokenizer_state *state)
{
  jsc_token *token = &state->current;
  token->type = JSC_TOKEN_NUMBER;
  token->start = state->source + state->position - 1;
  token->line = state->line;
  token->column = state->column - 1;

  size_t number_length = 0;
  bool is_hex = false;
  bool is_binary = false;
  bool is_octal = false;

  char first_digit = *(token->start);
  state->number_buffer[number_length++] = first_digit;

  if (first_digit == '0' && state->position < state->source_length)
  {
    char next = jsc_peek(state);

    if (next == 'x' || next == 'X')
    {
      is_hex = true;
      jsc_advance_position(state);
      state->number_buffer[number_length++] = next;

#if defined(__AVX512F__)
      if (state->vector_level >= JSC_SIMD_AVX512F && state->position + (1 << 6) <= state->source_length)
      {
        __m512i zero = _mm512_set1_epi8('0');
        __m512i nine = _mm512_set1_epi8('9');
        __m512i a_lower = _mm512_set1_epi8('a');
        __m512i f_lower = _mm512_set1_epi8('f');
        __m512i a_upper = _mm512_set1_epi8('A');
        __m512i f_upper = _mm512_set1_epi8('F');

        __m512i chunk = _mm512_loadu_si512((const __m512i *)(state->source + state->position));

        __mmask64 is_digit = _mm512_kand(
            _mm512_cmpge_epi8_mask(chunk, zero),
            _mm512_cmple_epi8_mask(chunk, nine));

        __mmask64 is_lower_hex = _mm512_kand(
            _mm512_cmpge_epi8_mask(chunk, a_lower),
            _mm512_cmple_epi8_mask(chunk, f_lower));

        __mmask64 is_upper_hex = _mm512_kand(
            _mm512_cmpge_epi8_mask(chunk, a_upper),
            _mm512_cmple_epi8_mask(chunk, f_upper));

        __mmask64 is_hex_digit = _mm512_kor(_mm512_kor(is_digit, is_lower_hex), is_upper_hex);

        if (is_hex_digit)
        {
          int trailing_zeros = _tzcnt_u64(~is_hex_digit);

          if (trailing_zeros > 0)
          {
            if (number_length + trailing_zeros < JSC_MAX_NUMBER_LENGTH)
            {
              for (int i = 0; i < trailing_zeros; i++)
              {
                state->number_buffer[number_length++] = state->source[state->position + i];
              }

              state->position += trailing_zeros;
              state->column += trailing_zeros;
            }
          }
        }
      }
#elif defined(__AVX2__)
      if (state->vector_level >= JSC_SIMD_AVX2 && state->position + (1 << 5) <= state->source_length)
      {
        __m256i zero = _mm256_set1_epi8('0');
        __m256i nine = _mm256_set1_epi8('9');
        __m256i a_lower = _mm256_set1_epi8('a');
        __m256i f_lower = _mm256_set1_epi8('f');
        __m256i a_upper = _mm256_set1_epi8('A');
        __m256i f_upper = _mm256_set1_epi8('F');

        __m256i chunk = _mm256_loadu_si256((const __m256i *)(state->source + state->position));

        __m256i is_digit = _mm256_and_si256(
            _mm256_cmpgt_epi8(chunk, _mm256_sub_epi8(zero, _mm256_set1_epi8(1))),
            _mm256_cmpgt_epi8(_mm256_add_epi8(nine, _mm256_set1_epi8(1)), chunk));

        __m256i is_lower_hex = _mm256_and_si256(
            _mm256_cmpgt_epi8(chunk, _mm256_sub_epi8(a_lower, _mm256_set1_epi8(1))),
            _mm256_cmpgt_epi8(_mm256_add_epi8(f_lower, _mm256_set1_epi8(1)), chunk));

        __m256i is_upper_hex = _mm256_and_si256(
            _mm256_cmpgt_epi8(chunk, _mm256_sub_epi8(a_upper, _mm256_set1_epi8(1))),
            _mm256_cmpgt_epi8(_mm256_add_epi8(f_upper, _mm256_set1_epi8(1)), chunk));

        __m256i is_hex_digit = _mm256_or_si256(_mm256_or_si256(is_digit, is_lower_hex), is_upper_hex);

        uint32_t mask = _mm256_movemask_epi8(is_hex_digit);

        if (mask != 0)
        {
          int trailing_zeros = __builtin_ctz(~mask);

          if (trailing_zeros > 0)
          {
            if (number_length + trailing_zeros < JSC_MAX_NUMBER_LENGTH)
            {
              for (int i = 0; i < trailing_zeros; i++)
              {
                state->number_buffer[number_length++] = state->source[state->position + i];
              }

              state->position += trailing_zeros;
              state->column += trailing_zeros;
            }
          }
        }
      }
#endif

      while (state->position < state->source_length)
      {
        char c = jsc_peek(state);

        if (!JSC_IS_HEX_DIGIT(c))
        {
          break;
        }

        jsc_advance_position(state);

        if (number_length < JSC_MAX_NUMBER_LENGTH)
        {
          state->number_buffer[number_length++] = c;
        }
      }
    }
    else if (next == 'b' || next == 'B')
    {
      is_binary = true;
      jsc_advance_position(state);
      state->number_buffer[number_length++] = next;

      while (state->position < state->source_length)
      {
        char c = jsc_peek(state);

        if (!JSC_IS_BINARY_DIGIT(c))
        {
          break;
        }

        jsc_advance_position(state);

        if (number_length < JSC_MAX_NUMBER_LENGTH)
        {
          state->number_buffer[number_length++] = c;
        }
      }
    }
    else if (next == 'o' || next == 'O')
    {
      is_octal = true;
      jsc_advance_position(state);
      state->number_buffer[number_length++] = next;

      while (state->position < state->source_length)
      {
        char c = jsc_peek(state);

        if (!JSC_IS_OCTAL_DIGIT(c))
        {
          break;
        }

        jsc_advance_position(state);

        if (number_length < JSC_MAX_NUMBER_LENGTH)
        {
          state->number_buffer[number_length++] = c;
        }
      }
    }
    else if (JSC_IS_DIGIT(next))
    {
      is_octal = true;

      while (state->position < state->source_length)
      {
        char c = jsc_peek(state);

        if (!JSC_IS_DIGIT(c))
        {
          if (c == '.' || c == 'e' || c == 'E')
          {
            is_octal = false;
            break;
          }

          if (!JSC_IS_OCTAL_DIGIT(c))
          {
            break;
          }
        }

        jsc_advance_position(state);

        if (number_length < JSC_MAX_NUMBER_LENGTH)
        {
          state->number_buffer[number_length++] = c;
        }
      }
    }
  }

  if (!is_hex && !is_binary && !is_octal)
  {
#if defined(__AVX512F__)
    if (state->vector_level >= JSC_SIMD_AVX512F && state->position + (1 << 6) <= state->source_length)
    {
      __m512i zero = _mm512_set1_epi8('0');
      __m512i nine = _mm512_set1_epi8('9');

      __m512i chunk = _mm512_loadu_si512((const __m512i *)(state->source + state->position));

      __mmask64 is_digit = _mm512_kand(
          _mm512_cmpge_epi8_mask(chunk, zero),
          _mm512_cmple_epi8_mask(chunk, nine));

      if (is_digit)
      {
        int trailing_zeros = _tzcnt_u64(~is_digit);

        if (trailing_zeros > 0)
        {
          if (number_length + trailing_zeros < JSC_MAX_NUMBER_LENGTH)
          {
            for (int i = 0; i < trailing_zeros; i++)
            {
              state->number_buffer[number_length++] = state->source[state->position + i];
            }

            state->position += trailing_zeros;
            state->column += trailing_zeros;
          }
        }
      }
    }
#elif defined(__AVX2__)
    if (state->vector_level >= JSC_SIMD_AVX2 && state->position + (1 << 5) <= state->source_length)
    {
      __m256i zero = _mm256_set1_epi8('0');
      __m256i nine = _mm256_set1_epi8('9');

      __m256i chunk = _mm256_loadu_si256((const __m256i *)(state->source + state->position));

      __m256i is_digit = _mm256_and_si256(
          _mm256_cmpgt_epi8(chunk, _mm256_sub_epi8(zero, _mm256_set1_epi8(1))),
          _mm256_cmpgt_epi8(_mm256_add_epi8(nine, _mm256_set1_epi8(1)), chunk));

      uint32_t mask = _mm256_movemask_epi8(is_digit);

      if (mask != 0)
      {
        int trailing_zeros = __builtin_ctz(~mask);

        if (trailing_zeros > 0)
        {
          if (number_length + trailing_zeros < JSC_MAX_NUMBER_LENGTH)
          {
            for (int i = 0; i < trailing_zeros; i++)
            {
              state->number_buffer[number_length++] = state->source[state->position + i];
            }

            state->position += trailing_zeros;
            state->column += trailing_zeros;
          }
        }
      }
    }
#endif

    while (state->position < state->source_length)
    {
      char c = jsc_peek(state);

      if (!JSC_IS_DIGIT(c))
      {
        break;
      }

      jsc_advance_position(state);

      if (number_length < JSC_MAX_NUMBER_LENGTH)
      {
        state->number_buffer[number_length++] = c;
      }
    }

    if (jsc_peek(state) == '.')
    {
      jsc_advance_position(state);

      if (number_length < JSC_MAX_NUMBER_LENGTH)
      {
        state->number_buffer[number_length++] = '.';
      }

#if defined(__AVX512F__)
      if (state->vector_level >= JSC_SIMD_AVX512F && state->position + (1 << 6) <= state->source_length)
      {
        __m512i zero = _mm512_set1_epi8('0');
        __m512i nine = _mm512_set1_epi8('9');

        __m512i chunk = _mm512_loadu_si512((const __m512i *)(state->source + state->position));

        __mmask64 is_digit = _mm512_kand(
            _mm512_cmpge_epi8_mask(chunk, zero),
            _mm512_cmple_epi8_mask(chunk, nine));

        if (is_digit)
        {
          int trailing_zeros = _tzcnt_u64(~is_digit);

          if (trailing_zeros > 0)
          {
            if (number_length + trailing_zeros < JSC_MAX_NUMBER_LENGTH)
            {
              for (int i = 0; i < trailing_zeros; i++)
              {
                state->number_buffer[number_length++] = state->source[state->position + i];
              }

              state->position += trailing_zeros;
              state->column += trailing_zeros;
            }
          }
        }
      }
#elif defined(__AVX2__)
      if (state->vector_level >= JSC_SIMD_AVX2 && state->position + (1 << 5) <= state->source_length)
      {
        __m256i zero = _mm256_set1_epi8('0');
        __m256i nine = _mm256_set1_epi8('9');

        __m256i chunk = _mm256_loadu_si256((const __m256i *)(state->source + state->position));

        __m256i is_digit = _mm256_and_si256(
            _mm256_cmpgt_epi8(chunk, _mm256_sub_epi8(zero, _mm256_set1_epi8(1))),
            _mm256_cmpgt_epi8(_mm256_add_epi8(nine, _mm256_set1_epi8(1)), chunk));

        uint32_t mask = _mm256_movemask_epi8(is_digit);

        if (mask != 0)
        {
          int trailing_zeros = __builtin_ctz(~mask);

          if (trailing_zeros > 0)
          {
            if (number_length + trailing_zeros < JSC_MAX_NUMBER_LENGTH)
            {
              for (int i = 0; i < trailing_zeros; i++)
              {
                state->number_buffer[number_length++] = state->source[state->position + i];
              }

              state->position += trailing_zeros;
              state->column += trailing_zeros;
            }
          }
        }
      }
#endif

      while (state->position < state->source_length)
      {
        char c = jsc_peek(state);

        if (!JSC_IS_DIGIT(c))
        {
          break;
        }

        jsc_advance_position(state);

        if (number_length < JSC_MAX_NUMBER_LENGTH)
        {
          state->number_buffer[number_length++] = c;
        }
      }
    }

    if (state->position < state->source_length)
    {
      char e = jsc_peek(state);

      if (e == 'e' || e == 'E')
      {
        jsc_advance_position(state);

        if (number_length < JSC_MAX_NUMBER_LENGTH)
        {
          state->number_buffer[number_length++] = e;
        }

        if (state->position < state->source_length)
        {
          char sign = jsc_peek(state);

          if (sign == '+' || sign == '-')
          {
            jsc_advance_position(state);

            if (number_length < JSC_MAX_NUMBER_LENGTH)
            {
              state->number_buffer[number_length++] = sign;
            }
          }
        }

        bool has_exponent = false;

        while (state->position < state->source_length)
        {
          char c = jsc_peek(state);

          if (!JSC_IS_DIGIT(c))
          {
            break;
          }

          has_exponent = true;
          jsc_advance_position(state);

          if (number_length < JSC_MAX_NUMBER_LENGTH)
          {
            state->number_buffer[number_length++] = c;
          }
        }

        if (!has_exponent)
        {
          jsc_set_token_error(state, "Invalid number literal: missing exponent");
          return;
        }
      }
    }
  }

  state->number_buffer[number_length] = '\0';

  if (is_hex)
  {
    token->number_value = strtol(state->number_buffer + 2, NULL, (1 << 4));
  }
  else if (is_binary)
  {
    token->number_value = strtol(state->number_buffer + 2, NULL, 2);
  }
  else if (is_octal && state->number_buffer[0] == '0' && state->number_buffer[1] >= '0' && state->number_buffer[1] <= '7')
  {
    token->number_value = strtol(state->number_buffer, NULL, 8);
  }
  else
  {
    token->number_value = atof(state->number_buffer);
  }

  token->length = state->position - (token->start - state->source);
}

jsc_tokenizer_state *jsc_tokenizer_init(const char *source, size_t length)
{
  jsc_tokenizer_state *state = (jsc_tokenizer_state *)malloc(sizeof(jsc_tokenizer_state));

  if (!state)
  {
    return NULL;
  }

  memset(state, 0, sizeof(jsc_tokenizer_state));

  state->source = source;
  state->source_length = length;
  state->position = 0;
  state->line = 1;
  state->column = 0;

  state->in_template = false;
  state->template_depth = 0;
  state->template_brace_depth = 0;

  state->error_message = NULL;

  state->vector_level = jsc_get_vector_level();
  state->eof_reached = false;

  return state;
}

jsc_token jsc_next_token(jsc_tokenizer_state *state)
{
  jsc_token token;
  memset(&token, 0, sizeof(jsc_token));

  if (state->eof_reached)
  {
    token.type = JSC_TOKEN_EOF;
    return token;
  }

  while (state->position < state->source_length)
  {
    char c = state->source[state->position];

    token.start = state->source + state->position;
    token.line = state->line;
    token.column = state->column;

    if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
    {
      if (c == '\n')
      {
        state->line++;
        state->column = 0;
      }
      else
      {
        state->column++;
      }

      state->position++;

      continue;
    }

    if (c == '/' && state->position + 1 < state->source_length)
    {
      if (state->source[state->position + 1] == '/')
      {
        state->position += 2;
        state->column += 2;

        while (state->position < state->source_length)
        {
          if (state->source[state->position] == '\n')
          {
            break;
          }
          state->position++;
          state->column++;
        }
        continue;
      }
      else if (state->source[state->position + 1] == '*')
      {
        state->position += 2;
        state->column += 2;

        while (state->position + 1 < state->source_length)
        {
          if (state->source[state->position] == '*' &&
              state->source[state->position + 1] == '/')
          {
            state->position += 2;
            state->column += 2;
            break;
          }

          if (state->source[state->position] == '\n')
          {
            state->line++;
            state->column = 0;
          }
          else
          {
            state->column++;
          }

          state->position++;
        }

        continue;
      }
    }

    state->position++;
    state->column++;

    if (c == '{')
    {
      token.type = JSC_TOKEN_LEFT_BRACE;
    }
    else if (c == '}')
    {
      token.type = JSC_TOKEN_RIGHT_BRACE;
    }
    else if (c == '(')
    {
      token.type = JSC_TOKEN_LEFT_PAREN;
    }
    else if (c == ')')
    {
      token.type = JSC_TOKEN_RIGHT_PAREN;
    }
    else if (c == '[')
    {
      token.type = JSC_TOKEN_LEFT_BRACKET;
    }
    else if (c == ']')
    {
      token.type = JSC_TOKEN_RIGHT_BRACKET;
    }
    else if (c == ';')
    {
      token.type = JSC_TOKEN_SEMICOLON;
    }
    else if (c == ':')
    {
      token.type = JSC_TOKEN_COLON;
    }
    else if (c == ',')
    {
      token.type = JSC_TOKEN_COMMA;
    }
    else if (c == '%')
    {
      token.type = JSC_TOKEN_MODULO;
    }
    else if (c == '~')
    {
      token.type = JSC_TOKEN_BITWISE_NOT;
    }
    else if (c == '^')
    {
      token.type = JSC_TOKEN_BITWISE_XOR;
    }
    else if (c == '&')
    {
      if (state->position < state->source_length && state->source[state->position] == '&')
      {
        state->position++;
        state->column++;
        token.type = JSC_TOKEN_LOGICAL_AND;
      }
      else
      {
        token.type = JSC_TOKEN_BITWISE_AND;
      }
    }
    else if (c == '|')
    {
      if (state->position < state->source_length && state->source[state->position] == '|')
      {
        state->position++;
        state->column++;
        token.type = JSC_TOKEN_LOGICAL_OR;
      }
      else
      {
        token.type = JSC_TOKEN_BITWISE_OR;
      }
    }
    else if (c == '!')
    {
      if (state->position < state->source_length && state->source[state->position] == '=')
      {
        state->position++;
        state->column++;

        if (state->position < state->source_length && state->source[state->position] == '=')
        {
          state->position++;
          state->column++;
          token.type = JSC_TOKEN_STRICT_NOT_EQUAL;
        }
        else
        {
          token.type = JSC_TOKEN_NOT_EQUAL;
        }
      }
      else
      {
        token.type = JSC_TOKEN_LOGICAL_NOT;
      }
    }
    else if (c == '?')
    {
      if (state->position < state->source_length)
      {
        if (state->source[state->position] == '?')
        {
          state->position++;
          state->column++;

          if (state->position < state->source_length && state->source[state->position] == '=')
          {
            state->position++;
            state->column++;
            token.type = JSC_TOKEN_NULLISH_COALESCING_ASSIGN;
          }
          else
          {
            token.type = JSC_TOKEN_NULLISH_COALESCING;
          }
        }
        else if (state->source[state->position] == '.')
        {
          state->position++;
          state->column++;
          token.type = JSC_TOKEN_OPTIONAL_CHAINING;
        }
        else
        {
          token.type = JSC_TOKEN_QUESTION_MARK;
        }
      }
      else
      {
        token.type = JSC_TOKEN_QUESTION_MARK;
      }
    }
    else if (c == '.')
    {
      if (state->position + 1 < state->source_length &&
          state->source[state->position] == '.' &&
          state->source[state->position + 1] == '.')
      {
        state->position += 2;
        state->column += 2;
        token.type = JSC_TOKEN_SPREAD;
      }
      else
      {
        token.type = JSC_TOKEN_PERIOD;
      }
    }
    else if (c == '+')
    {
      if (state->position < state->source_length)
      {
        if (state->source[state->position] == '+')
        {
          state->position++;
          state->column++;
          token.type = JSC_TOKEN_INCREMENT;
        }
        else if (state->source[state->position] == '=')
        {
          state->position++;
          state->column++;
          token.type = JSC_TOKEN_PLUS_ASSIGN;
        }
        else
        {
          token.type = JSC_TOKEN_PLUS;
        }
      }
      else
      {
        token.type = JSC_TOKEN_PLUS;
      }
    }
    else if (c == '-')
    {
      if (state->position < state->source_length)
      {
        if (state->source[state->position] == '-')
        {
          state->position++;
          state->column++;
          token.type = JSC_TOKEN_DECREMENT;
        }
        else if (state->source[state->position] == '=')
        {
          state->position++;
          state->column++;
          token.type = JSC_TOKEN_MINUS_ASSIGN;
        }
        else
        {
          token.type = JSC_TOKEN_MINUS;
        }
      }
      else
      {
        token.type = JSC_TOKEN_MINUS;
      }
    }
    else if (c == '*')
    {
      if (state->position < state->source_length)
      {
        if (state->source[state->position] == '*')
        {
          state->position++;
          state->column++;

          if (state->position < state->source_length && state->source[state->position] == '=')
          {
            state->position++;
            state->column++;
            token.type = JSC_TOKEN_EXPONENTIATION_ASSIGN;
          }
          else
          {
            token.type = JSC_TOKEN_EXPONENTIATION;
          }
        }
        else if (state->source[state->position] == '=')
        {
          state->position++;
          state->column++;
          token.type = JSC_TOKEN_MULTIPLY_ASSIGN;
        }
        else
        {
          token.type = JSC_TOKEN_MULTIPLY;
        }
      }
      else
      {
        token.type = JSC_TOKEN_MULTIPLY;
      }
    }
    else if (c == '/')
    {
      if (state->position < state->source_length && state->source[state->position] == '=')
      {
        state->position++;
        state->column++;
        token.type = JSC_TOKEN_DIVIDE_ASSIGN;
      }
      else
      {
        token.type = JSC_TOKEN_DIVIDE;
      }
    }
    else if (c == '=')
    {
      if (state->position < state->source_length)
      {
        if (state->source[state->position] == '=')
        {
          state->position++;
          state->column++;
          if (state->position < state->source_length && state->source[state->position] == '=')
          {
            state->position++;
            state->column++;
            token.type = JSC_TOKEN_STRICT_EQUAL;
          }
          else
          {
            token.type = JSC_TOKEN_EQUAL;
          }
        }
        else if (state->source[state->position] == '>')
        {
          state->position++;
          state->column++;
          token.type = JSC_TOKEN_ARROW;
        }
        else
        {
          token.type = JSC_TOKEN_ASSIGN;
        }
      }
      else
      {
        token.type = JSC_TOKEN_ASSIGN;
      }
    }
    else if (c == '<')
    {
      if (state->position < state->source_length)
      {
        if (state->source[state->position] == '<')
        {
          state->position++;
          state->column++;
          if (state->position < state->source_length && state->source[state->position] == '=')
          {
            state->position++;
            state->column++;
            token.type = JSC_TOKEN_LEFT_SHIFT_ASSIGN;
          }
          else
          {
            token.type = JSC_TOKEN_LEFT_SHIFT;
          }
        }
        else if (state->source[state->position] == '=')
        {
          state->position++;
          state->column++;
          token.type = JSC_TOKEN_LESS_THAN_EQUAL;
        }
        else
        {
          token.type = JSC_TOKEN_LESS_THAN;
        }
      }
      else
      {
        token.type = JSC_TOKEN_LESS_THAN;
      }
    }
    else if (c == '>')
    {
      if (state->position < state->source_length)
      {
        if (state->source[state->position] == '>')
        {
          state->position++;
          state->column++;

          if (state->position < state->source_length && state->source[state->position] == '>')
          {
            state->position++;
            state->column++;

            if (state->position < state->source_length && state->source[state->position] == '=')
            {
              state->position++;
              state->column++;
              token.type = JSC_TOKEN_UNSIGNED_RIGHT_SHIFT_ASSIGN;
            }
            else
            {
              token.type = JSC_TOKEN_UNSIGNED_RIGHT_SHIFT;
            }
          }
          else if (state->position < state->source_length && state->source[state->position] == '=')
          {
            state->position++;
            state->column++;
            token.type = JSC_TOKEN_RIGHT_SHIFT_ASSIGN;
          }
          else
          {
            token.type = JSC_TOKEN_RIGHT_SHIFT;
          }
        }
        else if (state->source[state->position] == '=')
        {
          state->position++;
          state->column++;
          token.type = JSC_TOKEN_GREATER_THAN_EQUAL;
        }
        else
        {
          token.type = JSC_TOKEN_GREATER_THAN;
        }
      }
      else
      {
        token.type = JSC_TOKEN_GREATER_THAN;
      }
    }
    else if (c == '\'')
    {
      token.type = JSC_TOKEN_STRING;
      size_t start = state->position;
      size_t string_length = 0;

      while (state->position < state->source_length)
      {
        char strchar = state->source[state->position];
        state->position++;
        state->column++;

        if (strchar == '\'')
        {
          break;
        }

        if (strchar == '\\' && state->position < state->source_length)
        {
          state->position++;
          state->column++;
        }

        string_length++;
      }

      size_t len = state->position - start - 1;
      token.string_value.data = malloc(len + 1);

      size_t j = 0;

      for (size_t i = 0; i < len; i++)
      {
        char ch = state->source[start + i];
        if (ch == '\\' && i + 1 < len)
        {
          i++;
          ch = state->source[start + i];
          switch (ch)
          {
          case 'n':
            token.string_value.data[j++] = '\n';
            break;
          case 't':
            token.string_value.data[j++] = '\t';
            break;
          case 'r':
            token.string_value.data[j++] = '\r';
            break;
          case '\\':
            token.string_value.data[j++] = '\\';
            break;
          case '\'':
            token.string_value.data[j++] = '\'';
            break;
          case '\"':
            token.string_value.data[j++] = '\"';
            break;
          default:
            token.string_value.data[j++] = ch;
          }
        }
        else
        {
          token.string_value.data[j++] = ch;
        }
      }

      token.string_value.data[j] = '\0';
      token.string_value.length = j;
    }
    else if (c == '"')
    {
      token.type = JSC_TOKEN_STRING;
      size_t start = state->position;
      size_t string_length = 0;

      while (state->position < state->source_length)
      {
        char strchar = state->source[state->position];
        state->position++;
        state->column++;

        if (strchar == '"')
        {
          break;
        }

        if (strchar == '\\' && state->position < state->source_length)
        {
          state->position++;
          state->column++;
        }

        string_length++;
      }

      size_t len = state->position - start - 1;
      token.string_value.data = malloc(len + 1);

      size_t j = 0;

      for (size_t i = 0; i < len; i++)
      {
        char ch = state->source[start + i];

        if (ch == '\\' && i + 1 < len)
        {
          i++;
          ch = state->source[start + i];
          switch (ch)
          {
          case 'n':
            token.string_value.data[j++] = '\n';
            break;
          case 't':
            token.string_value.data[j++] = '\t';
            break;
          case 'r':
            token.string_value.data[j++] = '\r';
            break;
          case '\\':
            token.string_value.data[j++] = '\\';
            break;
          case '\'':
            token.string_value.data[j++] = '\'';
            break;
          case '\"':
            token.string_value.data[j++] = '\"';
            break;
          default:
            token.string_value.data[j++] = ch;
          }
        }
        else
        {
          token.string_value.data[j++] = ch;
        }
      }
      token.string_value.data[j] = '\0';
      token.string_value.length = j;
    }
    else if (c == '`')
    {
      token.type = JSC_TOKEN_TEMPLATE;

      size_t start = state->position;
      size_t string_length = 0;

      while (state->position < state->source_length)
      {
        char strchar = state->source[state->position];
        state->position++;
        state->column++;

        if (strchar == '`')
        {
          break;
        }

        if (strchar == '\\' && state->position < state->source_length)
        {
          state->position++;
          state->column++;
        }

        string_length++;
      }

      size_t len = state->position - start - 1;
      token.string_value.data = malloc(len + 1);

      size_t j = 0;
      for (size_t i = 0; i < len; i++)
      {
        char ch = state->source[start + i];
        if (ch == '\\' && i + 1 < len)
        {
          i++;
          ch = state->source[start + i];
          switch (ch)
          {
          case 'n':
            token.string_value.data[j++] = '\n';
            break;
          case 't':
            token.string_value.data[j++] = '\t';
            break;
          case 'r':
            token.string_value.data[j++] = '\r';
            break;
          case '\\':
            token.string_value.data[j++] = '\\';
            break;
          case '\'':
            token.string_value.data[j++] = '\'';
            break;
          case '\"':
            token.string_value.data[j++] = '\"';
            break;
          case '`':
            token.string_value.data[j++] = '`';
            break;
          default:
            token.string_value.data[j++] = ch;
          }
        }
        else
        {
          token.string_value.data[j++] = ch;
        }
      }
      token.string_value.data[j] = '\0';
      token.string_value.length = j;
    }
    else if (c >= '0' && c <= '9')
    {
      token.type = JSC_TOKEN_NUMBER;
      size_t start = state->position - 1;

      while (state->position < state->source_length &&
             ((state->source[state->position] >= '0' && state->source[state->position] <= '9') ||
              state->source[state->position] == '.'))
      {
        state->position++;
        state->column++;
      }

      size_t len = state->position - start;
      char *numstr = malloc(len + 1);
      memcpy(numstr, state->source + start, len);
      numstr[len] = '\0';
      token.number_value = atof(numstr);
      free(numstr);
    }
    else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$')
    {
      size_t start = state->position - 1;

      while (state->position < state->source_length &&
             ((state->source[state->position] >= 'a' && state->source[state->position] <= 'z') ||
              (state->source[state->position] >= 'A' && state->source[state->position] <= 'Z') ||
              (state->source[state->position] >= '0' && state->source[state->position] <= '9') ||
              state->source[state->position] == '_' ||
              state->source[state->position] == '$'))
      {
        state->position++;
        state->column++;
      }

      size_t len = state->position - start;

      if (len == 2 && strncmp(state->source + start, "if", 2) == 0)
      {
        token.type = JSC_TOKEN_IF;
      }
      else if (len == 4 && strncmp(state->source + start, "else", 4) == 0)
      {
        token.type = JSC_TOKEN_ELSE;
      }
      else if (len == 3 && strncmp(state->source + start, "var", 3) == 0)
      {
        token.type = JSC_TOKEN_VAR;
      }
      else if (len == 6 && strncmp(state->source + start, "return", 6) == 0)
      {
        token.type = JSC_TOKEN_RETURN;
      }
      else if (len == 4 && strncmp(state->source + start, "true", 4) == 0)
      {
        token.type = JSC_TOKEN_TRUE;
      }
      else if (len == 5 && strncmp(state->source + start, "false", 5) == 0)
      {
        token.type = JSC_TOKEN_FALSE;
      }
      else if (len == 3 && strncmp(state->source + start, "for", 3) == 0)
      {
        token.type = JSC_TOKEN_FOR;
      }
      else if (len == 5 && strncmp(state->source + start, "while", 5) == 0)
      {
        token.type = JSC_TOKEN_WHILE;
      }
      else if (len == 5 && strncmp(state->source + start, "break", 5) == 0)
      {
        token.type = JSC_TOKEN_BREAK;
      }
      else if (len == 8 && strncmp(state->source + start, "continue", 8) == 0)
      {
        token.type = JSC_TOKEN_CONTINUE;
      }
      else if (len == 2 && strncmp(state->source + start, "do", 2) == 0)
      {
        token.type = JSC_TOKEN_DO;
      }
      else if (len == 6 && strncmp(state->source + start, "switch", 6) == 0)
      {
        token.type = JSC_TOKEN_SWITCH;
      }
      else if (len == 4 && strncmp(state->source + start, "case", 4) == 0)
      {
        token.type = JSC_TOKEN_CASE;
      }
      else if (len == 7 && strncmp(state->source + start, "default", 7) == 0)
      {
        token.type = JSC_TOKEN_DEFAULT;
      }
      else if (len == 4 && strncmp(state->source + start, "null", 4) == 0)
      {
        token.type = JSC_TOKEN_NULL;
      }
      else if (len == 3 && strncmp(state->source + start, "let", 3) == 0)
      {
        token.type = JSC_TOKEN_LET;
      }
      else if (len == 5 && strncmp(state->source + start, "const", 5) == 0)
      {
        token.type = JSC_TOKEN_CONST;
      }
      else if (len == 5 && strncmp(state->source + start, "class", 5) == 0)
      {
        token.type = JSC_TOKEN_CLASS;
      }
      else if (len == 8 && strncmp(state->source + start, "function", 8) == 0)
      {
        token.type = JSC_TOKEN_FUNCTION;
      }
      else if (len == 5 && strncmp(state->source + start, "async", 5) == 0)
      {
        token.type = JSC_TOKEN_ASYNC;
      }
      else if (len == 5 && strncmp(state->source + start, "await", 5) == 0)
      {
        token.type = JSC_TOKEN_AWAIT;
      }
      else
      {
        token.type = JSC_TOKEN_IDENTIFIER;
      }
    }
    else
    {
      token.type = JSC_TOKEN_ERROR;
      char error_msg[100];
      snprintf(error_msg, 100, "unexpected character: %c", c);
      jsc_set_token_error(state, error_msg);
    }

    token.length = state->position - (token.start - state->source);

    return token;
  }

  token.type = JSC_TOKEN_EOF;
  token.start = state->source + state->position;
  token.length = 0;
  state->eof_reached = true;

  return token;
}

static void jsc_vec_scan_identifier(jsc_tokenizer_state *state)
{
  jsc_token token;
  memset(&token, 0, sizeof(jsc_token));

  token.type = JSC_TOKEN_IDENTIFIER;
  token.start = state->source + state->position;
  token.line = state->line;
  token.column = state->column;

  char first_char = jsc_peek(state);
  jsc_advance_position(state);

  size_t identifier_length = 1;
  state->identifier_buffer[0] = first_char;

  size_t remaining = state->source_length - state->position;

#if defined(__AVX512F__)
  if (state->vector_level >= JSC_SIMD_AVX512F && remaining >= (1 << 6))
  {
    const __m512i underscore = _mm512_set1_epi8('_');
    const __m512i dollar = _mm512_set1_epi8('$');
    const __m512i zero = _mm512_set1_epi8('0');
    const __m512i nine = _mm512_set1_epi8('9');
    const __m512i a_lower = _mm512_set1_epi8('a');
    const __m512i z_lower = _mm512_set1_epi8('z');
    const __m512i a_upper = _mm512_set1_epi8('A');
    const __m512i z_upper = _mm512_set1_epi8('Z');

    while (remaining >= (1 << 6) && identifier_length + (1 << 6) < JSC_MAX_IDENTIFIER_LENGTH)
    {
      _mm_prefetch(state->source + state->position + JSC_PREFETCH_DISTANCE, _MM_HINT_T0);

      __m512i chunk = _mm512_loadu_si512((const __m512i *)(state->source + state->position));

      __mmask64 is_underscore = _mm512_cmpeq_epi8_mask(chunk, underscore);
      __mmask64 is_dollar = _mm512_cmpeq_epi8_mask(chunk, dollar);
      __mmask64 is_digit = _mm512_kand(
          _mm512_cmpge_epi8_mask(chunk, zero),
          _mm512_cmple_epi8_mask(chunk, nine));
      __mmask64 is_lower = _mm512_kand(
          _mm512_cmpge_epi8_mask(chunk, a_lower),
          _mm512_cmple_epi8_mask(chunk, z_lower));
      __mmask64 is_upper = _mm512_kand(
          _mm512_cmpge_epi8_mask(chunk, a_upper),
          _mm512_cmple_epi8_mask(chunk, z_upper));

      __mmask64 is_id_part = _mm512_kor(
          _mm512_kor(
              _mm512_kor(is_underscore, is_dollar),
              is_digit),
          _mm512_kor(is_lower, is_upper));

      if (is_id_part == 0)
      {
        break;
      }

      if (is_id_part == UINT64_MAX)
      {
        _mm512_storeu_si512((__m512i *)(state->identifier_buffer + identifier_length), chunk);
        identifier_length += (1 << 6);
        state->position += (1 << 6);
        state->column += (1 << 6);
        remaining -= (1 << 6);
        continue;
      }

      int trailing_zeros = _tzcnt_u64(~is_id_part);

      if (trailing_zeros > 0)
      {
        for (int i = 0; i < trailing_zeros; i++)
        {
          state->identifier_buffer[identifier_length++] = state->source[state->position++];
          state->column++;
        }

        remaining = state->source_length - state->position;
      }

      break;
    }
  }
#elif defined(__AVX2__)
  if (state->vector_level >= JSC_SIMD_AVX2 && remaining >= (1 << 5))
  {
    const __m256i underscore = _mm256_set1_epi8('_');
    const __m256i dollar = _mm256_set1_epi8('$');
    const __m256i zero = _mm256_set1_epi8('0');
    const __m256i nine = _mm256_set1_epi8('9');
    const __m256i a_lower = _mm256_set1_epi8('a');
    const __m256i z_lower = _mm256_set1_epi8('z');
    const __m256i a_upper = _mm256_set1_epi8('A');
    const __m256i z_upper = _mm256_set1_epi8('Z');

    while (remaining >= (1 << 5) && identifier_length + (1 << 5) < JSC_MAX_IDENTIFIER_LENGTH)
    {
      _mm_prefetch(state->source + state->position + JSC_PREFETCH_DISTANCE, _MM_HINT_T0);

      __m256i chunk = _mm256_loadu_si256((const __m256i *)(state->source + state->position));

      __m256i is_underscore = _mm256_cmpeq_epi8(chunk, underscore);
      __m256i is_dollar = _mm256_cmpeq_epi8(chunk, dollar);
      __m256i is_digit = _mm256_and_si256(
          _mm256_cmpgt_epi8(chunk, _mm256_sub_epi8(zero, _mm256_set1_epi8(1))),
          _mm256_cmpgt_epi8(_mm256_add_epi8(nine, _mm256_set1_epi8(1)), chunk));
      __m256i is_lower = _mm256_and_si256(
          _mm256_cmpgt_epi8(chunk, _mm256_sub_epi8(a_lower, _mm256_set1_epi8(1))),
          _mm256_cmpgt_epi8(_mm256_add_epi8(z_lower, _mm256_set1_epi8(1)), chunk));
      __m256i is_upper = _mm256_and_si256(
          _mm256_cmpgt_epi8(chunk, _mm256_sub_epi8(a_upper, _mm256_set1_epi8(1))),
          _mm256_cmpgt_epi8(_mm256_add_epi8(z_upper, _mm256_set1_epi8(1)), chunk));

      __m256i is_id_part = _mm256_or_si256(
          _mm256_or_si256(
              _mm256_or_si256(is_underscore, is_dollar),
              is_digit),
          _mm256_or_si256(is_lower, is_upper));

      uint32_t mask = _mm256_movemask_epi8(is_id_part);

      if (mask == 0)
      {
        break;
      }

      if (mask == 0xFFFFFFFF)
      {
        _mm256_storeu_si256((__m256i *)(state->identifier_buffer + identifier_length), chunk);
        identifier_length += (1 << 5);
        state->position += (1 << 5);
        state->column += (1 << 5);
        remaining -= (1 << 5);
        continue;
      }

      int trailing_zeros = __builtin_ctz(~mask);

      if (trailing_zeros > 0)
      {
        for (int i = 0; i < trailing_zeros; i++)
        {
          state->identifier_buffer[identifier_length++] = state->source[state->position++];
          state->column++;
        }

        remaining = state->source_length - state->position;
      }

      break;
    }
  }
#endif

  while (state->position < state->source_length && identifier_length < JSC_MAX_IDENTIFIER_LENGTH)
  {
    char c = jsc_peek(state);

    if (!JSC_IS_IDENTIFIER_PART(c))
    {
      break;
    }

    jsc_advance_position(state);
    state->identifier_buffer[identifier_length++] = c;
  }

  state->identifier_buffer[identifier_length] = '\0';
  token.length = identifier_length;

  if (jsc_check_keyword(state->identifier_buffer, identifier_length, &token))
  {
    state->current = token;
    return;
  }

  state->current = token;
}

static void jsc_scan_string(jsc_tokenizer_state *state, char quote)
{
  jsc_token token;
  memset(&token, 0, sizeof(jsc_token));

  token.type = JSC_TOKEN_STRING;
  token.start = state->source + state->position - 1;
  token.line = state->line;
  token.column = state->column - 1;

  size_t string_length = 0;

  while (state->position < state->source_length)
  {
    char c = jsc_peek(state);

    if (c == '\0' || JSC_IS_LINE_TERMINATOR(c))
    {
      jsc_set_token_error(state, "Unterminated string literal");
      state->current = token;
      return;
    }

    jsc_advance_position(state);

    if (c == quote)
    {
      break;
    }

    if (c == '\\')
    {
      if (state->position >= state->source_length)
      {
        jsc_set_token_error(state, "Unterminated string literal");
        state->current = token;
        return;
      }

      char escape = jsc_peek(state);
      jsc_advance_position(state);

      switch (escape)
      {
      case '\'':
      case '"':
      case '\\':
      case '/':
        state->string_buffer[string_length++] = escape;
        break;
      case 'b':
        state->string_buffer[string_length++] = '\b';
        break;
      case 'f':
        state->string_buffer[string_length++] = '\f';
        break;
      case 'n':
        state->string_buffer[string_length++] = '\n';
        break;
      case 'r':
        state->string_buffer[string_length++] = '\r';
        break;
      case 't':
        state->string_buffer[string_length++] = '\t';
        break;
      case 'v':
        state->string_buffer[string_length++] = '\v';
        break;
      case 'u':
      {
        if (state->position + 3 >= state->source_length)
        {
          jsc_set_token_error(state, "Invalid Unicode escape sequence");
          state->current = token;
          return;
        }

        uint32_t hex_value = 0;
        for (int i = 0; i < 4; i++)
        {
          char hex = jsc_peek(state);
          jsc_advance_position(state);

          if (!JSC_IS_HEX_DIGIT(hex))
          {
            jsc_set_token_error(state, "Invalid Unicode escape sequence");
            state->current = token;
            return;
          }

          hex_value = (hex_value << 4) | jsc_hex_value[(unsigned char)hex];
        }

        if (hex_value < 0x80)
        {
          state->string_buffer[string_length++] = (char)hex_value;
        }
        else if (hex_value < 0x800)
        {
          state->string_buffer[string_length++] = (char)(0xC0 | (hex_value >> 6));
          state->string_buffer[string_length++] = (char)(0x80 | (hex_value & 0x3F));
        }
        else
        {
          state->string_buffer[string_length++] = (char)(0xE0 | (hex_value >> 12));
          state->string_buffer[string_length++] = (char)(0x80 | ((hex_value >> 6) & 0x3F));
          state->string_buffer[string_length++] = (char)(0x80 | (hex_value & 0x3F));
        }
        break;
      }
      case 'x':
      {
        if (state->position + 1 >= state->source_length)
        {
          jsc_set_token_error(state, "Invalid hex escape sequence");
          state->current = token;
          return;
        }

        char hex1 = jsc_peek(state);
        jsc_advance_position(state);
        char hex2 = jsc_peek(state);
        jsc_advance_position(state);

        if (!JSC_IS_HEX_DIGIT(hex1) || !JSC_IS_HEX_DIGIT(hex2))
        {
          jsc_set_token_error(state, "Invalid hex escape sequence");
          state->current = token;
          return;
        }

        uint8_t hex_value = (jsc_hex_value[(unsigned char)hex1] << 4) | jsc_hex_value[(unsigned char)hex2];
        state->string_buffer[string_length++] = (char)hex_value;
        break;
      }
      default:
        state->string_buffer[string_length++] = escape;
        break;
      }
    }
    else
    {
      state->string_buffer[string_length++] = c;
    }

    if (string_length >= JSC_MAX_STRING_LENGTH - 4)
    {
      jsc_set_token_error(state, "String literal too long");
      state->current = token;
      return;
    }
  }

  token.length = state->position - (token.start - state->source);

  state->string_buffer[string_length] = '\0';
  token.string_value.data = malloc(string_length + 1);
  memcpy(token.string_value.data, state->string_buffer, string_length + 1);
  token.string_value.length = string_length;

  state->current = token;
}

void jsc_tokenizer_free(jsc_tokenizer_state *state)
{
  if (state)
  {
    if (state->error_message)
    {
      free(state->error_message);
    }

    if (state->current.type == JSC_TOKEN_STRING)
    {
      free(state->current.string_value.data);
    }
    else if (state->current.type == JSC_TOKEN_REGEXP)
    {
      free(state->current.regexp_value.data);
      free(state->current.regexp_value.flags);
    }

    free(state);
  }
}

bool jsc_tokenizer_has_error(jsc_tokenizer_state *state)
{
  return state->error_message != NULL;
}

const char *jsc_tokenizer_get_error(jsc_tokenizer_state *state)
{
  return state->error_message;
}

const char *jsc_token_type_to_string(jsc_token_type type)
{
  if (type >= 0 && type < sizeof(jsc_token_names) / sizeof(jsc_token_names[0]))
  {
    return jsc_token_names[type];
  }

  return "UNKNOWN";
}

bool jsc_token_is_keyword(jsc_token_type type)
{
  return type >= JSC_TOKEN_BREAK && type <= JSC_TOKEN_UNDEFINED;
}

bool jsc_token_is_operator(jsc_token_type type)
{
  return type >= JSC_TOKEN_ASSIGN && type <= JSC_TOKEN_ARROW;
}

bool jsc_token_is_punctuator(jsc_token_type type)
{
  return type >= JSC_TOKEN_SEMICOLON && type <= JSC_TOKEN_SPREAD;
}

bool jsc_token_is_literal(jsc_token_type type)
{
  return type >= JSC_TOKEN_IDENTIFIER && type <= JSC_TOKEN_TEMPLATE;
}
