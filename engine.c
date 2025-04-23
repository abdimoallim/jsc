#include "engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/stat.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#define PATH_SEPARATOR "\\"
#else
#include <unistd.h>
#define PATH_SEPARATOR "/"
#endif

static JavaVMOption jvm_options[] = {{.optionString = "-Djava.class.path=."}};

static JavaVMInitArgs jvm_args = {.version = JNI_VERSION_1_8,
                                  .nOptions = 1,
                                  .options = jvm_options,
                                  .ignoreUnrecognized = JNI_FALSE};

jsc_engine_state* jsc_engine_init(const char* class_name)
{
  jsc_engine_state* state = (jsc_engine_state*)malloc(sizeof(jsc_engine_state));

  if (!state)
  {
    return NULL;
  }

  memset(state, 0, sizeof(jsc_engine_state));

  state->class_name = strdup(class_name);

  if (!state->class_name)
  {
    free(state);
    return NULL;
  }

  state->global_scope = (jsc_scope*)malloc(sizeof(jsc_scope));

  if (!state->global_scope)
  {
    free(state->class_name);
    free(state);
    return NULL;
  }

  memset(state->global_scope, 0, sizeof(jsc_scope));
  state->current_scope = state->global_scope;

  char temp_path[1 << 10];

#if defined(_WIN32) || defined(_WIN64)
  GetTempPath(1 << 10, temp_path);
#else
  strcpy(temp_path, "/tmp/");
#endif

  char dir_name[1 << 10];
  sprintf(dir_name, "%sjsc_engine_%ld", temp_path, (long)time(NULL));

  if (mkdir(dir_name, 0755) != 0)
  {
    free(state->global_scope);
    free(state->class_name);
    free(state);
    return NULL;
  }

  state->temp_dir = strdup(dir_name);

  state->class_path = malloc(strlen(dir_name) + 2);
  sprintf(state->class_path, "%s", dir_name);

  jvm_options[0].optionString = malloc(strlen(state->class_path) + 20);
  sprintf(jvm_options[0].optionString, "-Djava.class.path=%s",
          state->class_path);

  return state;
}

void jsc_engine_free(jsc_engine_state* state)
{
  if (!state)
  {
    return;
  }

  if (state->env)
  {
    if (state->runtime_instance)
    {
      (*state->env)->DeleteGlobalRef(state->env, state->runtime_instance);
    }

    if (state->runtime_class)
    {
      (*state->env)->DeleteGlobalRef(state->env, state->runtime_class);
    }

    if (state->args)
    {
      (*state->env)->DeleteGlobalRef(state->env, state->args);
    }
  }

  if (state->jvm)
  {
    (*state->jvm)->DestroyJavaVM(state->jvm);
  }

  if (state->tokenizer)
  {
    jsc_tokenizer_free(state->tokenizer);
  }

  if (state->bytecode)
  {
    jsc_bytecode_free(state->bytecode);
  }

  if (state->class_name)
  {
    free(state->class_name);
  }

  if (state->error_message)
  {
    free(state->error_message);
  }

  if (state->temp_dir)
  {
    free(state->temp_dir);
  }

  if (state->class_path)
  {
    free(state->class_path);
  }

  if (jvm_options[0].optionString)
  {
    free(jvm_options[0].optionString);
  }

  jsc_scope* scope = state->global_scope;

  while (scope)
  {
    jsc_symbol* symbol = scope->symbols;

    while (symbol)
    {
      jsc_symbol* next = symbol->next;
      free(symbol->name);
      free(symbol);
      symbol = next;
    }

    jsc_scope* next = scope->next_sibling;

    if (scope->function_name)
    {
      free(scope->function_name);
    }

    free(scope);
    scope = next;
  }

  free(state);
}

bool jsc_engine_compile(jsc_engine_state* state, const char* source)
{
  state->tokenizer = jsc_tokenizer_init(source, strlen(source));

  if (!state->tokenizer)
  {
    jsc_engine_error(state, "failed to initialize tokenizer");
    return false;
  }

  state->bytecode = jsc_bytecode_create_class(
      state->class_name, "java/lang/Object", JSC_ACC_PUBLIC | JSC_ACC_SUPER);

  if (!state->bytecode)
  {
    jsc_engine_error(state, "failed to initialize bytecode");
    return false;
  }

  jsc_engine_advance(state);

  jsc_engine_parse_program(state);

  if (state->had_error)
  {
    return false;
  }

  char* class_file =
      malloc(strlen(state->temp_dir) + strlen(state->class_name) + 8);

  if (!class_file)
  {
    jsc_engine_error(state, "jsc_engine_compile malloc");
    return false;
  }

  sprintf(class_file, "%s%s%s.class", state->temp_dir, PATH_SEPARATOR,
          state->class_name);

  if (!jsc_bytecode_write_to_file(state->bytecode, class_file))
  {
    jsc_engine_error(state, "failed to write class file");
    free(class_file);
    return false;
  }

  free(class_file);

  return true;
}

bool jsc_engine_init_jvm(jsc_engine_state* state)
{
  if (state->jvm != NULL)
  {
    return true;
  }

  JNIEnv* env;

  jint result = JNI_CreateJavaVM(&state->jvm, (void**)&env, &jvm_args);

  if (result != JNI_OK)
  {
    jsc_engine_error(state, "failed to create JVM");
    return false;
  }

  state->env = env;

  jclass runtime_class = (*env)->FindClass(env, state->class_name);

  if (runtime_class == NULL)
  {
    (*env)->ExceptionClear(env);
    jsc_engine_error(state, "failed to find compiled class");
    return false;
  }

  state->runtime_class = (*env)->NewGlobalRef(env, runtime_class);
  (*env)->DeleteLocalRef(env, runtime_class);

  jmethodID main_method = (*env)->GetStaticMethodID(
      env, state->runtime_class, "main", "([Ljava/lang/String;)V");

  if (main_method == NULL)
  {
    (*env)->ExceptionClear(env);
    jsc_engine_error(state, "failed to find main method");
    return false;
  }

  state->execute_method = main_method;

  jclass string_class = (*env)->FindClass(env, "java/lang/String");
  if (string_class == NULL)
  {
    (*env)->ExceptionClear(env);
    jsc_engine_error(state, "failed to find String class");
    return false;
  }

  jobjectArray args = (*env)->NewObjectArray(env, 0, string_class, NULL);

  if (args == NULL)
  {
    (*env)->ExceptionClear(env);
    (*env)->DeleteLocalRef(env, string_class);
    jsc_engine_error(state, "failed to create arguments array");
    return false;
  }

  state->args = (*env)->NewGlobalRef(env, args);
  (*env)->DeleteLocalRef(env, args);
  (*env)->DeleteLocalRef(env, string_class);

  return true;
}

