#include "jsc_bytecode.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <endian.h>

static void jsc_write_u8(uint8_t** buffer, uint8_t value)
{
  **buffer = value;
  (*buffer)++;
}

static void jsc_write_u16(uint8_t** buffer, uint16_t value)
{
  uint16_t big_endian = htobe16(value);
  memcpy(*buffer, &big_endian, sizeof(uint16_t));
  *buffer += sizeof(uint16_t);
}

static void jsc_write_u32(uint8_t** buffer, uint32_t value)
{
  uint32_t big_endian = htobe32(value);
  memcpy(*buffer, &big_endian, sizeof(uint32_t));
  *buffer += sizeof(uint32_t);
}

static void jsc_write_bytes(uint8_t** buffer, const uint8_t* data,
                            size_t length)
{
  memcpy(*buffer, data, length);
  *buffer += length;
}

jsc_bytecode_context* jsc_bytecode_init(void)
{
  jsc_bytecode_context* ctx =
      (jsc_bytecode_context*)malloc(sizeof(jsc_bytecode_context));

  if (!ctx)
  {
    return NULL;
  }

  memset(ctx, 0, sizeof(jsc_bytecode_context));

  ctx->bytecode_capacity = (1 << 12);
  ctx->bytecode = (uint8_t*)malloc(ctx->bytecode_capacity);

  if (!ctx->bytecode)
  {
    free(ctx);
    return NULL;
  }

  ctx->major_version = 52;
  ctx->minor_version = 0;

  ctx->constant_pool = NULL;
  ctx->constant_pool_count = 1;

  ctx->methods = NULL;
  ctx->method_count = 0;

  ctx->fields = NULL;
  ctx->field_count = 0;

  ctx->attributes = NULL;
  ctx->attribute_count = 0;

  ctx->interfaces = NULL;
  ctx->interface_count = 0;

  ctx->access_flags = JSC_ACC_PUBLIC | JSC_ACC_SUPER;

  return ctx;
}

void jsc_bytecode_free(jsc_bytecode_context* ctx)
{
  if (!ctx)
  {
    return;
  }

  if (ctx->bytecode)
  {
    free(ctx->bytecode);
  }

  if (ctx->class_name)
  {
    free(ctx->class_name);
  }

  if (ctx->source_file)
  {
    free(ctx->source_file);
  }

  if (ctx->super_class_name)
  {
    free(ctx->super_class_name);
  }

  if (ctx->interfaces)
  {
    free(ctx->interfaces);
  }

  if (ctx->constant_pool)
  {
    for (uint16_t i = 1; i < ctx->constant_pool_count; i++)
    {
      if (ctx->constant_pool[i].tag == JSC_CP_UTF8)
      {
        free(ctx->constant_pool[i].utf8_info.bytes);
      }
    }

    free(ctx->constant_pool);
  }

  if (ctx->methods)
  {
    for (uint16_t i = 0; i < ctx->method_count; i++)
    {
      if (ctx->methods[i].attributes)
      {
        for (uint16_t j = 0; j < ctx->methods[i].attribute_count; j++)
        {
          free(ctx->methods[i].attributes[j].info);
        }

        free(ctx->methods[i].attributes);
      }
    }

    free(ctx->methods);
  }

  if (ctx->fields)
  {
    for (uint16_t i = 0; i < ctx->field_count; i++)
    {
      if (ctx->fields[i].attributes)
      {
        for (uint16_t j = 0; j < ctx->fields[i].attribute_count; j++)
        {
          free(ctx->fields[i].attributes[j].info);
        }

        free(ctx->fields[i].attributes);
      }
    }

    free(ctx->fields);
  }

  if (ctx->attributes)
  {
    for (uint16_t i = 0; i < ctx->attribute_count; i++)
    {
      free(ctx->attributes[i].info);
    }

    free(ctx->attributes);
  }

  free(ctx);
}

void jsc_bytecode_set_class_name(jsc_bytecode_context* ctx,
                                 const char* class_name)
{
  if (ctx->class_name)
  {
    free(ctx->class_name);
  }

  ctx->class_name = strdup(class_name);
}

void jsc_bytecode_set_super_class(jsc_bytecode_context* ctx,
                                  const char* super_class_name)
{
  if (ctx->super_class_name)
  {
    free(ctx->super_class_name);
  }

  ctx->super_class_name = strdup(super_class_name);
}

void jsc_bytecode_set_source_file(jsc_bytecode_context* ctx,
                                  const char* source_file)
{
  if (ctx->source_file)
  {
    free(ctx->source_file);
  }

  ctx->source_file = strdup(source_file);
}

void jsc_bytecode_set_access_flags(jsc_bytecode_context* ctx,
                                   uint16_t access_flags)
{
  ctx->access_flags = access_flags;
}

void jsc_bytecode_set_version(jsc_bytecode_context* ctx, uint16_t major,
                              uint16_t minor)
{
  ctx->major_version = major;
  ctx->minor_version = minor;
}

uint16_t jsc_bytecode_add_utf8_constant(jsc_bytecode_context* ctx,
                                        const char* str)
{
  if (!str)
  {
    return 0;
  }

  size_t len = strlen(str);

  if (len > UINT16_MAX)
  {
    return 0;
  }

  for (uint16_t i = 1; i < ctx->constant_pool_count; i++)
  {
    if (ctx->constant_pool[i].tag == JSC_CP_UTF8 &&
        ctx->constant_pool[i].utf8_info.length == len &&
        memcmp(ctx->constant_pool[i].utf8_info.bytes, str, len) == 0)
    {
      return i;
    }
  }

  ctx->constant_pool = (jsc_constant_pool_entry*)realloc(
      ctx->constant_pool,
      (ctx->constant_pool_count + 1) * sizeof(jsc_constant_pool_entry));

  jsc_constant_pool_entry* entry =
      &ctx->constant_pool[ctx->constant_pool_count];
  entry->tag = JSC_CP_UTF8;
  entry->utf8_info.length = (uint16_t)len;
  entry->utf8_info.bytes = (uint8_t*)malloc(len);
  memcpy(entry->utf8_info.bytes, str, len);

  return ctx->constant_pool_count++;
}

uint16_t jsc_bytecode_add_integer_constant(jsc_bytecode_context* ctx,
                                           int32_t value)
{
  for (uint16_t i = 1; i < ctx->constant_pool_count; i++)
  {
    if (ctx->constant_pool[i].tag == JSC_CP_INTEGER &&
        ctx->constant_pool[i].integer_info.value == value)
    {
      return i;
    }
  }

  ctx->constant_pool = (jsc_constant_pool_entry*)realloc(
      ctx->constant_pool,
      (ctx->constant_pool_count + 1) * sizeof(jsc_constant_pool_entry));

  jsc_constant_pool_entry* entry =
      &ctx->constant_pool[ctx->constant_pool_count];
  entry->tag = JSC_CP_INTEGER;
  entry->integer_info.value = value;

  return ctx->constant_pool_count++;
}

uint16_t jsc_bytecode_add_float_constant(jsc_bytecode_context* ctx, float value)
{
  for (uint16_t i = 1; i < ctx->constant_pool_count; i++)
  {
    if (ctx->constant_pool[i].tag == JSC_CP_FLOAT &&
        ctx->constant_pool[i].float_info.value == value)
    {
      return i;
    }
  }

  ctx->constant_pool = (jsc_constant_pool_entry*)realloc(
      ctx->constant_pool,
      (ctx->constant_pool_count + 1) * sizeof(jsc_constant_pool_entry));

  jsc_constant_pool_entry* entry =
      &ctx->constant_pool[ctx->constant_pool_count];
  entry->tag = JSC_CP_FLOAT;
  entry->float_info.value = value;

  return ctx->constant_pool_count++;
}

uint16_t jsc_bytecode_add_long_constant(jsc_bytecode_context* ctx,
                                        int64_t value)
{
  for (uint16_t i = 1; i < ctx->constant_pool_count; i++)
  {
    if (ctx->constant_pool[i].tag == JSC_CP_LONG &&
        ctx->constant_pool[i].long_info.value == value)
    {
      return i;
    }
  }

  ctx->constant_pool = (jsc_constant_pool_entry*)realloc(
      ctx->constant_pool,
      (ctx->constant_pool_count + 2) * sizeof(jsc_constant_pool_entry));

  uint16_t index = ctx->constant_pool_count;
  jsc_constant_pool_entry* entry = &ctx->constant_pool[index];
  entry->tag = JSC_CP_LONG;
  entry->long_info.value = value;

  ctx->constant_pool_count += 2;

  return index;
}

