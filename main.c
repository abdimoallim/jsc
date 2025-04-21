#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tokenizer.h"

void tokenize_and_print(const char *source, const char *description)
{
  printf("tokenizing %s:\n", description);

  size_t length = strlen(source);
  jsc_tokenizer_state *state = jsc_tokenizer_init(source, length);

  if (!state)
  {
    printf("jsc_tokenizer_init");
    return;
  }

  while (1)
  {
    jsc_token token = jsc_next_token(state);

    if (jsc_tokenizer_has_error(state))
    {
      printf("error: %s\n", jsc_tokenizer_get_error(state));
      break;
    }

    printf("[%3d:%2d] %-15s", token.line, token.column, jsc_token_type_to_string(token.type));

    if (token.type == JSC_TOKEN_IDENTIFIER)
    {
      printf(" '%.*s'", (int)token.length, token.start);
    }
    else if (token.type == JSC_TOKEN_STRING)
    {
      printf(" '%s'", token.string_value.data);
    }
    else if (token.type == JSC_TOKEN_NUMBER)
    {
      printf(" %g", token.number_value);
    }
    else if (token.type == JSC_TOKEN_REGEXP)
    {
      printf(" /%s/%s", token.regexp_value.data, token.regexp_value.flags);
    }

    printf("\n");

    if (token.type == JSC_TOKEN_EOF)
    {
      break;
    }
  }

  jsc_tokenizer_free(state);

  printf("\n");
}

int main()
{
  jsc_vector_level level = jsc_get_vector_level();
  printf("detected simd level: %d\n\n", level);

  const char *js_simple = "var x = 17 + 8;";
  tokenize_and_print(js_simple, "simple assignment");

  const char *js_keywords =
      "if (a) { "
      "  return true; "
      "} else { "
      "  return false; "
      "}";
  tokenize_and_print(js_keywords, "if-else statement");

  const char *js_strings =
      "var s = 'single'; "
      "var d = \"double\"; "
      "var t = `template ${1 + 2} string`;";
  tokenize_and_print(js_strings, "string literals");

  const char *js_regex =
      "var r = /^[a-z]+$/i; "
      "var m = r.test('a');";
  tokenize_and_print(js_regex, "regular expressions");

  const char *js_operators =
      "a + b - c * d / e % f && g || "
      "h ?? i == j != k === l !== m < "
      "n > o <= p >= q << r >> s >>> t";
  tokenize_and_print(js_operators, "operators");

  const char *js_es6 =
      "const f = (a, b) => a + b; "
      "class C { "
      "  constructor(n) { this.n = n; } "
      "}";
  tokenize_and_print(js_es6, "es6 features");

  const char *js_nullish =
      "const v = a?.b ?? 'c'; "
      "x &&= y; "
      "x ||= z; "
      "x ??= w;";
  tokenize_and_print(js_nullish, "nullish operators");

  const char *js_comments =
      "// line comment "
      "var x = 10; /* block comment */ "
      "/* multi "
      "   line "
      "   comment */";
  tokenize_and_print(js_comments, "comments");

  const char *js_func_control_flow =
      "function f(a, b) { "
      "  for (let i = 0; i < a.length; i++) { "
      "    if (a[i] > b) { "
      "      return a.slice(0, i); "
      "    } "
      "  } "
      "  return a; "
      "}";
  tokenize_and_print(js_func_control_flow, "function with control flow");

  return 0;
}