bool jsc_engine_load_class(jsc_engine_state* state, const char* class_file)
{
  if (state->env == NULL)
  {
    if (!jsc_engine_init_jvm(state))
    {
      return false;
    }
  }

  jclass class_loader_class =
      (*state->env)->FindClass(state->env, "java/lang/ClassLoader");
  if (class_loader_class == NULL)
  {
    (*state->env)->ExceptionClear(state->env);
    jsc_engine_error(state, "failed to find ClassLoader class");
    return false;
  }

  jmethodID get_system_class_loader =
      (*state->env)
          ->GetStaticMethodID(state->env, class_loader_class,
                              "getSystemClassLoader",
                              "()Ljava/lang/ClassLoader;");

  if (get_system_class_loader == NULL)
  {
    (*state->env)->ExceptionClear(state->env);
    (*state->env)->DeleteLocalRef(state->env, class_loader_class);
    jsc_engine_error(state, "failed to find getSystemClassLoader method");
    return false;
  }

  jobject class_loader =
      (*state->env)
          ->CallStaticObjectMethod(state->env, class_loader_class,
                                   get_system_class_loader);

  if (class_loader == NULL)
  {
    (*state->env)->ExceptionClear(state->env);
    (*state->env)->DeleteLocalRef(state->env, class_loader_class);
    jsc_engine_error(state, "failed to get system class loader");
    return false;
  }

  jmethodID load_class =
      (*state->env)
          ->GetMethodID(state->env, class_loader_class, "loadClass",
                        "(Ljava/lang/String;)Ljava/lang/Class;");

  if (load_class == NULL)
  {
    (*state->env)->ExceptionClear(state->env);
    (*state->env)->DeleteLocalRef(state->env, class_loader);
    (*state->env)->DeleteLocalRef(state->env, class_loader_class);
    jsc_engine_error(state, "failed to find loadClass method");
    return false;
  }

  jstring class_name_jstr =
      (*state->env)->NewStringUTF(state->env, state->class_name);
  if (class_name_jstr == NULL)
  {
    (*state->env)->ExceptionClear(state->env);
    (*state->env)->DeleteLocalRef(state->env, class_loader);
    (*state->env)->DeleteLocalRef(state->env, class_loader_class);
    jsc_engine_error(state, "failed to create class name string");
    return false;
  }

  jclass loaded_class = (*state->env)
                            ->CallObjectMethod(state->env, class_loader,
                                               load_class, class_name_jstr);

  (*state->env)->DeleteLocalRef(state->env, class_name_jstr);

  if (loaded_class == NULL || (*state->env)->ExceptionCheck(state->env))
  {
    (*state->env)->ExceptionClear(state->env);
    (*state->env)->DeleteLocalRef(state->env, class_loader);
    (*state->env)->DeleteLocalRef(state->env, class_loader_class);
    jsc_engine_error(state, "failed to load class");
    return false;
  }

  state->runtime_class = (*state->env)->NewGlobalRef(state->env, loaded_class);
  (*state->env)->DeleteLocalRef(state->env, loaded_class);

  jmethodID main_method =
      (*state->env)
          ->GetStaticMethodID(state->env, state->runtime_class, "main",
                              "([Ljava/lang/String;)V");

  if (main_method == NULL)
  {
    (*state->env)->ExceptionClear(state->env);
    (*state->env)->DeleteLocalRef(state->env, class_loader);
    (*state->env)->DeleteLocalRef(state->env, class_loader_class);
    jsc_engine_error(state, "failed to find main method");
    return false;
  }

  state->execute_method = main_method;

  (*state->env)->DeleteLocalRef(state->env, class_loader);
  (*state->env)->DeleteLocalRef(state->env, class_loader_class);

  return true;
}

jsc_value jsc_engine_run(jsc_engine_state* state)
{
  if (state->had_error)
  {
    return jsc_value_create_undefined();
  }

  if (!jsc_engine_init_jvm(state))
  {
    return jsc_value_create_undefined();
  }

  (*state->env)
      ->CallStaticVoidMethod(state->env, state->runtime_class,
                             state->execute_method, state->args);

  if ((*state->env)->ExceptionCheck(state->env))
  {
    jthrowable exception = (*state->env)->ExceptionOccurred(state->env);
    (*state->env)->ExceptionClear(state->env);

    jclass throwable_class =
        (*state->env)->FindClass(state->env, "java/lang/Throwable");
    jmethodID get_message =
        (*state->env)
            ->GetMethodID(state->env, throwable_class, "getMessage",
                          "()Ljava/lang/String;");

    jstring message =
        (*state->env)->CallObjectMethod(state->env, exception, get_message);

    const char* error_message =
        (*state->env)->GetStringUTFChars(state->env, message, NULL);
    jsc_engine_error(state, error_message);
    (*state->env)->ReleaseStringUTFChars(state->env, message, error_message);

    (*state->env)->DeleteLocalRef(state->env, message);
    (*state->env)->DeleteLocalRef(state->env, throwable_class);
    (*state->env)->DeleteLocalRef(state->env, exception);

    return jsc_value_create_undefined();
  }

  return jsc_value_create_undefined();
}

jsc_value jsc_engine_call_method(jsc_engine_state* state,
                                 const char* method_name, jsc_value* args,
                                 int arg_count)
{
  if (state->env == NULL)
  {
    if (!jsc_engine_init_jvm(state))
    {
      return jsc_value_create_undefined();
    }
  }

  jmethodID method_id = NULL;

  char descriptor[1 << 10] = "(";

  for (int i = 0; i < arg_count; i++)
  {
    strcat(descriptor, "Ljava/lang/Object;");
  }

  strcat(descriptor, ")Ljava/lang/Object;");

  method_id = (*state->env)
                  ->GetStaticMethodID(state->env, state->runtime_class,
                                      method_name, descriptor);

  if (method_id == NULL)
  {
    (*state->env)->ExceptionClear(state->env);
    jsc_engine_error(state, "method not found");
    return jsc_value_create_undefined();
  }

  jobjectArray jargs = NULL;

  if (arg_count > 0)
  {
    jclass object_class =
        (*state->env)->FindClass(state->env, "java/lang/Object");
    jargs = (*state->env)
                ->NewObjectArray(state->env, arg_count, object_class, NULL);
    (*state->env)->DeleteLocalRef(state->env, object_class);

    for (int i = 0; i < arg_count; i++)
    {
      jobject arg_obj = jsc_value_to_jobject(state->env, args[i]);
      (*state->env)->SetObjectArrayElement(state->env, jargs, i, arg_obj);
      (*state->env)->DeleteLocalRef(state->env, arg_obj);
    }
  }

  jobject result = (*state->env)
                       ->CallStaticObjectMethod(
                           state->env, state->runtime_class, method_id, jargs);

  if (jargs != NULL)
  {
    (*state->env)->DeleteLocalRef(state->env, jargs);
  }

  if ((*state->env)->ExceptionCheck(state->env))
  {
    jthrowable exception = (*state->env)->ExceptionOccurred(state->env);
    (*state->env)->ExceptionClear(state->env);

    jclass throwable_class =
        (*state->env)->FindClass(state->env, "java/lang/Throwable");
    jmethodID get_message =
        (*state->env)
            ->GetMethodID(state->env, throwable_class, "getMessage",
                          "()Ljava/lang/String;");

    jstring message =
        (*state->env)->CallObjectMethod(state->env, exception, get_message);

    const char* error_message =
        (*state->env)->GetStringUTFChars(state->env, message, NULL);
    jsc_engine_error(state, error_message);
    (*state->env)->ReleaseStringUTFChars(state->env, message, error_message);

    (*state->env)->DeleteLocalRef(state->env, message);
    (*state->env)->DeleteLocalRef(state->env, throwable_class);
    (*state->env)->DeleteLocalRef(state->env, exception);

    return jsc_value_create_undefined();
  }

  jsc_value ret_val = jsc_value_from_jobject(state->env, result);

  if (result != NULL)
  {
    (*state->env)->DeleteLocalRef(state->env, result);
  }

  return ret_val;
}

