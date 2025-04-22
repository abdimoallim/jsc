#ifndef JSC_BYTECODE_H
#define JSC_BYTECODE_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#define JSC_CP_UTF8 1
#define JSC_CP_INTEGER 3
#define JSC_CP_FLOAT 4
#define JSC_CP_LONG 5
#define JSC_CP_DOUBLE 6
#define JSC_CP_CLASS 7
#define JSC_CP_STRING 8
#define JSC_CP_FIELDREF 9
#define JSC_CP_METHODREF 10
#define JSC_CP_INTERFACE_METHODREF 11
#define JSC_CP_NAME_AND_TYPE 12
#define JSC_CP_METHOD_HANDLE 15
#define JSC_CP_METHOD_TYPE 16
#define JSC_CP_INVOKE_DYNAMIC 18

#define JSC_ACC_PUBLIC 0x0001
#define JSC_ACC_PRIVATE 0x0002
#define JSC_ACC_PROTECTED 0x0004
#define JSC_ACC_STATIC 0x0008
#define JSC_ACC_FINAL 0x0010
#define JSC_ACC_SUPER 0x0020
#define JSC_ACC_SYNCHRONIZED 0x0020
#define JSC_ACC_BRIDGE 0x0040
#define JSC_ACC_VOLATILE 0x0040
#define JSC_ACC_VARARGS 0x0080
#define JSC_ACC_TRANSIENT 0x0080
#define JSC_ACC_NATIVE 0x0100
#define JSC_ACC_INTERFACE 0x0200
#define JSC_ACC_ABSTRACT 0x0400
#define JSC_ACC_STRICT 0x0800
#define JSC_ACC_SYNTHETIC 0x1000
#define JSC_ACC_ANNOTATION 0x2000
#define JSC_ACC_ENUM 0x4000

#define JSC_MAX_LOCAL_VARS 65535
#define JSC_MAX_OPERAND_STACK 65535
#define JSC_MAX_CODE_SIZE 65535

typedef struct jsc_bytecode_state jsc_bytecode_state;
typedef struct jsc_constant_pool_entry jsc_constant_pool_entry;
typedef struct jsc_method jsc_method;
typedef struct jsc_field jsc_field;
typedef struct jsc_attribute jsc_attribute;
typedef struct jsc_exception_table_entry jsc_exception_table_entry;