uint16_t jsc_bytecode_add_double_constant(jsc_bytecode_context* ctx,
                                          double value)
{
  for (uint16_t i = 1; i < ctx->constant_pool_count; i++)
  {
    if (ctx->constant_pool[i].tag == JSC_CP_DOUBLE &&
        ctx->constant_pool[i].double_info.value == value)
    {
      return i;
    }
  }

  ctx->constant_pool = (jsc_constant_pool_entry*)realloc(
      ctx->constant_pool,
      (ctx->constant_pool_count + 2) * sizeof(jsc_constant_pool_entry));

  uint16_t index = ctx->constant_pool_count;
  jsc_constant_pool_entry* entry = &ctx->constant_pool[index];
  entry->tag = JSC_CP_DOUBLE;
  entry->double_info.value = value;

  ctx->constant_pool_count += 2;

  return index;
}

uint16_t jsc_bytecode_add_string_constant(jsc_bytecode_context* ctx,
                                          const char* str)
{
  uint16_t utf8_index = jsc_bytecode_add_utf8_constant(ctx, str);

  if (utf8_index == 0)
  {
    return 0;
  }

  for (uint16_t i = 1; i < ctx->constant_pool_count; i++)
  {
    if (ctx->constant_pool[i].tag == JSC_CP_STRING &&
        ctx->constant_pool[i].string_info.string_index == utf8_index)
    {
      return i;
    }
  }

  ctx->constant_pool = (jsc_constant_pool_entry*)realloc(
      ctx->constant_pool,
      (ctx->constant_pool_count + 1) * sizeof(jsc_constant_pool_entry));

  jsc_constant_pool_entry* entry =
      &ctx->constant_pool[ctx->constant_pool_count];
  entry->tag = JSC_CP_STRING;
  entry->string_info.string_index = utf8_index;

  return ctx->constant_pool_count++;
}

uint16_t jsc_bytecode_add_class_constant(jsc_bytecode_context* ctx,
                                         const char* class_name)
{
  uint16_t name_index = jsc_bytecode_add_utf8_constant(ctx, class_name);

  if (name_index == 0)
  {
    return 0;
  }

  for (uint16_t i = 1; i < ctx->constant_pool_count; i++)
  {
    if (ctx->constant_pool[i].tag == JSC_CP_CLASS &&
        ctx->constant_pool[i].class_info.name_index == name_index)
    {
      return i;
    }
  }

  ctx->constant_pool = (jsc_constant_pool_entry*)realloc(
      ctx->constant_pool,
      (ctx->constant_pool_count + 1) * sizeof(jsc_constant_pool_entry));

  jsc_constant_pool_entry* entry =
      &ctx->constant_pool[ctx->constant_pool_count];
  entry->tag = JSC_CP_CLASS;
  entry->class_info.name_index = name_index;

  return ctx->constant_pool_count++;
}

uint16_t jsc_bytecode_add_name_and_type_constant(jsc_bytecode_context* ctx,
                                                 const char* name,
                                                 const char* descriptor)
{
  uint16_t name_index = jsc_bytecode_add_utf8_constant(ctx, name);

  if (name_index == 0)
  {
    return 0;
  }

  uint16_t descriptor_index = jsc_bytecode_add_utf8_constant(ctx, descriptor);

  if (descriptor_index == 0)
  {
    return 0;
  }

  for (uint16_t i = 1; i < ctx->constant_pool_count; i++)
  {
    if (ctx->constant_pool[i].tag == JSC_CP_NAME_AND_TYPE &&
        ctx->constant_pool[i].name_and_type_info.name_index == name_index &&
        ctx->constant_pool[i].name_and_type_info.descriptor_index ==
            descriptor_index)
    {
      return i;
    }
  }

  ctx->constant_pool = (jsc_constant_pool_entry*)realloc(
      ctx->constant_pool,
      (ctx->constant_pool_count + 1) * sizeof(jsc_constant_pool_entry));

  jsc_constant_pool_entry* entry =
      &ctx->constant_pool[ctx->constant_pool_count];
  entry->tag = JSC_CP_NAME_AND_TYPE;
  entry->name_and_type_info.name_index = name_index;
  entry->name_and_type_info.descriptor_index = descriptor_index;

  return ctx->constant_pool_count++;
}

uint16_t jsc_bytecode_add_field_reference(jsc_bytecode_context* ctx,
                                          const char* class_name,
                                          const char* field_name,
                                          const char* field_descriptor)
{
  uint16_t class_index = jsc_bytecode_add_class_constant(ctx, class_name);
  if (class_index == 0)
  {
    return 0;
  }

  uint16_t name_and_type_index = jsc_bytecode_add_name_and_type_constant(
      ctx, field_name, field_descriptor);

  if (name_and_type_index == 0)
  {
    return 0;
  }

  for (uint16_t i = 1; i < ctx->constant_pool_count; i++)
  {
    if (ctx->constant_pool[i].tag == JSC_CP_FIELDREF &&
        ctx->constant_pool[i].fieldref_info.class_index == class_index &&
        ctx->constant_pool[i].fieldref_info.name_and_type_index ==
            name_and_type_index)
    {
      return i;
    }
  }

  ctx->constant_pool = (jsc_constant_pool_entry*)realloc(
      ctx->constant_pool,
      (ctx->constant_pool_count + 1) * sizeof(jsc_constant_pool_entry));

  jsc_constant_pool_entry* entry =
      &ctx->constant_pool[ctx->constant_pool_count];
  entry->tag = JSC_CP_FIELDREF;
  entry->fieldref_info.class_index = class_index;
  entry->fieldref_info.name_and_type_index = name_and_type_index;

  return ctx->constant_pool_count++;
}

uint16_t jsc_bytecode_add_method_reference(jsc_bytecode_context* ctx,
                                           const char* class_name,
                                           const char* method_name,
                                           const char* method_descriptor)
{
  uint16_t class_index = jsc_bytecode_add_class_constant(ctx, class_name);

  if (class_index == 0)
  {
    return 0;
  }

  uint16_t name_and_type_index = jsc_bytecode_add_name_and_type_constant(
      ctx, method_name, method_descriptor);

  if (name_and_type_index == 0)
  {
    return 0;
  }

  for (uint16_t i = 1; i < ctx->constant_pool_count; i++)
  {
    if (ctx->constant_pool[i].tag == JSC_CP_METHODREF &&
        ctx->constant_pool[i].methodref_info.class_index == class_index &&
        ctx->constant_pool[i].methodref_info.name_and_type_index ==
            name_and_type_index)
    {
      return i;
    }
  }

  ctx->constant_pool = (jsc_constant_pool_entry*)realloc(
      ctx->constant_pool,
      (ctx->constant_pool_count + 1) * sizeof(jsc_constant_pool_entry));

  jsc_constant_pool_entry* entry =
      &ctx->constant_pool[ctx->constant_pool_count];
  entry->tag = JSC_CP_METHODREF;
  entry->methodref_info.class_index = class_index;
  entry->methodref_info.name_and_type_index = name_and_type_index;

  return ctx->constant_pool_count++;
}

uint16_t jsc_bytecode_add_interface_method_reference(
    jsc_bytecode_context* ctx, const char* interface_name,
    const char* method_name, const char* method_descriptor)
{
  uint16_t class_index = jsc_bytecode_add_class_constant(ctx, interface_name);

  if (class_index == 0)
  {
    return 0;
  }

  uint16_t name_and_type_index = jsc_bytecode_add_name_and_type_constant(
      ctx, method_name, method_descriptor);

  if (name_and_type_index == 0)
  {
    return 0;
  }

  for (uint16_t i = 1; i < ctx->constant_pool_count; i++)
  {
    if (ctx->constant_pool[i].tag == JSC_CP_INTERFACE_METHODREF &&
        ctx->constant_pool[i].interface_methodref_info.class_index ==
            class_index &&
        ctx->constant_pool[i].interface_methodref_info.name_and_type_index ==
            name_and_type_index)
    {
      return i;
    }
  }

  ctx->constant_pool = (jsc_constant_pool_entry*)realloc(
      ctx->constant_pool,
      (ctx->constant_pool_count + 1) * sizeof(jsc_constant_pool_entry));

  jsc_constant_pool_entry* entry =
      &ctx->constant_pool[ctx->constant_pool_count];
  entry->tag = JSC_CP_INTERFACE_METHODREF;
  entry->interface_methodref_info.class_index = class_index;
  entry->interface_methodref_info.name_and_type_index = name_and_type_index;

  return ctx->constant_pool_count++;
}

uint16_t jsc_bytecode_add_interface(jsc_bytecode_context* ctx,
                                    const char* interface_name)
{
  uint16_t index = jsc_bytecode_add_class_constant(ctx, interface_name);

  if (index == 0)
  {
    return 0;
  }

  ctx->interfaces = (uint16_t*)realloc(
      ctx->interfaces, (ctx->interface_count + 1) * sizeof(uint16_t));

  ctx->interfaces[ctx->interface_count] = index;
  ctx->interface_count++;

  return index;
}