jsc_value jsc_engine_eval(const char* source)
{
  jsc_engine_state* state = jsc_engine_init("JSCScript");
  if (!state)
  {
    jsc_value undefined = jsc_value_create_undefined();
    return undefined;
  }

  if (!jsc_engine_compile(state, source))
  {
    jsc_value undefined = jsc_value_create_undefined();
    jsc_engine_free(state);
    return undefined;
  }

  if (!jsc_engine_init_jvm(state))
  {
    jsc_value undefined = jsc_value_create_undefined();
    jsc_engine_free(state);
    return undefined;
  }

  jsc_value result = jsc_engine_run(state);

  jsc_engine_free(state);

  return result;
}

jsc_symbol* jsc_engine_add_symbol(jsc_engine_state* state, const char* name,
                                  jsc_symbol_type type)
{
  jsc_symbol* existing = jsc_engine_lookup_symbol(state, name);

  if (existing && existing->scope_depth == state->current_scope->depth)
  {
    jsc_engine_error(state, "variable already declared in this scope");
    return NULL;
  }

  jsc_symbol* symbol = (jsc_symbol*)malloc(sizeof(jsc_symbol));

  if (!symbol)
  {
    jsc_engine_error(state, "jsc_engine_add_symbol malloc");
    return NULL;
  }

  memset(symbol, 0, sizeof(jsc_symbol));

  symbol->name = strdup(name);
  symbol->type = type;
  symbol->initialized = false;
  symbol->scope_depth = state->current_scope->depth;

  if (jsc_engine_is_global_scope(state))
  {
    symbol->index = 0;
  }
  else
  {
    symbol->index = state->local_index++;
    state->current_scope->local_count++;
  }

  symbol->next = state->current_scope->symbols;
  state->current_scope->symbols = symbol;

  return symbol;
}

jsc_symbol* jsc_engine_lookup_symbol(jsc_engine_state* state, const char* name)
{
  jsc_scope* scope = state->current_scope;

  while (scope)
  {
    jsc_symbol* symbol = scope->symbols;

    while (symbol)
    {
      if (strcmp(symbol->name, name) == 0)
      {
        return symbol;
      }

      symbol = symbol->next;
    }

    scope = scope->parent;
  }

  return NULL;
}

bool jsc_engine_match(jsc_engine_state* state, jsc_token_type type)
{
  if (!jsc_engine_check(state, type))
  {
    return false;
  }

  jsc_engine_advance(state);

  return true;
}

bool jsc_engine_check(jsc_engine_state* state, jsc_token_type type)
{
  return state->current_token.type == type;
}

void jsc_engine_advance(jsc_engine_state* state)
{
  state->current_token = jsc_next_token(state->tokenizer);

  if (jsc_tokenizer_has_error(state->tokenizer))
  {
    jsc_engine_error(state, jsc_tokenizer_get_error(state->tokenizer));
  }
}

void jsc_engine_error(jsc_engine_state* state, const char* message)
{
  if (state->had_error)
  {
    return;
  }

  state->had_error = true;

  if (state->error_message)
  {
    free(state->error_message);
  }

  state->error_message = strdup(message);
}

void jsc_engine_emit_byte(jsc_engine_state* state, uint8_t byte)
{
  jsc_bytecode_emit(state->bytecode, state->current_method, byte);

  state->stack_size++;

  if (state->stack_size > state->max_stack)
  {
    state->max_stack = state->stack_size;
  }
}

void jsc_engine_emit_bytes(jsc_engine_state* state, uint8_t byte1,
                           uint8_t byte2)
{
  jsc_engine_emit_byte(state, byte1);
  jsc_engine_emit_byte(state, byte2);
}

uint16_t jsc_engine_emit_jump(jsc_engine_state* state, uint8_t instruction)
{
  jsc_engine_emit_byte(state, instruction);

  uint8_t* code = jsc_bytecode_get_method_code(state->current_method);
  uint32_t code_length =
      jsc_bytecode_get_method_code_length(state->current_method);

  code[code_length++] = 0xFF;
  code[code_length++] = 0xFF;

  return code_length - 2;
}

void jsc_engine_patch_jump(jsc_engine_state* state, uint16_t offset)
{
  uint8_t* code = jsc_bytecode_get_method_code(state->current_method);
  uint32_t current = jsc_bytecode_get_method_code_length(state->current_method);

  int16_t jump = current - offset - 2;
  code[offset] = (jump >> 8) & 0xFF;
  code[offset + 1] = jump & 0xFF;
}

void jsc_engine_parse_program(jsc_engine_state* state)
{
  jsc_method* main_method = jsc_bytecode_create_method(
      state->bytecode, "main", "([Ljava/lang/String;)V",
      JSC_ACC_PUBLIC | JSC_ACC_STATIC, 100, 100);

  state->current_method = main_method;

  while (!jsc_engine_check(state, JSC_TOKEN_EOF))
  {
    jsc_engine_parse_declaration(state);

    if (state->had_error)
    {
      return;
    }
  }

  jsc_engine_emit_byte(state, JSC_JVM_RETURN);
}

void jsc_engine_parse_statement(jsc_engine_state* state)
{
  if (jsc_engine_match(state, JSC_TOKEN_IF))
  {
    jsc_engine_parse_if_statement(state);
  }
  else if (jsc_engine_match(state, JSC_TOKEN_WHILE))
  {
    jsc_engine_parse_while_statement(state);
  }
  else if (jsc_engine_match(state, JSC_TOKEN_FOR))
  {
    jsc_engine_parse_for_statement(state);
  }
  else if (jsc_engine_match(state, JSC_TOKEN_RETURN))
  {
    jsc_engine_parse_return_statement(state);
  }
  else if (jsc_engine_match(state, JSC_TOKEN_LEFT_BRACE))
  {
    jsc_engine_enter_scope(state);
    jsc_engine_parse_block(state);
    jsc_engine_exit_scope(state);
  }
  else
  {
    jsc_engine_parse_expression_statement(state);
  }
}

