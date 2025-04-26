#ifndef JSC_ENGINE_H
#define JSC_ENGINE_H

#include "jsc_tokenizer.h"
#include "jsc_bytecode.h"

#include <stdint.h>
#include <stdbool.h>
#include <jni.h>

typedef struct jsc_symbol jsc_symbol;
typedef struct jsc_scope jsc_scope;
typedef struct jsc_engine_context jsc_engine_context;
typedef struct jsc_value jsc_value;

typedef enum
{
  JSC_SYMBOL_VAR,
  JSC_SYMBOL_LET,
  JSC_SYMBOL_CONST,
  JSC_SYMBOL_FUNCTION,
  JSC_SYMBOL_PARAMETER,
  JSC_SYMBOL_THIS
} jsc_symbol_type;

typedef enum
{
  JSC_VALUE_UNDEFINED,
  JSC_VALUE_NULL,
  JSC_VALUE_BOOLEAN,
  JSC_VALUE_NUMBER,
  JSC_VALUE_STRING,
  JSC_VALUE_OBJECT,
  JSC_VALUE_FUNCTION,
  JSC_VALUE_ARRAY
} jsc_value_type;

struct jsc_symbol
{
  char* name;
  jsc_symbol_type type;
  bool initialized;
  uint16_t index;
  uint16_t scope_depth;
  jsc_symbol* next;
};

struct jsc_scope
{
  jsc_symbol* symbols;
  uint16_t depth;
  uint16_t local_count;
  bool is_function;
  char* function_name;
  jsc_scope* parent;
  jsc_scope* next_sibling;
  jsc_scope* first_child;
};

struct jsc_value
{
  jsc_value_type type;
  union
  {
    bool boolean_value;
    double number_value;
    char* string_value;
    jobject object_value;
  };
};

struct jsc_engine_context
{
  jsc_tokenizer_context* tokenizer;
  jsc_bytecode_context* bytecode;
  jsc_token current_token;
  jsc_scope* global_scope;
  jsc_scope* current_scope;
  jsc_method* current_method;
  char* class_name;
  uint16_t local_index;
  uint16_t stack_size;
  uint16_t max_stack;
  bool had_error;
  char* error_message;

  JavaVM* jvm;
  JNIEnv* env;
  jclass runtime_class;
  jmethodID execute_method;
  jmethodID call_method;
  jobject runtime_instance;
  jobjectArray args;

  char* class_path;
  char* temp_dir;
};

jsc_engine_context* jsc_engine_init(const char* class_name);
void jsc_engine_free(jsc_engine_context* ctx);

bool jsc_engine_compile(jsc_engine_context* ctx, const char* source);
bool jsc_engine_init_jvm(jsc_engine_context* ctx);
bool jsc_engine_load_class(jsc_engine_context* ctx, const char* class_file);
jsc_value jsc_engine_run(jsc_engine_context* ctx);
jsc_value jsc_engine_call_method(jsc_engine_context* ctx,
                                 const char* method_name, jsc_value* args,
                                 int arg_count);

jsc_value jsc_engine_eval(const char* source);

jsc_symbol* jsc_engine_add_symbol(jsc_engine_context* ctx, const char* name,
                                  jsc_symbol_type type);
jsc_symbol* jsc_engine_lookup_symbol(jsc_engine_context* ctx, const char* name);

bool jsc_engine_match(jsc_engine_context* ctx, jsc_token_type type);
bool jsc_engine_check(jsc_engine_context* ctx, jsc_token_type type);
void jsc_engine_advance(jsc_engine_context* ctx);
void jsc_engine_error(jsc_engine_context* ctx, const char* message);

void jsc_engine_emit_byte(jsc_engine_context* ctx, uint8_t byte);
void jsc_engine_emit_bytes(jsc_engine_context* ctx, uint8_t byte1,
                           uint8_t byte2);
uint16_t jsc_engine_emit_jump(jsc_engine_context* ctx, uint8_t instruction);
void jsc_engine_patch_jump(jsc_engine_context* ctx, uint16_t offset);

void jsc_engine_parse_program(jsc_engine_context* ctx);
void jsc_engine_parse_statement(jsc_engine_context* ctx);
void jsc_engine_parse_declaration(jsc_engine_context* ctx);
void jsc_engine_parse_var_declaration(jsc_engine_context* ctx,
                                      jsc_symbol_type type);
void jsc_engine_parse_function_declaration(jsc_engine_context* ctx);
void jsc_engine_parse_expression_statement(jsc_engine_context* ctx);
void jsc_engine_parse_if_statement(jsc_engine_context* ctx);
void jsc_engine_parse_while_statement(jsc_engine_context* ctx);
void jsc_engine_parse_for_statement(jsc_engine_context* ctx);
void jsc_engine_parse_return_statement(jsc_engine_context* ctx);
void jsc_engine_parse_block(jsc_engine_context* ctx);

void jsc_engine_parse_expr(jsc_engine_context* ctx);
void jsc_engine_parse_assign(jsc_engine_context* ctx);
void jsc_engine_parse_lor(jsc_engine_context* ctx);
void jsc_engine_parse_land(jsc_engine_context* ctx);
void jsc_engine_parse_eq(jsc_engine_context* ctx);
void jsc_engine_parse_cmp(jsc_engine_context* ctx);
void jsc_engine_parse_add(jsc_engine_context* ctx);
void jsc_engine_parse_mul(jsc_engine_context* ctx);
void jsc_engine_parse_unary(jsc_engine_context* ctx);
void jsc_engine_parse_call(jsc_engine_context* ctx);
void jsc_engine_parse_primary(jsc_engine_context* ctx);

void jsc_engine_enter_scope(jsc_engine_context* ctx);
void jsc_engine_exit_scope(jsc_engine_context* ctx);
bool jsc_engine_is_global_scope(jsc_engine_context* ctx);

void jsc_engine_begin_function(jsc_engine_context* ctx, const char* name,
                               const char* descriptor);
void jsc_engine_end_function(jsc_engine_context* ctx);

void jsc_engine_load_variable(jsc_engine_context* ctx, const char* name);
void jsc_engine_store_variable(jsc_engine_context* ctx, const char* name);

jsc_value jsc_value_create_undefined(void);
jsc_value jsc_value_create_null(void);
jsc_value jsc_value_create_boolean(bool value);
jsc_value jsc_value_create_number(double value);
jsc_value jsc_value_create_string(const char* value);
jsc_value jsc_value_from_jobject(JNIEnv* env, jobject obj);
jobject jsc_value_to_jobject(JNIEnv* env, jsc_value value);
char* jsc_value_to_string(jsc_value value);
void jsc_value_free(JNIEnv* env, jsc_value value);

char* jsc_engine_generate_descriptor(jsc_engine_context* ctx, int param_count);
char* jsc_engine_get_temp_filename(const char* prefix, const char* suffix);

#endif