jsc_method* jsc_bytecode_add_method(jsc_bytecode_context* ctx, const char* name,
                                    const char* descriptor,
                                    uint16_t access_flags)
{
  uint16_t name_index = jsc_bytecode_add_utf8_constant(ctx, name);

  if (name_index == 0)
  {
    return NULL;
  }

  uint16_t descriptor_index = jsc_bytecode_add_utf8_constant(ctx, descriptor);

  if (descriptor_index == 0)
  {
    return NULL;
  }

  ctx->methods = (jsc_method*)realloc(ctx->methods, (ctx->method_count + 1) *
                                                        sizeof(jsc_method));

  jsc_method* method = &ctx->methods[ctx->method_count];

  method->access_flags = access_flags;
  method->name_index = name_index;
  method->descriptor_index = descriptor_index;
  method->attributes = NULL;
  method->attribute_count = 0;

  ctx->method_count++;

  return method;
}

jsc_field* jsc_bytecode_add_field(jsc_bytecode_context* ctx, const char* name,
                                  const char* descriptor, uint16_t access_flags)
{
  uint16_t name_index = jsc_bytecode_add_utf8_constant(ctx, name);

  if (name_index == 0)
  {
    return NULL;
  }

  uint16_t descriptor_index = jsc_bytecode_add_utf8_constant(ctx, descriptor);

  if (descriptor_index == 0)
  {
    return NULL;
  }

  ctx->fields = (jsc_field*)realloc(ctx->fields,
                                    (ctx->field_count + 1) * sizeof(jsc_field));

  jsc_field* field = &ctx->fields[ctx->field_count];

  field->access_flags = access_flags;
  field->name_index = name_index;
  field->descriptor_index = descriptor_index;
  field->attributes = NULL;
  field->attribute_count = 0;

  ctx->field_count++;

  return field;
}

jsc_attribute* jsc_bytecode_add_attribute(jsc_bytecode_context* ctx,
                                          const char* name, uint32_t length)
{
  uint16_t name_index = jsc_bytecode_add_utf8_constant(ctx, name);

  if (name_index == 0)
  {
    return NULL;
  }

  ctx->attributes = (jsc_attribute*)realloc(
      ctx->attributes, (ctx->attribute_count + 1) * sizeof(jsc_attribute));

  jsc_attribute* attribute = &ctx->attributes[ctx->attribute_count];

  attribute->name_index = name_index;
  attribute->length = length;
  attribute->info = (uint8_t*)malloc(length);

  ctx->attribute_count++;

  return attribute;
}

jsc_attribute* jsc_bytecode_add_method_attribute(jsc_bytecode_context* ctx,
                                                 jsc_method* method,
                                                 const char* name,
                                                 uint32_t length)
{
  uint16_t name_index = jsc_bytecode_add_utf8_constant(ctx, name);

  if (name_index == 0)
    return NULL;

  if (method->attributes == NULL)
  {
    method->attributes = malloc(sizeof(jsc_attribute));

    if (!method->attributes)
      return NULL;

    method->attribute_count = 1;
  }
  else
  {
    jsc_attribute* new_attrs =
        realloc(method->attributes,
                (method->attribute_count + 1) * sizeof(jsc_attribute));

    if (!new_attrs)
      return NULL;

    method->attributes = new_attrs;
    method->attribute_count++;
  }

  jsc_attribute* attr = &method->attributes[method->attribute_count - 1];

  attr->name_index = name_index;
  attr->length = length;
  attr->info = malloc(length);

  if (!attr->info)
    return NULL;

  return attr;
}

jsc_attribute* jsc_bytecode_add_field_attribute(jsc_bytecode_context* ctx,
                                                jsc_field* field,
                                                const char* name,
                                                uint32_t length)
{
  uint16_t name_index = jsc_bytecode_add_utf8_constant(ctx, name);

  if (name_index == 0)
  {
    return NULL;
  }

  field->attributes = (jsc_attribute*)realloc(
      field->attributes, (field->attribute_count + 1) * sizeof(jsc_attribute));

  jsc_attribute* attribute = &field->attributes[field->attribute_count];
  attribute->name_index = name_index;
  attribute->length = length;
  attribute->info = (uint8_t*)malloc(length);

  field->attribute_count++;

  return attribute;
}

void jsc_bytecode_add_code_attribute(jsc_bytecode_context* ctx,
                                     jsc_method* method, uint16_t max_stack,
                                     uint16_t max_locals, uint8_t* code,
                                     uint32_t code_length)
{
  uint32_t attr_length = 2 + 2 + 4 + code_length + 2 + 2;

  jsc_attribute* code_attr =
      jsc_bytecode_add_method_attribute(ctx, method, "Code", attr_length);
  if (!code_attr)
  {
    return;
  }

  uint8_t* p = code_attr->info;

  uint16_t max_stack_be = htobe16(max_stack);
  memcpy(p, &max_stack_be, 2);
  p += 2;

  uint16_t max_locals_be = htobe16(max_locals);
  memcpy(p, &max_locals_be, 2);
  p += 2;

  uint32_t code_length_be = htobe32(code_length);
  memcpy(p, &code_length_be, 4);
  p += 4;

  if (code_length > 0 && code != NULL)
  {
    memcpy(p, code, code_length);
    p += code_length;
  }

  uint16_t exception_table_length = 0;
  uint16_t exception_table_length_be = htobe16(exception_table_length);
  memcpy(p, &exception_table_length_be, 2);
  p += 2;

  uint16_t attributes_count = 0;
  uint16_t attributes_count_be = htobe16(attributes_count);
  memcpy(p, &attributes_count_be, 2);
}

void jsc_bytecode_add_exception_table_entry(jsc_bytecode_context* ctx,
                                            jsc_attribute* code_attribute,
                                            uint16_t start_pc, uint16_t end_pc,
                                            uint16_t handler_pc,
                                            uint16_t catch_type)
{
  uint8_t* p = code_attribute->info;

  p += 2;
  p += 2;

  uint32_t code_length;
  memcpy(&code_length, p, 4);
  code_length = be32toh(code_length);
  p += 4;

  p += code_length;

  uint16_t exception_table_length;
  memcpy(&exception_table_length, p, 2);
  exception_table_length = be16toh(exception_table_length);

  uint32_t new_info_length = code_attribute->length + 8;
  uint8_t* new_info = (uint8_t*)malloc(new_info_length);

  memcpy(new_info, code_attribute->info, p - code_attribute->info);

  uint8_t* q = new_info + (p - code_attribute->info);

  uint16_t new_exception_table_length = exception_table_length + 1;
  uint16_t new_exception_table_length_be = htobe16(new_exception_table_length);
  memcpy(q, &new_exception_table_length_be, 2);
  q += 2;

  memcpy(q, p + 2, exception_table_length * 8);
  q += exception_table_length * 8;

  uint16_t start_pc_be = htobe16(start_pc);
  memcpy(q, &start_pc_be, 2);
  q += 2;

  uint16_t end_pc_be = htobe16(end_pc);
  memcpy(q, &end_pc_be, 2);
  q += 2;

  uint16_t handler_pc_be = htobe16(handler_pc);
  memcpy(q, &handler_pc_be, 2);
  q += 2;

  uint16_t catch_type_be = htobe16(catch_type);
  memcpy(q, &catch_type_be, 2);
  q += 2;

  memcpy(q, p + 2 + exception_table_length * 8,
         code_attribute->length - (p - code_attribute->info) - 2 -
             exception_table_length * 8);

  free(code_attribute->info);
  code_attribute->info = new_info;
  code_attribute->length = new_info_length;
}

/**
 * stack map frame procedure:
 *  find the position of the attributes count in the Code attribute,
 *  skip max_stack, max_locals, exception table, read attributes count,
 *  check if stack map table already exists and update accordingly.
 *  Read number of entries for frame, create a new attribute with increased
 *  size, +1 for a single SAME frame, write the entry count, copy existing
 *  entries, add a new frame, update attribute length and replace attribute info.
 */