void jsc_engine_parse_declaration(jsc_engine_state* state)
{
  if (jsc_engine_match(state, JSC_TOKEN_VAR))
  {
    jsc_engine_parse_var_declaration(state, JSC_SYMBOL_VAR);
  }
  else if (jsc_engine_match(state, JSC_TOKEN_LET))
  {
    jsc_engine_parse_var_declaration(state, JSC_SYMBOL_LET);
  }
  else if (jsc_engine_match(state, JSC_TOKEN_CONST))
  {
    jsc_engine_parse_var_declaration(state, JSC_SYMBOL_CONST);
  }
  else if (jsc_engine_match(state, JSC_TOKEN_FUNCTION))
  {
    jsc_engine_parse_function_declaration(state);
  }
  else
  {
    jsc_engine_parse_statement(state);
  }
}

void jsc_engine_parse_var_declaration(jsc_engine_state* state,
                                      jsc_symbol_type type)
{
  if (!jsc_engine_check(state, JSC_TOKEN_IDENTIFIER))
  {
    jsc_engine_error(state, "expected variable name");
    return;
  }

  char name_buffer[1 << 8];
  strncpy(name_buffer, state->current_token.start, state->current_token.length);
  name_buffer[state->current_token.length] = '\0';

  jsc_engine_advance(state);

  jsc_symbol* symbol = jsc_engine_add_symbol(state, name_buffer, type);

  if (!symbol)
  {
    return;
  }

  if (jsc_engine_match(state, JSC_TOKEN_ASSIGN))
  {
    jsc_engine_parse_expr(state);
    symbol->initialized = true;

    if (jsc_engine_is_global_scope(state))
    {
      char field_name[300];
      sprintf(field_name, "global_%s", name_buffer);

      jsc_field* field = jsc_bytecode_add_field(
          state->bytecode, field_name, "Ljava/lang/Object;",
          JSC_ACC_PUBLIC | JSC_ACC_STATIC);

      jsc_bytecode_emit_field_access(state->bytecode, state->current_method,
                                     JSC_JVM_PUTSTATIC, state->class_name,
                                     field_name, "Ljava/lang/Object;");
    }
    else
    {
      jsc_bytecode_emit_local_var(state->bytecode, state->current_method,
                                  JSC_JVM_ASTORE, symbol->index);
    }
  }
  else
  {
    if (type == JSC_SYMBOL_CONST)
    {
      jsc_engine_error(state, "const variable must be initialized");
      return;
    }

    jsc_bytecode_emit(state->bytecode, state->current_method,
                      JSC_JVM_ACONST_NULL);

    if (jsc_engine_is_global_scope(state))
    {
      char field_name[300];
      sprintf(field_name, "global_%s", name_buffer);

      jsc_field* field = jsc_bytecode_add_field(
          state->bytecode, field_name, "Ljava/lang/Object;",
          JSC_ACC_PUBLIC | JSC_ACC_STATIC);

      jsc_bytecode_emit_field_access(state->bytecode, state->current_method,
                                     JSC_JVM_PUTSTATIC, state->class_name,
                                     field_name, "Ljava/lang/Object;");
    }
    else
    {
      jsc_bytecode_emit_local_var(state->bytecode, state->current_method,
                                  JSC_JVM_ASTORE, symbol->index);
    }
  }

  if (!jsc_engine_match(state, JSC_TOKEN_SEMICOLON))
  {
    jsc_engine_error(state, "expected ';' after variable declaration");
  }
}

void jsc_engine_parse_function_declaration(jsc_engine_state* state)
{
  if (!jsc_engine_check(state, JSC_TOKEN_IDENTIFIER))
  {
    jsc_engine_error(state, "expected function name");
    return;
  }

  char name_buffer[1 << 8];
  strncpy(name_buffer, state->current_token.start, state->current_token.length);
  name_buffer[state->current_token.length] = '\0';

  jsc_engine_advance(state);

  jsc_symbol* symbol =
      jsc_engine_add_symbol(state, name_buffer, JSC_SYMBOL_FUNCTION);
  if (!symbol)
  {
    return;
  }

  if (!jsc_engine_match(state, JSC_TOKEN_LEFT_PAREN))
  {
    jsc_engine_error(state, "expected '(' after function name");
    return;
  }

  jsc_engine_enter_scope(state);
  state->current_scope->is_function = true;
  state->current_scope->function_name = strdup(name_buffer);

  int param_count = 0;
  if (!jsc_engine_check(state, JSC_TOKEN_RIGHT_PAREN))
  {
    do
    {
      if (!jsc_engine_check(state, JSC_TOKEN_IDENTIFIER))
      {
        jsc_engine_error(state, "expected parameter name");
        return;
      }

      char param_name[1 << 8];
      strncpy(param_name, state->current_token.start,
              state->current_token.length);
      param_name[state->current_token.length] = '\0';

      jsc_engine_advance(state);

      jsc_symbol* param =
          jsc_engine_add_symbol(state, param_name, JSC_SYMBOL_PARAMETER);
      if (!param)
      {
        return;
      }

      param_count++;
    } while (jsc_engine_match(state, JSC_TOKEN_COMMA));
  }

  if (!jsc_engine_match(state, JSC_TOKEN_RIGHT_PAREN))
  {
    jsc_engine_error(state, "expected ')' after parameters");
    return;
  }

  char* descriptor = jsc_engine_generate_descriptor(state, param_count);
  jsc_method* previous_method = state->current_method;

  jsc_engine_begin_function(state, name_buffer, descriptor);
  free(descriptor);

  if (!jsc_engine_match(state, JSC_TOKEN_LEFT_BRACE))
  {
    jsc_engine_error(state, "expected '{' before function body");
    return;
  }

  jsc_engine_parse_block(state);

  jsc_engine_emit_byte(state, JSC_JVM_ACONST_NULL);
  jsc_engine_emit_byte(state, JSC_JVM_ARETURN);

  jsc_engine_end_function(state);

  jsc_engine_exit_scope(state);

  state->current_method = previous_method;

  if (jsc_engine_is_global_scope(state))
  {
    char method_ref[300];
    sprintf(method_ref, "%s.%s", state->class_name, name_buffer);

    jsc_bytecode_emit_load_constant_string(state->bytecode,
                                           state->current_method, method_ref);

    char field_name[300];
    sprintf(field_name, "global_%s", name_buffer);

    jsc_field* field = jsc_bytecode_add_field(state->bytecode, field_name,
                                              "Ljava/lang/Object;",
                                              JSC_ACC_PUBLIC | JSC_ACC_STATIC);

    jsc_bytecode_emit_field_access(state->bytecode, state->current_method,
                                   JSC_JVM_PUTSTATIC, state->class_name,
                                   field_name, "Ljava/lang/Object;");
  }
}

void jsc_engine_parse_expression_statement(jsc_engine_state* state)
{
  jsc_engine_parse_expr(state);

  if (!jsc_engine_match(state, JSC_TOKEN_SEMICOLON))
  {
    jsc_engine_error(state, "expected ';' after expression");
    return;
  }

  jsc_engine_emit_byte(state, JSC_JVM_POP);
}

