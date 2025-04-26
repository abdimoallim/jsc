#include "jsc_tokenizer.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

static const uint8_t jsc_hex_value[1 << 8] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF};

static const char* jsc_token_names[] = {"NONE",
                                        "IDENTIFIER",
                                        "STRING",
                                        "NUMBER",
                                        "REGEXP",
                                        "TEMPLATE",
                                        "BREAK",
                                        "CASE",
                                        "CATCH",
                                        "CLASS",
                                        "CONST",
                                        "CONTINUE",
                                        "DEBUGGER",
                                        "DEFAULT",
                                        "DELETE",
                                        "DO",
                                        "ELSE",
                                        "EXPORT",
                                        "EXTENDS",
                                        "FINALLY",
                                        "FOR",
                                        "FUNCTION",
                                        "IF",
                                        "IMPORT",
                                        "IN",
                                        "INSTANCEOF",
                                        "NEW",
                                        "RETURN",
                                        "SUPER",
                                        "SWITCH",
                                        "THIS",
                                        "THROW",
                                        "TRY",
                                        "TYPEOF",
                                        "VAR",
                                        "VOID",
                                        "WHILE",
                                        "WITH",
                                        "YIELD",
                                        "AWAIT",
                                        "ASYNC",
                                        "LET",
                                        "STATIC",
                                        "TRUE",
                                        "FALSE",
                                        "NULL",
                                        "UNDEFINED",
                                        "=",
                                        "+",
                                        "-",
                                        "*",
                                        "/",
                                        "%",
                                        "++",
                                        "--",
                                        "==",
                                        "!=",
                                        "===",
                                        "!==",
                                        ">",
                                        "<",
                                        ">=",
                                        "<=",
                                        "&&",
                                        "||",
                                        "!",
                                        "&",
                                        "|",
                                        "^",
                                        "~",
                                        "<<",
                                        ">>",
                                        ">>>",
                                        "+=",
                                        "-=",
                                        "*=",
                                        "/=",
                                        "%=",
                                        "<<=",
                                        ">>=",
                                        ">>>=",
                                        "&=",
                                        "|=",
                                        "^=",
                                        "&&=",
                                        "||=",
                                        "??",
                                        "??=",
                                        "?.",
                                        "**",
                                        "**=",
                                        "=>",
                                        ";",
                                        ":",
                                        ",",
                                        ".",
                                        "?",
                                        "(",
                                        ")",
                                        "[",
                                        "]",
                                        "{",
                                        "}",
                                        "...",
                                        "TEMPLATE_START",
                                        "TEMPLATE_MIDDLE",
                                        "TEMPLATE_END",
                                        "EOF",
                                        "ERROR"};