void jsc_bytecode_add_stackmap_frame(jsc_bytecode_context* ctx,
                                     jsc_attribute* code_attr,
                                     uint16_t byte_offset, uint8_t frame_type)
{
  uint8_t* p = code_attr->info + 2 + 2;
  uint32_t code_length;
  memcpy(&code_length, p, 4);
  code_length = be32toh(code_length);
  p += 4 + code_length;

  uint16_t exception_table_length;
  memcpy(&exception_table_length, p, 2);
  exception_table_length = be16toh(exception_table_length);
  p += 2 + exception_table_length * 8;

  uint16_t attributes_count;
  memcpy(&attributes_count, p, 2);
  attributes_count = be16toh(attributes_count);

  uint16_t stack_map_index = 0;
  uint8_t* attr_ptr = p + 2;

  for (uint16_t i = 0; i < attributes_count; i++)
  {
    uint16_t attr_name_index;
    memcpy(&attr_name_index, attr_ptr, 2);
    attr_name_index = be16toh(attr_name_index);

    if (attr_name_index > 0 &&
        ctx->constant_pool[attr_name_index].tag == JSC_CP_UTF8 &&
        ctx->constant_pool[attr_name_index].utf8_info.length == 13 &&
        memcmp(ctx->constant_pool[attr_name_index].utf8_info.bytes,
               "StackMapTable", 13) == 0)
    {
      attr_ptr += 2;
      uint32_t attr_length;
      memcpy(&attr_length, attr_ptr, 4);
      attr_length = be32toh(attr_length);

      attr_ptr += 4;
      uint16_t entry_count;
      memcpy(&entry_count, attr_ptr, 2);
      entry_count = be16toh(entry_count);
      entry_count++;

      uint32_t new_attr_length = attr_length + 1;
      uint8_t* new_attr_info = malloc(new_attr_length);

      uint16_t new_entry_count_be = htobe16(entry_count);
      memcpy(new_attr_info, &new_entry_count_be, 2);

      memcpy(new_attr_info + 2, attr_ptr + 2, attr_length - 2);

      new_attr_info[attr_length] = frame_type;

      attr_ptr -= 4;
      uint32_t new_attr_length_be = htobe32(new_attr_length);
      memcpy(attr_ptr, &new_attr_length_be, 4);

      attr_ptr += 4;
      memcpy(attr_ptr, new_attr_info, new_attr_length);

      free(new_attr_info);
      return;
    }

    attr_ptr += 2;
    uint32_t attr_length;
    memcpy(&attr_length, attr_ptr, 4);
    attr_length = be32toh(attr_length);
    attr_ptr += 4 + attr_length;
  }

  uint16_t stackmap_name_index = /* if none is found, define one */
      jsc_bytecode_add_utf8_constant(ctx, "StackMapTable");

  uint32_t
      stackmap_attr_length = /* SAME frame (type 0) for no change in locals/stack */
      2 + 1;                 /* 2 bytes for num_entries + 1 byte for frame */
  uint8_t* stackmap_info = malloc(stackmap_attr_length);

  uint16_t num_entries = htobe16(1);
  memcpy(stackmap_info, &num_entries, 2);

  stackmap_info[2] = frame_type;

  uint32_t extra_length = 2 + 4 + stackmap_attr_length;
  uint32_t new_code_attr_length = code_attr->length + extra_length;
  uint8_t* new_code_attr_info = malloc(new_code_attr_length);

  uint32_t copy_length = p - code_attr->info;
  memcpy(new_code_attr_info, code_attr->info, copy_length);

  attributes_count++;
  uint16_t attributes_count_be = htobe16(attributes_count);
  memcpy(new_code_attr_info + copy_length, &attributes_count_be, 2);

  memcpy(new_code_attr_info + copy_length + 2, p + 2,
         code_attr->length - copy_length - 2);

  uint8_t* attr_pos = new_code_attr_info + code_attr->length;
  uint16_t name_index_be = htobe16(stackmap_name_index);
  memcpy(attr_pos, &name_index_be, 2);
  attr_pos += 2;

  uint32_t length_be = htobe32(stackmap_attr_length);
  memcpy(attr_pos, &length_be, 4);
  attr_pos += 4;

  memcpy(attr_pos, stackmap_info, stackmap_attr_length);

  free(code_attr->info);
  code_attr->info = new_code_attr_info;
  code_attr->length = new_code_attr_length;

  free(stackmap_info);
}

uint32_t jsc_bytecode_write(jsc_bytecode_context* ctx, uint8_t** out_buffer)
{
  uint32_t total_size = 0;

  total_size += 4;
  total_size += 2;
  total_size += 2;
  total_size += 2;

  for (uint16_t i = 1; i < ctx->constant_pool_count; i++)
  {
    total_size += 1;

    switch (ctx->constant_pool[i].tag)
    {
    case JSC_CP_UTF8:
      total_size += 2;
      total_size += ctx->constant_pool[i].utf8_info.length;
      break;
    case JSC_CP_INTEGER:
    case JSC_CP_FLOAT:
      total_size += 4;
      break;
    case JSC_CP_LONG:
    case JSC_CP_DOUBLE:
      total_size += 8;
      i++;
      break;
    case JSC_CP_CLASS:
    case JSC_CP_STRING:
      total_size += 2;
      break;
    case JSC_CP_FIELDREF:
    case JSC_CP_METHODREF:
    case JSC_CP_INTERFACE_METHODREF:
    case JSC_CP_NAME_AND_TYPE:
      total_size += 4;
      break;
    case JSC_CP_METHOD_HANDLE:
      total_size += 3;
      break;
    case JSC_CP_METHOD_TYPE:
      total_size += 2;
      break;
    case JSC_CP_INVOKE_DYNAMIC:
      total_size += 4;
      break;
    }
  }

  total_size += 2;
  total_size += 2;
  total_size += 2;

  total_size += 2;
  total_size += 2 * ctx->interface_count;

  total_size += 2;

  for (uint16_t i = 0; i < ctx->field_count; i++)
  {
    total_size += 2;
    total_size += 2;
    total_size += 2;
    total_size += 2;

    for (uint16_t j = 0; j < ctx->fields[i].attribute_count; j++)
    {
      total_size += 2;
      total_size += 4;
      total_size += ctx->fields[i].attributes[j].length;
    }
  }

  total_size += 2;

  for (uint16_t i = 0; i < ctx->method_count; i++)
  {
    total_size += 2;
    total_size += 2;
    total_size += 2;
    total_size += 2;

    for (uint16_t j = 0; j < ctx->methods[i].attribute_count; j++)
    {
      total_size += 2;
      total_size += 4;
      total_size += ctx->methods[i].attributes[j].length;
    }
  }

  total_size += 2;

  for (uint16_t i = 0; i < ctx->attribute_count; i++)
  {
    total_size += 2;
    total_size += 4;
    total_size += ctx->attributes[i].length;
  }

  uint8_t* buffer = (uint8_t*)malloc(total_size);

  if (!buffer)
  {
    return 0;
  }

  uint8_t* p = buffer;

  uint32_t magic = 0xCAFEBABE;
  jsc_write_u32(&p, magic);

  jsc_write_u16(&p, ctx->minor_version);
  jsc_write_u16(&p, ctx->major_version);

  jsc_write_u16(&p, ctx->constant_pool_count);

  for (uint16_t i = 1; i < ctx->constant_pool_count; i++)
  {
    jsc_write_u8(&p, ctx->constant_pool[i].tag);

    switch (ctx->constant_pool[i].tag)
    {
    case JSC_CP_UTF8:
      jsc_write_u16(&p, ctx->constant_pool[i].utf8_info.length);
      jsc_write_bytes(&p, ctx->constant_pool[i].utf8_info.bytes,
                      ctx->constant_pool[i].utf8_info.length);
      break;
    case JSC_CP_INTEGER:
      jsc_write_u32(&p, (uint32_t)ctx->constant_pool[i].integer_info.value);
      break;
    case JSC_CP_FLOAT:
    {
      uint32_t float_bits;
      memcpy(&float_bits, &ctx->constant_pool[i].float_info.value, 4);
      jsc_write_u32(&p, float_bits);
      break;
    }
    case JSC_CP_LONG:
    {
      uint64_t long_value = (uint64_t)ctx->constant_pool[i].long_info.value;
      jsc_write_u32(&p, (uint32_t)(long_value >> 32));
      jsc_write_u32(&p, (uint32_t)long_value);
      i++;
      break;
    }
    case JSC_CP_DOUBLE:
    {
      uint64_t double_bits;
      memcpy(&double_bits, &ctx->constant_pool[i].double_info.value, 8);
      jsc_write_u32(&p, (uint32_t)(double_bits >> 32));
      jsc_write_u32(&p, (uint32_t)double_bits);
      i++;
      break;
    }
    case JSC_CP_CLASS:
      jsc_write_u16(&p, ctx->constant_pool[i].class_info.name_index);
      break;
    case JSC_CP_STRING:
      jsc_write_u16(&p, ctx->constant_pool[i].string_info.string_index);
      break;
    case JSC_CP_FIELDREF:
      jsc_write_u16(&p, ctx->constant_pool[i].fieldref_info.class_index);
      jsc_write_u16(&p,
                    ctx->constant_pool[i].fieldref_info.name_and_type_index);
      break;
    case JSC_CP_METHODREF:
      jsc_write_u16(&p, ctx->constant_pool[i].methodref_info.class_index);
      jsc_write_u16(&p,
                    ctx->constant_pool[i].methodref_info.name_and_type_index);
      break;
    case JSC_CP_INTERFACE_METHODREF:
      jsc_write_u16(&p,
                    ctx->constant_pool[i].interface_methodref_info.class_index);
      jsc_write_u16(
          &p,
          ctx->constant_pool[i].interface_methodref_info.name_and_type_index);
      break;
    case JSC_CP_NAME_AND_TYPE:
      jsc_write_u16(&p, ctx->constant_pool[i].name_and_type_info.name_index);
      jsc_write_u16(&p,
                    ctx->constant_pool[i].name_and_type_info.descriptor_index);
      break;
    case JSC_CP_METHOD_HANDLE:
      jsc_write_u8(&p, ctx->constant_pool[i].method_handle_info.reference_kind);
      jsc_write_u16(&p,
                    ctx->constant_pool[i].method_handle_info.reference_index);
      break;
    case JSC_CP_METHOD_TYPE:
      jsc_write_u16(&p,
                    ctx->constant_pool[i].method_type_info.descriptor_index);
      break;
    case JSC_CP_INVOKE_DYNAMIC:
      jsc_write_u16(&p, ctx->constant_pool[i]
                            .invoke_dynamic_info.bootstrap_method_attr_index);
      jsc_write_u16(
          &p, ctx->constant_pool[i].invoke_dynamic_info.name_and_type_index);
      break;
    }
  }

  jsc_write_u16(&p, ctx->access_flags);
  jsc_write_u16(&p, ctx->this_class);
  jsc_write_u16(&p, ctx->super_class);

  jsc_write_u16(&p, ctx->interface_count);

  for (uint16_t i = 0; i < ctx->interface_count; i++)
  {
    jsc_write_u16(&p, ctx->interfaces[i]);
  }

  jsc_write_u16(&p, ctx->field_count);

  for (uint16_t i = 0; i < ctx->field_count; i++)
  {
    jsc_write_u16(&p, ctx->fields[i].access_flags);
    jsc_write_u16(&p, ctx->fields[i].name_index);
    jsc_write_u16(&p, ctx->fields[i].descriptor_index);
    jsc_write_u16(&p, ctx->fields[i].attribute_count);

    for (uint16_t j = 0; j < ctx->fields[i].attribute_count; j++)
    {
      jsc_write_u16(&p, ctx->fields[i].attributes[j].name_index);
      jsc_write_u32(&p, ctx->fields[i].attributes[j].length);
      jsc_write_bytes(&p, ctx->fields[i].attributes[j].info,
                      ctx->fields[i].attributes[j].length);
    }
  }

  jsc_write_u16(&p, ctx->method_count);

  for (uint16_t i = 0; i < ctx->method_count; i++)
  {
    jsc_write_u16(&p, ctx->methods[i].access_flags);
    jsc_write_u16(&p, ctx->methods[i].name_index);
    jsc_write_u16(&p, ctx->methods[i].descriptor_index);
    jsc_write_u16(&p, ctx->methods[i].attribute_count);

    for (uint16_t j = 0; j < ctx->methods[i].attribute_count; j++)
    {
      jsc_write_u16(&p, ctx->methods[i].attributes[j].name_index);
      jsc_write_u32(&p, ctx->methods[i].attributes[j].length);
      jsc_write_bytes(&p, ctx->methods[i].attributes[j].info,
                      ctx->methods[i].attributes[j].length);
    }
  }

  jsc_write_u16(&p, ctx->attribute_count);

  for (uint16_t i = 0; i < ctx->attribute_count; i++)
  {
    jsc_write_u16(&p, ctx->attributes[i].name_index);
    jsc_write_u32(&p, ctx->attributes[i].length);
    jsc_write_bytes(&p, ctx->attributes[i].info, ctx->attributes[i].length);
  }

  *out_buffer = buffer;

  return total_size;
}