void jsc_engine_parse_if_statement(jsc_engine_state* state)
{
  if (!jsc_engine_match(state, JSC_TOKEN_LEFT_PAREN))
  {
    jsc_engine_error(state, "expected '(' after 'if'");
    return;
  }

  jsc_engine_parse_expr(state);

  if (!jsc_engine_match(state, JSC_TOKEN_RIGHT_PAREN))
  {
    jsc_engine_error(state, "expected ')' after condition");
    return;
  }

  uint16_t then_jump = jsc_engine_emit_jump(state, JSC_JVM_IFEQ);

  jsc_engine_parse_statement(state);

  uint16_t else_jump = jsc_engine_emit_jump(state, JSC_JVM_GOTO);

  jsc_engine_patch_jump(state, then_jump);

  if (jsc_engine_match(state, JSC_TOKEN_ELSE))
  {
    jsc_engine_parse_statement(state);
  }

  jsc_engine_patch_jump(state, else_jump);
}

void jsc_engine_parse_while_statement(jsc_engine_state* state)
{
  uint32_t loop_start =
      jsc_bytecode_get_method_code_length(state->current_method);

  if (!jsc_engine_match(state, JSC_TOKEN_LEFT_PAREN))
  {
    jsc_engine_error(state, "expected '(' after 'while'");
    return;
  }

  jsc_engine_parse_expr(state);

  if (!jsc_engine_match(state, JSC_TOKEN_RIGHT_PAREN))
  {
    jsc_engine_error(state, "expected ')' after condition");
    return;
  }

  uint16_t exit_jump = jsc_engine_emit_jump(state, JSC_JVM_IFEQ);

  jsc_engine_parse_statement(state);

  jsc_bytecode_emit_jump(
      state->bytecode, state->current_method, JSC_JVM_GOTO,
      -(jsc_bytecode_get_method_code_length(state->current_method) -
        loop_start + 3));

  jsc_engine_patch_jump(state, exit_jump);
}

void jsc_engine_parse_for_statement(jsc_engine_state* state)
{
  jsc_engine_enter_scope(state);

  if (!jsc_engine_match(state, JSC_TOKEN_LEFT_PAREN))
  {
    jsc_engine_error(state, "expected '(' after 'for'");
    return;
  }

  if (jsc_engine_match(state, JSC_TOKEN_SEMICOLON))
  {
  }
  else if (jsc_engine_match(state, JSC_TOKEN_VAR))
  {
    jsc_engine_parse_var_declaration(state, JSC_SYMBOL_VAR);
  }
  else if (jsc_engine_match(state, JSC_TOKEN_LET))
  {
    jsc_engine_parse_var_declaration(state, JSC_SYMBOL_LET);
  }
  else
  {
    jsc_engine_parse_expression_statement(state);
  }

  uint32_t loop_start =
      jsc_bytecode_get_method_code_length(state->current_method);

  uint16_t exit_jump = 0;
  if (!jsc_engine_check(state, JSC_TOKEN_SEMICOLON))
  {
    jsc_engine_parse_expr(state);

    if (!jsc_engine_match(state, JSC_TOKEN_SEMICOLON))
    {
      jsc_engine_error(state, "expected ';' after loop condition");
      return;
    }

    exit_jump = jsc_engine_emit_jump(state, JSC_JVM_IFEQ);
  }
  else
  {
    jsc_engine_advance(state);
  }

  if (!jsc_engine_check(state, JSC_TOKEN_RIGHT_PAREN))
  {
    uint16_t body_jump = jsc_engine_emit_jump(state, JSC_JVM_GOTO);

    uint32_t increment_start =
        jsc_bytecode_get_method_code_length(state->current_method);

    jsc_engine_parse_expr(state);
    jsc_engine_emit_byte(state, JSC_JVM_POP);

    if (!jsc_engine_match(state, JSC_TOKEN_RIGHT_PAREN))
    {
      jsc_engine_error(state, "expected ')' after for clauses");
      return;
    }

    jsc_bytecode_emit_jump(
        state->bytecode, state->current_method, JSC_JVM_GOTO,
        -(jsc_bytecode_get_method_code_length(state->current_method) -
          loop_start + 3));

    jsc_engine_patch_jump(state, body_jump);

    jsc_engine_parse_statement(state);

    jsc_bytecode_emit_jump(
        state->bytecode, state->current_method, JSC_JVM_GOTO,
        -(jsc_bytecode_get_method_code_length(state->current_method) -
          increment_start + 3));
  }
  else
  {
    jsc_engine_advance(state);

    jsc_engine_parse_statement(state);

    jsc_bytecode_emit_jump(
        state->bytecode, state->current_method, JSC_JVM_GOTO,
        -(jsc_bytecode_get_method_code_length(state->current_method) -
          loop_start + 3));
  }

  if (exit_jump != 0)
  {
    jsc_engine_patch_jump(state, exit_jump);
  }

  jsc_engine_exit_scope(state);
}

void jsc_engine_parse_return_statement(jsc_engine_state* state)
{
  if (jsc_engine_is_global_scope(state))
  {
    jsc_engine_error(state, "cannot return from global scope");
    return;
  }

  if (jsc_engine_check(state, JSC_TOKEN_SEMICOLON))
  {
    jsc_engine_advance(state);
    jsc_engine_emit_byte(state, JSC_JVM_ACONST_NULL);
    jsc_engine_emit_byte(state, JSC_JVM_ARETURN);
    return;
  }

  jsc_engine_parse_expr(state);

  if (!jsc_engine_match(state, JSC_TOKEN_SEMICOLON))
  {
    jsc_engine_error(state, "expected ';' after return value");
    return;
  }

  jsc_engine_emit_byte(state, JSC_JVM_ARETURN);
}

void jsc_engine_parse_block(jsc_engine_state* state)
{
  while (!jsc_engine_check(state, JSC_TOKEN_RIGHT_BRACE) &&
         !jsc_engine_check(state, JSC_TOKEN_EOF))
  {
    jsc_engine_parse_declaration(state);
  }

  if (!jsc_engine_match(state, JSC_TOKEN_RIGHT_BRACE))
  {
    jsc_engine_error(state, "expected '}' after block");
    return;
  }
}

void jsc_engine_parse_expr(jsc_engine_state* state)
{
  jsc_engine_parse_assign(state);
}

void jsc_engine_parse_assign(jsc_engine_state* state)
{
  jsc_engine_parse_lor(state);

  if (jsc_engine_match(state, JSC_TOKEN_ASSIGN))
  {
    jsc_token token = state->current_token;
    jsc_token_type operator_type = token.type;

    jsc_engine_parse_assign(state);

    if (operator_type == JSC_TOKEN_ASSIGN)
    {
      char name_buffer[1 << 8];
      strncpy(name_buffer, token.start, token.length);
      name_buffer[token.length] = '\0';

      jsc_engine_store_variable(state, name_buffer);
    }
  }
}