static const struct
{
  const char* keyword;
  jsc_token_type type;
} jsc_keywords[] = {{"break", JSC_TOKEN_BREAK},
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

#define JSC_IS_WHITESPACE(c)                                                   \
  ((c) == ' ' || (c) == '\t' || (c) == '\n' || (c) == '\r' || (c) == '\f' ||   \
   (c) == '\v')
#define JSC_IS_LETTER(c)                                                       \
  (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z'))
#define JSC_IS_DIGIT(c) ((c) >= '0' && (c) <= '9')
#define JSC_IS_HEX_DIGIT(c)                                                    \
  (JSC_IS_DIGIT(c) || ((c) >= 'a' && (c) <= 'f') || ((c) >= 'A' && (c) <= 'F'))
#define JSC_IS_OCTAL_DIGIT(c) ((c) >= '0' && (c) <= '7')
#define JSC_IS_BINARY_DIGIT(c) ((c) == '0' || (c) == '1')
#define JSC_IS_IDENTIFIER_START(c)                                             \
  (JSC_IS_LETTER(c) || (c) == '_' || (c) == '$')
#define JSC_IS_IDENTIFIER_PART(c)                                              \
  (JSC_IS_IDENTIFIER_START(c) || JSC_IS_DIGIT(c))
#define JSC_IS_LINE_TERMINATOR(c) ((c) == '\n' || (c) == '\r')

static void jsc_vec_scan_identifier(jsc_tokenizer_context* ctx);
static void jsc_scan_string(jsc_tokenizer_context* ctx, char quote);
static void jsc_scan_template(jsc_tokenizer_context* ctx);
static void jsc_scan_number(jsc_tokenizer_context* ctx);
static void jsc_scan_regexp(jsc_tokenizer_context* ctx);

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

static void jsc_set_token_error(jsc_tokenizer_context* ctx, const char* message)
{
  if (ctx->error_message)
  {
    free(ctx->error_message);
  }

  ctx->error_message = strdup(message);
  ctx->current.type = JSC_TOKEN_ERROR;
}

static JSC_FORCE_INLINE void jsc_advance_position(jsc_tokenizer_context* ctx)
{
  if (JSC_UNLIKELY(ctx->position >= ctx->source_length))
  {
    ctx->eof_reached = true;
    return;
  }

  char c = ctx->source[ctx->position];
  ctx->position++;

  if (c == '\n')
  {
    ctx->line++;
    ctx->column = 0;
  }
  else if (c == '\r')
  {
    if (ctx->position < ctx->source_length &&
        ctx->source[ctx->position] == '\n')
    {
      ctx->position++;
    }

    ctx->line++;
    ctx->column = 0;
  }
  else
  {
    ctx->column++;
  }
}

static JSC_FORCE_INLINE char jsc_peek(jsc_tokenizer_context* ctx)
{
  if (JSC_UNLIKELY(ctx->position >= ctx->source_length))
  {
    return '\0';
  }

  return ctx->source[ctx->position];
}

static JSC_FORCE_INLINE char jsc_peek_n(jsc_tokenizer_context* ctx, size_t n)
{
  if (JSC_UNLIKELY(ctx->position + n >= ctx->source_length))
  {
    return '\0';
  }

  return ctx->source[ctx->position + n];
}

static JSC_FORCE_INLINE bool jsc_match(jsc_tokenizer_context* ctx,
                                       char expected)
{
  if (jsc_peek(ctx) != expected)
  {
    return false;
  }

  jsc_advance_position(ctx);

  return true;
}

static void jsc_vec_skip_whitespace(jsc_tokenizer_context* ctx)
{
  size_t remaining = ctx->source_length - ctx->position;

#if defined(__AVX512F__)
  if (ctx->vector_level >= JSC_SIMD_AVX512F && remaining >= (1 << 6))
  {
    const __m512i spaces = _mm512_set1_epi8(' ');
    const __m512i tabs = _mm512_set1_epi8('\t');
    const __m512i newlines = _mm512_set1_epi8('\n');
    const __m512i carriage_returns = _mm512_set1_epi8('\r');
    const __m512i form_feeds = _mm512_set1_epi8('\f');
    const __m512i vertical_tabs = _mm512_set1_epi8('\v');

    while (remaining >= (1 << 6))
    {
      _mm_prefetch(ctx->source + ctx->position + JSC_PREFETCH_DISTANCE,
                   _MM_HINT_T0);

      __m512i chunk =
          _mm512_loadu_si512((const __m512i*)(ctx->source + ctx->position));

      __mmask64 is_space = _mm512_cmpeq_epi8_mask(chunk, spaces);
      __mmask64 is_tab = _mm512_cmpeq_epi8_mask(chunk, tabs);
      __mmask64 is_newline = _mm512_cmpeq_epi8_mask(chunk, newlines);
      __mmask64 is_cr = _mm512_cmpeq_epi8_mask(chunk, carriage_returns);
      __mmask64 is_ff = _mm512_cmpeq_epi8_mask(chunk, form_feeds);
      __mmask64 is_vt = _mm512_cmpeq_epi8_mask(chunk, vertical_tabs);

      __mmask64 is_whitespace =
          is_space | is_tab | is_newline | is_cr | is_ff | is_vt;

      if (is_whitespace == 0)
      {
        break;
      }

      if (is_whitespace == UINT64_MAX)
      {
        int newline_count = _mm_popcnt_u64(_mm512_movepi8_mask(newlines));
        int cr_count = _mm_popcnt_u64(_mm512_movepi8_mask(carriage_returns));

        ctx->line += newline_count + cr_count;

        if (newline_count > 0 || cr_count > 0)
        {
          ctx->column = 0;
        }
        else
        {
          ctx->column += (1 << 6);
        }

        ctx->position += (1 << 6);
        remaining -= (1 << 6);

        continue;
      }

      int trailing_zeros = _tzcnt_u64(~is_whitespace);

      if (trailing_zeros > 0)
      {
        for (int i = 0; i < trailing_zeros; i++)
        {
          jsc_advance_position(ctx);
        }

        remaining = ctx->source_length - ctx->position;
      }
      else
      {
        break;
      }
    }
  }
#elif defined(__AVX2__)
  if (ctx->vector_level >= JSC_SIMD_AVX2 && remaining >= (1 << 5))
  {
    const __m256i spaces = _mm256_set1_epi8(' ');
    const __m256i tabs = _mm256_set1_epi8('\t');
    const __m256i newlines = _mm256_set1_epi8('\n');
    const __m256i carriage_returns = _mm256_set1_epi8('\r');
    const __m256i form_feeds = _mm256_set1_epi8('\f');
    const __m256i vertical_tabs = _mm256_set1_epi8('\v');

    while (remaining >= (1 << 5))
    {
      _mm_prefetch(ctx->source + ctx->position + JSC_PREFETCH_DISTANCE,
                   _MM_HINT_T0);

      __m256i chunk =
          _mm256_loadu_si256((const __m256i*)(ctx->source + ctx->position));

      __m256i is_space = _mm256_cmpeq_epi8(chunk, spaces);
      __m256i is_tab = _mm256_cmpeq_epi8(chunk, tabs);
      __m256i is_newline = _mm256_cmpeq_epi8(chunk, newlines);
      __m256i is_cr = _mm256_cmpeq_epi8(chunk, carriage_returns);
      __m256i is_ff = _mm256_cmpeq_epi8(chunk, form_feeds);
      __m256i is_vt = _mm256_cmpeq_epi8(chunk, vertical_tabs);

      __m256i is_whitespace =
          _mm256_or_si256(_mm256_or_si256(_mm256_or_si256(is_space, is_tab),
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

        ctx->line += newline_count + cr_count;

        if (newline_count > 0 || cr_count > 0)
        {
          ctx->column = 0;
        }
        else
        {
          ctx->column += (1 << 5);
        }

        ctx->position += (1 << 5);
        remaining -= (1 << 5);

        continue;
      }

      int trailing_zeros = __builtin_ctz(~mask);

      if (trailing_zeros > 0)
      {
        for (int i = 0; i < trailing_zeros; i++)
        {
          jsc_advance_position(ctx);
        }

        remaining = ctx->source_length - ctx->position;
      }
      else
      {
        break;
      }
    }
  }
#elif defined(__SSE4_2__)
  if (ctx->vector_level >= JSC_SIMD_SSE42 && remaining >= (1 << 4))
  {
    const __m128i spaces = _mm_set1_epi8(' ');
    const __m128i tabs = _mm_set1_epi8('\t');
    const __m128i newlines = _mm_set1_epi8('\n');
    const __m128i carriage_returns = _mm_set1_epi8('\r');
    const __m128i form_feeds = _mm_set1_epi8('\f');
    const __m128i vertical_tabs = _mm_set1_epi8('\v');

    while (remaining >= (1 << 4))
    {
      _mm_prefetch(ctx->source + ctx->position + JSC_PREFETCH_DISTANCE,
                   _MM_HINT_T0);

      __m128i chunk =
          _mm_loadu_si128((const __m128i*)(ctx->source + ctx->position));

      __m128i is_space = _mm_cmpeq_epi8(chunk, spaces);
      __m128i is_tab = _mm_cmpeq_epi8(chunk, tabs);
      __m128i is_newline = _mm_cmpeq_epi8(chunk, newlines);
      __m128i is_cr = _mm_cmpeq_epi8(chunk, carriage_returns);
      __m128i is_ff = _mm_cmpeq_epi8(chunk, form_feeds);
      __m128i is_vt = _mm_cmpeq_epi8(chunk, vertical_tabs);

      __m128i is_whitespace =
          _mm_or_si128(_mm_or_si128(_mm_or_si128(is_space, is_tab),
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

        ctx->line += newline_count + cr_count;

        if (newline_count > 0 || cr_count > 0)
        {
          ctx->column = 0;
        }
        else
        {
          ctx->column += (1 << 4);
        }

        ctx->position += (1 << 4);
        remaining -= (1 << 4);

        continue;
      }

      int trailing_zeros = __builtin_ctz(~mask);

      if (trailing_zeros > 0)
      {
        for (int i = 0; i < trailing_zeros; i++)
        {
          jsc_advance_position(ctx);
        }
        remaining = ctx->source_length - ctx->position;
      }
      else
      {
        break;
      }
    }
  }
#elif defined(__SSE2__)
  if (ctx->vector_level >= JSC_SIMD_SSE2 && remaining >= (1 << 4))
  {
    const __m128i spaces = _mm_set1_epi8(' ');
    const __m128i tabs = _mm_set1_epi8('\t');
    const __m128i newlines = _mm_set1_epi8('\n');
    const __m128i carriage_returns = _mm_set1_epi8('\r');
    const __m128i form_feeds = _mm_set1_epi8('\f');
    const __m128i vertical_tabs = _mm_set1_epi8('\v');

    while (remaining >= (1 << 4))
    {
      __m128i chunk =
          _mm_loadu_si128((const __m128i*)(ctx->source + ctx->position));

      __m128i is_space = _mm_cmpeq_epi8(chunk, spaces);
      __m128i is_tab = _mm_cmpeq_epi8(chunk, tabs);
      __m128i is_newline = _mm_cmpeq_epi8(chunk, newlines);
      __m128i is_cr = _mm_cmpeq_epi8(chunk, carriage_returns);
      __m128i is_ff = _mm_cmpeq_epi8(chunk, form_feeds);
      __m128i is_vt = _mm_cmpeq_epi8(chunk, vertical_tabs);

      __m128i is_whitespace =
          _mm_or_si128(_mm_or_si128(_mm_or_si128(is_space, is_tab),
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

        ctx->line += newline_count + cr_count;

        if (newline_count > 0 || cr_count > 0)
        {
          ctx->column = 0;
        }
        else
        {
          ctx->column += (1 << 4);
        }

        ctx->position += (1 << 4);
        remaining -= (1 << 4);

        continue;
      }

      int i = 0;

      while ((mask & (1 << i)) && i < (1 << 4))
      {
        jsc_advance_position(ctx);
        i++;
      }

      remaining = ctx->source_length - ctx->position;
    }
  }
#endif

  while (ctx->position < ctx->source_length)
  {
    char c = jsc_peek(ctx);

    if (!JSC_IS_WHITESPACE(c))
    {
      break;
    }

    jsc_advance_position(ctx);
  }
}

static void jsc_vec_skip_comment(jsc_tokenizer_context* ctx)
{
  if (jsc_peek(ctx) == '/' && jsc_peek_n(ctx, 1) == '/')
  {
    jsc_advance_position(ctx);
    jsc_advance_position(ctx);

    size_t remaining = ctx->source_length - ctx->position;

#if defined(__AVX512F__)
    if (ctx->vector_level >= JSC_SIMD_AVX512F && remaining >= (1 << 6))
    {
      const __m512i newlines = _mm512_set1_epi8('\n');
      const __m512i carriage_returns = _mm512_set1_epi8('\r');

      while (remaining >= (1 << 6))
      {
        _mm_prefetch(ctx->source + ctx->position + JSC_PREFETCH_DISTANCE,
                     _MM_HINT_T0);

        __m512i chunk =
            _mm512_loadu_si512((const __m512i*)(ctx->source + ctx->position));

        __mmask64 is_newline = _mm512_cmpeq_epi8_mask(chunk, newlines);
        __mmask64 is_cr = _mm512_cmpeq_epi8_mask(chunk, carriage_returns);

        __mmask64 is_line_term = is_newline | is_cr;

        if (is_line_term == 0)
        {
          ctx->position += (1 << 6);
          ctx->column += (1 << 6);
          remaining -= (1 << 6);
          continue;
        }

        int trailing_zeros = _tzcnt_u64(is_line_term);
        ctx->position += trailing_zeros;
        ctx->column += trailing_zeros;
        break;
      }
    }
#elif defined(__AVX2__)
    if (ctx->vector_level >= JSC_SIMD_AVX2 && remaining >= (1 << 5))
    {
      const __m256i newlines = _mm256_set1_epi8('\n');
      const __m256i carriage_returns = _mm256_set1_epi8('\r');

      while (remaining >= (1 << 5))
      {
        _mm_prefetch(ctx->source + ctx->position + JSC_PREFETCH_DISTANCE,
                     _MM_HINT_T0);

        __m256i chunk =
            _mm256_loadu_si256((const __m256i*)(ctx->source + ctx->position));

        __m256i is_newline = _mm256_cmpeq_epi8(chunk, newlines);
        __m256i is_cr = _mm256_cmpeq_epi8(chunk, carriage_returns);

        __m256i is_line_term = _mm256_or_si256(is_newline, is_cr);

        uint32_t mask = _mm256_movemask_epi8(is_line_term);

        if (mask == 0)
        {
          ctx->position += (1 << 5);
          ctx->column += (1 << 5);
          remaining -= (1 << 5);
          continue;
        }

        int trailing_zeros = __builtin_ctz(mask);
        ctx->position += trailing_zeros;
        ctx->column += trailing_zeros;
        break;
      }
    }
#elif defined(__SSE4_2__)
    if (ctx->vector_level >= JSC_SIMD_SSE42 && remaining >= (1 << 4))
    {
      const __m128i newlines = _mm_set1_epi8('\n');
      const __m128i carriage_returns = _mm_set1_epi8('\r');

      while (remaining >= (1 << 4))
      {
        _mm_prefetch(ctx->source + ctx->position + JSC_PREFETCH_DISTANCE,
                     _MM_HINT_T0);

        __m128i chunk =
            _mm_loadu_si128((const __m128i*)(ctx->source + ctx->position));

        __m128i is_newline = _mm_cmpeq_epi8(chunk, newlines);
        __m128i is_cr = _mm_cmpeq_epi8(chunk, carriage_returns);

        __m128i is_line_term = _mm_or_si128(is_newline, is_cr);

        uint16_t mask = _mm_movemask_epi8(is_line_term);

        if (mask == 0)
        {
          ctx->position += (1 << 4);
          ctx->column += (1 << 4);
          remaining -= (1 << 4);
          continue;
        }

        int trailing_zeros = __builtin_ctz(mask);
        ctx->position += trailing_zeros;
        ctx->column += trailing_zeros;
        break;
      }
    }
#elif defined(__SSE2__)
    if (ctx->vector_level >= JSC_SIMD_SSE2 && remaining >= (1 << 4))
    {
      const __m128i newlines = _mm_set1_epi8('\n');
      const __m128i carriage_returns = _mm_set1_epi8('\r');

      while (remaining >= (1 << 4))
      {
        __m128i chunk =
            _mm_loadu_si128((const __m128i*)(ctx->source + ctx->position));

        __m128i is_newline = _mm_cmpeq_epi8(chunk, newlines);
        __m128i is_cr = _mm_cmpeq_epi8(chunk, carriage_returns);

        __m128i is_line_term = _mm_or_si128(is_newline, is_cr);

        uint16_t mask = _mm_movemask_epi8(is_line_term);

        if (mask == 0)
        {
          ctx->position += (1 << 4);
          ctx->column += (1 << 4);
          remaining -= (1 << 4);
          continue;
        }

        int i = 0;

        while (i < (1 << 4) && !(mask & (1 << i)))
        {
          i++;
        }

        ctx->position += i;
        ctx->column += i;
        break;
      }
    }
#endif

    while (ctx->position < ctx->source_length)
    {
      char c = jsc_peek(ctx);

      if (JSC_IS_LINE_TERMINATOR(c))
      {
        break;
      }

      jsc_advance_position(ctx);
    }
  }
  else if (jsc_peek(ctx) == '/' && jsc_peek_n(ctx, 1) == '*')
  {
    jsc_advance_position(ctx);
    jsc_advance_position(ctx);

    size_t remaining = ctx->source_length - ctx->position;
    char last_char = '\0';

#if defined(__AVX512F__)
    if (ctx->vector_level >= JSC_SIMD_AVX512F && remaining >= (1 << 6))
    {
      const __m512i stars = _mm512_set1_epi8('*');
      const __m512i slashes = _mm512_set1_epi8('/');

      while (remaining >= (1 << 6))
      {
        _mm_prefetch(ctx->source + ctx->position + JSC_PREFETCH_DISTANCE,
                     _MM_HINT_T0);

        __m512i chunk =
            _mm512_loadu_si512((const __m512i*)(ctx->source + ctx->position));

        __mmask64 is_star = _mm512_cmpeq_epi8_mask(chunk, stars);

        if (is_star == 0)
        {
          last_char = '\0';
          uint32_t newline_count = _mm512_movepi8_mask(
              _mm512_cmpeq_epi8(chunk, _mm512_set1_epi8('\n')));
          uint32_t cr_count = _mm512_movepi8_mask(
              _mm512_cmpeq_epi8(chunk, _mm512_set1_epi8('\r')));

          ctx->line += _mm_popcnt_u32(newline_count) + _mm_popcnt_u32(cr_count);
          ctx->position += (1 << 6);
          remaining -= (1 << 6);
          continue;
        }

        for (int i = 0; i < (1 << 6); i++)
        {
          char c = ctx->source[ctx->position];
          jsc_advance_position(ctx);

          if (c == '*' && jsc_peek(ctx) == '/')
          {
            jsc_advance_position(ctx);
            return;
          }
        }

        remaining = ctx->source_length - ctx->position;
      }
    }
#elif defined(__AVX2__)
    if (ctx->vector_level >= JSC_SIMD_AVX2 && remaining >= (1 << 5))
    {
      const __m256i stars = _mm256_set1_epi8('*');
      const __m256i slashes = _mm256_set1_epi8('/');

      while (remaining >= (1 << 5))
      {
        _mm_prefetch(ctx->source + ctx->position + JSC_PREFETCH_DISTANCE,
                     _MM_HINT_T0);

        __m256i chunk =
            _mm256_loadu_si256((const __m256i*)(ctx->source + ctx->position));

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

          ctx->line += newline_count + cr_count;
          ctx->position += (1 << 5);
          remaining -= (1 << 5);
          continue;
        }

        for (int i = 0; i < (1 << 5); i++)
        {
          char c = ctx->source[ctx->position];
          jsc_advance_position(ctx);

          if (c == '*' && jsc_peek(ctx) == '/')
          {
            jsc_advance_position(ctx);
            return;
          }
        }

        remaining = ctx->source_length - ctx->position;
      }
    }
#elif defined(__SSE4_2__)
    if (ctx->vector_level >= JSC_SIMD_SSE42 && remaining >= (1 << 4))
    {
      const __m128i stars = _mm_set1_epi8('*');
      const __m128i slashes = _mm_set1_epi8('/');

      while (remaining >= (1 << 4))
      {
        _mm_prefetch(ctx->source + ctx->position + JSC_PREFETCH_DISTANCE,
                     _MM_HINT_T0);

        __m128i chunk =
            _mm_loadu_si128((const __m128i*)(ctx->source + ctx->position));

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

          ctx->line += newline_count + cr_count;
          ctx->position += (1 << 4);
          remaining -= (1 << 4);
          continue;
        }

        for (int i = 0; i < (1 << 4); i++)
        {
          char c = ctx->source[ctx->position];
          jsc_advance_position(ctx);

          if (c == '*' && jsc_peek(ctx) == '/')
          {
            jsc_advance_position(ctx);
            return;
          }
        }

        remaining = ctx->source_length - ctx->position;
      }
    }
#endif

    while (ctx->position < ctx->source_length)
    {
      char c = jsc_peek(ctx);
      jsc_advance_position(ctx);

      if (last_char == '*' && c == '/')
      {
        break;
      }

      last_char = c;
    }
  }
}

static void jsc_skip_whitespace_and_comments(jsc_tokenizer_context* ctx)
{
  bool skipped_something;

  do
  {
    size_t old_position = ctx->position;

    jsc_vec_skip_whitespace(ctx);

    if (jsc_peek(ctx) == '/' &&
        (jsc_peek_n(ctx, 1) == '/' || jsc_peek_n(ctx, 1) == '*'))
    {
      jsc_vec_skip_comment(ctx);
    }

    skipped_something = old_position != ctx->position;
  } while (skipped_something);
}

static bool jsc_check_keyword(const char* str, size_t len, jsc_token* token)
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

static void jsc_scan_template(jsc_tokenizer_context* ctx)
{
  jsc_token* token = &ctx->current;
  token->start = ctx->source + ctx->position - 1;
  token->line = ctx->line;
  token->column = ctx->column - 1;

  size_t string_length = 0;

  if (ctx->in_template)
  {
    token->type = JSC_TOKEN_TEMPLATE_MIDDLE;
  }
  else
  {
    token->type = JSC_TOKEN_TEMPLATE_START;
    ctx->in_template = true;
    ctx->template_depth = 0;
  }

  while (ctx->position < ctx->source_length)
  {
    char c = jsc_peek(ctx);

    if (c == '\0')
    {
      jsc_set_token_error(ctx, "Unterminated template literal");
      return;
    }

    jsc_advance_position(ctx);

    if (c == '`')
    {
      if (token->type == JSC_TOKEN_TEMPLATE_START)
      {
        ctx->string_buffer[string_length] = '\0';
        token->string_value.data = malloc(string_length + 1);
        memcpy(token->string_value.data, ctx->string_buffer, string_length + 1);
        token->string_value.length = string_length;

        token->type = JSC_TOKEN_STRING;
        ctx->in_template = false;
      }
      else
      {
        ctx->string_buffer[string_length] = '\0';
        token->string_value.data = malloc(string_length + 1);
        memcpy(token->string_value.data, ctx->string_buffer, string_length + 1);
        token->string_value.length = string_length;

        token->type = JSC_TOKEN_TEMPLATE_END;
        ctx->in_template = false;
      }
      break;
    }

    if (c == '$' && jsc_peek(ctx) == '{')
    {
      jsc_advance_position(ctx);

      ctx->string_buffer[string_length] = '\0';
      token->string_value.data = malloc(string_length + 1);
      memcpy(token->string_value.data, ctx->string_buffer, string_length + 1);
      token->string_value.length = string_length;

      ctx->template_depth++;
      ctx->template_brace_depth++;
      break;
    }

    if (c == '\\')
    {
      if (ctx->position >= ctx->source_length)
      {
        jsc_set_token_error(ctx, "Unterminated template literal");
        return;
      }

      char escape = jsc_peek(ctx);
      jsc_advance_position(ctx);

      switch (escape)
      {
      case '\'':
      case '"':
      case '\\':
      case '`':
      case '$':
        ctx->string_buffer[string_length++] = escape;
        break;
      case 'b':
        ctx->string_buffer[string_length++] = '\b';
        break;
      case 'f':
        ctx->string_buffer[string_length++] = '\f';
        break;
      case 'n':
        ctx->string_buffer[string_length++] = '\n';
        break;
      case 'r':
        ctx->string_buffer[string_length++] = '\r';
        break;
      case 't':
        ctx->string_buffer[string_length++] = '\t';
        break;
      case 'v':
        ctx->string_buffer[string_length++] = '\v';
        break;
      case 'u':
      {
        if (ctx->position + 3 >= ctx->source_length)
        {
          jsc_set_token_error(ctx, "Invalid Unicode escape sequence");
          return;
        }

        uint32_t hex_value = 0;

        for (int i = 0; i < 4; i++)
        {
          char hex = jsc_peek(ctx);
          jsc_advance_position(ctx);

          if (!JSC_IS_HEX_DIGIT(hex))
          {
            jsc_set_token_error(ctx, "Invalid Unicode escape sequence");
            return;
          }

          hex_value = (hex_value << 4) | jsc_hex_value[(unsigned char)hex];
        }

        if (hex_value < 0x80)
        {
          ctx->string_buffer[string_length++] = (char)hex_value;
        }
        else if (hex_value < 0x800)
        {
          ctx->string_buffer[string_length++] = (char)(0xC0 | (hex_value >> 6));
          ctx->string_buffer[string_length++] =
              (char)(0x80 | (hex_value & 0x3F));
        }
        else
        {
          ctx->string_buffer[string_length++] =
              (char)(0xE0 | (hex_value >> 12));
          ctx->string_buffer[string_length++] =
              (char)(0x80 | ((hex_value >> 6) & 0x3F));
          ctx->string_buffer[string_length++] =
              (char)(0x80 | (hex_value & 0x3F));
        }
        break;
      }
      case 'x':
      {
        if (ctx->position + 1 >= ctx->source_length)
        {
          jsc_set_token_error(ctx, "Invalid hex escape sequence");
          return;
        }

        char hex1 = jsc_peek(ctx);
        jsc_advance_position(ctx);
        char hex2 = jsc_peek(ctx);
        jsc_advance_position(ctx);

        if (!JSC_IS_HEX_DIGIT(hex1) || !JSC_IS_HEX_DIGIT(hex2))
        {
          jsc_set_token_error(ctx, "Invalid hex escape sequence");
          return;
        }

        uint8_t hex_value = (jsc_hex_value[(unsigned char)hex1] << 4) |
                            jsc_hex_value[(unsigned char)hex2];
        ctx->string_buffer[string_length++] = (char)hex_value;
        break;
      }
      default:
        ctx->string_buffer[string_length++] = escape;
        break;
      }
    }
    else
    {
      ctx->string_buffer[string_length++] = c;
    }

    if (string_length >= JSC_MAX_STRING_LENGTH - 4)
    {
      jsc_set_token_error(ctx, "Template literal too long");
      return;
    }
  }

  token->length = ctx->position - (token->start - ctx->source);
}

static void jsc_scan_regexp(jsc_tokenizer_context* ctx)
{
  jsc_token* token = &ctx->current;
  token->type = JSC_TOKEN_REGEXP;
  token->start = ctx->source + ctx->position - 1;
  token->line = ctx->line;
  token->column = ctx->column - 1;

  size_t pattern_length = 0;
  size_t flags_length = 0;

  while (ctx->position < ctx->source_length)
  {
    char c = jsc_peek(ctx);

    if (c == '\0' || JSC_IS_LINE_TERMINATOR(c))
    {
      jsc_set_token_error(ctx, "Unterminated regular expression literal");
      return;
    }

    jsc_advance_position(ctx);

    if (c == '/')
    {
      break;
    }

    if (c == '\\')
    {
      if (ctx->position >= ctx->source_length)
      {
        jsc_set_token_error(ctx, "Unterminated regular expression literal");
        return;
      }

      ctx->regexp_buffer[pattern_length++] = c;
      c = jsc_peek(ctx);
      jsc_advance_position(ctx);
    }

    if (pattern_length >= JSC_MAX_REGEXP_LENGTH - 2)
    {
      jsc_set_token_error(ctx, "Regular expression literal too long");
      return;
    }

    ctx->regexp_buffer[pattern_length++] = c;
  }

  while (ctx->position < ctx->source_length)
  {
    char c = jsc_peek(ctx);

    if (!JSC_IS_IDENTIFIER_PART(c))
    {
      break;
    }

    if (flags_length >= 63)
    {
      jsc_set_token_error(ctx, "Regular expression flags too long");
      return;
    }

    jsc_advance_position(ctx);
    ctx->regexp_flags_buffer[flags_length++] = c;
  }

  token->length = ctx->position - (token->start - ctx->source);

  ctx->regexp_buffer[pattern_length] = '\0';
  token->regexp_value.data = malloc(pattern_length + 1);
  memcpy(token->regexp_value.data, ctx->regexp_buffer, pattern_length + 1);
  token->regexp_value.length = pattern_length;

  ctx->regexp_flags_buffer[flags_length] = '\0';
  token->regexp_value.flags = malloc(flags_length + 1);
  memcpy(token->regexp_value.flags, ctx->regexp_flags_buffer, flags_length + 1);
  token->regexp_value.flags_length = flags_length;
}

static void jsc_scan_number(jsc_tokenizer_context* ctx)
{
  jsc_token* token = &ctx->current;
  token->type = JSC_TOKEN_NUMBER;
  token->start = ctx->source + ctx->position - 1;
  token->line = ctx->line;
  token->column = ctx->column - 1;

  size_t number_length = 0;
  bool is_hex = false;
  bool is_binary = false;
  bool is_octal = false;

  char first_digit = *(token->start);
  ctx->number_buffer[number_length++] = first_digit;

  if (first_digit == '0' && ctx->position < ctx->source_length)
  {
    char next = jsc_peek(ctx);

    if (next == 'x' || next == 'X')
    {
      is_hex = true;
      jsc_advance_position(ctx);
      ctx->number_buffer[number_length++] = next;

#if defined(__AVX512F__)
      if (ctx->vector_level >= JSC_SIMD_AVX512F &&
          ctx->position + (1 << 6) <= ctx->source_length)
      {
        __m512i zero = _mm512_set1_epi8('0');
        __m512i nine = _mm512_set1_epi8('9');
        __m512i a_lower = _mm512_set1_epi8('a');
        __m512i f_lower = _mm512_set1_epi8('f');
        __m512i a_upper = _mm512_set1_epi8('A');
        __m512i f_upper = _mm512_set1_epi8('F');

        __m512i chunk =
            _mm512_loadu_si512((const __m512i*)(ctx->source + ctx->position));

        __mmask64 is_digit = _mm512_kand(_mm512_cmpge_epi8_mask(chunk, zero),
                                         _mm512_cmple_epi8_mask(chunk, nine));

        __mmask64 is_lower_hex =
            _mm512_kand(_mm512_cmpge_epi8_mask(chunk, a_lower),
                        _mm512_cmple_epi8_mask(chunk, f_lower));

        __mmask64 is_upper_hex =
            _mm512_kand(_mm512_cmpge_epi8_mask(chunk, a_upper),
                        _mm512_cmple_epi8_mask(chunk, f_upper));

        __mmask64 is_hex_digit =
            _mm512_kor(_mm512_kor(is_digit, is_lower_hex), is_upper_hex);

        if (is_hex_digit)
        {
          int trailing_zeros = _tzcnt_u64(~is_hex_digit);

          if (trailing_zeros > 0)
          {
            if (number_length + trailing_zeros < JSC_MAX_NUMBER_LENGTH)
            {
              for (int i = 0; i < trailing_zeros; i++)
              {
                ctx->number_buffer[number_length++] =
                    ctx->source[ctx->position + i];
              }

              ctx->position += trailing_zeros;
              ctx->column += trailing_zeros;
            }
          }
        }
      }
#elif defined(__AVX2__)
      if (ctx->vector_level >= JSC_SIMD_AVX2 &&
          ctx->position + (1 << 5) <= ctx->source_length)
      {
        __m256i zero = _mm256_set1_epi8('0');
        __m256i nine = _mm256_set1_epi8('9');
        __m256i a_lower = _mm256_set1_epi8('a');
        __m256i f_lower = _mm256_set1_epi8('f');
        __m256i a_upper = _mm256_set1_epi8('A');
        __m256i f_upper = _mm256_set1_epi8('F');

        __m256i chunk =
            _mm256_loadu_si256((const __m256i*)(ctx->source + ctx->position));

        __m256i is_digit = _mm256_and_si256(
            _mm256_cmpgt_epi8(chunk,
                              _mm256_sub_epi8(zero, _mm256_set1_epi8(1))),
            _mm256_cmpgt_epi8(_mm256_add_epi8(nine, _mm256_set1_epi8(1)),
                              chunk));

        __m256i is_lower_hex = _mm256_and_si256(
            _mm256_cmpgt_epi8(chunk,
                              _mm256_sub_epi8(a_lower, _mm256_set1_epi8(1))),
            _mm256_cmpgt_epi8(_mm256_add_epi8(f_lower, _mm256_set1_epi8(1)),
                              chunk));

        __m256i is_upper_hex = _mm256_and_si256(
            _mm256_cmpgt_epi8(chunk,
                              _mm256_sub_epi8(a_upper, _mm256_set1_epi8(1))),
            _mm256_cmpgt_epi8(_mm256_add_epi8(f_upper, _mm256_set1_epi8(1)),
                              chunk));

        __m256i is_hex_digit = _mm256_or_si256(
            _mm256_or_si256(is_digit, is_lower_hex), is_upper_hex);

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
                ctx->number_buffer[number_length++] =
                    ctx->source[ctx->position + i];
              }

              ctx->position += trailing_zeros;
              ctx->column += trailing_zeros;
            }
          }
        }
      }
#endif

      while (ctx->position < ctx->source_length)
      {
        char c = jsc_peek(ctx);

        if (!JSC_IS_HEX_DIGIT(c))
        {
          break;
        }

        jsc_advance_position(ctx);

        if (number_length < JSC_MAX_NUMBER_LENGTH)
        {
          ctx->number_buffer[number_length++] = c;
        }
      }
    }
    else if (next == 'b' || next == 'B')
    {
      is_binary = true;
      jsc_advance_position(ctx);
      ctx->number_buffer[number_length++] = next;

      while (ctx->position < ctx->source_length)
      {
        char c = jsc_peek(ctx);

        if (!JSC_IS_BINARY_DIGIT(c))
        {
          break;
        }

        jsc_advance_position(ctx);

        if (number_length < JSC_MAX_NUMBER_LENGTH)
        {
          ctx->number_buffer[number_length++] = c;
        }
      }
    }
    else if (next == 'o' || next == 'O')
    {
      is_octal = true;
      jsc_advance_position(ctx);
      ctx->number_buffer[number_length++] = next;

      while (ctx->position < ctx->source_length)
      {
        char c = jsc_peek(ctx);

        if (!JSC_IS_OCTAL_DIGIT(c))
        {
          break;
        }

        jsc_advance_position(ctx);

        if (number_length < JSC_MAX_NUMBER_LENGTH)
        {
          ctx->number_buffer[number_length++] = c;
        }
      }
    }
    else if (JSC_IS_DIGIT(next))
    {
      is_octal = true;

      while (ctx->position < ctx->source_length)
      {
        char c = jsc_peek(ctx);

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

        jsc_advance_position(ctx);

        if (number_length < JSC_MAX_NUMBER_LENGTH)
        {
          ctx->number_buffer[number_length++] = c;
        }
      }
    }
  }

  if (!is_hex && !is_binary && !is_octal)
  {
#if defined(__AVX512F__)
    if (ctx->vector_level >= JSC_SIMD_AVX512F &&
        ctx->position + (1 << 6) <= ctx->source_length)
    {
      __m512i zero = _mm512_set1_epi8('0');
      __m512i nine = _mm512_set1_epi8('9');

      __m512i chunk =
          _mm512_loadu_si512((const __m512i*)(ctx->source + ctx->position));

      __mmask64 is_digit = _mm512_kand(_mm512_cmpge_epi8_mask(chunk, zero),
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
              ctx->number_buffer[number_length++] =
                  ctx->source[ctx->position + i];
            }

            ctx->position += trailing_zeros;
            ctx->column += trailing_zeros;
          }
        }
      }
    }
