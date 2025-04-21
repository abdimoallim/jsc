#ifndef JSC_TOKENIZER_H
#define JSC_TOKENIZER_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
#include <x86intrin.h>
#endif

#define JSC_TOKEN_BUFFER_SIZE (1 << 13)
#define JSC_MAX_IDENTIFIER_LENGTH (1 << 12)
#define JSC_MAX_STRING_LENGTH (1 << 16)
#define JSC_MAX_NUMBER_LENGTH (1 << 7)
#define JSC_MAX_REGEXP_LENGTH (1 << 12)

#if defined(_MSC_VER)
#define JSC_FORCE_INLINE __forceinline
#define JSC_ALIGN(x) __declspec(align(x))
#define JSC_RESTRICT __restrict
#elif defined(__GNUC__) || defined(__clang__)
#define JSC_FORCE_INLINE __attribute__((always_inline)) inline
#define JSC_ALIGN(x) __attribute__((aligned(x)))
#define JSC_RESTRICT __restrict
#endif

#define JSC_LIKELY(x) __builtin_expect(!!(x), 1)
#define JSC_UNLIKELY(x) __builtin_expect(!!(x), 0)

#define JSC_VECTOR_WIDTH_BYTES (1 << 6)
#define JSC_PREFETCH_DISTANCE (1 << 10)

typedef enum
{
  JSC_TOKEN_NONE = 0,

  JSC_TOKEN_IDENTIFIER,
  JSC_TOKEN_STRING,
  JSC_TOKEN_NUMBER,
  JSC_TOKEN_REGEXP,
  JSC_TOKEN_TEMPLATE,

  JSC_TOKEN_BREAK,
  JSC_TOKEN_CASE,
  JSC_TOKEN_CATCH,
  JSC_TOKEN_CLASS,
  JSC_TOKEN_CONST,
  JSC_TOKEN_CONTINUE,
  JSC_TOKEN_DEBUGGER,
  JSC_TOKEN_DEFAULT,
  JSC_TOKEN_DELETE,
  JSC_TOKEN_DO,
  JSC_TOKEN_ELSE,
  JSC_TOKEN_EXPORT,
  JSC_TOKEN_EXTENDS,
  JSC_TOKEN_FINALLY,
  JSC_TOKEN_FOR,
  JSC_TOKEN_FUNCTION,
  JSC_TOKEN_IF,
  JSC_TOKEN_IMPORT,
  JSC_TOKEN_IN,
  JSC_TOKEN_INSTANCEOF,
  JSC_TOKEN_NEW,
  JSC_TOKEN_RETURN,
  JSC_TOKEN_SUPER,
  JSC_TOKEN_SWITCH,
  JSC_TOKEN_THIS,
  JSC_TOKEN_THROW,
  JSC_TOKEN_TRY,
  JSC_TOKEN_TYPEOF,
  JSC_TOKEN_VAR,
  JSC_TOKEN_VOID,
  JSC_TOKEN_WHILE,
  JSC_TOKEN_WITH,
  JSC_TOKEN_YIELD,
  JSC_TOKEN_AWAIT,
  JSC_TOKEN_ASYNC,
  JSC_TOKEN_LET,
  JSC_TOKEN_STATIC,

  JSC_TOKEN_TRUE,
  JSC_TOKEN_FALSE,
  JSC_TOKEN_NULL,
  JSC_TOKEN_UNDEFINED,

  JSC_TOKEN_ASSIGN,
  JSC_TOKEN_PLUS,
  JSC_TOKEN_MINUS,
  JSC_TOKEN_MULTIPLY,
  JSC_TOKEN_DIVIDE,
  JSC_TOKEN_MODULO,
  JSC_TOKEN_INCREMENT,
  JSC_TOKEN_DECREMENT,
  JSC_TOKEN_EQUAL,
  JSC_TOKEN_NOT_EQUAL,
  JSC_TOKEN_STRICT_EQUAL,
  JSC_TOKEN_STRICT_NOT_EQUAL,
  JSC_TOKEN_GREATER_THAN,
  JSC_TOKEN_LESS_THAN,
  JSC_TOKEN_GREATER_THAN_EQUAL,
  JSC_TOKEN_LESS_THAN_EQUAL,
  JSC_TOKEN_LOGICAL_AND,
  JSC_TOKEN_LOGICAL_OR,
  JSC_TOKEN_LOGICAL_NOT,
  JSC_TOKEN_BITWISE_AND,
  JSC_TOKEN_BITWISE_OR,
  JSC_TOKEN_BITWISE_XOR,
  JSC_TOKEN_BITWISE_NOT,
  JSC_TOKEN_LEFT_SHIFT,
  JSC_TOKEN_RIGHT_SHIFT,
  JSC_TOKEN_UNSIGNED_RIGHT_SHIFT,
  JSC_TOKEN_PLUS_ASSIGN,
  JSC_TOKEN_MINUS_ASSIGN,
  JSC_TOKEN_MULTIPLY_ASSIGN,
  JSC_TOKEN_DIVIDE_ASSIGN,
  JSC_TOKEN_MODULO_ASSIGN,
  JSC_TOKEN_LEFT_SHIFT_ASSIGN,
  JSC_TOKEN_RIGHT_SHIFT_ASSIGN,
  JSC_TOKEN_UNSIGNED_RIGHT_SHIFT_ASSIGN,
  JSC_TOKEN_BITWISE_AND_ASSIGN,
  JSC_TOKEN_BITWISE_OR_ASSIGN,
  JSC_TOKEN_BITWISE_XOR_ASSIGN,
  JSC_TOKEN_LOGICAL_AND_ASSIGN,
  JSC_TOKEN_LOGICAL_OR_ASSIGN,
  JSC_TOKEN_NULLISH_COALESCING,
  JSC_TOKEN_NULLISH_COALESCING_ASSIGN,
  JSC_TOKEN_OPTIONAL_CHAINING,
  JSC_TOKEN_EXPONENTIATION,
  JSC_TOKEN_EXPONENTIATION_ASSIGN,
  JSC_TOKEN_ARROW,

  JSC_TOKEN_SEMICOLON,
  JSC_TOKEN_COLON,
  JSC_TOKEN_COMMA,
  JSC_TOKEN_PERIOD,
  JSC_TOKEN_QUESTION_MARK,
  JSC_TOKEN_LEFT_PAREN,
  JSC_TOKEN_RIGHT_PAREN,
  JSC_TOKEN_LEFT_BRACKET,
  JSC_TOKEN_RIGHT_BRACKET,
  JSC_TOKEN_LEFT_BRACE,
  JSC_TOKEN_RIGHT_BRACE,
  JSC_TOKEN_SPREAD,

  JSC_TOKEN_TEMPLATE_START,
  JSC_TOKEN_TEMPLATE_MIDDLE,
  JSC_TOKEN_TEMPLATE_END,

  JSC_TOKEN_EOF,
  JSC_TOKEN_ERROR
} jsc_token_type;