void jsc_engine_parse_lor(jsc_engine_state* state)
{
  jsc_engine_parse_land(state);

  while (jsc_engine_match(state, JSC_TOKEN_LOGICAL_OR))
  {
    uint16_t end_jump = jsc_engine_emit_jump(state, JSC_JVM_IFNE);

    jsc_engine_parse_land(state);

    jsc_engine_patch_jump(state, end_jump);
  }
}

void jsc_engine_parse_land(jsc_engine_state* state)
{
  jsc_engine_parse_eq(state);

  while (jsc_engine_match(state, JSC_TOKEN_LOGICAL_AND))
  {
    uint16_t end_jump = jsc_engine_emit_jump(state, JSC_JVM_IFEQ);

    jsc_engine_parse_eq(state);

    jsc_engine_patch_jump(state, end_jump);
  }
}

void jsc_engine_parse_eq(jsc_engine_state* state)
{
  jsc_engine_parse_cmp(state);

  while (jsc_engine_match(state, JSC_TOKEN_EQUAL) ||
         jsc_engine_match(state, JSC_TOKEN_NOT_EQUAL) ||
         jsc_engine_match(state, JSC_TOKEN_STRICT_EQUAL) ||
         jsc_engine_match(state, JSC_TOKEN_STRICT_NOT_EQUAL))
  {

    jsc_token_type operator_type = state->current_token.type;

    jsc_engine_parse_cmp(state);

    switch (operator_type)
    {
    case JSC_TOKEN_EQUAL:
    case JSC_TOKEN_STRICT_EQUAL:
      jsc_bytecode_emit(state->bytecode, state->current_method,
                        JSC_JVM_IF_ACMPEQ);
      break;
    case JSC_TOKEN_NOT_EQUAL:
    case JSC_TOKEN_STRICT_NOT_EQUAL:
      jsc_bytecode_emit(state->bytecode, state->current_method,
                        JSC_JVM_IF_ACMPNE);
      break;
    default:
      break;
    }
  }
}

void jsc_engine_parse_cmp(jsc_engine_state* state)
{
  jsc_engine_parse_add(state);

  while (jsc_engine_match(state, JSC_TOKEN_LESS_THAN) ||
         jsc_engine_match(state, JSC_TOKEN_GREATER_THAN) ||
         jsc_engine_match(state, JSC_TOKEN_LESS_THAN_EQUAL) ||
         jsc_engine_match(state, JSC_TOKEN_GREATER_THAN_EQUAL))
  {

    jsc_token_type operator_type = state->current_token.type;

    jsc_engine_parse_add(state);

    switch (operator_type)
    {
    case JSC_TOKEN_LESS_THAN:
      jsc_bytecode_emit(state->bytecode, state->current_method,
                        JSC_JVM_IF_ICMPLT);
      break;
    case JSC_TOKEN_GREATER_THAN:
      jsc_bytecode_emit(state->bytecode, state->current_method,
                        JSC_JVM_IF_ICMPGT);
      break;
    case JSC_TOKEN_LESS_THAN_EQUAL:
      jsc_bytecode_emit(state->bytecode, state->current_method,
                        JSC_JVM_IF_ICMPLE);
      break;
    case JSC_TOKEN_GREATER_THAN_EQUAL:
      jsc_bytecode_emit(state->bytecode, state->current_method,
                        JSC_JVM_IF_ICMPGE);
      break;
    default:
      break;
    }
  }
}

void jsc_engine_parse_add(jsc_engine_state* state)
{
  jsc_engine_parse_mul(state);

  while (jsc_engine_match(state, JSC_TOKEN_PLUS) ||
         jsc_engine_match(state, JSC_TOKEN_MINUS))
  {
    jsc_token_type operator_type = state->current_token.type;

    jsc_engine_parse_mul(state);

    switch (operator_type)
    {
    case JSC_TOKEN_PLUS:
      jsc_bytecode_emit(state->bytecode, state->current_method, JSC_JVM_IADD);
      break;
    case JSC_TOKEN_MINUS:
      jsc_bytecode_emit(state->bytecode, state->current_method, JSC_JVM_ISUB);
      break;
    default:
      break;
    }
  }
}

void jsc_engine_parse_mul(jsc_engine_state* state)
{
  jsc_engine_parse_unary(state);

  while (jsc_engine_match(state, JSC_TOKEN_MULTIPLY) ||
         jsc_engine_match(state, JSC_TOKEN_DIVIDE) ||
         jsc_engine_match(state, JSC_TOKEN_MODULO))
  {

    jsc_token_type operator_type = state->current_token.type;

    jsc_engine_parse_unary(state);

    switch (operator_type)
    {
    case JSC_TOKEN_MULTIPLY:
      jsc_bytecode_emit(state->bytecode, state->current_method, JSC_JVM_IMUL);
      break;
    case JSC_TOKEN_DIVIDE:
      jsc_bytecode_emit(state->bytecode, state->current_method, JSC_JVM_IDIV);
      break;
    case JSC_TOKEN_MODULO:
      jsc_bytecode_emit(state->bytecode, state->current_method, JSC_JVM_IREM);
      break;
    default:
      break;
    }
  }
}

void jsc_engine_parse_unary(jsc_engine_state* state)
{
  if (jsc_engine_match(state, JSC_TOKEN_LOGICAL_NOT))
  {
    jsc_engine_parse_unary(state);
    jsc_engine_emit_byte(state, JSC_JVM_ICONST_0);
    jsc_engine_emit_byte(state, JSC_JVM_IF_ICMPEQ);
    jsc_engine_emit_byte(state, JSC_JVM_ICONST_1);
    jsc_engine_emit_byte(state, JSC_JVM_GOTO);
    jsc_engine_emit_byte(state, JSC_JVM_ICONST_0);
  }
  else if (jsc_engine_match(state, JSC_TOKEN_MINUS))
  {
    jsc_engine_parse_unary(state);
    jsc_engine_emit_byte(state, JSC_JVM_INEG);
  }
  else
  {
    jsc_engine_parse_call(state);
  }
}

void jsc_engine_parse_call(jsc_engine_state* state)
{
  jsc_engine_parse_primary(state);

  if (jsc_engine_match(state, JSC_TOKEN_LEFT_PAREN))
  {
    int arg_count = 0;

    jclass object_array_class =
        (*state->env)->FindClass(state->env, "[Ljava/lang/Object;");
    jmethodID array_init =
        (*state->env)
            ->GetMethodID(state->env, object_array_class, "<init>", "(I)V");

    jobjectArray arg_array =
        (*state->env)
            ->NewObjectArray(state->env, arg_count, object_array_class, NULL);

    if (!jsc_engine_check(state, JSC_TOKEN_RIGHT_PAREN))
    {
      do
      {
        jsc_engine_parse_expr(state);
        arg_count++;

        jobject arg =
            (*state->env)
                ->GetObjectArrayElement(state->env, arg_array, arg_count - 1);
        (*state->env)
            ->SetObjectArrayElement(state->env, arg_array, arg_count - 1, arg);

      } while (jsc_engine_match(state, JSC_TOKEN_COMMA));
    }

    if (!jsc_engine_match(state, JSC_TOKEN_RIGHT_PAREN))
    {
      jsc_engine_error(state, "expected ')' after arguments");
      return;
    }

    jsc_bytecode_emit_invoke_static(
        state->bytecode, state->current_method, state->class_name, "invoke",
        "(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;");
  }
}