#elif defined(__AVX2__)
    if (ctx->vector_level >= JSC_SIMD_AVX2 &&
        ctx->position + (1 << 5) <= ctx->source_length)
    {
      __m256i zero = _mm256_set1_epi8('0');
      __m256i nine = _mm256_set1_epi8('9');

      __m256i chunk =
          _mm256_loadu_si256((const __m256i*)(ctx->source + ctx->position));

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
              ctx->number_buffer[number_length++] =
                  ctx->source[ctx->position + i];
            }

            ctx->position += trailing_zeros;
            ctx->column += trailing_zeros;
          }
        }
      }
    }
#endif

    while (ctx->position < ctx->source_length)
    {
      char c = jsc_peek(ctx);

      if (!JSC_IS_DIGIT(c))
      {
        break;
      }

      jsc_advance_position(ctx);

      if (number_length < JSC_MAX_NUMBER_LENGTH)
      {
        ctx->number_buffer[number_length++] = c;
      }
    }

    if (jsc_peek(ctx) == '.')
    {
      jsc_advance_position(ctx);

      if (number_length < JSC_MAX_NUMBER_LENGTH)
      {
        ctx->number_buffer[number_length++] = '.';
      }

#if defined(__AVX512F__)
      if (ctx->vector_level >= JSC_SIMD_AVX512F &&
          ctx->position + (1 << 6) <= ctx->source_length)
      {
        __m512i zero = _mm512_set1_epi8('0');
        __m512i nine = _mm512_set1_epi8('9');

        __m512i chunk =
            _mm512_loadu_si512((const __m512i*)(ctx->source + ctx->position));

        __mmask64 is_digit = _mm512_kand(_mm512_cmpge_epi8_mask(chunk, zero),
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
                ctx->number_buffer[number_length++] =
                    ctx->source[ctx->position + i];
              }

              ctx->position += trailing_zeros;
              ctx->column += trailing_zeros;
            }
          }
        }
      }
