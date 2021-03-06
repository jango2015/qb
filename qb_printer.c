/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2012 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Chung Leong <cleong@cal.berkeley.edu>                        |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#include "qb.h"

extern const char compressed_table_zend_op_names[];

const char * qb_get_zend_op_name(qb_printer_context *cxt, uint32_t opcode) {
	if(!cxt->pool->zend_op_names) {
		qb_uncompress_table(compressed_table_zend_op_names, (void ***) &cxt->pool->zend_op_names, &cxt->pool->zend_op_name_count, 0);
	}
	if(cxt->pool->zend_op_names && opcode < cxt->pool->zend_op_name_count) {
		return cxt->pool->zend_op_names[opcode];
	}
	return "ZEND_UNKNOWN";
}

static void qb_print_value(qb_printer_context *cxt, int8_t *bytes, uint32_t type) {
	USE_TSRM
	char buffer[64];
	uint32_t len;
	len = qb_element_to_string(buffer, sizeof(buffer), bytes, type);
	php_write(buffer, len TSRMLS_CC);
}

static qb_variable * qb_find_variable_with_address(qb_printer_context *cxt, qb_address *address) {
	uint32_t i;
	for(i = 0; i < cxt->compiler_context->variable_count; i++) {
		qb_variable *qvar = cxt->compiler_context->variables[i];
		if(qvar->address == address && (qvar->name || qvar == cxt->compiler_context->return_variable)) {
			return qvar;
		}
	}
	return NULL;
}

static qb_variable * qb_find_variable_with_size_address(qb_printer_context *cxt, qb_address *address, uint32_t *p_depth, int32_t *p_recursive) {
	uint32_t i, j;
	for(i = 0; i < cxt->compiler_context->variable_count; i++) {
		qb_variable *qvar = cxt->compiler_context->variables[i];
		if(qvar->address) {
			for(j = 0; j < qvar->address->dimension_count; j++) {
				if(qvar->address->dimension_addresses[j] == address) {
					*p_depth = j + 1;
					*p_recursive = FALSE;
					return qvar;
				} else if(qvar->address->array_size_addresses[j] == address) {
					*p_depth = j + 1;
					*p_recursive = TRUE;
					return qvar;
				}
			}
		}
	}
	return NULL;
}

static void qb_print_address(qb_printer_context *cxt, qb_address *address) {
	uint32_t i;
	if(IS_IMMUTABLE(address)) {
		if(address->flags & QB_ADDRESS_STRING) {
			uint32_t len = VALUE(U32, address->array_size_address);
			char *string = (char *) ARRAY(U08, address);
			php_printf("\"%.*s\"", len, string);
		} else {
			if(IS_SCALAR(address)) {
				qb_print_value(cxt, ARRAY(I08, address), address->type);
			} else {
				uint32_t count = ARRAY_SIZE(address);
				int8_t *bytes = ARRAY(I08, address);
				php_printf("[");
				for(i = 0; i < count; i++) {
					qb_print_value(cxt, bytes, address->type);
					if(i < count - 1) {
						php_printf(", ");
					}
					if(i == 3 && count > 5) {
						php_printf("...(%d more)", count - 4);
						break;
					}
					bytes += BYTE_COUNT(1, address->type);
				}
				php_printf("]");
			}
		}
	} else if(address->source_address) {
		if(address->source_address->dimension_count == address->dimension_count + 1) {
			// array element
			qb_address *array_address = address->source_address, *a;
			uint32_t index;
			for(a = array_address; a; a = a->source_address) {
				if(a->source_address && a->source_address->dimension_count == a->dimension_count + 1) {
					array_address = a->source_address;
				}
			}
			qb_print_address(cxt, array_address);
			if(address->mode == QB_ADDRESS_MODE_SCA) {
				// array element referenced by constant indices
				index = ELEMENT_COUNT(address->segment_offset - array_address->segment_offset, address->type);
				php_printf("[%d]", index);
			} else {
				uint32_t depth;
				int32_t recursive;
				php_printf("[");
				// leave it empty it's the dimension (i.e. end of the array)
				if(!qb_find_variable_with_size_address(cxt, address, &depth, &recursive) || !recursive) {
					qb_print_address(cxt, address->array_index_address);
				}
				php_printf("]");
			}
		} else if(address->source_address->dimension_count == address->dimension_count 
			  && (address->array_index_address != address->source_address->array_index_address || address->array_size_address != address->source_address->array_size_address)) {
			// array slice
			if(IS_IMMUTABLE(address->array_index_address) && IS_IMMUTABLE(address->array_size_address)) {
				uint32_t index = VALUE(U32, address->array_index_address);
				uint32_t size = VALUE(U32, address->array_size_address);
				php_printf("(%u:%u..%u:%u)", address->segment_selector, address->segment_offset + index, address->segment_selector, address->segment_offset + index + size - 1);
			}
		} else {
			qb_print_address(cxt, address->source_address); 
		}
	} else {
		qb_variable *qvar;
		uint32_t depth;
		int32_t recursive;
		if((qvar = qb_find_variable_with_address(cxt, address))) {
			if(qvar->name) {
				if(qvar->flags & QB_VARIABLE_CLASS) {
					if(qvar->zend_class) {
						php_printf("self::$%s", qvar->name);
					} else {
						php_printf("static::$%s", qvar->name);
					}
				} else if(qvar->flags & QB_VARIABLE_CLASS_INSTANCE) {
					php_printf("$this->%s", qvar->name);
				} else if(qvar->flags & QB_VARIABLE_CLASS_CONSTANT) {
						php_printf("static::%s", qvar->name);
				} else {
					php_printf("$%s", qvar->name);
				}
			} else {
				php_printf("(retval)");
			}
		} else if((qvar = qb_find_variable_with_size_address(cxt, address, &depth, &recursive))) {
			php_printf("count($%s", qvar->name);
			while(--depth > 0) {
				php_printf("[]");
			}
			php_printf((recursive) ? ", true)" : ")");
		} else {
			if(IS_SCALAR(address)) {
				php_printf("(%u:%u)", address->segment_selector, address->segment_offset);
			} else {
				if(IS_VARIABLE_LENGTH(address)) {
					php_printf("(%u:%u...)", address->segment_selector, address->segment_offset);
				} else {
					php_printf("(%u:%u..%u:%u)", address->segment_selector, address->segment_offset, address->segment_selector, address->segment_offset + (ARRAY_SIZE(address) - 1));
				}
			}
		}
	}
}