bool jsc_bytecode_write_to_file(jsc_bytecode_context* ctx, const char* filename)
{
  uint8_t* buffer;
  uint32_t size = jsc_bytecode_write(ctx, &buffer);

  if (size == 0)
  {
    return false;
  }

  FILE* file = fopen(filename, "wb");

  if (!file)
  {
    free(buffer);
    return false;
  }

  size_t written = fwrite(buffer, 1, size, file);
  fclose(file);

  free(buffer);

  return written == size;
}

jsc_bytecode_context* jsc_bytecode_create_class(const char* class_name,
                                                const char* super_class_name,
                                                uint16_t access_flags)
{
  jsc_bytecode_context* ctx = jsc_bytecode_init();

  if (!ctx)
  {
    return NULL;
  }

  jsc_bytecode_set_class_name(ctx, class_name);
  jsc_bytecode_set_super_class(ctx, super_class_name ? super_class_name
                                                     : "java/lang/Object");
  jsc_bytecode_set_access_flags(ctx, access_flags);

  uint16_t class_name_idx = jsc_bytecode_add_utf8_constant(ctx, class_name);
  ctx->this_class = jsc_bytecode_add_class_constant(ctx, class_name);

  uint16_t super_name_idx = jsc_bytecode_add_utf8_constant(
      ctx, super_class_name ? super_class_name : "java/lang/Object");
  ctx->super_class = jsc_bytecode_add_class_constant(
      ctx, super_class_name ? super_class_name : "java/lang/Object");

  return ctx;
}

jsc_method* jsc_bytecode_create_method(jsc_bytecode_context* ctx,
                                       const char* name, const char* descriptor,
                                       uint16_t access_flags,
                                       uint16_t max_stack, uint16_t max_locals)
{
  jsc_method* method =
      jsc_bytecode_add_method(ctx, name, descriptor, access_flags);

  if (!method)
    return NULL;

  uint32_t code_length = 0;

  uint32_t attr_length = 2 + 2 + 4 + code_length + 2 + 2;

  uint16_t code_index = jsc_bytecode_add_utf8_constant(ctx, "Code");
  method->attributes = malloc(sizeof(jsc_attribute));

  if (!method->attributes)
    return NULL;

  method->attribute_count = 1;
  method->attributes[0].name_index = code_index;
  method->attributes[0].length = attr_length;
  method->attributes[0].info = malloc(attr_length);

  if (!method->attributes[0].info)
  {
    free(method->attributes);
    method->attributes = NULL;
    return NULL;
  }

  uint8_t* p = method->attributes[0].info;

  uint16_t max_stack_be = htobe16(max_stack);
  max_stack = max_stack < 2 ? 2 : max_stack;
  memcpy(p, &max_stack_be, 2);
  p += 2;

  uint16_t max_locals_be = htobe16(max_locals);
  memcpy(p, &max_locals_be, 2);
  p += 2;

  uint32_t code_length_be = htobe32(code_length);
  memcpy(p, &code_length_be, 4);
  p += 4;

  uint16_t exception_table_length = 0;
  uint16_t exception_table_length_be = htobe16(exception_table_length);
  memcpy(p, &exception_table_length_be, 2);
  p += 2;

  uint16_t attributes_count = 0;
  uint16_t attributes_count_be = htobe16(attributes_count);
  memcpy(p, &attributes_count_be, 2);

  return method;
}

/**
 * @brief emit a JVM opcode to a jsc_method
 * 
 * @details Allocate new buffer for the code attribute, copy header (max_stack, max_locals),
 *          update code length, copy existing code, add new opcode, copy exception
 *          table & attributes from after the code, replace attribute info.
 */
void jsc_bytecode_emit(jsc_bytecode_context* ctx, jsc_method* method,
                       uint8_t opcode)
{
  if (!method || method->attribute_count == 0)
    return;

  jsc_attribute* code_attr = NULL;

  for (uint16_t i = 0; i < method->attribute_count; i++)
  {
    const jsc_constant_pool_entry* entry =
        &ctx->constant_pool[method->attributes[i].name_index];
    if (entry->tag == JSC_CP_UTF8 && entry->utf8_info.length == 4 &&
        memcmp(entry->utf8_info.bytes, "Code", 4) == 0)
    {
      code_attr = &method->attributes[i];
      break;
    }
  }

  if (!code_attr)
    return;

  uint8_t* info = code_attr->info;
  uint32_t code_length;
  memcpy(&code_length, info + 4, 4); /* skip max_stack(2) + max_locals(2) */
  code_length = be32toh(code_length);

  uint32_t new_code_length = code_length + 1;
  uint32_t new_attribute_length = code_attr->length + 1;

  uint8_t* new_info = malloc(new_attribute_length);
  if (!new_info)
    return;

  memcpy(new_info, info, 4);

  uint32_t new_code_length_be = htobe32(new_code_length);
  memcpy(new_info + 4, &new_code_length_be, 4);

  memcpy(new_info + 8, info + 8, code_length);

  new_info[8 + code_length] = opcode;

  size_t tail_size = code_attr->length - (8 + code_length);
  if (tail_size > 0)
  {
    memcpy(new_info + 8 + new_code_length, info + 8 + code_length, tail_size);
  }

  free(code_attr->info);
  code_attr->info = new_info;
  code_attr->length = new_attribute_length;
}