#elif defined(__AVX2__)
      if (ctx->vector_level >= JSC_SIMD_AVX2 &&
          ctx->position + (1 << 5) <= ctx->source_length)
      {
        __m256i zero = _mm256_set1_epi8('0');
        __m256i nine = _mm256_set1_epi8('9');

        __m256i chunk =
            _mm256_loadu_si256((const __m256i*)(ctx->source + ctx->position));

        __m256i is_digit = _mm256_and_si256(
            _mm256_cmpgt_epi8(chunk,
                              _mm256_sub_epi8(zero, _mm256_set1_epi8(1))),
            _mm256_cmpgt_epi8(_mm256_add_epi8(nine, _mm256_set1_epi8(1)),
                              chunk));

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
                ctx->number_buffer[number_length++] =
                    ctx->source[ctx->position + i];
              }

              ctx->position += trailing_zeros;
              ctx->column += trailing_zeros;
            }
          }
        }
      }
#endif

      while (ctx->position < ctx->source_length)
      {
        char c = jsc_peek(ctx);

        if (!JSC_IS_DIGIT(c))
        {
          break;
        }

        jsc_advance_position(ctx);

        if (number_length < JSC_MAX_NUMBER_LENGTH)
        {
          ctx->number_buffer[number_length++] = c;
        }
      }
    }

    if (ctx->position < ctx->source_length)
    {
      char e = jsc_peek(ctx);

      if (e == 'e' || e == 'E')
      {
        jsc_advance_position(ctx);

        if (number_length < JSC_MAX_NUMBER_LENGTH)
        {
          ctx->number_buffer[number_length++] = e;
        }

        if (ctx->position < ctx->source_length)
        {
          char sign = jsc_peek(ctx);

          if (sign == '+' || sign == '-')
          {
            jsc_advance_position(ctx);

            if (number_length < JSC_MAX_NUMBER_LENGTH)
            {
              ctx->number_buffer[number_length++] = sign;
            }
          }
        }

        bool has_exponent = false;

        while (ctx->position < ctx->source_length)
        {
          char c = jsc_peek(ctx);

          if (!JSC_IS_DIGIT(c))
          {
            break;
          }

          has_exponent = true;
          jsc_advance_position(ctx);

          if (number_length < JSC_MAX_NUMBER_LENGTH)
          {
            ctx->number_buffer[number_length++] = c;
          }
        }

        if (!has_exponent)
        {
          jsc_set_token_error(ctx, "Invalid number literal: missing exponent");
          return;
        }
      }
    }
  }

  ctx->number_buffer[number_length] = '\0';

  if (is_hex)
  {
    token->number_value = strtol(ctx->number_buffer + 2, NULL, (1 << 4));
  }
  else if (is_binary)
  {
    token->number_value = strtol(ctx->number_buffer + 2, NULL, 2);
  }
  else if (is_octal && ctx->number_buffer[0] == '0' &&
           ctx->number_buffer[1] >= '0' && ctx->number_buffer[1] <= '7')
  {
    token->number_value = strtol(ctx->number_buffer, NULL, 8);
  }
  else
  {
    token->number_value = atof(ctx->number_buffer);
  }

  token->length = ctx->position - (token->start - ctx->source);
}