typedef enum
{
  JSC_JVM_NOP = 0x00,
  JSC_JVM_ACONST_NULL = 0x01,
  JSC_JVM_ICONST_M1 = 0x02,
  JSC_JVM_ICONST_0 = 0x03,
  JSC_JVM_ICONST_1 = 0x04,
  JSC_JVM_ICONST_2 = 0x05,
  JSC_JVM_ICONST_3 = 0x06,
  JSC_JVM_ICONST_4 = 0x07,
  JSC_JVM_ICONST_5 = 0x08,
  JSC_JVM_LCONST_0 = 0x09,
  JSC_JVM_LCONST_1 = 0x0a,
  JSC_JVM_FCONST_0 = 0x0b,
  JSC_JVM_FCONST_1 = 0x0c,
  JSC_JVM_FCONST_2 = 0x0d,
  JSC_JVM_DCONST_0 = 0x0e,
  JSC_JVM_DCONST_1 = 0x0f,
  JSC_JVM_BIPUSH = 0x10,
  JSC_JVM_SIPUSH = 0x11,
  JSC_JVM_LDC = 0x12,
  JSC_JVM_LDC_W = 0x13,
  JSC_JVM_LDC2_W = 0x14,
  JSC_JVM_ILOAD = 0x15,
  JSC_JVM_LLOAD = 0x16,
  JSC_JVM_FLOAD = 0x17,
  JSC_JVM_DLOAD = 0x18,
  JSC_JVM_ALOAD = 0x19,
  JSC_JVM_ILOAD_0 = 0x1a,
  JSC_JVM_ILOAD_1 = 0x1b,
  JSC_JVM_ILOAD_2 = 0x1c,
  JSC_JVM_ILOAD_3 = 0x1d,
  JSC_JVM_LLOAD_0 = 0x1e,
  JSC_JVM_LLOAD_1 = 0x1f,
  JSC_JVM_LLOAD_2 = 0x20,
  JSC_JVM_LLOAD_3 = 0x21,
  JSC_JVM_FLOAD_0 = 0x22,
  JSC_JVM_FLOAD_1 = 0x23,
  JSC_JVM_FLOAD_2 = 0x24,
  JSC_JVM_FLOAD_3 = 0x25,
  JSC_JVM_DLOAD_0 = 0x26,
  JSC_JVM_DLOAD_1 = 0x27,
  JSC_JVM_DLOAD_2 = 0x28,
  JSC_JVM_DLOAD_3 = 0x29,
  JSC_JVM_ALOAD_0 = 0x2a,
  JSC_JVM_ALOAD_1 = 0x2b,
  JSC_JVM_ALOAD_2 = 0x2c,
  JSC_JVM_ALOAD_3 = 0x2d,
  JSC_JVM_IALOAD = 0x2e,
  JSC_JVM_LALOAD = 0x2f,
  JSC_JVM_FALOAD = 0x30,
  JSC_JVM_DALOAD = 0x31,
  JSC_JVM_AALOAD = 0x32,
  JSC_JVM_BALOAD = 0x33,
  JSC_JVM_CALOAD = 0x34,
  JSC_JVM_SALOAD = 0x35,
  JSC_JVM_ISTORE = 0x36,
  JSC_JVM_LSTORE = 0x37,
  JSC_JVM_FSTORE = 0x38,
  JSC_JVM_DSTORE = 0x39,
  JSC_JVM_ASTORE = 0x3a,
  JSC_JVM_ISTORE_0 = 0x3b,
  JSC_JVM_ISTORE_1 = 0x3c,
  JSC_JVM_ISTORE_2 = 0x3d,
  JSC_JVM_ISTORE_3 = 0x3e,
  JSC_JVM_LSTORE_0 = 0x3f,
  JSC_JVM_LSTORE_1 = 0x40,
  JSC_JVM_LSTORE_2 = 0x41,
  JSC_JVM_LSTORE_3 = 0x42,
  JSC_JVM_FSTORE_0 = 0x43,
  JSC_JVM_FSTORE_1 = 0x44,
  JSC_JVM_FSTORE_2 = 0x45,
  JSC_JVM_FSTORE_3 = 0x46,
  JSC_JVM_DSTORE_0 = 0x47,
  JSC_JVM_DSTORE_1 = 0x48,
  JSC_JVM_DSTORE_2 = 0x49,
  JSC_JVM_DSTORE_3 = 0x4a,
  JSC_JVM_ASTORE_0 = 0x4b,
  JSC_JVM_ASTORE_1 = 0x4c,
  JSC_JVM_ASTORE_2 = 0x4d,
  JSC_JVM_ASTORE_3 = 0x4e,
  JSC_JVM_IASTORE = 0x4f,
  JSC_JVM_LASTORE = 0x50,
  JSC_JVM_FASTORE = 0x51,
  JSC_JVM_DASTORE = 0x52,
  JSC_JVM_AASTORE = 0x53,
  JSC_JVM_BASTORE = 0x54,
  JSC_JVM_CASTORE = 0x55,
  JSC_JVM_SASTORE = 0x56,
  JSC_JVM_POP = 0x57,
  JSC_JVM_POP2 = 0x58,
  JSC_JVM_DUP = 0x59,
  JSC_JVM_DUP_X1 = 0x5a,
  JSC_JVM_DUP_X2 = 0x5b,
  JSC_JVM_DUP2 = 0x5c,
  JSC_JVM_DUP2_X1 = 0x5d,
  JSC_JVM_DUP2_X2 = 0x5e,
  JSC_JVM_SWAP = 0x5f,
  JSC_JVM_IADD = 0x60,
  JSC_JVM_LADD = 0x61,
  JSC_JVM_FADD = 0x62,
  JSC_JVM_DADD = 0x63,
  JSC_JVM_ISUB = 0x64,
  JSC_JVM_LSUB = 0x65,
  JSC_JVM_FSUB = 0x66,
  JSC_JVM_DSUB = 0x67,
  JSC_JVM_IMUL = 0x68,
  JSC_JVM_LMUL = 0x69,
  JSC_JVM_FMUL = 0x6a,
  JSC_JVM_DMUL = 0x6b,
  JSC_JVM_IDIV = 0x6c,
  JSC_JVM_LDIV = 0x6d,
  JSC_JVM_FDIV = 0x6e,
  JSC_JVM_DDIV = 0x6f,
  JSC_JVM_IREM = 0x70,
  JSC_JVM_LREM = 0x71,
  JSC_JVM_FREM = 0x72,
  JSC_JVM_DREM = 0x73,
  JSC_JVM_INEG = 0x74,
  JSC_JVM_LNEG = 0x75,
  JSC_JVM_FNEG = 0x76,
  JSC_JVM_DNEG = 0x77,
  JSC_JVM_ISHL = 0x78,
  JSC_JVM_LSHL = 0x79,
  JSC_JVM_ISHR = 0x7a,
  JSC_JVM_LSHR = 0x7b,
  JSC_JVM_IUSHR = 0x7c,
  JSC_JVM_LUSHR = 0x7d,
  JSC_JVM_IAND = 0x7e,
  JSC_JVM_LAND = 0x7f,
  JSC_JVM_IOR = 0x80,
  JSC_JVM_LOR = 0x81,
  JSC_JVM_IXOR = 0x82,
  JSC_JVM_LXOR = 0x83,
  JSC_JVM_IINC = 0x84,
  JSC_JVM_I2L = 0x85,
  JSC_JVM_I2F = 0x86,
  JSC_JVM_I2D = 0x87,
  JSC_JVM_L2I = 0x88,
  JSC_JVM_L2F = 0x89,
  JSC_JVM_L2D = 0x8a,
  JSC_JVM_F2I = 0x8b,
  JSC_JVM_F2L = 0x8c,
  JSC_JVM_F2D = 0x8d,
  JSC_JVM_D2I = 0x8e,
  JSC_JVM_D2L = 0x8f,
  JSC_JVM_D2F = 0x90,
  JSC_JVM_I2B = 0x91,
  JSC_JVM_I2C = 0x92,
  JSC_JVM_I2S = 0x93,
  JSC_JVM_LCMP = 0x94,
  JSC_JVM_FCMPL = 0x95,
  JSC_JVM_FCMPG = 0x96,
  JSC_JVM_DCMPL = 0x97,
  JSC_JVM_DCMPG = 0x98,
  JSC_JVM_IFEQ = 0x99,
  JSC_JVM_IFNE = 0x9a,
  JSC_JVM_IFLT = 0x9b,
  JSC_JVM_IFGE = 0x9c,
  JSC_JVM_IFGT = 0x9d,
  JSC_JVM_IFLE = 0x9e,
  JSC_JVM_IF_ICMPEQ = 0x9f,
  JSC_JVM_IF_ICMPNE = 0xa0,
  JSC_JVM_IF_ICMPLT = 0xa1,
  JSC_JVM_IF_ICMPGE = 0xa2,
  JSC_JVM_IF_ICMPGT = 0xa3,
  JSC_JVM_IF_ICMPLE = 0xa4,
  JSC_JVM_IF_ACMPEQ = 0xa5,
  JSC_JVM_IF_ACMPNE = 0xa6,
  JSC_JVM_GOTO = 0xa7,
  JSC_JVM_JSR = 0xa8,
  JSC_JVM_RET = 0xa9,
  JSC_JVM_TABLESWITCH = 0xaa,
  JSC_JVM_LOOKUPSWITCH = 0xab,
  JSC_JVM_IRETURN = 0xac,
  JSC_JVM_LRETURN = 0xad,
  JSC_JVM_FRETURN = 0xae,
  JSC_JVM_DRETURN = 0xaf,
  JSC_JVM_ARETURN = 0xb0,
  JSC_JVM_RETURN = 0xb1,
  JSC_JVM_GETSTATIC = 0xb2,
  JSC_JVM_PUTSTATIC = 0xb3,
  JSC_JVM_GETFIELD = 0xb4,
  JSC_JVM_PUTFIELD = 0xb5,
  JSC_JVM_INVOKEVIRTUAL = 0xb6,
  JSC_JVM_INVOKESPECIAL = 0xb7,
  JSC_JVM_INVOKESTATIC = 0xb8,
  JSC_JVM_INVOKEINTERFACE = 0xb9,
  JSC_JVM_INVOKEDYNAMIC = 0xba,
  JSC_JVM_NEW = 0xbb,
  JSC_JVM_NEWARRAY = 0xbc,
  JSC_JVM_ANEWARRAY = 0xbd,
  JSC_JVM_ARRAYLENGTH = 0xbe,
  JSC_JVM_ATHROW = 0xbf,
  JSC_JVM_CHECKCAST = 0xc0,
  JSC_JVM_INSTANCEOF = 0xc1,
  JSC_JVM_MONITORENTER = 0xc2,
  JSC_JVM_MONITOREXIT = 0xc3,
  JSC_JVM_WIDE = 0xc4,
  JSC_JVM_MULTIANEWARRAY = 0xc5,
  JSC_JVM_IFNULL = 0xc6,
  JSC_JVM_IFNONNULL = 0xc7,
  JSC_JVM_GOTO_W = 0xc8,
  JSC_JVM_JSR_W = 0xc9
} jsc_jvm_opcode;

