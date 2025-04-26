#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jsc_bytecode.h"
#include "jsc_tokenizer.h"
#include "jsc_engine.h"

void test_engine_basic()
{
  printf("testing execution...\n");
  const char* source = "let n = 7 * 3;"
                       "let F = false;"
                       "let p = n - 2;"
                       "n = n - 2;";
                      //  "while (true) {}";
                      //  "function add(a, b) { return a + b; }"
                      //  "let k = add(2, 3);";
                      //  "for (let i = 0; i < 3; i++) {}";
                      //  "if (true) { n = 1; } else { n = 0; }";
  jsc_value out = jsc_engine_eval(source);
  printf("output: %s\n", jsc_value_to_string(out));
  printf("done\n");
}

void test_bytecode_basic()
{
  printf("testing basic bytecode codegen...\n");

  jsc_bytecode_context* state = jsc_bytecode_create_class(
      "Basic", "java/lang/Object", JSC_ACC_PUBLIC | JSC_ACC_SUPER);

  if (!state)
  {
    printf("jsc_bytecode_create_class\n");
    return;
  }

  jsc_method* constructor =
      jsc_bytecode_add_method(state, "<init>", "()V", JSC_ACC_PUBLIC);

  if (!constructor)
  {
    printf("jsc_bytecode_add_method\n");
    jsc_bytecode_free(state);
    return;
  }

  uint8_t* code = malloc(1 << 6);

  if (!code)
  {
    printf("malloc\n");
    jsc_bytecode_free(state);
    return;
  }

  uint32_t code_length = 0;

  code[code_length++] = JSC_JVM_ALOAD_0;

  uint16_t init_ref = jsc_bytecode_add_method_reference(
      state, "java/lang/Object", "<init>", "()V");
  code[code_length++] = JSC_JVM_INVOKESPECIAL;
  *(uint16_t*)(code + code_length) = htobe16(init_ref);
  code_length += 2;

  code[code_length++] = JSC_JVM_RETURN;

  jsc_bytecode_add_code_attribute(state, constructor, 1, 1, code, code_length);

  jsc_method* main_method = jsc_bytecode_add_method(
      state, "main", "([Ljava/lang/String;)V", JSC_ACC_PUBLIC | JSC_ACC_STATIC);

  if (!main_method)
  {
    printf("failed to add main method\n");
    free(code);
    jsc_bytecode_free(state);
    return;
  }

  code_length = 0;

  uint16_t system_out = jsc_bytecode_add_field_reference(
      state, "java/lang/System", "out", "Ljava/io/PrintStream;");
  code[code_length++] = JSC_JVM_GETSTATIC;
  *(uint16_t*)(code + code_length) = htobe16(system_out);
  code_length += 2;

  uint16_t msg_str = jsc_bytecode_add_string_constant(state, "hello world");
  code[code_length++] = JSC_JVM_LDC;
  code[code_length++] = (uint8_t)msg_str;

  uint16_t println = jsc_bytecode_add_method_reference(
      state, "java/io/PrintStream", "println", "(Ljava/lang/String;)V");
  code[code_length++] = JSC_JVM_INVOKEVIRTUAL;
  *(uint16_t*)(code + code_length) = htobe16(println);
  code_length += 2;

  code[code_length++] = JSC_JVM_RETURN;

  jsc_bytecode_add_code_attribute(state, main_method, 2, 1, code, code_length);

  free(code);

  if (!jsc_bytecode_write_to_file(state, "Basic.class"))
  {
    printf("jsc_bytecode_write_to_file\n");
    jsc_bytecode_free(state);
    return;
  }

  printf("created Basic.class...\n");
  jsc_bytecode_free(state);
}