jsc_tokenizer_context* jsc_tokenizer_init(const char* source, size_t length)
{
  jsc_tokenizer_context* ctx =
      (jsc_tokenizer_context*)malloc(sizeof(jsc_tokenizer_context));

  if (!ctx)
  {
    return NULL;
  }

  memset(ctx, 0, sizeof(jsc_tokenizer_context));

  ctx->source = source;
  ctx->source_length = length;
  ctx->position = 0;
  ctx->line = 1;
  ctx->column = 0;

  ctx->in_template = false;
  ctx->template_depth = 0;
  ctx->template_brace_depth = 0;

  ctx->error_message = NULL;

  ctx->vector_level = jsc_get_vector_level();
  ctx->eof_reached = false;

  return ctx;
}

jsc_token jsc_next_token(jsc_tokenizer_context* ctx)
{
  jsc_token token;
  memset(&token, 0, sizeof(jsc_token));

  if (ctx->eof_reached)
  {
    token.type = JSC_TOKEN_EOF;
    return token;
  }

  while (ctx->position < ctx->source_length)
  {
    char c = ctx->source[ctx->position];

    token.start = ctx->source + ctx->position;
    token.line = ctx->line;
    token.column = ctx->column;

    if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
    {
      if (c == '\n')
      {
        ctx->line++;
        ctx->column = 0;
      }
      else
      {
        ctx->column++;
      }

      ctx->position++;

      continue;
    }

    if (c == '/' && ctx->position + 1 < ctx->source_length)
    {
      if (ctx->source[ctx->position + 1] == '/')
      {
        ctx->position += 2;
        ctx->column += 2;

        while (ctx->position < ctx->source_length)
        {
          if (ctx->source[ctx->position] == '\n')
          {
            break;
          }
          ctx->position++;
          ctx->column++;
        }
        continue;
      }
      else if (ctx->source[ctx->position + 1] == '*')
      {
        ctx->position += 2;
        ctx->column += 2;

        while (ctx->position + 1 < ctx->source_length)
        {
          if (ctx->source[ctx->position] == '*' &&
              ctx->source[ctx->position + 1] == '/')
          {
            ctx->position += 2;
            ctx->column += 2;
            break;
          }

          if (ctx->source[ctx->position] == '\n')
          {
            ctx->line++;
            ctx->column = 0;
          }
          else
          {
            ctx->column++;
          }

          ctx->position++;
        }

        continue;
      }
    }

    ctx->position++;
    ctx->column++;

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
      if (ctx->position < ctx->source_length &&
          ctx->source[ctx->position] == '&')
      {
        ctx->position++;
        ctx->column++;
        token.type = JSC_TOKEN_LOGICAL_AND;
      }
      else
      {
        token.type = JSC_TOKEN_BITWISE_AND;
      }
    }
    else if (c == '|')
    {
      if (ctx->position < ctx->source_length &&
          ctx->source[ctx->position] == '|')
      {
        ctx->position++;
        ctx->column++;
        token.type = JSC_TOKEN_LOGICAL_OR;
      }
      else
      {
        token.type = JSC_TOKEN_BITWISE_OR;
      }
    }
    else if (c == '!')
    {
      if (ctx->position < ctx->source_length &&
          ctx->source[ctx->position] == '=')
      {
        ctx->position++;
        ctx->column++;

        if (ctx->position < ctx->source_length &&
            ctx->source[ctx->position] == '=')
        {
          ctx->position++;
          ctx->column++;
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
      if (ctx->position < ctx->source_length)
      {
        if (ctx->source[ctx->position] == '?')
        {
          ctx->position++;
          ctx->column++;

          if (ctx->position < ctx->source_length &&
              ctx->source[ctx->position] == '=')
          {
            ctx->position++;
            ctx->column++;
            token.type = JSC_TOKEN_NULLISH_COALESCING_ASSIGN;
          }
          else
          {
            token.type = JSC_TOKEN_NULLISH_COALESCING;
          }
        }
        else if (ctx->source[ctx->position] == '.')
        {
          ctx->position++;
          ctx->column++;
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
      if (ctx->position + 1 < ctx->source_length &&
          ctx->source[ctx->position] == '.' &&
          ctx->source[ctx->position + 1] == '.')
      {
        ctx->position += 2;
        ctx->column += 2;
        token.type = JSC_TOKEN_SPREAD;
      }
      else
      {
        token.type = JSC_TOKEN_PERIOD;
      }
    }
    else if (c == '+')
    {
      if (ctx->position < ctx->source_length)
      {
        if (ctx->source[ctx->position] == '+')
        {
          ctx->position++;
          ctx->column++;
          token.type = JSC_TOKEN_INCREMENT;
        }
        else if (ctx->source[ctx->position] == '=')
        {
          ctx->position++;
          ctx->column++;
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
      if (ctx->position < ctx->source_length)
      {
        if (ctx->source[ctx->position] == '-')
        {
          ctx->position++;
          ctx->column++;
          token.type = JSC_TOKEN_DECREMENT;
        }
        else if (ctx->source[ctx->position] == '=')
        {
          ctx->position++;
          ctx->column++;
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
      if (ctx->position < ctx->source_length)
      {
        if (ctx->source[ctx->position] == '*')
        {
          ctx->position++;
          ctx->column++;

          if (ctx->position < ctx->source_length &&
              ctx->source[ctx->position] == '=')
          {
            ctx->position++;
            ctx->column++;
            token.type = JSC_TOKEN_EXPONENTIATION_ASSIGN;
          }
          else
          {
            token.type = JSC_TOKEN_EXPONENTIATION;
          }
        }
        else if (ctx->source[ctx->position] == '=')
        {
          ctx->position++;
          ctx->column++;
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
      if (ctx->position < ctx->source_length &&
          ctx->source[ctx->position] == '=')
      {
        ctx->position++;
        ctx->column++;
        token.type = JSC_TOKEN_DIVIDE_ASSIGN;
      }
      else
      {
        token.type = JSC_TOKEN_DIVIDE;
      }
    }
    else if (c == '=')
    {
      if (ctx->position < ctx->source_length)
      {
        if (ctx->source[ctx->position] == '=')
        {
          ctx->position++;
          ctx->column++;
          if (ctx->position < ctx->source_length &&
              ctx->source[ctx->position] == '=')
          {
            ctx->position++;
            ctx->column++;
            token.type = JSC_TOKEN_STRICT_EQUAL;
          }
          else
          {
            token.type = JSC_TOKEN_EQUAL;
          }
        }
        else if (ctx->source[ctx->position] == '>')
        {
          ctx->position++;
          ctx->column++;
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
      if (ctx->position < ctx->source_length)
      {
        if (ctx->source[ctx->position] == '<')
        {
          ctx->position++;
          ctx->column++;
          if (ctx->position < ctx->source_length &&
              ctx->source[ctx->position] == '=')
          {
            ctx->position++;
            ctx->column++;
            token.type = JSC_TOKEN_LEFT_SHIFT_ASSIGN;
          }
          else
          {
            token.type = JSC_TOKEN_LEFT_SHIFT;
          }
        }
        else if (ctx->source[ctx->position] == '=')
        {
          ctx->position++;
          ctx->column++;
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
      if (ctx->position < ctx->source_length)
      {
        if (ctx->source[ctx->position] == '>')
        {
          ctx->position++;
          ctx->column++;

          if (ctx->position < ctx->source_length &&
              ctx->source[ctx->position] == '>')
          {
            ctx->position++;
            ctx->column++;

            if (ctx->position < ctx->source_length &&
                ctx->source[ctx->position] == '=')
            {
              ctx->position++;
              ctx->column++;
              token.type = JSC_TOKEN_UNSIGNED_RIGHT_SHIFT_ASSIGN;
            }
            else
            {
              token.type = JSC_TOKEN_UNSIGNED_RIGHT_SHIFT;
            }
          }
          else if (ctx->position < ctx->source_length &&
                   ctx->source[ctx->position] == '=')
          {
            ctx->position++;
            ctx->column++;
            token.type = JSC_TOKEN_RIGHT_SHIFT_ASSIGN;
          }
          else
          {
            token.type = JSC_TOKEN_RIGHT_SHIFT;
          }
        }
        else if (ctx->source[ctx->position] == '=')
        {
          ctx->position++;
          ctx->column++;
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
      size_t start = ctx->position;
      size_t string_length = 0;

      while (ctx->position < ctx->source_length)
      {
        char strchar = ctx->source[ctx->position];
        ctx->position++;
        ctx->column++;

        if (strchar == '\'')
        {
          break;
        }

        if (strchar == '\\' && ctx->position < ctx->source_length)
        {
          ctx->position++;
          ctx->column++;
        }

        string_length++;
      }

      size_t len = ctx->position - start - 1;
      token.string_value.data = malloc(len + 1);

      size_t j = 0;

      for (size_t i = 0; i < len; i++)
      {
        char ch = ctx->source[start + i];
        if (ch == '\\' && i + 1 < len)
        {
          i++;
          ch = ctx->source[start + i];
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
      size_t start = ctx->position;
      size_t string_length = 0;

      while (ctx->position < ctx->source_length)
      {
        char strchar = ctx->source[ctx->position];
        ctx->position++;
        ctx->column++;

        if (strchar == '"')
        {
          break;
        }

        if (strchar == '\\' && ctx->position < ctx->source_length)
        {
          ctx->position++;
          ctx->column++;
        }

        string_length++;
      }

      size_t len = ctx->position - start - 1;
      token.string_value.data = malloc(len + 1);

      size_t j = 0;

      for (size_t i = 0; i < len; i++)
      {
        char ch = ctx->source[start + i];

        if (ch == '\\' && i + 1 < len)
        {
          i++;
          ch = ctx->source[start + i];
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

      size_t start = ctx->position;
      size_t string_length = 0;

      while (ctx->position < ctx->source_length)
      {
        char strchar = ctx->source[ctx->position];
        ctx->position++;
        ctx->column++;

        if (strchar == '`')
        {
          break;
        }

        if (strchar == '\\' && ctx->position < ctx->source_length)
        {
          ctx->position++;
          ctx->column++;
        }

        string_length++;
      }

      size_t len = ctx->position - start - 1;
      token.string_value.data = malloc(len + 1);

      size_t j = 0;
      for (size_t i = 0; i < len; i++)
      {
        char ch = ctx->source[start + i];
        if (ch == '\\' && i + 1 < len)
        {
          i++;
          ch = ctx->source[start + i];
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
      size_t start = ctx->position - 1;

      while (ctx->position < ctx->source_length &&
             ((ctx->source[ctx->position] >= '0' &&
               ctx->source[ctx->position] <= '9') ||
              ctx->source[ctx->position] == '.'))
      {
        ctx->position++;
        ctx->column++;
      }

      size_t len = ctx->position - start;
      char* numstr = malloc(len + 1);
      memcpy(numstr, ctx->source + start, len);
      numstr[len] = '\0';
      token.number_value = atof(numstr);
      free(numstr);
    }
    else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' ||
             c == '$')
    {
      size_t start = ctx->position - 1;

      while (ctx->position < ctx->source_length &&
             ((ctx->source[ctx->position] >= 'a' &&
               ctx->source[ctx->position] <= 'z') ||
              (ctx->source[ctx->position] >= 'A' &&
               ctx->source[ctx->position] <= 'Z') ||
              (ctx->source[ctx->position] >= '0' &&
               ctx->source[ctx->position] <= '9') ||
              ctx->source[ctx->position] == '_' ||
              ctx->source[ctx->position] == '$'))
      {
        ctx->position++;
        ctx->column++;
      }

      size_t len = ctx->position - start;

      if (len == 2 && strncmp(ctx->source + start, "if", 2) == 0)
      {
        token.type = JSC_TOKEN_IF;
      }
      else if (len == 4 && strncmp(ctx->source + start, "else", 4) == 0)
      {
        token.type = JSC_TOKEN_ELSE;
      }
      else if (len == 3 && strncmp(ctx->source + start, "var", 3) == 0)
      {
        token.type = JSC_TOKEN_VAR;
      }
      else if (len == 6 && strncmp(ctx->source + start, "return", 6) == 0)
      {
        token.type = JSC_TOKEN_RETURN;
      }
      else if (len == 4 && strncmp(ctx->source + start, "true", 4) == 0)
      {
        token.type = JSC_TOKEN_TRUE;
      }
      else if (len == 5 && strncmp(ctx->source + start, "false", 5) == 0)
      {
        token.type = JSC_TOKEN_FALSE;
      }
      else if (len == 3 && strncmp(ctx->source + start, "for", 3) == 0)
      {
        token.type = JSC_TOKEN_FOR;
      }
      else if (len == 5 && strncmp(ctx->source + start, "while", 5) == 0)
      {
        token.type = JSC_TOKEN_WHILE;
      }
      else if (len == 5 && strncmp(ctx->source + start, "break", 5) == 0)
      {
        token.type = JSC_TOKEN_BREAK;
      }
      else if (len == 8 && strncmp(ctx->source + start, "continue", 8) == 0)
      {
        token.type = JSC_TOKEN_CONTINUE;
      }
      else if (len == 2 && strncmp(ctx->source + start, "do", 2) == 0)
      {
        token.type = JSC_TOKEN_DO;
      }
      else if (len == 6 && strncmp(ctx->source + start, "switch", 6) == 0)
      {
        token.type = JSC_TOKEN_SWITCH;
      }
      else if (len == 4 && strncmp(ctx->source + start, "case", 4) == 0)
      {
        token.type = JSC_TOKEN_CASE;
      }
      else if (len == 7 && strncmp(ctx->source + start, "default", 7) == 0)
      {
        token.type = JSC_TOKEN_DEFAULT;
      }
      else if (len == 4 && strncmp(ctx->source + start, "null", 4) == 0)
      {
        token.type = JSC_TOKEN_NULL;
      }
      else if (len == 3 && strncmp(ctx->source + start, "let", 3) == 0)
      {
        token.type = JSC_TOKEN_LET;
      }
      else if (len == 5 && strncmp(ctx->source + start, "const", 5) == 0)
      {
        token.type = JSC_TOKEN_CONST;
      }
      else if (len == 5 && strncmp(ctx->source + start, "class", 5) == 0)
      {
        token.type = JSC_TOKEN_CLASS;
      }
      else if (len == 8 && strncmp(ctx->source + start, "function", 8) == 0)
      {
        token.type = JSC_TOKEN_FUNCTION;
      }
      else if (len == 5 && strncmp(ctx->source + start, "async", 5) == 0)
      {
        token.type = JSC_TOKEN_ASYNC;
      }
      else if (len == 5 && strncmp(ctx->source + start, "await", 5) == 0)
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
      jsc_set_token_error(ctx, error_msg);
    }

    token.length = ctx->position - (token.start - ctx->source);

    return token;
  }

  token.type = JSC_TOKEN_EOF;
  token.start = ctx->source + ctx->position;
  token.length = 0;
  ctx->eof_reached = true;

  return token;
}

static void jsc_vec_scan_identifier(jsc_tokenizer_context* ctx)
{
  jsc_token token;
  memset(&token, 0, sizeof(jsc_token));

  token.type = JSC_TOKEN_IDENTIFIER;
  token.start = ctx->source + ctx->position;
  token.line = ctx->line;
  token.column = ctx->column;

  char first_char = jsc_peek(ctx);
  jsc_advance_position(ctx);

  size_t identifier_length = 1;
  ctx->identifier_buffer[0] = first_char;

  size_t remaining = ctx->source_length - ctx->position;

#if defined(__AVX512F__)
  if (ctx->vector_level >= JSC_SIMD_AVX512F && remaining >= (1 << 6))
  {
    const __m512i underscore = _mm512_set1_epi8('_');
    const __m512i dollar = _mm512_set1_epi8('$');
    const __m512i zero = _mm512_set1_epi8('0');
    const __m512i nine = _mm512_set1_epi8('9');
    const __m512i a_lower = _mm512_set1_epi8('a');
    const __m512i z_lower = _mm512_set1_epi8('z');
    const __m512i a_upper = _mm512_set1_epi8('A');
    const __m512i z_upper = _mm512_set1_epi8('Z');

    while (remaining >= (1 << 6) &&
           identifier_length + (1 << 6) < JSC_MAX_IDENTIFIER_LENGTH)
    {
      _mm_prefetch(ctx->source + ctx->position + JSC_PREFETCH_DISTANCE,
                   _MM_HINT_T0);

      __m512i chunk =
          _mm512_loadu_si512((const __m512i*)(ctx->source + ctx->position));

      __mmask64 is_underscore = _mm512_cmpeq_epi8_mask(chunk, underscore);
      __mmask64 is_dollar = _mm512_cmpeq_epi8_mask(chunk, dollar);
      __mmask64 is_digit = _mm512_kand(_mm512_cmpge_epi8_mask(chunk, zero),
                                       _mm512_cmple_epi8_mask(chunk, nine));
      __mmask64 is_lower = _mm512_kand(_mm512_cmpge_epi8_mask(chunk, a_lower),
                                       _mm512_cmple_epi8_mask(chunk, z_lower));
      __mmask64 is_upper = _mm512_kand(_mm512_cmpge_epi8_mask(chunk, a_upper),
                                       _mm512_cmple_epi8_mask(chunk, z_upper));

      __mmask64 is_id_part =
          _mm512_kor(_mm512_kor(_mm512_kor(is_underscore, is_dollar), is_digit),
                     _mm512_kor(is_lower, is_upper));

      if (is_id_part == 0)
      {
        break;
      }

      if (is_id_part == UINT64_MAX)
      {
        _mm512_storeu_si512(
            (__m512i*)(ctx->identifier_buffer + identifier_length), chunk);
        identifier_length += (1 << 6);
        ctx->position += (1 << 6);
        ctx->column += (1 << 6);
        remaining -= (1 << 6);
        continue;
      }

      int trailing_zeros = _tzcnt_u64(~is_id_part);

      if (trailing_zeros > 0)
      {
        for (int i = 0; i < trailing_zeros; i++)
        {
          ctx->identifier_buffer[identifier_length++] =
              ctx->source[ctx->position++];
          ctx->column++;
        }

        remaining = ctx->source_length - ctx->position;
      }

      break;
    }
  }
#elif defined(__AVX2__)
  if (ctx->vector_level >= JSC_SIMD_AVX2 && remaining >= (1 << 5))
  {
    const __m256i underscore = _mm256_set1_epi8('_');
    const __m256i dollar = _mm256_set1_epi8('$');
    const __m256i zero = _mm256_set1_epi8('0');
    const __m256i nine = _mm256_set1_epi8('9');
    const __m256i a_lower = _mm256_set1_epi8('a');
    const __m256i z_lower = _mm256_set1_epi8('z');
    const __m256i a_upper = _mm256_set1_epi8('A');
    const __m256i z_upper = _mm256_set1_epi8('Z');

    while (remaining >= (1 << 5) &&
           identifier_length + (1 << 5) < JSC_MAX_IDENTIFIER_LENGTH)
    {
      _mm_prefetch(ctx->source + ctx->position + JSC_PREFETCH_DISTANCE,
                   _MM_HINT_T0);

      __m256i chunk =
          _mm256_loadu_si256((const __m256i*)(ctx->source + ctx->position));

      __m256i is_underscore = _mm256_cmpeq_epi8(chunk, underscore);
      __m256i is_dollar = _mm256_cmpeq_epi8(chunk, dollar);
      __m256i is_digit = _mm256_and_si256(
          _mm256_cmpgt_epi8(chunk, _mm256_sub_epi8(zero, _mm256_set1_epi8(1))),
          _mm256_cmpgt_epi8(_mm256_add_epi8(nine, _mm256_set1_epi8(1)), chunk));
      __m256i is_lower = _mm256_and_si256(
          _mm256_cmpgt_epi8(chunk,
                            _mm256_sub_epi8(a_lower, _mm256_set1_epi8(1))),
          _mm256_cmpgt_epi8(_mm256_add_epi8(z_lower, _mm256_set1_epi8(1)),
                            chunk));
      __m256i is_upper = _mm256_and_si256(
          _mm256_cmpgt_epi8(chunk,
                            _mm256_sub_epi8(a_upper, _mm256_set1_epi8(1))),
          _mm256_cmpgt_epi8(_mm256_add_epi8(z_upper, _mm256_set1_epi8(1)),
                            chunk));

      __m256i is_id_part = _mm256_or_si256(
          _mm256_or_si256(_mm256_or_si256(is_underscore, is_dollar), is_digit),
          _mm256_or_si256(is_lower, is_upper));

      uint32_t mask = _mm256_movemask_epi8(is_id_part);

      if (mask == 0)
      {
        break;
      }

      if (mask == 0xFFFFFFFF)
      {
        _mm256_storeu_si256(
            (__m256i*)(ctx->identifier_buffer + identifier_length), chunk);
        identifier_length += (1 << 5);
        ctx->position += (1 << 5);
        ctx->column += (1 << 5);
        remaining -= (1 << 5);
        continue;
      }

      int trailing_zeros = __builtin_ctz(~mask);

      if (trailing_zeros > 0)
      {
        for (int i = 0; i < trailing_zeros; i++)
        {
          ctx->identifier_buffer[identifier_length++] =
              ctx->source[ctx->position++];
          ctx->column++;
        }

        remaining = ctx->source_length - ctx->position;
      }

      break;
    }
  }
#endif

  while (ctx->position < ctx->source_length &&
         identifier_length < JSC_MAX_IDENTIFIER_LENGTH)
  {
    char c = jsc_peek(ctx);

    if (!JSC_IS_IDENTIFIER_PART(c))
    {
      break;
    }

    jsc_advance_position(ctx);
    ctx->identifier_buffer[identifier_length++] = c;
  }

  ctx->identifier_buffer[identifier_length] = '\0';
  token.length = identifier_length;

  if (jsc_check_keyword(ctx->identifier_buffer, identifier_length, &token))
  {
    ctx->current = token;
    return;
  }

  ctx->current = token;
}

static void jsc_scan_string(jsc_tokenizer_context* ctx, char quote)
{
  jsc_token token;
  memset(&token, 0, sizeof(jsc_token));

  token.type = JSC_TOKEN_STRING;
  token.start = ctx->source + ctx->position - 1;
  token.line = ctx->line;
  token.column = ctx->column - 1;

  size_t string_length = 0;

  while (ctx->position < ctx->source_length)
  {
    char c = jsc_peek(ctx);

    if (c == '\0' || JSC_IS_LINE_TERMINATOR(c))
    {
      jsc_set_token_error(ctx, "Unterminated string literal");
      ctx->current = token;
      return;
    }

    jsc_advance_position(ctx);

    if (c == quote)
    {
      break;
    }

    if (c == '\\')
    {
      if (ctx->position >= ctx->source_length)
      {
        jsc_set_token_error(ctx, "Unterminated string literal");
        ctx->current = token;
        return;
      }

      char escape = jsc_peek(ctx);
      jsc_advance_position(ctx);

      switch (escape)
      {
      case '\'':
      case '"':
      case '\\':
      case '/':
        ctx->string_buffer[string_length++] = escape;
        break;
      case 'b':
        ctx->string_buffer[string_length++] = '\b';
        break;
      case 'f':
        ctx->string_buffer[string_length++] = '\f';
        break;
      case 'n':
        ctx->string_buffer[string_length++] = '\n';
        break;
      case 'r':
        ctx->string_buffer[string_length++] = '\r';
        break;
      case 't':
        ctx->string_buffer[string_length++] = '\t';
        break;
      case 'v':
        ctx->string_buffer[string_length++] = '\v';
        break;
      case 'u':
      {
        if (ctx->position + 3 >= ctx->source_length)
        {
          jsc_set_token_error(ctx, "Invalid Unicode escape sequence");
          ctx->current = token;
          return;
        }

        uint32_t hex_value = 0;
        for (int i = 0; i < 4; i++)
        {
          char hex = jsc_peek(ctx);
          jsc_advance_position(ctx);

          if (!JSC_IS_HEX_DIGIT(hex))
          {
            jsc_set_token_error(ctx, "Invalid Unicode escape sequence");
            ctx->current = token;
            return;
          }

          hex_value = (hex_value << 4) | jsc_hex_value[(unsigned char)hex];
        }

        if (hex_value < 0x80)
        {
          ctx->string_buffer[string_length++] = (char)hex_value;
        }
        else if (hex_value < 0x800)
        {
          ctx->string_buffer[string_length++] = (char)(0xC0 | (hex_value >> 6));
          ctx->string_buffer[string_length++] =
              (char)(0x80 | (hex_value & 0x3F));
        }
        else
        {
          ctx->string_buffer[string_length++] =
              (char)(0xE0 | (hex_value >> 12));
          ctx->string_buffer[string_length++] =
              (char)(0x80 | ((hex_value >> 6) & 0x3F));
          ctx->string_buffer[string_length++] =
              (char)(0x80 | (hex_value & 0x3F));
        }
        break;
      }
      case 'x':
      {
        if (ctx->position + 1 >= ctx->source_length)
        {
          jsc_set_token_error(ctx, "Invalid hex escape sequence");
          ctx->current = token;
          return;
        }

        char hex1 = jsc_peek(ctx);
        jsc_advance_position(ctx);
        char hex2 = jsc_peek(ctx);
        jsc_advance_position(ctx);

        if (!JSC_IS_HEX_DIGIT(hex1) || !JSC_IS_HEX_DIGIT(hex2))
        {
          jsc_set_token_error(ctx, "Invalid hex escape sequence");
          ctx->current = token;
          return;
        }

        uint8_t hex_value = (jsc_hex_value[(unsigned char)hex1] << 4) |
                            jsc_hex_value[(unsigned char)hex2];
        ctx->string_buffer[string_length++] = (char)hex_value;
        break;
      }
      default:
        ctx->string_buffer[string_length++] = escape;
        break;
      }
    }
    else
    {
      ctx->string_buffer[string_length++] = c;
    }

    if (string_length >= JSC_MAX_STRING_LENGTH - 4)
    {
      jsc_set_token_error(ctx, "String literal too long");
      ctx->current = token;
      return;
    }
  }

  token.length = ctx->position - (token.start - ctx->source);

  ctx->string_buffer[string_length] = '\0';
  token.string_value.data = malloc(string_length + 1);
  memcpy(token.string_value.data, ctx->string_buffer, string_length + 1);
  token.string_value.length = string_length;

  ctx->current = token;
}

void jsc_tokenizer_free(jsc_tokenizer_context* ctx)
{
  if (ctx)
  {
    if (ctx->error_message)
    {
      free(ctx->error_message);
    }

    if (ctx->current.type == JSC_TOKEN_STRING)
    {
      free(ctx->current.string_value.data);
    }
    else if (ctx->current.type == JSC_TOKEN_REGEXP)
    {
      free(ctx->current.regexp_value.data);
      free(ctx->current.regexp_value.flags);
    }

    free(ctx);
  }
}

bool jsc_tokenizer_has_error(jsc_tokenizer_context* ctx)
{
  return ctx->error_message != NULL;
}

const char* jsc_tokenizer_get_error(jsc_tokenizer_context* ctx)
{
  return ctx->error_message;
}

const char* jsc_token_type_to_string(jsc_token_type type)
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