struct jsc_bytecode_state
{
  uint8_t* bytecode;
  size_t bytecode_size;
  size_t bytecode_capacity;

  jsc_constant_pool_entry* constant_pool;
  uint16_t constant_pool_count;

  jsc_method* methods;
  uint16_t method_count;

  jsc_field* fields;
  uint16_t field_count;

  jsc_attribute* attributes;
  uint16_t attribute_count;

  uint16_t access_flags;
  uint16_t this_class;
  uint16_t super_class;

  uint16_t* interfaces;
  uint16_t interface_count;

  char* class_name;
  char* source_file;
  char* super_class_name;

  uint16_t major_version;
  uint16_t minor_version;
};

struct jsc_constant_pool_entry
{
  uint8_t tag;
  union
  {
    struct
    {
      uint16_t name_index;
    } class_info;

    struct
    {
      uint16_t class_index;
      uint16_t name_and_type_index;
    } fieldref_info;

    struct
    {
      uint16_t class_index;
      uint16_t name_and_type_index;
    } methodref_info;

    struct
    {
      uint16_t class_index;
      uint16_t name_and_type_index;
    } interface_methodref_info;

    struct
    {
      uint16_t string_index;
    } string_info;

    struct
    {
      int32_t value;
    } integer_info;

    struct
    {
      float value;
    } float_info;