extern const char compressed_table_op_names[];

static const char * qb_get_op_name(qb_printer_context *cxt, uint32_t opcode) {
	if(!cxt->pool->op_names) {
		// decompress the opname table
		qb_uncompress_table(compressed_table_op_names, (void ***) &cxt->pool->op_names, &cxt->pool->op_name_count, 0);
	}
	if(cxt->pool->op_names && opcode < cxt->pool->op_name_count) {
		return cxt->pool->op_names[opcode];
	}
	return "?";
}

static void qb_print_op(qb_printer_context *cxt, qb_op *qop, uint32_t index) {
	uint32_t i;
	const char *name = qb_get_op_name(cxt, qop->opcode);
	php_printf("%04d: %s ", index, name);
	for(i = 0; i < qop->operand_count; i++) {
		qb_operand *operand = &qop->operands[i];
		switch(operand->type) {
			case QB_OPERAND_ADDRESS: {
				qb_print_address(cxt, operand->address);
			}	break;
			case QB_OPERAND_SEGMENT_SELECTOR: {
				php_printf("@%d", operand->address->segment_selector);
			}	break;
			case QB_OPERAND_ELEMENT_SIZE: {
				php_printf("sizeof(%s)", type_names[operand->address->type]);
			}	break;
			case QB_OPERAND_NUMBER: {
				php_printf("'%d'", operand->number);
			}	break;
			default: {
				php_printf("(ERROR)");
				break;
			}
		}
		php_printf(" ");
	}
	for(i = 0; i < qop->jump_target_count; i++) {
		uint32_t jump_target_index = qop->jump_target_indices[i];
		php_printf("<%04d> ", jump_target_index);
	}
	if(qop->flags & QB_OP_NEED_LINE_IDENTIFIER) {
		php_printf("(line %d)", LINE_NUMBER(qop->line_id));
	}
	php_printf("\n");
}

void qb_print_ops(qb_printer_context *cxt) {
	uint32_t i;
	php_printf("; %s()\n", cxt->compiler_context->function_prototype.name);
	for(i = 0; i < cxt->compiler_context->op_count; i++) {
		qb_op *qop = cxt->compiler_context->ops[i];
		if(qop->opcode != QB_NOP) {
			qb_print_op(cxt, qop, i);
		}
	}
}

static void qb_print_zend_ops(qb_printer_context *cxt) {
	uint32_t i = 0;
	for(i = 0; i < cxt->compiler_context->zend_op_array->last; i++) {
		zend_op *zop = &cxt->compiler_context->zend_op_array->opcodes[i];
		if(zop->opcode != QB_USER_OPCODE) {
			const char *opname = qb_get_zend_op_name(cxt, zop->opcode);
			php_printf("[%04d] %s (line number: %d)\n", i, opname, zop->lineno);
		}
	}
}

static qb_pbj_texture * qb_find_pbj_texture_by_id(qb_printer_context *cxt, uint32_t image_id) {
	qb_pbj_translator_context *translator_cxt = cxt->compiler_context->translator_context;
	uint32_t i;
	for(i = 0; i < translator_cxt->texture_count; i++) {
		qb_pbj_texture *texture = &translator_cxt->textures[i];
		if(texture->image_id == image_id) {
			return texture;
		}
	}
	return NULL;
}