void jsc_engine_parse_primary(jsc_engine_state* state)
{
  if (jsc_engine_match(state, JSC_TOKEN_TRUE))
  {
    jsc_bytecode_emit_load_constant_int(state->bytecode, state->current_method,
                                        1);
  }
  else if (jsc_engine_match(state, JSC_TOKEN_FALSE))
  {
    jsc_bytecode_emit_load_constant_int(state->bytecode, state->current_method,
                                        0);
  }
  else if (jsc_engine_match(state, JSC_TOKEN_NULL))
  {
    jsc_engine_emit_byte(state, JSC_JVM_ACONST_NULL);
  }
  else if (jsc_engine_match(state, JSC_TOKEN_NUMBER))
  {
    jsc_bytecode_emit_load_constant_double(state->bytecode,
                                           state->current_method,
                                           state->current_token.number_value);
  }
  else if (jsc_engine_match(state, JSC_TOKEN_STRING))
  {
    jsc_bytecode_emit_load_constant_string(
        state->bytecode, state->current_method,
        state->current_token.string_value.data);
  }
  else if (jsc_engine_match(state, JSC_TOKEN_IDENTIFIER))
  {
    char name_buffer[1 << 8];
    strncpy(name_buffer, state->current_token.start,
            state->current_token.length);
    name_buffer[state->current_token.length] = '\0';

    jsc_engine_load_variable(state, name_buffer);
  }
  else if (jsc_engine_match(state, JSC_TOKEN_LEFT_PAREN))
  {
    jsc_engine_parse_expr(state);

    if (!jsc_engine_match(state, JSC_TOKEN_RIGHT_PAREN))
    {
      jsc_engine_error(state, "expected ')' after expression");
      return;
    }
  }
  else
  {
    jsc_engine_error(state, "expected expression");
  }
}

void jsc_engine_enter_scope(jsc_engine_state* state)
{
  jsc_scope* scope = (jsc_scope*)malloc(sizeof(jsc_scope));

  if (!scope)
  {
    jsc_engine_error(state, "jsc_engine_enter_scope malloc");
    return;
  }

  memset(scope, 0, sizeof(jsc_scope));

  scope->depth = state->current_scope->depth + 1;
  scope->parent = state->current_scope;

  if (state->current_scope->first_child)
  {
    jsc_scope* sibling = state->current_scope->first_child;

    while (sibling->next_sibling)
    {
      sibling = sibling->next_sibling;
    }

    sibling->next_sibling = scope;
  }
  else
  {
    state->current_scope->first_child = scope;
  }

  state->current_scope = scope;
}

void jsc_engine_exit_scope(jsc_engine_state* state)
{
  state->current_scope = state->current_scope->parent;
}

bool jsc_engine_is_global_scope(jsc_engine_state* state)
{
  return state->current_scope == state->global_scope;
}

void jsc_engine_begin_function(jsc_engine_state* state, const char* name,
                               const char* descriptor)
{
  jsc_method* method =
      jsc_bytecode_create_method(state->bytecode, name, descriptor,
                                 JSC_ACC_PUBLIC | JSC_ACC_STATIC, 100, 100);

  state->current_method = method;
  state->local_index = 0;
  state->stack_size = 0;
  state->max_stack = 0;
}

void jsc_engine_end_function(jsc_engine_state* state)
{
}

void jsc_engine_load_variable(jsc_engine_state* state, const char* name)
{
  jsc_symbol* symbol = jsc_engine_lookup_symbol(state, name);

  if (!symbol)
  {
    jsc_engine_error(state, "undefined variable");
    return;
  }

  if (!symbol->initialized && symbol->type == JSC_SYMBOL_CONST)
  {
    jsc_engine_error(state,
                     "cannot access const variable before initialization");
    return;
  }

  if (jsc_engine_is_global_scope(state) || symbol->scope_depth == 0)
  {
    char field_name[300];
    sprintf(field_name, "global_%s", name);

    jsc_bytecode_emit_field_access(state->bytecode, state->current_method,
                                   JSC_JVM_GETSTATIC, state->class_name,
                                   field_name, "Ljava/lang/Object;");
  }
  else
  {
    jsc_bytecode_emit_local_var(state->bytecode, state->current_method,
                                JSC_JVM_ALOAD, symbol->index);
  }
}

void jsc_engine_store_variable(jsc_engine_state* state, const char* name)
{
  jsc_symbol* symbol = jsc_engine_lookup_symbol(state, name);

  if (!symbol)
  {
    jsc_engine_error(state, "undefined variable");
    return;
  }

  if (symbol->initialized && symbol->type == JSC_SYMBOL_CONST)
  {
    jsc_engine_error(state, "cannot reassign const variable");
    return;
  }

  symbol->initialized = true;

  if (jsc_engine_is_global_scope(state) || symbol->scope_depth == 0)
  {
    char field_name[300];
    sprintf(field_name, "global_%s", name);

    jsc_bytecode_emit_field_access(state->bytecode, state->current_method,
                                   JSC_JVM_PUTSTATIC, state->class_name,
                                   field_name, "Ljava/lang/Object;");
  }
  else
  {
    jsc_bytecode_emit_local_var(state->bytecode, state->current_method,
                                JSC_JVM_ASTORE, symbol->index);
  }
}

jsc_value jsc_value_create_undefined(void)
{
  jsc_value value;
  memset(&value, 0, sizeof(jsc_value));
  value.type = JSC_VALUE_UNDEFINED;
  return value;
}

jsc_value jsc_value_create_null(void)
{
  jsc_value value;
  memset(&value, 0, sizeof(jsc_value));
  value.type = JSC_VALUE_NULL;
  return value;
}

jsc_value jsc_value_create_boolean(bool value)
{
  jsc_value js_value;
  memset(&js_value, 0, sizeof(jsc_value));
  js_value.type = JSC_VALUE_BOOLEAN;
  js_value.boolean_value = value;
  return js_value;
}

jsc_value jsc_value_create_number(double value)
{
  jsc_value js_value;
  memset(&js_value, 0, sizeof(jsc_value));
  js_value.type = JSC_VALUE_NUMBER;
  js_value.number_value = value;
  return js_value;
}

jsc_value jsc_value_create_string(const char* value)
{
  jsc_value js_value;
  memset(&js_value, 0, sizeof(jsc_value));
  js_value.type = JSC_VALUE_STRING;
  js_value.string_value = strdup(value);
  return js_value;
}