    struct
    {
      int64_t value;
    } long_info;

    struct
    {
      double value;
    } double_info;

    struct
    {
      uint16_t name_index;
      uint16_t descriptor_index;
    } name_and_type_info;

    struct
    {
      uint16_t length;
      uint8_t* bytes;
    } utf8_info;

    struct
    {
      uint8_t reference_kind;
      uint16_t reference_index;
    } method_handle_info;

    struct
    {
      uint16_t descriptor_index;
    } method_type_info;

    struct
    {
      uint16_t bootstrap_method_attr_index;
      uint16_t name_and_type_index;
    } invoke_dynamic_info;
  };
};

struct jsc_method
{
  uint16_t access_flags;
  uint16_t name_index;
  uint16_t descriptor_index;
  jsc_attribute* attributes;
  uint16_t attribute_count;
};

struct jsc_field
{
  uint16_t access_flags;
  uint16_t name_index;
  uint16_t descriptor_index;
  jsc_attribute* attributes;
  uint16_t attribute_count;
};

struct jsc_attribute
{
  uint16_t name_index;
  uint32_t length;
  uint8_t* info;
};

struct jsc_exception_table_entry
{
  uint16_t start_pc;
  uint16_t end_pc;
  uint16_t handler_pc;
  uint16_t catch_type;
};

