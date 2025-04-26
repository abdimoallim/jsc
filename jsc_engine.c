#include "jsc_engine.h"

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

jsc_engine_context* jsc_engine_init(const char* class_name)
{
  jsc_engine_context* ctx =
      (jsc_engine_context*)malloc(sizeof(jsc_engine_context));

  if (!ctx)
  {
    return NULL;
  }

  memset(ctx, 0, sizeof(jsc_engine_context));

  ctx->class_name = strdup(class_name);

  if (!ctx->class_name)
  {
    free(ctx);
    return NULL;
  }

  ctx->global_scope = (jsc_scope*)malloc(sizeof(jsc_scope));

  if (!ctx->global_scope)
  {
    free(ctx->class_name);
    free(ctx);
    return NULL;
  }

  memset(ctx->global_scope, 0, sizeof(jsc_scope));
  ctx->current_scope = ctx->global_scope;

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
    free(ctx->global_scope);
    free(ctx->class_name);
    free(ctx);
    return NULL;
  }

  ctx->temp_dir = strdup(dir_name);

  ctx->class_path = malloc(strlen(dir_name) + 2);
  sprintf(ctx->class_path, "%s", dir_name);

  jvm_options[0].optionString = malloc(strlen(ctx->class_path) + 20);
  sprintf(jvm_options[0].optionString, "-Djava.class.path=%s", ctx->class_path);

  return ctx;
}

void jsc_engine_free(jsc_engine_context* ctx)
{
  if (!ctx)
  {
    return;
  }

  if (ctx->env)
  {
    if (ctx->runtime_instance)
    {
      (*ctx->env)->DeleteGlobalRef(ctx->env, ctx->runtime_instance);
    }

    if (ctx->runtime_class)
    {
      (*ctx->env)->DeleteGlobalRef(ctx->env, ctx->runtime_class);
    }

    if (ctx->args)
    {
      (*ctx->env)->DeleteGlobalRef(ctx->env, ctx->args);
    }
  }

  if (ctx->jvm)
  {
    (*ctx->jvm)->DestroyJavaVM(ctx->jvm);
  }

  if (ctx->tokenizer)
  {
    jsc_tokenizer_free(ctx->tokenizer);
  }

  if (ctx->bytecode)
  {
    jsc_bytecode_free(ctx->bytecode);
  }

  if (ctx->class_name)
  {
    free(ctx->class_name);
  }

  if (ctx->error_message)
  {
    free(ctx->error_message);
  }

  if (ctx->temp_dir)
  {
    free(ctx->temp_dir);
  }

  if (ctx->class_path)
  {
    free(ctx->class_path);
  }

  if (jvm_options[0].optionString)
  {
    free(jvm_options[0].optionString);
  }

  jsc_scope* scope = ctx->global_scope;

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

  free(ctx);
}

bool jsc_engine_compile(jsc_engine_context* ctx, const char* source)
{
  ctx->tokenizer = jsc_tokenizer_init(source, strlen(source));

  if (!ctx->tokenizer)
  {
    jsc_engine_error(ctx, "failed to initialize tokenizer");
    return false;
  }

  ctx->bytecode = jsc_bytecode_create_class(ctx->class_name, "java/lang/Object",
                                            JSC_ACC_PUBLIC | JSC_ACC_SUPER);

  if (!ctx->bytecode)
  {
    jsc_engine_error(ctx, "failed to initialize bytecode");
    return false;
  }

  jsc_engine_advance(ctx);

  jsc_engine_parse_program(ctx);

  if (ctx->had_error)
  {
    return false;
  }

  char* class_file =
      malloc(strlen(ctx->temp_dir) + strlen(ctx->class_name) + 8);

  if (!class_file)
  {
    jsc_engine_error(ctx, "jsc_engine_compile malloc");
    return false;
  }

  sprintf(class_file, "%s%s%s.class", ctx->temp_dir, PATH_SEPARATOR,
          ctx->class_name);

  if (!jsc_bytecode_write_to_file(ctx->bytecode, class_file))
  {
    jsc_engine_error(ctx, "failed to write class file");
    free(class_file);
    return false;
  }

  free(class_file);

  return true;
}

bool jsc_engine_init_jvm(jsc_engine_context* ctx)
{
  if (ctx->jvm != NULL)
  {
    return true;
  }

  JNIEnv* env;

  jint result = JNI_CreateJavaVM(&ctx->jvm, (void**)&env, &jvm_args);

  if (result != JNI_OK)
  {
    jsc_engine_error(ctx, "failed to create JVM");
    return false;
  }

  ctx->env = env;

  jclass runtime_class = (*env)->FindClass(env, ctx->class_name);

  if (runtime_class == NULL)
  {
    (*env)->ExceptionClear(env);
    jsc_engine_error(ctx, "failed to find compiled class");
    return false;
  }

  ctx->runtime_class = (*env)->NewGlobalRef(env, runtime_class);
  (*env)->DeleteLocalRef(env, runtime_class);

  jmethodID main_method = (*env)->GetStaticMethodID(
      env, ctx->runtime_class, "main", "([Ljava/lang/String;)V");

  if (main_method == NULL)
  {
    (*env)->ExceptionClear(env);
    jsc_engine_error(ctx, "failed to find main method");
    return false;
  }

  ctx->execute_method = main_method;

  jclass string_class = (*env)->FindClass(env, "java/lang/String");
  if (string_class == NULL)
  {
    (*env)->ExceptionClear(env);
    jsc_engine_error(ctx, "failed to find String class");
    return false;
  }

  jobjectArray args = (*env)->NewObjectArray(env, 0, string_class, NULL);

  if (args == NULL)
  {
    (*env)->ExceptionClear(env);
    (*env)->DeleteLocalRef(env, string_class);
    jsc_engine_error(ctx, "failed to create arguments array");
    return false;
  }

  ctx->args = (*env)->NewGlobalRef(env, args);
  (*env)->DeleteLocalRef(env, args);
  (*env)->DeleteLocalRef(env, string_class);

  return true;
}