void test_bytecode()
{
  printf("testing bytecode module...\n");

  jsc_bytecode_context* state = jsc_bytecode_create_class(
      "TestClass", "java/lang/Object", JSC_ACC_PUBLIC | JSC_ACC_SUPER);

  if (!state)
  {
    printf("jsc_bytecode_create_class\n");
    return;
  }

  jsc_bytecode_set_source_file(state, "Test.js");
  jsc_bytecode_set_version(state, 52, 0);

  jsc_method* constructor =
      jsc_bytecode_create_method(state, "<init>", "()V", JSC_ACC_PUBLIC, 1, 1);

  if (!constructor)
  {
    printf("jsc_bytecode_create_method for constructor\n");
    jsc_bytecode_free(state);
    return;
  }

  jsc_bytecode_emit_constructor(state, constructor, "java/lang/Object");

  jsc_method* main_method =
      jsc_bytecode_create_method(state, "main", "([Ljava/lang/String;)V",
                                 JSC_ACC_PUBLIC | JSC_ACC_STATIC, 10, 5);
  if (!main_method)
  {
    printf("jsc_bytecode_create_method for main\n");
    jsc_bytecode_free(state);
    return;
  }

  jsc_bytecode_emit_load_constant_string(state, main_method, "hello world");
  jsc_bytecode_emit_field_access(state, main_method, JSC_JVM_GETSTATIC,
                                 "java/lang/System", "out",
                                 "Ljava/io/PrintStream;");
  jsc_bytecode_emit_u8(state, main_method, JSC_JVM_SWAP, 0);
  jsc_bytecode_emit_invoke_virtual(state, main_method, "java/io/PrintStream",
                                   "println", "(Ljava/lang/String;)V");

  jsc_bytecode_emit_load_constant_int(state, main_method, 10);
  jsc_bytecode_emit_local_var(state, main_method, JSC_JVM_ISTORE, 1);

  jsc_bytecode_emit_local_var(state, main_method, JSC_JVM_ILOAD, 1);
  jsc_bytecode_emit_load_constant_int(state, main_method, 5);
  jsc_bytecode_emit(state, main_method, JSC_JVM_IADD);
  jsc_bytecode_emit_local_var(state, main_method, JSC_JVM_ISTORE, 2);

  jsc_bytecode_emit_field_access(state, main_method, JSC_JVM_GETSTATIC,
                                 "java/lang/System", "out",
                                 "Ljava/io/PrintStream;");
  jsc_bytecode_emit_local_var(state, main_method, JSC_JVM_ILOAD, 2);
  jsc_bytecode_emit_invoke_virtual(state, main_method, "java/io/PrintStream",
                                   "println", "(I)V");

  jsc_bytecode_emit(state, main_method, JSC_JVM_RETURN);

  jsc_bytecode_emit(state, main_method, JSC_JVM_RETURN);

  for (uint16_t i = 0; i < main_method->attribute_count; i++)
  {
    jsc_attribute* attr = &main_method->attributes[i];
    uint16_t name_index = attr->name_index;

    const jsc_constant_pool_entry* name_entry =
        &state->constant_pool[name_index];
    if (name_entry->tag == JSC_CP_UTF8 && name_entry->utf8_info.length == 4 &&
        memcmp(name_entry->utf8_info.bytes, "Code", 4) == 0)
    {

      uint32_t new_length = attr->length + 12 + 10;
      uint8_t* new_info = (uint8_t*)malloc(new_length);
      if (!new_info)
      {
        printf("memory allocation failed\n");
        jsc_bytecode_free(state);
        return;
      }

      memcpy(new_info, attr->info, attr->length);
      free(attr->info);
      attr->info = new_info;
      attr->length = new_length;

      break;
    }
  }

  jsc_bytecode_emit_local_variable(state, main_method, "sum", "I", 0, 100, 2);
  jsc_bytecode_emit_line_number(state, main_method, 1, 0);

  jsc_method* compute_method = jsc_bytecode_create_method(
      state, "compute", "(II)I", JSC_ACC_PUBLIC | JSC_ACC_STATIC, 2, 2);
  if (!compute_method)
  {
    printf("jsc_bytecode_create_method for compute\n");
    jsc_bytecode_free(state);
    return;
  }

  // jsc_bytecode_emit_local_variable(state, main_method, "sum", "I", 0, 100, 2);
  // jsc_bytecode_emit_line_number(state, main_method, 1, 0);

  // jsc_method* compute_method = jsc_bytecode_create_method(
  //     state, "compute", "(II)I", JSC_ACC_PUBLIC | JSC_ACC_STATIC, 2, 2);
  //
  // if (!compute_method)
  // {
  //   printf("jsc_bytecode_create_method for compute\n");
  //   jsc_bytecode_free(state);
  //   return;
  // }

  jsc_bytecode_emit_local_var(state, compute_method, JSC_JVM_ILOAD, 0);
  jsc_bytecode_emit_local_var(state, compute_method, JSC_JVM_ILOAD, 1);
  jsc_bytecode_emit(state, compute_method, JSC_JVM_IADD);
  jsc_bytecode_emit(state, compute_method, JSC_JVM_IRETURN);

  jsc_method* loop_method = jsc_bytecode_create_method(
      state, "loop", "(I)I", JSC_ACC_PUBLIC | JSC_ACC_STATIC, 3, 3);

  if (!loop_method)
  {
    printf("jsc_bytecode_create_method for loop\n");
    jsc_bytecode_free(state);
    return;
  }

  jsc_bytecode_emit_load_constant_int(state, loop_method, 0);
  jsc_bytecode_emit_local_var(state, loop_method, JSC_JVM_ISTORE, 1);

  uint32_t loop_start = jsc_bytecode_get_method_code_length(loop_method);

  jsc_bytecode_emit_local_var(state, loop_method, JSC_JVM_ILOAD, 1);
  jsc_bytecode_emit_local_var(state, loop_method, JSC_JVM_ILOAD, 0);
  jsc_bytecode_emit(state, loop_method, JSC_JVM_IF_ICMPGE);
  jsc_bytecode_emit_u16(state, loop_method, 0, 13);

  jsc_bytecode_emit_local_var(state, loop_method, JSC_JVM_ILOAD, 1);
  jsc_bytecode_emit_load_constant_int(state, loop_method, 1);
  jsc_bytecode_emit(state, loop_method, JSC_JVM_IADD);
  jsc_bytecode_emit_local_var(state, loop_method, JSC_JVM_ISTORE, 1);

  jsc_bytecode_emit(state, loop_method, JSC_JVM_GOTO);

  uint32_t current_offset = jsc_bytecode_get_method_code_length(loop_method);
  int16_t jump_offset = (int16_t)(loop_start - current_offset);

  jsc_bytecode_emit_u16(state, loop_method, 0, jump_offset - 3);

  jsc_bytecode_emit_local_var(state, loop_method, JSC_JVM_ILOAD, 1);
  jsc_bytecode_emit(state, loop_method, JSC_JVM_IRETURN);

  jsc_field* counter_field = jsc_bytecode_add_field(
      state, "counter", "I", JSC_ACC_PRIVATE | JSC_ACC_STATIC);

  if (!counter_field)
  {
    printf("jsc_bytecode_add_field\n");
    jsc_bytecode_free(state);
    return;
  }

  jsc_method* array_method = jsc_bytecode_create_method(
      state, "createArray", "(I)[I", JSC_ACC_PUBLIC | JSC_ACC_STATIC, 3, 2);

  if (!array_method)
  {
    printf("jsc_bytecode_create_method for array\n");
    jsc_bytecode_free(state);
    return;
  }

  jsc_bytecode_emit_local_var(state, array_method, JSC_JVM_ILOAD, 0);
  jsc_bytecode_emit_u8(state, array_method, JSC_JVM_NEWARRAY, 10);
  jsc_bytecode_emit_local_var(state, array_method, JSC_JVM_ASTORE, 1);

  jsc_bytecode_emit_local_var(state, array_method, JSC_JVM_ALOAD, 1);
  jsc_bytecode_emit_load_constant_int(state, array_method, 0);
  jsc_bytecode_emit_load_constant_int(state, array_method, 100);
  jsc_bytecode_emit(state, array_method, JSC_JVM_IASTORE);

  jsc_bytecode_emit_local_var(state, array_method, JSC_JVM_ALOAD, 1);
  jsc_bytecode_emit(state, array_method, JSC_JVM_ARETURN);

  jsc_method* exception_method = jsc_bytecode_create_method(
      state, "tryCatch", "()V", JSC_ACC_PUBLIC | JSC_ACC_STATIC, 2, 1);

  if (!exception_method)
  {
    printf("jsc_bytecode_create_method for exception\n");
    jsc_bytecode_free(state);
    return;
  }

  uint32_t try_start = jsc_bytecode_get_method_code_length(exception_method);

  jsc_bytecode_emit_load_constant_string(state, exception_method,
                                         "attempt risky operation");
  jsc_bytecode_emit_field_access(state, exception_method, JSC_JVM_GETSTATIC,
                                 "java/lang/System", "out",
                                 "Ljava/io/PrintStream;");
  jsc_bytecode_emit_u8(state, exception_method, JSC_JVM_SWAP, 0);
  jsc_bytecode_emit_invoke_virtual(state, exception_method,
                                   "java/io/PrintStream", "println",
                                   "(Ljava/lang/String;)V");

  jsc_bytecode_emit_u8(state, exception_method, JSC_JVM_ACONST_NULL, 0);
  jsc_bytecode_emit_invoke_virtual(state, exception_method, "java/lang/Object",
                                   "toString", "()Ljava/lang/String;");
  jsc_bytecode_emit(state, exception_method, JSC_JVM_POP);

  uint32_t try_end = jsc_bytecode_get_method_code_length(exception_method);

  jsc_bytecode_emit(state, exception_method, JSC_JVM_GOTO);
  jsc_bytecode_emit_u16(state, exception_method, 0, 13);

  uint32_t handler_start =
      jsc_bytecode_get_method_code_length(exception_method);

  jsc_bytecode_emit(state, exception_method, JSC_JVM_ASTORE_0);
  jsc_bytecode_emit_load_constant_string(state, exception_method,
                                         "caught exception");
  jsc_bytecode_emit_field_access(state, exception_method, JSC_JVM_GETSTATIC,
                                 "java/lang/System", "out",
                                 "Ljava/io/PrintStream;");
  jsc_bytecode_emit_u8(state, exception_method, JSC_JVM_SWAP, 0);
  jsc_bytecode_emit_invoke_virtual(state, exception_method,
                                   "java/io/PrintStream", "println",
                                   "(Ljava/lang/String;)V");

  uint32_t handler_end = jsc_bytecode_get_method_code_length(exception_method);

  jsc_bytecode_emit(state, exception_method, JSC_JVM_RETURN);

  for (uint16_t i = 0; i < exception_method->attribute_count; i++)
  {
    if (strncmp((char*)state
                    ->constant_pool[exception_method->attributes[i].name_index]
                    .utf8_info.bytes,
                "Code", 4) == 0)
    {
      jsc_bytecode_add_exception_table_entry(
          state, &exception_method->attributes[i], try_start, try_end,
          handler_start,
          jsc_bytecode_add_class_constant(state, "java/lang/Exception"));
      break;
    }
  }

  jsc_method* float_method = jsc_bytecode_create_method(
      state, "floatOps", "(FF)F", JSC_ACC_PUBLIC | JSC_ACC_STATIC, 2, 2);
  if (!float_method)
  {
    printf("jsc_bytecode_create_method for float\n");
    jsc_bytecode_free(state);
    return;
  }

  jsc_bytecode_emit_local_var(state, float_method, JSC_JVM_FLOAD, 0);
  jsc_bytecode_emit_local_var(state, float_method, JSC_JVM_FLOAD, 1);
  jsc_bytecode_emit(state, float_method, JSC_JVM_FMUL);
  jsc_bytecode_emit_load_constant_float(state, float_method, 2.5f);
  jsc_bytecode_emit(state, float_method, JSC_JVM_FADD);
  jsc_bytecode_emit(state, float_method, JSC_JVM_FRETURN);

  jsc_method* double_method = jsc_bytecode_create_method(
      state, "doubleOps", "(DD)D", JSC_ACC_PUBLIC | JSC_ACC_STATIC, 2, 2);
  if (!double_method)
  {
    printf("jsc_bytecode_create_method for double\n");
    jsc_bytecode_free(state);
    return;
  }

  jsc_bytecode_emit_local_var(state, double_method, JSC_JVM_DLOAD, 0);
  jsc_bytecode_emit_local_var(state, double_method, JSC_JVM_DLOAD, 0);
  jsc_bytecode_emit(state, double_method, JSC_JVM_DMUL);
  jsc_bytecode_emit_load_constant_double(state, double_method, 1.5);
  jsc_bytecode_emit(state, double_method, JSC_JVM_DADD);
  jsc_bytecode_emit(state, double_method, JSC_JVM_DRETURN);

  jsc_method* string_method = jsc_bytecode_create_method(
      state, "stringCat",
      "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
      JSC_ACC_PUBLIC | JSC_ACC_STATIC, 3, 2);
  if (!string_method)
  {
    printf("jsc_bytecode_create_method for string\n");
    jsc_bytecode_free(state);
    return;
  }

  jsc_bytecode_emit_new(state, string_method, "java/lang/StringBuilder");
  jsc_bytecode_emit(state, string_method, JSC_JVM_DUP);
  jsc_bytecode_emit_invoke_special(state, string_method,
                                   "java/lang/StringBuilder", "<init>", "()V");

  jsc_bytecode_emit_local_var(state, string_method, JSC_JVM_ALOAD, 0);
  jsc_bytecode_emit_invoke_virtual(
      state, string_method, "java/lang/StringBuilder", "append",
      "(Ljava/lang/String;)Ljava/lang/StringBuilder;");

  jsc_bytecode_emit_local_var(state, string_method, JSC_JVM_ALOAD, 1);
  jsc_bytecode_emit_invoke_virtual(
      state, string_method, "java/lang/StringBuilder", "append",
      "(Ljava/lang/String;)Ljava/lang/StringBuilder;");

  jsc_bytecode_emit_invoke_virtual(state, string_method,
                                   "java/lang/StringBuilder", "toString",
                                   "()Ljava/lang/String;");
  jsc_bytecode_emit(state, string_method, JSC_JVM_ARETURN);

  jsc_method* object_method =
      jsc_bytecode_create_method(state, "createObject", "()Ljava/lang/Object;",
                                 JSC_ACC_PUBLIC | JSC_ACC_STATIC, 2, 1);
  if (!object_method)
  {
    printf("jsc_bytecode_create_method for object\n");
    jsc_bytecode_free(state);
    return;
  }

  jsc_bytecode_emit_new(state, object_method, "java/lang/Object");
  jsc_bytecode_emit(state, object_method, JSC_JVM_DUP);
  jsc_bytecode_emit_invoke_special(state, object_method, "java/lang/Object",
                                   "<init>", "()V");
  jsc_bytecode_emit(state, object_method, JSC_JVM_ARETURN);

  jsc_method* interface_method = jsc_bytecode_create_method(
      state, "callInterface", "(Ljava/lang/Runnable;)V",
      JSC_ACC_PUBLIC | JSC_ACC_STATIC, 1, 1);
  if (!interface_method)
  {
    printf("jsc_bytecode_create_method for interface\n");
    jsc_bytecode_free(state);
    return;
  }

  jsc_bytecode_emit_local_var(state, interface_method, JSC_JVM_ALOAD, 0);
  jsc_bytecode_emit_invoke_interface(state, interface_method,
                                     "java/lang/Runnable", "run", "()V", 1);
  jsc_bytecode_emit(state, interface_method, JSC_JVM_RETURN);

  jsc_bytecode_add_interface(state, "java/io/Serializable");

  uint8_t* buffer;
  uint32_t size = jsc_bytecode_write(state, &buffer);

  if (size == 0)
  {
    printf("jsc_bytecode_write\n");
    jsc_bytecode_free(state);
    return;
  }

  FILE* file = fopen("TestClass.class", "wb");

  if (!file)
  {
    printf("failed to open file for writing\n");
    free(buffer);
    jsc_bytecode_free(state);
    return;
  }

  size_t written = fwrite(buffer, 1, size, file);
  fclose(file);

  free(buffer);

  if (written != size)
  {
    printf("failed to write class file\n");
    jsc_bytecode_free(state);
    return;
  }

  if (!jsc_bytecode_write_to_file(state, "TestClass2.class"))
  {
    printf("jsc_bytecode_write_to_file\n");
    jsc_bytecode_free(state);
    return;
  }

  printf("bytecode tests completed...\n");
  printf("created TestClass.class & TestClass2.class\n");

  jsc_bytecode_free(state);
}