void jsc_bytecode_emit_u8(jsc_bytecode_context* ctx, jsc_method* method,
                          uint8_t opcode, uint8_t operand)
{
  if (!method || method->attribute_count == 0)
  {
    return;
  }

  jsc_attribute* code_attr = NULL;
  for (uint16_t i = 0; i < method->attribute_count; i++)
  {
    uint16_t name_index = method->attributes[i].name_index;
    if (name_index > 0 && ctx->constant_pool[name_index].tag == JSC_CP_UTF8 &&
        ctx->constant_pool[name_index].utf8_info.length == 4 &&
        memcmp(ctx->constant_pool[name_index].utf8_info.bytes, "Code", 4) == 0)
    {
      code_attr = &method->attributes[i];
      break;
    }
  }

  if (!code_attr)
  {
    return;
  }

  uint8_t* p = code_attr->info + 2 + 2;
  uint32_t code_length;
  memcpy(&code_length, p, 4);
  code_length = be32toh(code_length);
  p += 4;

  uint32_t new_length = code_length + 2;
  uint32_t new_attr_length = code_attr->length + 2;

  uint8_t* new_info = malloc(new_attr_length);
  if (!new_info)
    return;

  memcpy(new_info, code_attr->info, 4);

  uint32_t new_code_length_be = htobe32(new_length);
  memcpy(new_info + 4, &new_code_length_be, 4);

  memcpy(new_info + 8, p, code_length);

  new_info[8 + code_length] = opcode;
  new_info[8 + code_length + 1] = operand;

  memcpy(new_info + 8 + new_length, p + code_length,
         code_attr->length - (8 + code_length));

  free(code_attr->info);
  code_attr->info = new_info;
  code_attr->length = new_attr_length;
}

void jsc_bytecode_emit_u16(jsc_bytecode_context* ctx, jsc_method* method,
                           uint8_t opcode, uint16_t operand)
{
  if (!method || method->attribute_count == 0)
  {
    return;
  }

  jsc_attribute* code_attr = NULL;
  for (uint16_t i = 0; i < method->attribute_count; i++)
  {
    uint16_t name_index = method->attributes[i].name_index;
    if (name_index > 0 && ctx->constant_pool[name_index].tag == JSC_CP_UTF8 &&
        ctx->constant_pool[name_index].utf8_info.length == 4 &&
        memcmp(ctx->constant_pool[name_index].utf8_info.bytes, "Code", 4) == 0)
    {
      code_attr = &method->attributes[i];
      break;
    }
  }

  if (!code_attr)
  {
    return;
  }

  uint8_t* p = code_attr->info + 2 + 2;
  uint32_t code_length;
  memcpy(&code_length, p, 4);
  code_length = be32toh(code_length);
  p += 4;

  uint32_t new_length = code_length + 3;
  uint32_t new_attr_length = code_attr->length + 3;

  uint8_t* new_info = malloc(new_attr_length);
  if (!new_info)
    return;

  memcpy(new_info, code_attr->info, 4);

  uint32_t new_code_length_be = htobe32(new_length);
  memcpy(new_info + 4, &new_code_length_be, 4);

  memcpy(new_info + 8, p, code_length);

  new_info[8 + code_length] = opcode;

  uint16_t operand_be = htobe16(operand);
  memcpy(new_info + 8 + code_length + 1, &operand_be, 2);

  memcpy(new_info + 8 + new_length, p + code_length,
         code_attr->length - (8 + code_length));

  free(code_attr->info);
  code_attr->info = new_info;
  code_attr->length = new_attr_length;
}

void jsc_bytecode_emit_jump(jsc_bytecode_context* ctx, jsc_method* method,
                            uint8_t opcode, int16_t offset)
{
  jsc_bytecode_emit_u16(ctx, method, opcode, (uint16_t)offset);
}

void jsc_bytecode_emit_local_var(jsc_bytecode_context* ctx, jsc_method* method,
                                 uint8_t opcode, uint16_t index)
{
  if (index <= 3 && (opcode == JSC_JVM_ILOAD || opcode == JSC_JVM_LLOAD ||
                     opcode == JSC_JVM_FLOAD || opcode == JSC_JVM_DLOAD ||
                     opcode == JSC_JVM_ALOAD))
  {
    uint8_t short_opcode =
        (opcode - JSC_JVM_ILOAD) * 4 + JSC_JVM_ILOAD_0 + index;
    jsc_bytecode_emit(ctx, method, short_opcode);
  }
  else if (index <= 3 &&
           (opcode == JSC_JVM_ISTORE || opcode == JSC_JVM_LSTORE ||
            opcode == JSC_JVM_FSTORE || opcode == JSC_JVM_DSTORE ||
            opcode == JSC_JVM_ASTORE))
  {
    uint8_t short_opcode =
        (opcode - JSC_JVM_ISTORE) * 4 + JSC_JVM_ISTORE_0 + index;
    jsc_bytecode_emit(ctx, method, short_opcode);
  }
  else if (index < 256)
  {
    jsc_bytecode_emit_u8(ctx, method, opcode, (uint8_t)index);
  }
  else
  {
    jsc_bytecode_emit(ctx, method, JSC_JVM_WIDE);
    jsc_bytecode_emit_u16(ctx, method, opcode, index);
  }
}

void jsc_bytecode_emit_const_load(jsc_bytecode_context* ctx, jsc_method* method,
                                  uint16_t index)
{
  if (index < 256)
  {
    jsc_bytecode_emit_u8(ctx, method, JSC_JVM_LDC, (uint8_t)index);
  }
  else
  {
    jsc_bytecode_emit_u16(ctx, method, JSC_JVM_LDC_W, index);
  }
}

void jsc_bytecode_emit_invoke_virtual(jsc_bytecode_context* ctx,
                                      jsc_method* method,
                                      const char* class_name,
                                      const char* method_name,
                                      const char* descriptor)
{
  uint16_t method_ref = jsc_bytecode_add_method_reference(
      ctx, class_name, method_name, descriptor);
  jsc_bytecode_emit_u16(ctx, method, JSC_JVM_INVOKEVIRTUAL, method_ref);
}

void jsc_bytecode_emit_invoke_special(jsc_bytecode_context* ctx,
                                      jsc_method* method,
                                      const char* class_name,
                                      const char* method_name,
                                      const char* descriptor)
{
  uint16_t method_ref = jsc_bytecode_add_method_reference(
      ctx, class_name, method_name, descriptor);
  jsc_bytecode_emit_u16(ctx, method, JSC_JVM_INVOKESPECIAL, method_ref);
}

void jsc_bytecode_emit_invoke_static(jsc_bytecode_context* ctx,
                                     jsc_method* method, const char* class_name,
                                     const char* method_name,
                                     const char* descriptor)
{
  uint16_t method_ref = jsc_bytecode_add_method_reference(
      ctx, class_name, method_name, descriptor);
  jsc_bytecode_emit_u16(ctx, method, JSC_JVM_INVOKESTATIC, method_ref);
}

void jsc_bytecode_emit_invoke_interface(jsc_bytecode_context* ctx,
                                        jsc_method* method,
                                        const char* interface_name,
                                        const char* method_name,
                                        const char* descriptor, uint8_t count)
{
  uint16_t method_ref = jsc_bytecode_add_interface_method_reference(
      ctx, interface_name, method_name, descriptor);

  for (uint16_t i = 0; i < method->attribute_count; i++)
  {
    jsc_attribute* attr = &method->attributes[i];
    uint16_t name_index = attr->name_index;

    const jsc_constant_pool_entry* name_entry = &ctx->constant_pool[name_index];
    if (name_entry->tag == JSC_CP_UTF8 && name_entry->utf8_info.length == 4 &&
        memcmp(name_entry->utf8_info.bytes, "Code", 4) == 0)
    {

      uint8_t* p = attr->info + 2 + 2;
      uint32_t code_length;
      memcpy(&code_length, p, 4);
      code_length = be32toh(code_length);

      p += 4;

      p[code_length] = JSC_JVM_INVOKEINTERFACE;
      uint16_t method_ref_be = htobe16(method_ref);
      memcpy(&p[code_length + 1], &method_ref_be, 2);
      p[code_length + 3] = count;
      p[code_length + 4] = 0;

      code_length += 5;

      uint32_t code_length_be = htobe32(code_length);
      memcpy(attr->info + 4, &code_length_be, 4);

      break;
    }
  }
}

void jsc_bytecode_emit_field_access(jsc_bytecode_context* ctx,
                                    jsc_method* method, uint8_t opcode,
                                    const char* class_name,
                                    const char* field_name,
                                    const char* descriptor)
{
  uint16_t field_ref =
      jsc_bytecode_add_field_reference(ctx, class_name, field_name, descriptor);
  jsc_bytecode_emit_u16(ctx, method, opcode, field_ref);
}

void jsc_bytecode_emit_new(jsc_bytecode_context* ctx, jsc_method* method,
                           const char* class_name)
{
  uint16_t class_index = jsc_bytecode_add_class_constant(ctx, class_name);
  jsc_bytecode_emit_u16(ctx, method, JSC_JVM_NEW, class_index);
}

void jsc_bytecode_emit_newarray(jsc_bytecode_context* ctx, jsc_method* method,
                                uint8_t array_type)
{
  jsc_bytecode_emit_u8(ctx, method, JSC_JVM_NEWARRAY, array_type);
}

void jsc_bytecode_emit_anewarray(jsc_bytecode_context* ctx, jsc_method* method,
                                 const char* component_type)
{
  uint16_t component_index =
      jsc_bytecode_add_class_constant(ctx, component_type);
  jsc_bytecode_emit_u16(ctx, method, JSC_JVM_ANEWARRAY, component_index);
}