static void qb_print_pbj_address(qb_printer_context *cxt, qb_pbj_address *address) {
	if(address->dimension) {
		qb_pbj_parameter *parameter = NULL;//qb_find_pbj_parameter_by_address(cxt, address);
		if(parameter) {
			if(parameter->destination.channel_id <= PBJ_CHANNEL_A) {
				// don't show channels when it's a scalar
				php_printf("%s ", parameter->name);
				return;
			} else {
				php_printf("%s.", parameter->name);
			}
		} else {
			if(address->register_id & PBJ_REGISTER_INT) {
				php_printf("i%d.", address->register_id & ~PBJ_REGISTER_INT);
			} else {
				php_printf("f%d.", address->register_id);
			}
		}
		if(address->dimension == 1) {
			static const char *channels[] = { "r", "g", "b", "a", "rg", "gb", "ba", "rgb", "gba", "rgba" };
			const char *channel_name = channels[address->channel_id];
			if(address->channel_mask == INVALID_INDEX) {
				php_printf("%s", channel_name);
			} else {
				uint32_t i;
				for(i = 0; i < address->channel_count; i++) {
					php_printf("%c", channel_name[address->channel_mask >> (i * 3)]);
				}
			}
		} else {
			static const char *matrix[] = { "", "", "m2x2", "m3x3", "m4x4" };
			php_printf("%s", matrix[address->dimension]);
		}
		php_printf(" ");
	}
}

extern const char compressed_table_pbj_op_names[];

static const char * qb_get_pbj_op_name(qb_printer_context *cxt, uint32_t opcode) {
	if(!cxt->pool->pbj_op_names) {
		qb_uncompress_table(compressed_table_pbj_op_names, (void ***) &cxt->pool->pbj_op_names, &cxt->pool->pbj_op_name_count, 0);
	}
	if(cxt->pool->pbj_op_names && opcode < cxt->pool->pbj_op_name_count) {
		return cxt->pool->pbj_op_names[opcode];
	}
	return "?";
}

static void qb_print_pbj_op(qb_printer_context *cxt, qb_pbj_op *pop, uint32_t pop_index) {
	const char *op_name = qb_get_pbj_op_name(cxt, pop->opcode);
	//uint32_t i;
	php_printf("%04d: %s ", pop_index, op_name);

	if(pop->opcode == PBJ_SAMPLE_NEAREST || pop->opcode == PBJ_SAMPLE_BILINEAR) {
		qb_pbj_texture *texture = qb_find_pbj_texture_by_id(cxt, pop->image_id);
		if(texture) {
			php_printf("%s ", texture->name);
		} else {
			php_printf("t%d ", pop->image_id);
		}
	}
	if(pop->opcode == PBJ_LOAD_CONSTANT) {
		if(pop->constant.type == PBJ_TYPE_INT) {
			php_printf("%d ", pop->constant.int_value);
		} else {
			php_printf("%G ", pop->constant.float_value);
		}
	} else {
		qb_print_pbj_address(cxt, &pop->source);
	}
	if(pop->opcode == PBJ_SELECT || pop->opcode == PBJ_SMOOTH_STEP) {
		qb_pbj_op *data_pop = pop + 1;
		qb_print_pbj_address(cxt, &data_pop->source2);
		qb_print_pbj_address(cxt, &data_pop->source3);
	}
	if(!(pop->opcode >= PBJ_IF && pop->opcode <= PBJ_END_IF)) {
		qb_print_pbj_address(cxt, &pop->destination);
	}
	php_printf("\n");
}

static void qb_print_pbj_ops(qb_printer_context *cxt) {
	qb_pbj_translator_context *translator_cxt = cxt->compiler_context->translator_context;
	uint32_t i;
	for(i = 0; i < translator_cxt->pbj_op_count; i++) {
		qb_pbj_op *pop = &translator_cxt->pbj_ops[i];
		if(pop->opcode != PBJ_OP_DATA) {
			qb_print_pbj_op(cxt, pop, i);
		}
	}
}

void qb_print_source_ops(qb_printer_context *cxt) {
	switch(cxt->compiler_context->translation) {
		case QB_TRANSLATION_PHP: {
			qb_print_zend_ops(cxt);
		}	break;
		case QB_TRANSLATION_PBJ: {
			qb_print_pbj_ops(cxt);
		}	break;
	}
}

void qb_initialize_printer_context(qb_printer_context *cxt, qb_compiler_context *compiler_cxt TSRMLS_DC) {
	cxt->compiler_context = compiler_cxt;
	cxt->pool = compiler_cxt->pool;
	cxt->storage = compiler_cxt->storage;

	SAVE_TSRMLS
}

void qb_free_printer_context(qb_printer_context *cxt) {
}