bool jsc_engine_load_class(jsc_engine_context* ctx, const char* class_file)
{
  if (ctx->env == NULL)
  {
    if (!jsc_engine_init_jvm(ctx))
    {
      return false;
    }
  }

  jclass class_loader_class =
      (*ctx->env)->FindClass(ctx->env, "java/lang/ClassLoader");
  if (class_loader_class == NULL)
  {
    (*ctx->env)->ExceptionClear(ctx->env);
    jsc_engine_error(ctx, "failed to find ClassLoader class");
    return false;
  }

  jmethodID get_system_class_loader = (*ctx->env)->GetStaticMethodID(
      ctx->env, class_loader_class, "getSystemClassLoader",
      "()Ljava/lang/ClassLoader;");

  if (get_system_class_loader == NULL)
  {
    (*ctx->env)->ExceptionClear(ctx->env);
    (*ctx->env)->DeleteLocalRef(ctx->env, class_loader_class);
    jsc_engine_error(ctx, "failed to find getSystemClassLoader method");
    return false;
  }

  jobject class_loader = (*ctx->env)->CallStaticObjectMethod(
      ctx->env, class_loader_class, get_system_class_loader);

  if (class_loader == NULL)
  {
    (*ctx->env)->ExceptionClear(ctx->env);
    (*ctx->env)->DeleteLocalRef(ctx->env, class_loader_class);
    jsc_engine_error(ctx, "failed to get system class loader");
    return false;
  }

  jmethodID load_class =
      (*ctx->env)->GetMethodID(ctx->env, class_loader_class, "loadClass",
                               "(Ljava/lang/String;)Ljava/lang/Class;");

  if (load_class == NULL)
  {
    (*ctx->env)->ExceptionClear(ctx->env);
    (*ctx->env)->DeleteLocalRef(ctx->env, class_loader);
    (*ctx->env)->DeleteLocalRef(ctx->env, class_loader_class);
    jsc_engine_error(ctx, "failed to find loadClass method");
    return false;
  }

  jstring class_name_jstr =
      (*ctx->env)->NewStringUTF(ctx->env, ctx->class_name);
  if (class_name_jstr == NULL)
  {
    (*ctx->env)->ExceptionClear(ctx->env);
    (*ctx->env)->DeleteLocalRef(ctx->env, class_loader);
    (*ctx->env)->DeleteLocalRef(ctx->env, class_loader_class);
    jsc_engine_error(ctx, "failed to create class name string");
    return false;
  }

  jclass loaded_class = (*ctx->env)->CallObjectMethod(
      ctx->env, class_loader, load_class, class_name_jstr);

  (*ctx->env)->DeleteLocalRef(ctx->env, class_name_jstr);

  if (loaded_class == NULL || (*ctx->env)->ExceptionCheck(ctx->env))
  {
    (*ctx->env)->ExceptionClear(ctx->env);
    (*ctx->env)->DeleteLocalRef(ctx->env, class_loader);
    (*ctx->env)->DeleteLocalRef(ctx->env, class_loader_class);
    jsc_engine_error(ctx, "failed to load class");
    return false;
  }

  ctx->runtime_class = (*ctx->env)->NewGlobalRef(ctx->env, loaded_class);
  (*ctx->env)->DeleteLocalRef(ctx->env, loaded_class);

  jmethodID main_method = (*ctx->env)->GetStaticMethodID(
      ctx->env, ctx->runtime_class, "main", "([Ljava/lang/String;)V");

  if (main_method == NULL)
  {
    (*ctx->env)->ExceptionClear(ctx->env);
    (*ctx->env)->DeleteLocalRef(ctx->env, class_loader);
    (*ctx->env)->DeleteLocalRef(ctx->env, class_loader_class);
    jsc_engine_error(ctx, "failed to find main method");
    return false;
  }

  ctx->execute_method = main_method;

  (*ctx->env)->DeleteLocalRef(ctx->env, class_loader);
  (*ctx->env)->DeleteLocalRef(ctx->env, class_loader_class);

  return true;
}

jsc_value jsc_engine_run(jsc_engine_context* ctx)
{
  if (ctx->had_error)
  {
    return jsc_value_create_undefined();
  }

  if (!jsc_engine_init_jvm(ctx))
  {
    return jsc_value_create_undefined();
  }

  (*ctx->env)->CallStaticVoidMethod(ctx->env, ctx->runtime_class,
                                    ctx->execute_method, ctx->args);

  if ((*ctx->env)->ExceptionCheck(ctx->env))
  {
    jthrowable exception = (*ctx->env)->ExceptionOccurred(ctx->env);
    (*ctx->env)->ExceptionClear(ctx->env);

    jclass throwable_class =
        (*ctx->env)->FindClass(ctx->env, "java/lang/Throwable");
    jmethodID get_message = (*ctx->env)->GetMethodID(
        ctx->env, throwable_class, "getMessage", "()Ljava/lang/String;");

    jstring message =
        (*ctx->env)->CallObjectMethod(ctx->env, exception, get_message);

    const char* error_message =
        (*ctx->env)->GetStringUTFChars(ctx->env, message, NULL);
    jsc_engine_error(ctx, error_message);
    (*ctx->env)->ReleaseStringUTFChars(ctx->env, message, error_message);

    (*ctx->env)->DeleteLocalRef(ctx->env, message);
    (*ctx->env)->DeleteLocalRef(ctx->env, throwable_class);
    (*ctx->env)->DeleteLocalRef(ctx->env, exception);

    return jsc_value_create_undefined();
  }

  return jsc_value_create_undefined();
}