jsc_value jsc_value_from_jobject(JNIEnv* env, jobject obj)
{
  if (obj == NULL)
  {
    return jsc_value_create_null();
  }

  jclass obj_class = (*env)->GetObjectClass(env, obj);

  jclass boolean_class = (*env)->FindClass(env, "java/lang/Boolean");
  jclass number_class = (*env)->FindClass(env, "java/lang/Number");
  jclass string_class = (*env)->FindClass(env, "java/lang/String");

  if ((*env)->IsInstanceOf(env, obj, boolean_class))
  {
    jmethodID boolean_value =
        (*env)->GetMethodID(env, boolean_class, "booleanValue", "()Z");
    jboolean value = (*env)->CallBooleanMethod(env, obj, boolean_value);

    (*env)->DeleteLocalRef(env, boolean_class);
    (*env)->DeleteLocalRef(env, number_class);
    (*env)->DeleteLocalRef(env, string_class);
    (*env)->DeleteLocalRef(env, obj_class);

    return jsc_value_create_boolean(value);
  }
  else if ((*env)->IsInstanceOf(env, obj, number_class))
  {
    jmethodID double_value =
        (*env)->GetMethodID(env, number_class, "doubleValue", "()D");
    jdouble value = (*env)->CallDoubleMethod(env, obj, double_value);

    (*env)->DeleteLocalRef(env, boolean_class);
    (*env)->DeleteLocalRef(env, number_class);
    (*env)->DeleteLocalRef(env, string_class);
    (*env)->DeleteLocalRef(env, obj_class);

    return jsc_value_create_number(value);
  }
  else if ((*env)->IsInstanceOf(env, obj, string_class))
  {
    jmethodID to_string = (*env)->GetMethodID(env, string_class, "toString",
                                              "()Ljava/lang/String;");
    jstring jstr = (jstring)(*env)->CallObjectMethod(env, obj, to_string);

    const char* str = (*env)->GetStringUTFChars(env, jstr, NULL);
    jsc_value value = jsc_value_create_string(str);
    (*env)->ReleaseStringUTFChars(env, jstr, str);

    (*env)->DeleteLocalRef(env, jstr);
    (*env)->DeleteLocalRef(env, boolean_class);
    (*env)->DeleteLocalRef(env, number_class);
    (*env)->DeleteLocalRef(env, string_class);
    (*env)->DeleteLocalRef(env, obj_class);

    return value;
  }
  else
  {
    jsc_value value;
    memset(&value, 0, sizeof(jsc_value));
    value.type = JSC_VALUE_OBJECT;
    value.object_value = (*env)->NewGlobalRef(env, obj);

    (*env)->DeleteLocalRef(env, boolean_class);
    (*env)->DeleteLocalRef(env, number_class);
    (*env)->DeleteLocalRef(env, string_class);
    (*env)->DeleteLocalRef(env, obj_class);

    return value;
  }
}

jobject jsc_value_to_jobject(JNIEnv* env, jsc_value value)
{
  switch (value.type)
  {
  case JSC_VALUE_NULL:
    return NULL;

  case JSC_VALUE_BOOLEAN:
  {
    jclass boolean_class = (*env)->FindClass(env, "java/lang/Boolean");
    jmethodID boolean_init =
        (*env)->GetMethodID(env, boolean_class, "<init>", "(Z)V");
    jobject boolean_obj = (*env)->NewObject(env, boolean_class, boolean_init,
                                            value.boolean_value);

    jobject result = (*env)->NewGlobalRef(env, boolean_obj);
    (*env)->DeleteLocalRef(env, boolean_obj);
    (*env)->DeleteLocalRef(env, boolean_class);

    return result;
  }

  case JSC_VALUE_NUMBER:
  {
    jclass double_class = (*env)->FindClass(env, "java/lang/Double");
    jmethodID double_init =
        (*env)->GetMethodID(env, double_class, "<init>", "(D)V");
    jobject double_obj =
        (*env)->NewObject(env, double_class, double_init, value.number_value);

    jobject result = (*env)->NewGlobalRef(env, double_obj);
    (*env)->DeleteLocalRef(env, double_obj);
    (*env)->DeleteLocalRef(env, double_class);

    return result;
  }

  case JSC_VALUE_STRING:
  {
    jstring string_obj = (*env)->NewStringUTF(env, value.string_value);

    jobject result = (*env)->NewGlobalRef(env, string_obj);
    (*env)->DeleteLocalRef(env, string_obj);

    return result;
  }

  case JSC_VALUE_OBJECT:
    return value.object_value;

  case JSC_VALUE_UNDEFINED:
  default:
  {
    jclass object_class = (*env)->FindClass(env, "java/lang/Object");
    jmethodID object_init =
        (*env)->GetMethodID(env, object_class, "<init>", "()V");
    jobject object_obj = (*env)->NewObject(env, object_class, object_init);

    jobject result = (*env)->NewGlobalRef(env, object_obj);
    (*env)->DeleteLocalRef(env, object_obj);
    (*env)->DeleteLocalRef(env, object_class);

    return result;
  }
  }
}

void jsc_value_free(JNIEnv* env, jsc_value value)
{
  if (value.type == JSC_VALUE_STRING && value.string_value)
  {
    free(value.string_value);
  }
  else if (value.type == JSC_VALUE_OBJECT && value.object_value)
  {
    (*env)->DeleteGlobalRef(env, value.object_value);
  }
}

char* jsc_engine_generate_descriptor(jsc_engine_state* state, int param_count)
{
  char* descriptor = malloc(param_count * 20 + 20);

  if (!descriptor)
  {
    jsc_engine_error(state, "jsc_engine_generate_descriptor malloc");
    return NULL;
  }

  strcpy(descriptor, "(");

  for (int i = 0; i < param_count; i++)
  {
    strcat(descriptor, "Ljava/lang/Object;");
  }

  strcat(descriptor, ")Ljava/lang/Object;");

  return descriptor;
}

char* jsc_engine_get_temp_filename(const char* prefix, const char* suffix)
{
  char* template = malloc(strlen(prefix) + strlen(suffix) + 12);

  if (!template)
  {
    return NULL;
  }

  sprintf(template, "%s%ld%s", prefix, (long)time(NULL), suffix);

  return template;
}

char* jsc_value_to_string(jsc_value value)
{
  char buffer[1 << 10];

  switch (value.type)
  {
  case JSC_VALUE_UNDEFINED:
    return strdup("undefined");

  case JSC_VALUE_NULL:
    return strdup("null");

  case JSC_VALUE_BOOLEAN:
    return strdup(value.boolean_value ? "true" : "false");

  case JSC_VALUE_NUMBER:
    snprintf(buffer, sizeof(buffer), "%g", value.number_value);
    return strdup(buffer);

  case JSC_VALUE_STRING:
    return strdup(value.string_value);

  case JSC_VALUE_OBJECT:
    return strdup("[object Object]");

  case JSC_VALUE_FUNCTION:
    return strdup("[function]");

  case JSC_VALUE_ARRAY:
    return strdup("[]");

  default:
    return strdup("[unknown]");
  }
}