jsc_bytecode_state* jsc_bytecode_init(void);
void jsc_bytecode_free(jsc_bytecode_state* state);

void jsc_bytecode_set_class_name(jsc_bytecode_state* state,
                                 const char* class_name);
void jsc_bytecode_set_super_class(jsc_bytecode_state* state,
                                  const char* super_class_name);
void jsc_bytecode_set_source_file(jsc_bytecode_state* state,
                                  const char* source_file);
void jsc_bytecode_set_access_flags(jsc_bytecode_state* state,
                                   uint16_t access_flags);
void jsc_bytecode_set_version(jsc_bytecode_state* state, uint16_t major,
                              uint16_t minor);

uint16_t jsc_bytecode_add_interface(jsc_bytecode_state* state,
                                    const char* interface_name);

uint16_t jsc_bytecode_add_utf8_constant(jsc_bytecode_state* state,
                                        const char* str);
uint16_t jsc_bytecode_add_integer_constant(jsc_bytecode_state* state,
                                           int32_t value);
uint16_t jsc_bytecode_add_float_constant(jsc_bytecode_state* state,
                                         float value);
uint16_t jsc_bytecode_add_long_constant(jsc_bytecode_state* state,
                                        int64_t value);
uint16_t jsc_bytecode_add_double_constant(jsc_bytecode_state* state,
                                          double value);
uint16_t jsc_bytecode_add_string_constant(jsc_bytecode_state* state,
                                          const char* str);
uint16_t jsc_bytecode_add_class_constant(jsc_bytecode_state* state,
                                         const char* class_name);
uint16_t jsc_bytecode_add_name_and_type_constant(jsc_bytecode_state* state,
                                                 const char* name,
                                                 const char* descriptor);
uint16_t jsc_bytecode_add_field_reference(jsc_bytecode_state* state,
                                          const char* class_name,
                                          const char* field_name,
                                          const char* field_descriptor);
uint16_t jsc_bytecode_add_method_reference(jsc_bytecode_state* state,
                                           const char* class_name,
                                           const char* method_name,
                                           const char* method_descriptor);
uint16_t jsc_bytecode_add_interface_method_reference(
    jsc_bytecode_state* state, const char* interface_name,
    const char* method_name, const char* method_descriptor);

jsc_method* jsc_bytecode_add_method(jsc_bytecode_state* state, const char* name,
                                    const char* descriptor,
                                    uint16_t access_flags);
jsc_field* jsc_bytecode_add_field(jsc_bytecode_state* state, const char* name,
                                  const char* descriptor,
                                  uint16_t access_flags);

jsc_attribute* jsc_bytecode_add_attribute(jsc_bytecode_state* state,
                                          const char* name, uint32_t length);
jsc_attribute* jsc_bytecode_add_method_attribute(jsc_bytecode_state* state,
                                                 jsc_method* method,
                                                 const char* name,
                                                 uint32_t length);
jsc_attribute* jsc_bytecode_add_field_attribute(jsc_bytecode_state* state,
                                                jsc_field* field,
                                                const char* name,
                                                uint32_t length);

void jsc_bytecode_add_code_attribute(jsc_bytecode_state* state,
                                     jsc_method* method, uint16_t max_stack,
                                     uint16_t max_locals, uint8_t* code,
                                     uint32_t code_length);
void jsc_bytecode_add_exception_table_entry(jsc_bytecode_state* state,
                                            jsc_attribute* code_attribute,
                                            uint16_t start_pc, uint16_t end_pc,
                                            uint16_t handler_pc,
                                            uint16_t catch_type);

uint32_t jsc_bytecode_write(jsc_bytecode_state* state, uint8_t** out_buffer);
bool jsc_bytecode_write_to_file(jsc_bytecode_state* state,
                                const char* filename);

jsc_bytecode_state* jsc_bytecode_create_class(const char* class_name,
                                              const char* super_class_name,
                                              uint16_t access_flags);