typedef enum
{
  JSC_SIMD_NONE = 0,
  JSC_SIMD_SSE2,
  JSC_SIMD_SSE3,
  JSC_SIMD_SSSE3,
  JSC_SIMD_SSE41,
  JSC_SIMD_SSE42,
  JSC_SIMD_AVX,
  JSC_SIMD_AVX2,
  JSC_SIMD_AVX512F
} jsc_vector_level;

typedef struct
{
  jsc_token_type type;
  const char *start;
  size_t length;
  uint32_t line;
  uint32_t column;

  union
  {
    double number_value;

    struct
    {
      char *data;
      size_t length;
    } string_value;

    struct
    {
      char *data;
      size_t length;
      char *flags;
      size_t flags_length;
    } regexp_value;
  };
} jsc_token;

typedef struct jsc_tokenizer_state jsc_tokenizer_state;

struct jsc_tokenizer_state
{
  const char *source;
  size_t source_length;
  size_t position;

  uint32_t line;
  uint32_t column;

  bool in_template;
  int template_depth;
  int template_brace_depth;

  jsc_token current;

  char *error_message;

  JSC_ALIGN(1 << 6)
  char identifier_buffer[JSC_MAX_IDENTIFIER_LENGTH];
  JSC_ALIGN(1 << 6)
  char string_buffer[JSC_MAX_STRING_LENGTH];
  JSC_ALIGN(1 << 6)
  char number_buffer[JSC_MAX_NUMBER_LENGTH];
  JSC_ALIGN(1 << 6)
  char regexp_buffer[JSC_MAX_REGEXP_LENGTH];
  JSC_ALIGN(1 << 6)
  char regexp_flags_buffer[1 << 6];

  jsc_vector_level vector_level;
  bool eof_reached;
};

jsc_vector_level jsc_get_vector_level(void);
jsc_tokenizer_state *jsc_tokenizer_init(const char *source, size_t length);
void jsc_tokenizer_free(jsc_tokenizer_state *state);
jsc_token jsc_next_token(jsc_tokenizer_state *state);
bool jsc_tokenizer_has_error(jsc_tokenizer_state *state);
const char *jsc_tokenizer_get_error(jsc_tokenizer_state *state);
const char *jsc_token_type_to_string(jsc_token_type type);
bool jsc_token_is_keyword(jsc_token_type type);
bool jsc_token_is_operator(jsc_token_type type);
bool jsc_token_is_punctuator(jsc_token_type type);
bool jsc_token_is_literal(jsc_token_type type);

#endif