void jsc_bytecode_emit_return(jsc_bytecode_context* ctx, jsc_method* method,
                              char type)
{
  uint8_t opcode;

  switch (type)
  {
  case 'I':
    opcode = JSC_JVM_IRETURN;
    break;
  case 'L':
    opcode = JSC_JVM_LRETURN;
    break;
  case 'F':
    opcode = JSC_JVM_FRETURN;
    break;
  case 'D':
    opcode = JSC_JVM_DRETURN;
    break;
  case 'A':
    opcode = JSC_JVM_ARETURN;
    break;
  case 'V':
    opcode = JSC_JVM_RETURN;
    break;
  default:
    opcode = JSC_JVM_RETURN;
  }

  jsc_bytecode_emit(ctx, method, opcode);
}

uint32_t jsc_bytecode_get_method_code_length(jsc_method* method)
{
  for (uint16_t i = 0; i < method->attribute_count; i++)
  {
    jsc_attribute* attr = &method->attributes[i];

    uint8_t* p = attr->info + 2 + 2;
    uint32_t code_length;
    memcpy(&code_length, p, 4);
    code_length = be32toh(code_length);

    return code_length;
  }

  return 0;
}

uint8_t* jsc_bytecode_get_method_code(jsc_method* method)
{
  for (uint16_t i = 0; i < method->attribute_count; i++)
  {
    jsc_attribute* attr = &method->attributes[i];

    uint8_t* p = attr->info + 2 + 2 + 4;

    return p;
  }

  return NULL;
}

uint32_t jsc_bytecode_get_method_code_offset(jsc_method* method)
{
  for (uint16_t i = 0; i < method->attribute_count; i++)
  {
    jsc_attribute* attr = &method->attributes[i];

    return 2 + 2 + 4;
  }

  return 0;
}

void jsc_bytecode_emit_constructor(jsc_bytecode_context* ctx,
                                   jsc_method* method, const char* super_class)
{
  jsc_bytecode_emit(ctx, method, JSC_JVM_ALOAD_0);
  jsc_bytecode_emit_invoke_special(ctx, method, super_class, "<init>", "()V");
  jsc_bytecode_emit(ctx, method, JSC_JVM_RETURN);
}

void jsc_bytecode_emit_line_number(jsc_bytecode_context* ctx,
                                   jsc_method* method, uint16_t line_number,
                                   uint16_t start_pc)
{
  for (uint16_t i = 0; i < method->attribute_count; i++)
  {
    jsc_attribute* attr = &method->attributes[i];
    uint16_t name_index = attr->name_index;

    const jsc_constant_pool_entry* name_entry = &ctx->constant_pool[name_index];

    if (name_entry->tag == JSC_CP_UTF8 && name_entry->utf8_info.length == 4 &&
        memcmp(name_entry->utf8_info.bytes, "Code", 4) == 0)
    {

      uint8_t* p = attr->info + 2 + 2;
      uint32_t code_length;
      memcpy(&code_length, p, 4);
      code_length = be32toh(code_length);

      p += 4 + code_length;

      uint16_t exception_table_length;
      memcpy(&exception_table_length, p, 2);
      exception_table_length = be16toh(exception_table_length);

      p += 2 + exception_table_length * 8;

      uint16_t attributes_count;
      memcpy(&attributes_count, p, 2);
      attributes_count = be16toh(attributes_count);

      p += 2;

      uint16_t line_number_table_index = 0;

      for (uint16_t j = 0; j < attributes_count; j++)
      {
        uint16_t attr_name_index;
        memcpy(&attr_name_index, p, 2);
        attr_name_index = be16toh(attr_name_index);

        const jsc_constant_pool_entry* attr_name_entry =
            &ctx->constant_pool[attr_name_index];

        if (attr_name_entry->tag == JSC_CP_UTF8 &&
            attr_name_entry->utf8_info.length == 15 &&
            memcmp(attr_name_entry->utf8_info.bytes, "LineNumberTable", 15) ==
                0)
        {

          line_number_table_index = j;
          break;
        }

        p += 2;
        uint32_t attr_length;
        memcpy(&attr_length, p, 4);
        attr_length = be32toh(attr_length);

        p += 4 + attr_length;
      }

      if (line_number_table_index == 0)
      {
        uint16_t name_index =
            jsc_bytecode_add_utf8_constant(ctx, "LineNumberTable");
        uint32_t attr_length = 2 + 4;
        attr->length += attr_length;
        attr->info = (uint8_t*)realloc(attr->info, attr->length);

        uint16_t attributes_count_be = htobe16(attributes_count + 1);
        memcpy(p - 2, &attributes_count_be, 2);

        uint16_t name_index_be = htobe16(name_index);
        memcpy(p, &name_index_be, 2);
        p += 2;

        uint32_t line_table_length = 2;
        uint32_t line_table_length_be = htobe32(line_table_length);
        memcpy(p, &line_table_length_be, 4);
        p += 4;

        uint16_t line_count = 1;
        uint16_t line_count_be = htobe16(line_count);
        memcpy(p, &line_count_be, 2);
        p += 2;

        uint16_t start_pc_be = htobe16(start_pc);
        memcpy(p, &start_pc_be, 2);
        p += 2;

        uint16_t line_number_be = htobe16(line_number);
        memcpy(p, &line_number_be, 2);
      }
      else
      {
        /*??*/
      }

      break;
    }
  }
}

void jsc_bytecode_emit_local_variable(jsc_bytecode_context* ctx,
                                      jsc_method* method, const char* name,
                                      const char* descriptor, uint16_t start_pc,
                                      uint16_t length, uint16_t index)
{
  uint16_t name_index = jsc_bytecode_add_utf8_constant(ctx, name);
  uint16_t descriptor_index = jsc_bytecode_add_utf8_constant(ctx, descriptor);

  for (uint16_t i = 0; i < method->attribute_count; i++)
  {
    jsc_attribute* attr = &method->attributes[i];
    uint16_t attr_name_index = attr->name_index;

    const jsc_constant_pool_entry* name_entry =
        &ctx->constant_pool[attr_name_index];
    if (name_entry->tag == JSC_CP_UTF8 && name_entry->utf8_info.length == 4 &&
        memcmp(name_entry->utf8_info.bytes, "Code", 4) == 0)
    {

      uint8_t* p = attr->info + 2 + 2;
      uint32_t code_length;
      memcpy(&code_length, p, 4);
      code_length = be32toh(code_length);

      p += 4 + code_length;

      uint16_t exception_table_length;
      memcpy(&exception_table_length, p, 2);
      exception_table_length = be16toh(exception_table_length);

      p += 2 + exception_table_length * 8;

      uint16_t attributes_count;
      memcpy(&attributes_count, p, 2);
      attributes_count = be16toh(attributes_count);

      p += 2;

      uint16_t local_variable_table_index = 0;
      bool found = false;

      for (uint16_t j = 0; j < attributes_count; j++)
      {
        uint16_t attr_name_index;
        memcpy(&attr_name_index, p, 2);
        attr_name_index = be16toh(attr_name_index);

        const jsc_constant_pool_entry* attr_name_entry =
            &ctx->constant_pool[attr_name_index];
        if (attr_name_entry->tag == JSC_CP_UTF8 &&
            attr_name_entry->utf8_info.length == 18 &&
            memcmp(attr_name_entry->utf8_info.bytes, "LocalVariableTable",
                   18) == 0)
        {

          found = true;
          local_variable_table_index = j;

          p += 2;
          uint32_t attr_length;
          memcpy(&attr_length, p, 4);
          attr_length = be32toh(attr_length);

          p += 4;

          uint16_t local_var_count;
          memcpy(&local_var_count, p, 2);
          local_var_count = be16toh(local_var_count);
          local_var_count++;

          uint32_t new_attr_length = attr_length + 10;
          uint32_t new_attr_length_be = htobe32(new_attr_length);
          memcpy(p - 4, &new_attr_length_be, 4);

          uint16_t local_var_count_be = htobe16(local_var_count);
          memcpy(p, &local_var_count_be, 2);

          p += 2 + local_var_count * 10;

          uint16_t start_pc_be = htobe16(start_pc);
          memcpy(p, &start_pc_be, 2);
          p += 2;

          uint16_t length_be = htobe16(length);
          memcpy(p, &length_be, 2);
          p += 2;

          uint16_t name_index_be = htobe16(name_index);
          memcpy(p, &name_index_be, 2);
          p += 2;

          uint16_t descriptor_index_be = htobe16(descriptor_index);
          memcpy(p, &descriptor_index_be, 2);
          p += 2;

          uint16_t index_be = htobe16(index);
          memcpy(p, &index_be, 2);

          break;
        }

        p += 2;
        uint32_t attr_length;
        memcpy(&attr_length, p, 4);
        attr_length = be32toh(attr_length);

        p += 4 + attr_length;
      }

      if (!found)
      {
        uint16_t table_name_index =
            jsc_bytecode_add_utf8_constant(ctx, "LocalVariableTable");
        uint32_t attr_length = 2 + 10;

        p = attr->info + 2 + 2 + 4 + code_length + 2 +
            exception_table_length * 8;

        uint16_t attributes_count;
        memcpy(&attributes_count, p, 2);
        attributes_count = be16toh(attributes_count);
        attributes_count++;

        uint16_t attributes_count_be = htobe16(attributes_count);
        memcpy(p, &attributes_count_be, 2);

        p += 2;

        for (uint16_t j = 0; j < attributes_count - 1; j++)
        {
          p += 2;
          uint32_t attr_length;
          memcpy(&attr_length, p, 4);
          attr_length = be32toh(attr_length);

          p += 4 + attr_length;
        }

        uint16_t table_name_index_be = htobe16(table_name_index);
        memcpy(p, &table_name_index_be, 2);
        p += 2;

        uint32_t attr_length_be = htobe32(attr_length);
        memcpy(p, &attr_length_be, 4);
        p += 4;

        uint16_t local_var_count = 1;
        uint16_t local_var_count_be = htobe16(local_var_count);
        memcpy(p, &local_var_count_be, 2);
        p += 2;

        uint16_t start_pc_be = htobe16(start_pc);
        memcpy(p, &start_pc_be, 2);
        p += 2;

        uint16_t length_be = htobe16(length);
        memcpy(p, &length_be, 2);
        p += 2;

        uint16_t name_index_be = htobe16(name_index);
        memcpy(p, &name_index_be, 2);
        p += 2;

        uint16_t descriptor_index_be = htobe16(descriptor_index);
        memcpy(p, &descriptor_index_be, 2);
        p += 2;

        uint16_t index_be = htobe16(index);
        memcpy(p, &index_be, 2);
      }

      break;
    }
  }
}