void tokenize_and_print(const char* source, const char* description)
{
  printf("tokenizing %s:\n", description);

  size_t length = strlen(source);
  jsc_tokenizer_context* state = jsc_tokenizer_init(source, length);

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

    printf("[%3d:%2d] %-15s", token.line, token.column,
           jsc_token_type_to_string(token.type));

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

void test_tokenize()
{
  jsc_vector_level level = jsc_get_vector_level();
  printf("detected simd level: %d\n\n", level);

  const char* js_simple = "var x = 17 + 8;";
  tokenize_and_print(js_simple, "simple assignment");

  const char* js_keywords = "if (a) { "
                            "  return true; "
                            "} else { "
                            "  return false; "
                            "}";
  tokenize_and_print(js_keywords, "if-else statement");

  const char* js_strings = "var s = 'single'; "
                           "var d = \"double\"; "
                           "var t = `template ${1 + 2} string`;";
  tokenize_and_print(js_strings, "string literals");

  const char* js_regex = "var r = /^[a-z]+$/i; "
                         "var m = r.test('a');";
  tokenize_and_print(js_regex, "regular expressions");

  const char* js_operators = "a + b - c * d / e % f && g || "
                             "h ?? i == j != k === l !== m < "
                             "n > o <= p >= q << r >> s >>> t";
  tokenize_and_print(js_operators, "operators");

  const char* js_es6 = "const f = (a, b) => a + b; "
                       "class C { "
                       "  constructor(n) { this.n = n; } "
                       "}";
  tokenize_and_print(js_es6, "es6 features");

  const char* js_nullish = "const v = a?.b ?? 'c'; "
                           "x &&= y; "
                           "x ||= z; "
                           "x ??= w;";
  tokenize_and_print(js_nullish, "nullish operators");

  const char* js_comments = "// line comment "
                            "var x = 10; /* block comment */ "
                            "/* multi "
                            "   line "
                            "   comment */";
  tokenize_and_print(js_comments, "comments");

  const char* js_func_control_flow = "function f(a, b) { "
                                     "  for (let i = 0; i < a.length; i++) { "
                                     "    if (a[i] > b) { "
                                     "      return a.slice(0, i); "
                                     "    } "
                                     "  } "
                                     "  return a; "
                                     "}";
  tokenize_and_print(js_func_control_flow, "function with control flow");
}

int main()
{

  // test_tokenize();
  // test_bytecode_basic();
  // test_bytecode();
  test_engine_basic();

  return 0;
}