jsc_value jsc_engine_call_method(jsc_engine_context* ctx,
                                 const char* method_name, jsc_value* args,
                                 int arg_count)
{
  if (ctx->env == NULL)
  {
    if (!jsc_engine_init_jvm(ctx))
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

  method_id = (*ctx->env)->GetStaticMethodID(ctx->env, ctx->runtime_class,
                                             method_name, descriptor);

  if (method_id == NULL)
  {
    (*ctx->env)->ExceptionClear(ctx->env);
    jsc_engine_error(ctx, "method not found");
    return jsc_value_create_undefined();
  }

  jobjectArray jargs = NULL;

  if (arg_count > 0)
  {
    jclass object_class = (*ctx->env)->FindClass(ctx->env, "java/lang/Object");
    jargs =
        (*ctx->env)->NewObjectArray(ctx->env, arg_count, object_class, NULL);
    (*ctx->env)->DeleteLocalRef(ctx->env, object_class);

    for (int i = 0; i < arg_count; i++)
    {
      jobject arg_obj = jsc_value_to_jobject(ctx->env, args[i]);
      (*ctx->env)->SetObjectArrayElement(ctx->env, jargs, i, arg_obj);
      (*ctx->env)->DeleteLocalRef(ctx->env, arg_obj);
    }
  }

  jobject result = (*ctx->env)->CallStaticObjectMethod(
      ctx->env, ctx->runtime_class, method_id, jargs);

  if (jargs != NULL)
  {
    (*ctx->env)->DeleteLocalRef(ctx->env, jargs);
  }

  if ((*ctx->env)->ExceptionCheck(ctx->env))
  {
    jthrowable exception = (*ctx->env)->ExceptionOccurred(ctx->env);
    (*ctx->env)->ExceptionClear(ctx->env);

    jclass throwable_class =
        (*ctx->env)->FindClass(ctx->env, "java/lang/Throwable");
    jmethodID get_message = (*ctx->env)->GetMethodID(
        ctx->env, throwable_class, "getMessage", "()Ljava/lang/String;");

    jstring message =
        (*ctx->env)->CallObjectMethod(ctx->env, exception, get_message);

    const char* error_message =
        (*ctx->env)->GetStringUTFChars(ctx->env, message, NULL);
    jsc_engine_error(ctx, error_message);
    (*ctx->env)->ReleaseStringUTFChars(ctx->env, message, error_message);

    (*ctx->env)->DeleteLocalRef(ctx->env, message);
    (*ctx->env)->DeleteLocalRef(ctx->env, throwable_class);
    (*ctx->env)->DeleteLocalRef(ctx->env, exception);

    return jsc_value_create_undefined();
  }

  jsc_value ret_val = jsc_value_from_jobject(ctx->env, result);

  if (result != NULL)
  {
    (*ctx->env)->DeleteLocalRef(ctx->env, result);
  }

  return ret_val;
}

jsc_value jsc_engine_eval(const char* source)
{
  jsc_engine_context* ctx = jsc_engine_init("JSCScript");

  if (!ctx)
  {
    jsc_value undefined = jsc_value_create_undefined();
    return undefined;
  }

  if (!jsc_engine_compile(ctx, source))
  {
    // jsc_value undefined = jsc_value_create_undefined();
    // jsc_engine_free(ctx);
    // return undefined;
  }

  if (!jsc_bytecode_write_to_file(ctx->bytecode, "JSCScript.class"))
  {
    jsc_value undefined = jsc_value_create_undefined();
    jsc_engine_free(ctx);
    return undefined;
  }

  if (!jsc_engine_init_jvm(ctx))
  {
    jsc_value undefined = jsc_value_create_undefined();
    jsc_engine_free(ctx);
    return undefined;
  }

  jsc_value result = jsc_engine_run(ctx);

  jsc_engine_free(ctx);

  return result;
}

jsc_symbol* jsc_engine_add_symbol(jsc_engine_context* ctx, const char* name,
                                  jsc_symbol_type type)
{
  jsc_symbol* existing = jsc_engine_lookup_symbol(ctx, name);

  if (existing && existing->scope_depth == ctx->current_scope->depth)
  {
    jsc_engine_error(ctx, "variable already declared in this scope");
    return NULL;
  }

  jsc_symbol* symbol = (jsc_symbol*)malloc(sizeof(jsc_symbol));

  if (!symbol)
  {
    jsc_engine_error(ctx, "jsc_engine_add_symbol malloc");
    return NULL;
  }

  memset(symbol, 0, sizeof(jsc_symbol));

  symbol->name = strdup(name);
  symbol->type = type;
  symbol->initialized = false;
  symbol->scope_depth = ctx->current_scope->depth;

  if (jsc_engine_is_global_scope(ctx))
  {
    symbol->index = 0;
  }
  else
  {
    symbol->index = ctx->local_index++;
    ctx->current_scope->local_count++;
  }

  symbol->next = ctx->current_scope->symbols;
  ctx->current_scope->symbols = symbol;

  return symbol;
}

jsc_symbol* jsc_engine_lookup_symbol(jsc_engine_context* ctx, const char* name)
{
  jsc_scope* scope = ctx->current_scope;

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

bool jsc_engine_match(jsc_engine_context* ctx, jsc_token_type type)
{
  if (!jsc_engine_check(ctx, type))
  {
    return false;
  }

  jsc_engine_advance(ctx);

  return true;
}

bool jsc_engine_check(jsc_engine_context* ctx, jsc_token_type type)
{
  return ctx->current_token.type == type;
}

void jsc_engine_advance(jsc_engine_context* ctx)
{
  ctx->current_token = jsc_next_token(ctx->tokenizer);

  if (jsc_tokenizer_has_error(ctx->tokenizer))
  {
    jsc_engine_error(ctx, jsc_tokenizer_get_error(ctx->tokenizer));
  }
}

void jsc_engine_error(jsc_engine_context* ctx, const char* message)
{
  if (ctx->had_error)
  {
    return;
  }

  ctx->had_error = true;

  if (ctx->error_message)
  {
    free(ctx->error_message);
  }

  ctx->error_message = strdup(message);
}

void jsc_engine_emit_byte(jsc_engine_context* ctx, uint8_t byte)
{
  jsc_bytecode_emit(ctx->bytecode, ctx->current_method, byte);

  switch (byte)
  {
  case JSC_JVM_POP:
    ctx->stack_size -= 1;
    break;
  case JSC_JVM_POP2:
    ctx->stack_size -= 2;
    break;
  case JSC_JVM_IADD:
  case JSC_JVM_ISUB:
  case JSC_JVM_IMUL:
  case JSC_JVM_IDIV:
  case JSC_JVM_IREM:
  case JSC_JVM_ISHL:
  case JSC_JVM_ISHR:
  case JSC_JVM_IUSHR:
  case JSC_JVM_IAND:
  case JSC_JVM_IOR:
  case JSC_JVM_IXOR:
  case JSC_JVM_FADD:
  case JSC_JVM_FSUB:
  case JSC_JVM_FMUL:
  case JSC_JVM_FDIV:
  case JSC_JVM_FREM:
    ctx->stack_size -= 1;
    break;
  case JSC_JVM_LADD:
  case JSC_JVM_LSUB:
  case JSC_JVM_LMUL:
  case JSC_JVM_LDIV:
  case JSC_JVM_LREM:
  case JSC_JVM_LSHL:
  case JSC_JVM_LSHR:
  case JSC_JVM_LUSHR:
  case JSC_JVM_LAND:
  case JSC_JVM_LOR:
  case JSC_JVM_LXOR:
  case JSC_JVM_DADD:
  case JSC_JVM_DSUB:
  case JSC_JVM_DMUL:
  case JSC_JVM_DDIV:
  case JSC_JVM_DREM:
    ctx->stack_size -= 2;
    break;
  case JSC_JVM_PUTSTATIC:
    ctx->stack_size -= 1;
    break;
  case JSC_JVM_PUTFIELD:
    ctx->stack_size -= 2;
    break;
  case JSC_JVM_IRETURN:
  case JSC_JVM_FRETURN:
  case JSC_JVM_ARETURN:
    ctx->stack_size -= 1;
    break;
  case JSC_JVM_LRETURN:
  case JSC_JVM_DRETURN:
    ctx->stack_size -= 2;
    break;
  case JSC_JVM_NEW:
    ctx->stack_size += 1;
    break;
  case JSC_JVM_DUP:
    ctx->stack_size += 1;
    break;
  case JSC_JVM_INVOKESPECIAL:
    ctx->stack_size -= 2;
    break;
  case JSC_JVM_INVOKEVIRTUAL:
  case JSC_JVM_INVOKESTATIC:
    ctx->stack_size -= 1;
    break;
  case JSC_JVM_GETSTATIC:
    ctx->stack_size += 1;
    break;
  case JSC_JVM_ACONST_NULL:
  case JSC_JVM_ICONST_M1:
  case JSC_JVM_ICONST_0:
  case JSC_JVM_ICONST_1:
  case JSC_JVM_ICONST_2:
  case JSC_JVM_ICONST_3:
  case JSC_JVM_ICONST_4:
  case JSC_JVM_ICONST_5:
  case JSC_JVM_FCONST_0:
  case JSC_JVM_FCONST_1:
  case JSC_JVM_FCONST_2:
  case JSC_JVM_BIPUSH:
  case JSC_JVM_SIPUSH:
  case JSC_JVM_LDC:
  case JSC_JVM_LDC_W:
    ctx->stack_size += 1;
    break;
  case JSC_JVM_LCONST_0:
  case JSC_JVM_LCONST_1:
  case JSC_JVM_DCONST_0:
  case JSC_JVM_DCONST_1:
  case JSC_JVM_LDC2_W:
    ctx->stack_size += 2;
    break;
  case JSC_JVM_ALOAD_0:
  case JSC_JVM_ALOAD_1:
  case JSC_JVM_ALOAD_2:
  case JSC_JVM_ALOAD_3:
  case JSC_JVM_ALOAD:
    ctx->stack_size += 1;
    break;
  case JSC_JVM_ASTORE_0:
  case JSC_JVM_ASTORE_1:
  case JSC_JVM_ASTORE_2:
  case JSC_JVM_ASTORE_3:
  case JSC_JVM_ASTORE:
    ctx->stack_size -= 1;
    break;
  }

  if (ctx->stack_size < 0)
    ctx->stack_size = 0;

  if (ctx->stack_size > ctx->max_stack)
  {
    ctx->max_stack = ctx->stack_size;
  }
}

void jsc_engine_emit_bytes(jsc_engine_context* ctx, uint8_t byte1,
                           uint8_t byte2)
{
  jsc_engine_emit_byte(ctx, byte1);
  jsc_engine_emit_byte(ctx, byte2);
}

uint16_t jsc_engine_emit_jump(jsc_engine_context* ctx, uint8_t instruction)
{
  jsc_engine_emit_byte(ctx, instruction);

  uint32_t code_offset =
      jsc_bytecode_get_method_code_length(ctx->current_method);

  jsc_bytecode_emit_u16(ctx->bytecode, ctx->current_method, 0, 0);

  return code_offset;
}

void jsc_engine_patch_jump(jsc_engine_context* ctx, uint16_t offset)
{
  uint8_t* code = jsc_bytecode_get_method_code(ctx->current_method);
  uint32_t current = jsc_bytecode_get_method_code_length(ctx->current_method);

  int16_t jump = current - offset - 2;

  uint16_t jump_be = htobe16((uint16_t)jump); /* jump offset to big-endian */
  memcpy(&code[offset], &jump_be, 2);
}

void jsc_engine_parse_program(jsc_engine_context* ctx)
{
  jsc_method* main_method = jsc_bytecode_create_method(
      ctx->bytecode, "main", "([Ljava/lang/String;)V",
      JSC_ACC_PUBLIC | JSC_ACC_STATIC, 10, 100);

  // ctx->bytecode->major_version = 49;
  ctx->current_method = main_method;
  ctx->stack_size = 0;
  ctx->max_stack = 10;
  ctx->local_index = 1;

  while (!jsc_engine_check(ctx, JSC_TOKEN_EOF))
  {
    jsc_engine_parse_declaration(ctx);

    if (ctx->had_error)
    {
      return;
    }

    ctx->stack_size = 0;
  }

  jsc_engine_emit_byte(ctx, JSC_JVM_RETURN);

  for (uint16_t i = 0; i < main_method->attribute_count; i++)
  {
    jsc_attribute* attr = &main_method->attributes[i];
    uint16_t name_index = attr->name_index;

    if (name_index > 0 &&
        ctx->bytecode->constant_pool[name_index].tag == JSC_CP_UTF8 &&
        ctx->bytecode->constant_pool[name_index].utf8_info.length == 4 &&
        memcmp(ctx->bytecode->constant_pool[name_index].utf8_info.bytes, "Code",
               4) == 0)
    {
      uint16_t max_stack_be = htobe16(ctx->max_stack);
      memcpy(attr->info, &max_stack_be, 2);

      uint16_t max_locals_be = htobe16(ctx->local_index);
      memcpy(attr->info + 2, &max_locals_be, 2);

      break;
    }
  }
}

void jsc_engine_parse_statement(jsc_engine_context* ctx)
{
  if (jsc_engine_match(ctx, JSC_TOKEN_IF))
  {
    jsc_engine_parse_if_statement(ctx);
  }
  else if (jsc_engine_match(ctx, JSC_TOKEN_WHILE))
  {
    jsc_engine_parse_while_statement(ctx);
  }
  else if (jsc_engine_match(ctx, JSC_TOKEN_FOR))
  {
    jsc_engine_parse_for_statement(ctx);
  }
  else if (jsc_engine_match(ctx, JSC_TOKEN_RETURN))
  {
    jsc_engine_parse_return_statement(ctx);
  }
  else if (jsc_engine_match(ctx, JSC_TOKEN_LEFT_BRACE))
  {
    jsc_engine_enter_scope(ctx);
    jsc_engine_parse_block(ctx);
    jsc_engine_exit_scope(ctx);
  }
  else
  {
    jsc_engine_parse_expression_statement(ctx);
  }
}

void jsc_engine_parse_declaration(jsc_engine_context* ctx)
{
  if (jsc_engine_match(ctx, JSC_TOKEN_VAR))
  {
    jsc_engine_parse_var_declaration(ctx, JSC_SYMBOL_VAR);
  }
  else if (jsc_engine_match(ctx, JSC_TOKEN_LET))
  {
    jsc_engine_parse_var_declaration(ctx, JSC_SYMBOL_LET);
  }
  else if (jsc_engine_match(ctx, JSC_TOKEN_CONST))
  {
    jsc_engine_parse_var_declaration(ctx, JSC_SYMBOL_CONST);
  }
  else if (jsc_engine_match(ctx, JSC_TOKEN_FUNCTION))
  {
    jsc_engine_parse_function_declaration(ctx);
  }
  else
  {
    jsc_engine_parse_statement(ctx);
  }
}

void jsc_engine_parse_var_declaration(jsc_engine_context* ctx,
                                      jsc_symbol_type type)
{
  if (!jsc_engine_check(ctx, JSC_TOKEN_IDENTIFIER))
  {
    jsc_engine_error(ctx, "expected variable name");
    return;
  }

  char name_buffer[1 << 8];
  strncpy(name_buffer, ctx->current_token.start, ctx->current_token.length);
  name_buffer[ctx->current_token.length] = '\0';

  jsc_engine_advance(ctx);

  jsc_symbol* symbol = jsc_engine_add_symbol(ctx, name_buffer, type);

  if (!symbol)
  {
    return;
  }

  if (jsc_engine_match(ctx, JSC_TOKEN_ASSIGN))
  {
    jsc_engine_parse_expr(ctx);
    symbol->initialized = true;

    if (jsc_engine_is_global_scope(ctx))
    {
      char field_name[300];
      sprintf(field_name, "global_%s", name_buffer);

      jsc_field* field = jsc_bytecode_add_field(
          ctx->bytecode, field_name, "Ljava/lang/Object;",
          JSC_ACC_PUBLIC | JSC_ACC_STATIC);

      jsc_bytecode_emit_field_access(ctx->bytecode, ctx->current_method,
                                     JSC_JVM_PUTSTATIC, ctx->class_name,
                                     field_name, "Ljava/lang/Object;");
    }
    else
    {
      jsc_bytecode_emit_local_var(ctx->bytecode, ctx->current_method,
                                  JSC_JVM_ASTORE, symbol->index);
    }
  }
  else
  {
    if (type == JSC_SYMBOL_CONST)
    {
      jsc_engine_error(ctx, "const variable must be initialized");
      return;
    }

    jsc_bytecode_emit(ctx->bytecode, ctx->current_method, JSC_JVM_ACONST_NULL);

    if (jsc_engine_is_global_scope(ctx))
    {
      char field_name[300];
      sprintf(field_name, "global_%s", name_buffer);

      jsc_field* field = jsc_bytecode_add_field(
          ctx->bytecode, field_name, "Ljava/lang/Object;",
          JSC_ACC_PUBLIC | JSC_ACC_STATIC);

      jsc_bytecode_emit_field_access(ctx->bytecode, ctx->current_method,
                                     JSC_JVM_PUTSTATIC, ctx->class_name,
                                     field_name, "Ljava/lang/Object;");
    }
    else
    {
      jsc_bytecode_emit_local_var(ctx->bytecode, ctx->current_method,
                                  JSC_JVM_ASTORE, symbol->index);
    }
  }

  if (!jsc_engine_match(ctx, JSC_TOKEN_SEMICOLON))
  {
    jsc_engine_error(ctx, "expected ';' after variable declaration");
  }
}

void jsc_engine_parse_function_declaration(jsc_engine_context* ctx)
{
  if (!jsc_engine_check(ctx, JSC_TOKEN_IDENTIFIER))
  {
    jsc_engine_error(ctx, "expected function name");
    return;
  }

  char name_buffer[1 << 8];
  strncpy(name_buffer, ctx->current_token.start, ctx->current_token.length);
  name_buffer[ctx->current_token.length] = '\0';

  jsc_engine_advance(ctx);

  jsc_symbol* symbol =
      jsc_engine_add_symbol(ctx, name_buffer, JSC_SYMBOL_FUNCTION);
  if (!symbol)
  {
    return;
  }

  if (!jsc_engine_match(ctx, JSC_TOKEN_LEFT_PAREN))
  {
    jsc_engine_error(ctx, "expected '(' after function name");
    return;
  }

  jsc_engine_enter_scope(ctx);
  ctx->current_scope->is_function = true;
  ctx->current_scope->function_name = strdup(name_buffer);

  int param_count = 0;
  if (!jsc_engine_check(ctx, JSC_TOKEN_RIGHT_PAREN))
  {
    do
    {
      if (!jsc_engine_check(ctx, JSC_TOKEN_IDENTIFIER))
      {
        jsc_engine_error(ctx, "expected parameter name");
        return;
      }

      char param_name[1 << 8];
      strncpy(param_name, ctx->current_token.start, ctx->current_token.length);
      param_name[ctx->current_token.length] = '\0';

      jsc_engine_advance(ctx);

      jsc_symbol* param =
          jsc_engine_add_symbol(ctx, param_name, JSC_SYMBOL_PARAMETER);
      if (!param)
      {
        return;
      }

      param_count++;
    } while (jsc_engine_match(ctx, JSC_TOKEN_COMMA));
  }

  if (!jsc_engine_match(ctx, JSC_TOKEN_RIGHT_PAREN))
  {
    jsc_engine_error(ctx, "expected ')' after parameters");
    return;
  }

  char* descriptor = jsc_engine_generate_descriptor(ctx, param_count);
  jsc_method* previous_method = ctx->current_method;

  jsc_engine_begin_function(ctx, name_buffer, descriptor);
  free(descriptor);

  if (!jsc_engine_match(ctx, JSC_TOKEN_LEFT_BRACE))
  {
    jsc_engine_error(ctx, "expected '{' before function body");
    return;
  }

  jsc_engine_parse_block(ctx);

  jsc_engine_emit_byte(ctx, JSC_JVM_ACONST_NULL);
  jsc_engine_emit_byte(ctx, JSC_JVM_ARETURN);

  jsc_engine_end_function(ctx);

  jsc_engine_exit_scope(ctx);

  ctx->current_method = previous_method;

  if (jsc_engine_is_global_scope(ctx))
  {
    char method_ref[300];
    sprintf(method_ref, "%s.%s", ctx->class_name, name_buffer);

    jsc_bytecode_emit_load_constant_string(ctx->bytecode, ctx->current_method,
                                           method_ref);

    char field_name[300];
    sprintf(field_name, "global_%s", name_buffer);

    jsc_field* field =
        jsc_bytecode_add_field(ctx->bytecode, field_name, "Ljava/lang/Object;",
                               JSC_ACC_PUBLIC | JSC_ACC_STATIC);

    jsc_bytecode_emit_field_access(ctx->bytecode, ctx->current_method,
                                   JSC_JVM_PUTSTATIC, ctx->class_name,
                                   field_name, "Ljava/lang/Object;");
  }
}

void jsc_engine_parse_expression_statement(jsc_engine_context* ctx)
{
  jsc_engine_parse_expr(ctx);

  if (!jsc_engine_match(ctx, JSC_TOKEN_SEMICOLON))
  {
    jsc_engine_error(ctx, "expected ';' after expression");
    return;
  }

  jsc_engine_emit_byte(ctx, JSC_JVM_POP);
}

void jsc_engine_parse_if_statement(jsc_engine_context* ctx)
{
  if (!jsc_engine_match(ctx, JSC_TOKEN_LEFT_PAREN))
  {
    jsc_engine_error(ctx, "expected '(' after 'if'");
    return;
  }

  jsc_engine_parse_expr(ctx);

  if (!jsc_engine_match(ctx, JSC_TOKEN_RIGHT_PAREN))
  {
    jsc_engine_error(ctx, "expected ')' after condition");
    return;
  }

  /* unbox the object and convert to boolean */
  jsc_bytecode_emit_invoke_virtual(ctx->bytecode, ctx->current_method,
                                   "java/lang/Object", "equals",
                                   "(Ljava/lang/Object;)Z");
  ctx->stack_size -= 1;

  uint16_t then_jump = jsc_engine_emit_jump(ctx, JSC_JVM_IFEQ);

  /*mark*/ jsc_bytecode_emit(ctx->bytecode, ctx->current_method, JSC_JVM_NOP);

  jsc_engine_parse_statement(ctx);

  uint16_t else_jump = jsc_engine_emit_jump(ctx, JSC_JVM_GOTO);

  jsc_engine_patch_jump(ctx, then_jump);

  /*mark*/ jsc_bytecode_emit(ctx->bytecode, ctx->current_method, JSC_JVM_NOP);

  if (jsc_engine_match(ctx, JSC_TOKEN_ELSE))
  {
    jsc_engine_parse_statement(ctx);
  }

  jsc_engine_patch_jump(ctx, else_jump);
}

void jsc_engine_parse_while_statement(jsc_engine_context* ctx)
{
  uint32_t loop_start =
      jsc_bytecode_get_method_code_length(ctx->current_method);

  if (!jsc_engine_match(ctx, JSC_TOKEN_LEFT_PAREN))
  {
    jsc_engine_error(ctx, "expected '(' after 'while'");
    return;
  }

  jsc_engine_parse_expr(ctx);

  if (!jsc_engine_match(ctx, JSC_TOKEN_RIGHT_PAREN))
  {
    jsc_engine_error(ctx, "expected ')' after condition");
    return;
  }

  uint16_t exit_jump = jsc_engine_emit_jump(ctx, JSC_JVM_IFEQ);

  jsc_engine_parse_statement(ctx);

  jsc_bytecode_emit_jump(
      ctx->bytecode, ctx->current_method, JSC_JVM_GOTO,
      -(jsc_bytecode_get_method_code_length(ctx->current_method) - loop_start +
        3));

  jsc_engine_patch_jump(ctx, exit_jump);
}

void jsc_engine_parse_for_statement(jsc_engine_context* ctx)
{
  jsc_engine_enter_scope(ctx);

  if (!jsc_engine_match(ctx, JSC_TOKEN_LEFT_PAREN))
  {
    jsc_engine_error(ctx, "expected '(' after 'for'");
    return;
  }

  if (jsc_engine_match(ctx, JSC_TOKEN_SEMICOLON))
  {
  }
  else if (jsc_engine_match(ctx, JSC_TOKEN_VAR))
  {
    jsc_engine_parse_var_declaration(ctx, JSC_SYMBOL_VAR);
  }
  else if (jsc_engine_match(ctx, JSC_TOKEN_LET))
  {
    jsc_engine_parse_var_declaration(ctx, JSC_SYMBOL_LET);
  }
  else
  {
    jsc_engine_parse_expression_statement(ctx);
  }

  uint32_t loop_start =
      jsc_bytecode_get_method_code_length(ctx->current_method);

  uint16_t exit_jump = 0;
  if (!jsc_engine_check(ctx, JSC_TOKEN_SEMICOLON))
  {
    jsc_engine_parse_expr(ctx);

    if (!jsc_engine_match(ctx, JSC_TOKEN_SEMICOLON))
    {
      jsc_engine_error(ctx, "expected ';' after loop condition");
      return;
    }

    exit_jump = jsc_engine_emit_jump(ctx, JSC_JVM_IFEQ);
  }
  else
  {
    jsc_engine_advance(ctx);
  }

  if (!jsc_engine_check(ctx, JSC_TOKEN_RIGHT_PAREN))
  {
    uint16_t body_jump = jsc_engine_emit_jump(ctx, JSC_JVM_GOTO);

    uint32_t increment_start =
        jsc_bytecode_get_method_code_length(ctx->current_method);

    jsc_engine_parse_expr(ctx);
    jsc_engine_emit_byte(ctx, JSC_JVM_POP);

    if (!jsc_engine_match(ctx, JSC_TOKEN_RIGHT_PAREN))
    {
      jsc_engine_error(ctx, "expected ')' after for clauses");
      return;
    }

    jsc_bytecode_emit_jump(
        ctx->bytecode, ctx->current_method, JSC_JVM_GOTO,
        -(jsc_bytecode_get_method_code_length(ctx->current_method) -
          loop_start + 3));

    jsc_engine_patch_jump(ctx, body_jump);

    jsc_engine_parse_statement(ctx);

    jsc_bytecode_emit_jump(
        ctx->bytecode, ctx->current_method, JSC_JVM_GOTO,
        -(jsc_bytecode_get_method_code_length(ctx->current_method) -
          increment_start + 3));
  }
  else
  {
    jsc_engine_advance(ctx);

    jsc_engine_parse_statement(ctx);

    jsc_bytecode_emit_jump(
        ctx->bytecode, ctx->current_method, JSC_JVM_GOTO,
        -(jsc_bytecode_get_method_code_length(ctx->current_method) -
          loop_start + 3));
  }

  if (exit_jump != 0)
  {
    jsc_engine_patch_jump(ctx, exit_jump);
  }

  jsc_engine_exit_scope(ctx);
}

void jsc_engine_parse_return_statement(jsc_engine_context* ctx)
{
  if (jsc_engine_is_global_scope(ctx))
  {
    jsc_engine_error(ctx, "cannot return from global scope");
    return;
  }

  if (jsc_engine_check(ctx, JSC_TOKEN_SEMICOLON))
  {
    jsc_engine_advance(ctx);
    jsc_engine_emit_byte(ctx, JSC_JVM_ACONST_NULL);
    jsc_engine_emit_byte(ctx, JSC_JVM_ARETURN);
    return;
  }

  jsc_engine_parse_expr(ctx);

  if (!jsc_engine_match(ctx, JSC_TOKEN_SEMICOLON))
  {
    jsc_engine_error(ctx, "expected ';' after return value");
    return;
  }

  jsc_engine_emit_byte(ctx, JSC_JVM_ARETURN);
}

void jsc_engine_parse_block(jsc_engine_context* ctx)
{
  while (!jsc_engine_check(ctx, JSC_TOKEN_RIGHT_BRACE) &&
         !jsc_engine_check(ctx, JSC_TOKEN_EOF))
  {
    jsc_engine_parse_declaration(ctx);
  }

  if (!jsc_engine_match(ctx, JSC_TOKEN_RIGHT_BRACE))
  {
    jsc_engine_error(ctx, "expected '}' after block");
    return;
  }
}

void jsc_engine_parse_expr(jsc_engine_context* ctx)
{
  jsc_engine_parse_assign(ctx);
}

void jsc_engine_parse_assign(jsc_engine_context* ctx)
{
  jsc_engine_parse_lor(ctx);

  if (jsc_engine_match(ctx, JSC_TOKEN_ASSIGN))
  {
    jsc_token token = ctx->current_token;
    jsc_token_type operator_type = token.type;

    jsc_engine_parse_assign(ctx);

    if (operator_type == JSC_TOKEN_ASSIGN)
    {
      char name_buffer[1 << 8];
      strncpy(name_buffer, token.start, token.length);
      name_buffer[token.length] = '\0';

      jsc_engine_store_variable(ctx, name_buffer);
    }
  }
}

void jsc_engine_parse_lor(jsc_engine_context* ctx)
{
  jsc_engine_parse_land(ctx);

  while (jsc_engine_match(ctx, JSC_TOKEN_LOGICAL_OR))
  {
    uint16_t end_jump = jsc_engine_emit_jump(ctx, JSC_JVM_IFNE);

    jsc_engine_parse_land(ctx);

    jsc_engine_patch_jump(ctx, end_jump);
  }
}

void jsc_engine_parse_land(jsc_engine_context* ctx)
{
  jsc_engine_parse_eq(ctx);

  while (jsc_engine_match(ctx, JSC_TOKEN_LOGICAL_AND))
  {
    uint16_t end_jump = jsc_engine_emit_jump(ctx, JSC_JVM_IFEQ);

    jsc_engine_parse_eq(ctx);

    jsc_engine_patch_jump(ctx, end_jump);
  }
}

void jsc_engine_parse_eq(jsc_engine_context* ctx)
{
  jsc_engine_parse_cmp(ctx);

  while (jsc_engine_match(ctx, JSC_TOKEN_EQUAL) ||
         jsc_engine_match(ctx, JSC_TOKEN_NOT_EQUAL) ||
         jsc_engine_match(ctx, JSC_TOKEN_STRICT_EQUAL) ||
         jsc_engine_match(ctx, JSC_TOKEN_STRICT_NOT_EQUAL))
  {

    jsc_token_type operator_type = ctx->current_token.type;

    jsc_engine_parse_cmp(ctx);

    switch (operator_type)
    {
    case JSC_TOKEN_EQUAL:
    case JSC_TOKEN_STRICT_EQUAL:
      jsc_bytecode_emit(ctx->bytecode, ctx->current_method, JSC_JVM_IF_ACMPEQ);
      break;
    case JSC_TOKEN_NOT_EQUAL:
    case JSC_TOKEN_STRICT_NOT_EQUAL:
      jsc_bytecode_emit(ctx->bytecode, ctx->current_method, JSC_JVM_IF_ACMPNE);
      break;
    default:
      break;
    }
  }
}

void jsc_engine_parse_cmp(jsc_engine_context* ctx)
{
  jsc_engine_parse_add(ctx);

  while (jsc_engine_match(ctx, JSC_TOKEN_LESS_THAN) ||
         jsc_engine_match(ctx, JSC_TOKEN_GREATER_THAN) ||
         jsc_engine_match(ctx, JSC_TOKEN_LESS_THAN_EQUAL) ||
         jsc_engine_match(ctx, JSC_TOKEN_GREATER_THAN_EQUAL))
  {

    jsc_token_type operator_type = ctx->current_token.type;

    jsc_engine_parse_add(ctx);

    switch (operator_type)
    {
    case JSC_TOKEN_LESS_THAN:
      jsc_bytecode_emit(ctx->bytecode, ctx->current_method, JSC_JVM_IF_ICMPLT);
      break;
    case JSC_TOKEN_GREATER_THAN:
      jsc_bytecode_emit(ctx->bytecode, ctx->current_method, JSC_JVM_IF_ICMPGT);
      break;
    case JSC_TOKEN_LESS_THAN_EQUAL:
      jsc_bytecode_emit(ctx->bytecode, ctx->current_method, JSC_JVM_IF_ICMPLE);
      break;
    case JSC_TOKEN_GREATER_THAN_EQUAL:
      jsc_bytecode_emit(ctx->bytecode, ctx->current_method, JSC_JVM_IF_ICMPGE);
      break;
    default:
      break;
    }
  }
}

void jsc_engine_parse_add(jsc_engine_context* ctx)
{
  jsc_engine_parse_mul(ctx);

  while (jsc_engine_match(ctx, JSC_TOKEN_PLUS) ||
         jsc_engine_match(ctx, JSC_TOKEN_MINUS))
  {
    jsc_token_type operator_type = ctx->current_token.type;

    jsc_engine_parse_mul(ctx);

    switch (operator_type)
    {
    case JSC_TOKEN_PLUS:
      jsc_bytecode_emit(ctx->bytecode, ctx->current_method, JSC_JVM_IADD);
      break;
    case JSC_TOKEN_MINUS:
      jsc_bytecode_emit(ctx->bytecode, ctx->current_method, JSC_JVM_ISUB);
      break;
    default:
      break;
    }
  }
}

void jsc_engine_parse_mul(jsc_engine_context* ctx)
{
  jsc_engine_parse_unary(ctx);

  while (jsc_engine_match(ctx, JSC_TOKEN_MULTIPLY) ||
         jsc_engine_match(ctx, JSC_TOKEN_DIVIDE) ||
         jsc_engine_match(ctx, JSC_TOKEN_MODULO))
  {

    jsc_token_type operator_type = ctx->current_token.type;

    jsc_engine_parse_unary(ctx);

    switch (operator_type)
    {
    case JSC_TOKEN_MULTIPLY:
      jsc_bytecode_emit(ctx->bytecode, ctx->current_method, JSC_JVM_IMUL);
      break;
    case JSC_TOKEN_DIVIDE:
      jsc_bytecode_emit(ctx->bytecode, ctx->current_method, JSC_JVM_IDIV);
      break;
    case JSC_TOKEN_MODULO:
      jsc_bytecode_emit(ctx->bytecode, ctx->current_method, JSC_JVM_IREM);
      break;
    default:
      break;
    }
  }
}

void jsc_engine_parse_unary(jsc_engine_context* ctx)
{
  if (jsc_engine_match(ctx, JSC_TOKEN_LOGICAL_NOT))
  {
    jsc_engine_parse_unary(ctx);
    jsc_engine_emit_byte(ctx, JSC_JVM_ICONST_0);
    jsc_engine_emit_byte(ctx, JSC_JVM_IF_ICMPEQ);
    jsc_engine_emit_byte(ctx, JSC_JVM_ICONST_1);
    jsc_engine_emit_byte(ctx, JSC_JVM_GOTO);
    jsc_engine_emit_byte(ctx, JSC_JVM_ICONST_0);
  }
  else if (jsc_engine_match(ctx, JSC_TOKEN_MINUS))
  {
    jsc_engine_parse_unary(ctx);
    jsc_engine_emit_byte(ctx, JSC_JVM_INEG);
  }
  else
  {
    jsc_engine_parse_call(ctx);
  }
}

void jsc_engine_parse_call(jsc_engine_context* ctx)
{
  jsc_engine_parse_primary(ctx);

  if (jsc_engine_match(ctx, JSC_TOKEN_LEFT_PAREN))
  {
    int arg_count = 0;

    jclass object_array_class =
        (*ctx->env)->FindClass(ctx->env, "[Ljava/lang/Object;");
    jmethodID array_init = (*ctx->env)->GetMethodID(
        ctx->env, object_array_class, "<init>", "(I)V");

    jobjectArray arg_array = (*ctx->env)->NewObjectArray(
        ctx->env, arg_count, object_array_class, NULL);

    if (!jsc_engine_check(ctx, JSC_TOKEN_RIGHT_PAREN))
    {
      do
      {
        jsc_engine_parse_expr(ctx);
        arg_count++;

        jobject arg = (*ctx->env)->GetObjectArrayElement(ctx->env, arg_array,
                                                         arg_count - 1);
        (*ctx->env)->SetObjectArrayElement(ctx->env, arg_array, arg_count - 1,
                                           arg);

      } while (jsc_engine_match(ctx, JSC_TOKEN_COMMA));
    }

    if (!jsc_engine_match(ctx, JSC_TOKEN_RIGHT_PAREN))
    {
      jsc_engine_error(ctx, "expected ')' after arguments");
      return;
    }

    jsc_bytecode_emit_invoke_static(
        ctx->bytecode, ctx->current_method, ctx->class_name, "invoke",
        "(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;");
  }
}

void jsc_engine_parse_primary(jsc_engine_context* ctx)
{
  if (jsc_engine_match(ctx, JSC_TOKEN_TRUE))
  {
    jsc_bytecode_emit_load_constant_int_boxed(ctx->bytecode,
                                              ctx->current_method, 1);
  }
  else if (jsc_engine_match(ctx, JSC_TOKEN_FALSE))
  {
    jsc_bytecode_emit_load_constant_int_boxed(ctx->bytecode,
                                              ctx->current_method, 0);
  }
  else if (jsc_engine_match(ctx, JSC_TOKEN_NULL))
  {
    jsc_engine_emit_byte(ctx, JSC_JVM_ACONST_NULL);
  }
  else if (jsc_engine_match(ctx, JSC_TOKEN_NUMBER))
  {
    jsc_bytecode_emit_load_constant_double_boxed(
        ctx->bytecode, ctx->current_method, ctx->current_token.number_value);
  }
  else if (jsc_engine_match(ctx, JSC_TOKEN_STRING))
  {
    jsc_bytecode_emit_load_constant_string(
        ctx->bytecode, ctx->current_method,
        ctx->current_token.string_value.data);
  }
  else if (jsc_engine_match(ctx, JSC_TOKEN_IDENTIFIER))
  {
    char name_buffer[1 << 8];
    strncpy(name_buffer, ctx->current_token.start, ctx->current_token.length);
    name_buffer[ctx->current_token.length] = '\0';

    jsc_engine_load_variable(ctx, name_buffer);
  }
  else if (jsc_engine_match(ctx, JSC_TOKEN_LEFT_PAREN))
  {
    jsc_engine_parse_expr(ctx);

    if (!jsc_engine_match(ctx, JSC_TOKEN_RIGHT_PAREN))
    {
      jsc_engine_error(ctx, "expected ')' after expression");
      return;
    }
  }
  else
  {
    jsc_engine_error(ctx, "expected expression");
  }
}

void jsc_engine_enter_scope(jsc_engine_context* ctx)
{
  jsc_scope* scope = (jsc_scope*)malloc(sizeof(jsc_scope));

  if (!scope)
  {
    jsc_engine_error(ctx, "jsc_engine_enter_scope malloc");
    return;
  }

  memset(scope, 0, sizeof(jsc_scope));

  scope->depth = ctx->current_scope->depth + 1;
  scope->parent = ctx->current_scope;

  if (ctx->current_scope->first_child)
  {
    jsc_scope* sibling = ctx->current_scope->first_child;

    while (sibling->next_sibling)
    {
      sibling = sibling->next_sibling;
    }

    sibling->next_sibling = scope;
  }
  else
  {
    ctx->current_scope->first_child = scope;
  }

  ctx->current_scope = scope;
}

void jsc_engine_exit_scope(jsc_engine_context* ctx)
{
  ctx->current_scope = ctx->current_scope->parent;
}

bool jsc_engine_is_global_scope(jsc_engine_context* ctx)
{
  return ctx->current_scope == ctx->global_scope;
}

void jsc_engine_begin_function(jsc_engine_context* ctx, const char* name,
                               const char* descriptor)
{
  jsc_method* method =
      jsc_bytecode_create_method(ctx->bytecode, name, descriptor,
                                 JSC_ACC_PUBLIC | JSC_ACC_STATIC, 100, 100);

  ctx->current_method = method;
  ctx->local_index = 0;
  ctx->stack_size = 0;
  ctx->max_stack = 0;
}

void jsc_engine_end_function(jsc_engine_context* ctx)
{
}

void jsc_engine_load_variable(jsc_engine_context* ctx, const char* name)
{
  jsc_symbol* symbol = jsc_engine_lookup_symbol(ctx, name);

  if (!symbol)
  {
    jsc_engine_error(ctx, "undefined variable");
    return;
  }

  if (!symbol->initialized && symbol->type == JSC_SYMBOL_CONST)
  {
    jsc_engine_error(ctx, "cannot access const variable before initialization");
    return;
  }

  if (jsc_engine_is_global_scope(ctx) || symbol->scope_depth == 0)
  {
    char field_name[300];
    sprintf(field_name, "global_%s", name);

    jsc_bytecode_emit_field_access(ctx->bytecode, ctx->current_method,
                                   JSC_JVM_GETSTATIC, ctx->class_name,
                                   field_name, "Ljava/lang/Object;");
    ctx->stack_size += 1;
  }
  else
  {
    jsc_bytecode_emit_local_var(ctx->bytecode, ctx->current_method,
                                JSC_JVM_ALOAD, symbol->index);
    ctx->stack_size += 1;
  }

  if (ctx->stack_size > ctx->max_stack)
  {
    ctx->max_stack = ctx->stack_size;
  }
}

void jsc_engine_store_variable(jsc_engine_context* ctx, const char* name)
{
  jsc_symbol* symbol = jsc_engine_lookup_symbol(ctx, name);

  if (!symbol)
  {
    jsc_engine_error(ctx, "undefined variable");
    return;
  }

  if (symbol->initialized && symbol->type == JSC_SYMBOL_CONST)
  {
    jsc_engine_error(ctx, "cannot reassign const variable");
    return;
  }

  symbol->initialized = true;

  if (jsc_engine_is_global_scope(ctx) || symbol->scope_depth == 0)
  {
    char field_name[300];
    sprintf(field_name, "global_%s", name);

    jsc_bytecode_emit_field_access(ctx->bytecode, ctx->current_method,
                                   JSC_JVM_PUTSTATIC, ctx->class_name,
                                   field_name, "Ljava/lang/Object;");
    ctx->stack_size -= 1;
  }
  else
  {
    jsc_bytecode_emit_local_var(ctx->bytecode, ctx->current_method,
                                JSC_JVM_ASTORE, symbol->index);
    ctx->stack_size -= 1;
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

char* jsc_engine_generate_descriptor(jsc_engine_context* ctx, int param_count)
{
  char* descriptor = malloc(param_count * 20 + 20);

  if (!descriptor)
  {
    jsc_engine_error(ctx, "jsc_engine_generate_descriptor malloc");
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