void jsc_bytecode_emit_load_constant_int(jsc_bytecode_context* ctx,
                                         jsc_method* method, int32_t value)
{
  if (value >= -1 && value <= 5)
  {
    jsc_bytecode_emit(ctx, method, JSC_JVM_ICONST_0 + value);
  }
  else if (value >= -128 && value <= 127)
  {
    jsc_bytecode_emit_u8(ctx, method, JSC_JVM_BIPUSH, (uint8_t)value);
  }
  else if (value >= -32768 && value <= 32767)
  {
    jsc_bytecode_emit_u16(ctx, method, JSC_JVM_SIPUSH, (uint16_t)value);
  }
  else
  {
    uint16_t const_index = jsc_bytecode_add_integer_constant(ctx, value);
    jsc_bytecode_emit_const_load(ctx, method, const_index);
  }
}

void jsc_bytecode_emit_load_constant_long(jsc_bytecode_context* ctx,
                                          jsc_method* method, int64_t value)
{
  if (value == 0)
  {
    jsc_bytecode_emit(ctx, method, JSC_JVM_LCONST_0);
  }
  else if (value == 1)
  {
    jsc_bytecode_emit(ctx, method, JSC_JVM_LCONST_1);
  }
  else
  {
    uint16_t const_index = jsc_bytecode_add_long_constant(ctx, value);
    jsc_bytecode_emit_u16(ctx, method, JSC_JVM_LDC2_W, const_index);
  }
}

void jsc_bytecode_emit_load_constant_float(jsc_bytecode_context* ctx,
                                           jsc_method* method, float value)
{
  if (value == 0.0f)
  {
    jsc_bytecode_emit(ctx, method, JSC_JVM_FCONST_0);
  }
  else if (value == 1.0f)
  {
    jsc_bytecode_emit(ctx, method, JSC_JVM_FCONST_1);
  }
  else if (value == 2.0f)
  {
    jsc_bytecode_emit(ctx, method, JSC_JVM_FCONST_2);
  }
  else
  {
    uint16_t const_index = jsc_bytecode_add_float_constant(ctx, value);
    jsc_bytecode_emit_const_load(ctx, method, const_index);
  }
}

void jsc_bytecode_emit_load_constant_double(jsc_bytecode_context* ctx,
                                            jsc_method* method, double value)
{
  if (value == 0.0)
  {
    jsc_bytecode_emit(ctx, method, JSC_JVM_DCONST_0);
  }
  else if (value == 1.0)
  {
    jsc_bytecode_emit(ctx, method, JSC_JVM_DCONST_1);
  }
  else
  {
    uint16_t const_index = jsc_bytecode_add_double_constant(ctx, value);
    jsc_bytecode_emit_u16(ctx, method, JSC_JVM_LDC2_W, const_index);
  }
}

void jsc_bytecode_emit_load_constant_string(jsc_bytecode_context* ctx,
                                            jsc_method* method,
                                            const char* value)
{
  uint16_t const_index = jsc_bytecode_add_string_constant(ctx, value);
  jsc_bytecode_emit_const_load(ctx, method, const_index);
}

void jsc_bytecode_emit_load_constant_int_boxed(jsc_bytecode_context* ctx,
                                               jsc_method* method,
                                               int32_t value)
{
  jsc_bytecode_emit_new(ctx, method, "java/lang/Integer");
  jsc_bytecode_emit(ctx, method, JSC_JVM_DUP);

  if (value >= -1 && value <= 5)
  {
    jsc_bytecode_emit(ctx, method, JSC_JVM_ICONST_0 + value);
  }
  else if (value >= -128 && value <= 127)
  {
    jsc_bytecode_emit_u8(ctx, method, JSC_JVM_BIPUSH, (uint8_t)value);
  }
  else if (value >= -32768 && value <= 32767)
  {
    jsc_bytecode_emit_u16(ctx, method, JSC_JVM_SIPUSH, (uint16_t)value);
  }
  else
  {
    uint16_t const_index = jsc_bytecode_add_integer_constant(ctx, value);
    jsc_bytecode_emit_const_load(ctx, method, const_index);
  }

  jsc_bytecode_emit_invoke_special(ctx, method, "java/lang/Integer", "<init>",
                                   "(I)V");
}

void jsc_bytecode_emit_load_constant_long_boxed(jsc_bytecode_context* ctx,
                                                jsc_method* method,
                                                int64_t value)
{
  jsc_bytecode_emit_new(ctx, method, "java/lang/Long");
  jsc_bytecode_emit(ctx, method, JSC_JVM_DUP);

  if (value == 0)
  {
    jsc_bytecode_emit(ctx, method, JSC_JVM_LCONST_0);
  }
  else if (value == 1)
  {
    jsc_bytecode_emit(ctx, method, JSC_JVM_LCONST_1);
  }
  else
  {
    uint16_t const_index = jsc_bytecode_add_long_constant(ctx, value);
    jsc_bytecode_emit_u16(ctx, method, JSC_JVM_LDC2_W, const_index);
  }

  jsc_bytecode_emit_invoke_special(ctx, method, "java/lang/Long", "<init>",
                                   "(J)V");
}

void jsc_bytecode_emit_load_constant_float_boxed(jsc_bytecode_context* ctx,
                                                 jsc_method* method,
                                                 float value)
{
  jsc_bytecode_emit_new(ctx, method, "java/lang/Float");
  jsc_bytecode_emit(ctx, method, JSC_JVM_DUP);

  if (value == 0.0f)
  {
    jsc_bytecode_emit(ctx, method, JSC_JVM_FCONST_0);
  }
  else if (value == 1.0f)
  {
    jsc_bytecode_emit(ctx, method, JSC_JVM_FCONST_1);
  }
  else if (value == 2.0f)
  {
    jsc_bytecode_emit(ctx, method, JSC_JVM_FCONST_2);
  }
  else
  {
    uint16_t const_index = jsc_bytecode_add_float_constant(ctx, value);
    jsc_bytecode_emit_const_load(ctx, method, const_index);
  }

  jsc_bytecode_emit_invoke_special(ctx, method, "java/lang/Float", "<init>",
                                   "(F)V");
}

void jsc_bytecode_emit_load_constant_double_boxed(jsc_bytecode_context* ctx,
                                                  jsc_method* method,
                                                  double value)
{
  jsc_bytecode_emit_new(ctx, method, "java/lang/Double");
  jsc_bytecode_emit(ctx, method, JSC_JVM_DUP);

  if (value == 0.0)
  {
    jsc_bytecode_emit(ctx, method, JSC_JVM_DCONST_0);
  }
  else if (value == 1.0)
  {
    jsc_bytecode_emit(ctx, method, JSC_JVM_DCONST_1);
  }
  else
  {
    uint16_t const_index = jsc_bytecode_add_double_constant(ctx, value);
    jsc_bytecode_emit_u16(ctx, method, JSC_JVM_LDC2_W, const_index);
  }

  jsc_bytecode_emit_invoke_special(ctx, method, "java/lang/Double", "<init>",
                                   "(D)V");
}