jsc_method* jsc_bytecode_create_method(jsc_bytecode_state* state,
                                       const char* name, const char* descriptor,
                                       uint16_t access_flags,
                                       uint16_t max_stack, uint16_t max_locals);

void jsc_bytecode_emit(jsc_bytecode_state* state, jsc_method* method,
                       uint8_t opcode);
void jsc_bytecode_emit_u8(jsc_bytecode_state* state, jsc_method* method,
                          uint8_t opcode, uint8_t operand);
void jsc_bytecode_emit_u16(jsc_bytecode_state* state, jsc_method* method,
                           uint8_t opcode, uint16_t operand);
void jsc_bytecode_emit_jump(jsc_bytecode_state* state, jsc_method* method,
                            uint8_t opcode, int16_t offset);
void jsc_bytecode_emit_local_var(jsc_bytecode_state* state, jsc_method* method,
                                 uint8_t opcode, uint16_t index);
void jsc_bytecode_emit_const_load(jsc_bytecode_state* state, jsc_method* method,
                                  uint16_t index);
void jsc_bytecode_emit_invoke_virtual(jsc_bytecode_state* state,
                                      jsc_method* method,
                                      const char* class_name,
                                      const char* method_name,
                                      const char* descriptor);
void jsc_bytecode_emit_invoke_special(jsc_bytecode_state* state,
                                      jsc_method* method,
                                      const char* class_name,
                                      const char* method_name,
                                      const char* descriptor);
void jsc_bytecode_emit_invoke_static(jsc_bytecode_state* state,
                                     jsc_method* method, const char* class_name,
                                     const char* method_name,
                                     const char* descriptor);
void jsc_bytecode_emit_invoke_interface(jsc_bytecode_state* state,
                                        jsc_method* method,
                                        const char* interface_name,
                                        const char* method_name,
                                        const char* descriptor, uint8_t count);
void jsc_bytecode_emit_field_access(jsc_bytecode_state* state,
                                    jsc_method* method, uint8_t opcode,
                                    const char* class_name,
                                    const char* field_name,
                                    const char* descriptor);
void jsc_bytecode_emit_new(jsc_bytecode_state* state, jsc_method* method,
                           const char* class_name);
void jsc_bytecode_emit_newarray(jsc_bytecode_state* state, jsc_method* method,
                                uint8_t array_type);
void jsc_bytecode_emit_anewarray(jsc_bytecode_state* state, jsc_method* method,
                                 const char* component_type);
void jsc_bytecode_emit_return(jsc_bytecode_state* state, jsc_method* method,
                              char type);

uint32_t jsc_bytecode_get_method_code_length(jsc_method* method);
uint8_t* jsc_bytecode_get_method_code(jsc_method* method);
uint32_t jsc_bytecode_get_method_code_offset(jsc_method* method);

void jsc_bytecode_emit_constructor(jsc_bytecode_state* state,
                                   jsc_method* method, const char* super_class);
void jsc_bytecode_emit_line_number(jsc_bytecode_state* state,
                                   jsc_method* method, uint16_t line_number,
                                   uint16_t start_pc);
void jsc_bytecode_emit_local_variable(jsc_bytecode_state* state,
                                      jsc_method* method, const char* name,
                                      const char* descriptor, uint16_t start_pc,
                                      uint16_t length, uint16_t index);

void jsc_bytecode_emit_load_constant_int(jsc_bytecode_state* state,
                                         jsc_method* method, int32_t value);
void jsc_bytecode_emit_load_constant_long(jsc_bytecode_state* state,
                                          jsc_method* method, int64_t value);
void jsc_bytecode_emit_load_constant_float(jsc_bytecode_state* state,
                                           jsc_method* method, float value);
void jsc_bytecode_emit_load_constant_double(jsc_bytecode_state* state,
                                            jsc_method* method, double value);
void jsc_bytecode_emit_load_constant_string(jsc_bytecode_state* state,
                                            jsc_method* method,
                                            const char* value);

#endif
