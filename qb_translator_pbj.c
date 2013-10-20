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
#include "qb_translator_pbj.h"

static uint8_t qb_pbj_read_I08(qb_pbj_translator_context *cxt) {
	uint8_t value = 0;
	if(cxt->pbj_data_end - cxt->pbj_data >= 1) { 
		value = *((uint8_t *) cxt->pbj_data);
		cxt->pbj_data += 1;
	}
	return value;
}

static uint16_t qb_pbj_read_I16(qb_pbj_translator_context *cxt) {
	uint16_t value = 0;
	if(cxt->pbj_data_end - cxt->pbj_data >= 2) { 
		value = SWAP_LE_I16(*((uint16_t *) cxt->pbj_data));
		cxt->pbj_data += 2;
	}
	return value;
}

static uint32_t qb_pbj_read_I32(qb_pbj_translator_context *cxt) {
	uint32_t value = 0;
	if(cxt->pbj_data_end - cxt->pbj_data >= 4) { 
		value = SWAP_LE_I32(*((uint32_t *) cxt->pbj_data));
		cxt->pbj_data += 4;
	}
	return value;
}

static float32_t qb_pbj_read_F32(qb_pbj_translator_context *cxt) {
	float32_t value = 0.0f;
	if(cxt->pbj_data_end - cxt->pbj_data >= 4) { 
		uint32_t int_value = SWAP_BE_I32(*((uint32_t *) cxt->pbj_data));
		value = *((float32_t *) &int_value);
		cxt->pbj_data += 4;
	}
	return value;
}

static const char * qb_pbj_read_string(qb_pbj_translator_context *cxt, uint32_t *p_length) {
	const char *value = "";
	if(p_length) {
		uint16_t len = qb_pbj_read_I16(cxt);
		if(cxt->pbj_data_end - cxt->pbj_data >= len) { 
			value = (const char *) cxt->pbj_data;
			*p_length = len;
		} else {
			*p_length = 0;
		}
		cxt->pbj_data += len;
	} else {
		const char *start = (const char *) cxt->pbj_data;
		while(cxt->pbj_data < cxt->pbj_data_end) {
			if(*cxt->pbj_data == '\0') {
				value = start;
				cxt->pbj_data++;
				break;
			}
			cxt->pbj_data++; 
		}
	}
	return value;
}

static qb_pbj_parameter * qb_pbj_find_parameter_by_address(qb_pbj_translator_context *cxt, qb_pbj_address *address) {
	uint32_t i;
	for(i = 0; i < cxt->pbj_parameter_count; i++) {
		qb_pbj_parameter *parameter = &cxt->pbj_parameters[i];
		qb_pbj_address *destination = &parameter->destination;
		if(destination->register_id == address->register_id) {
			if(destination->dimension > 1) {
				return parameter;
			} else {
				// see if the address is pointing to a channel used by the parameter
				uint32_t j, k;
				for(j = 0; j < address->channel_count; j++) {
					uint32_t channel = address->channels[j];
					for(k = 0; k < destination->channel_count; k++) {
						if(channel == destination->channels[k]) {
							return parameter;
						}
					}
				}
			}
		}
	}
	return NULL;
}

static qb_pbj_texture * qb_pbj_find_texture_by_id(qb_pbj_translator_context *cxt, uint32_t image_id) {
	uint32_t i;
	for(i = 0; i < cxt->pbj_texture_count; i++) {
		qb_pbj_texture *texture = &cxt->pbj_textures[i];
		if(texture->image_id == image_id) {
			return texture;
		}
	}
	return NULL;
}

static void qb_pbj_read_value(qb_pbj_translator_context *cxt, uint32_t type, qb_pbj_value *value) {
	uint32_t i;
	value->type = type;
	switch(type) {
		case PBJ_TYPE_FLOAT: {
			value->float1 = qb_pbj_read_F32(cxt);
		}	break;
		case PBJ_TYPE_FLOAT2: {
			for(i = 0; i < 2; i++) {
				value->float2[i] = qb_pbj_read_F32(cxt);
			}
		}	break;
		case PBJ_TYPE_FLOAT3: {
			for(i = 0; i < 3; i++) {
				value->float3[i] = qb_pbj_read_F32(cxt);
			}
		}	break;
		case PBJ_TYPE_FLOAT4: {
			for(i = 0; i < 4; i++) {
				value->float4[i] = qb_pbj_read_F32(cxt);
			}
		}	break;
		case PBJ_TYPE_FLOAT2X2: {
			for(i = 0; i < 4; i++) {
				value->float2x2[i] = qb_pbj_read_F32(cxt);
			}
		}	break;
		case PBJ_TYPE_FLOAT3X3: {
			for(i = 0; i < 9; i++) {
				value->float3x3[i] = qb_pbj_read_F32(cxt);
			}
		}	break;
		case PBJ_TYPE_FLOAT4X4: {
			for(i = 0; i < 16; i++) {
				value->float4x4[i] = qb_pbj_read_F32(cxt);
			}
		}	break;
		case PBJ_TYPE_BOOL:
		case PBJ_TYPE_INT: {
			value->int1 = qb_pbj_read_I16(cxt);
		}	break;
		case PBJ_TYPE_BOOL2:
		case PBJ_TYPE_INT2: {
			for(i = 0; i < 2; i++) {
				value->int2[i] = qb_pbj_read_I16(cxt);
			}
		}	break;
		case PBJ_TYPE_BOOL3:
		case PBJ_TYPE_INT3: {
			for(i = 0; i < 3; i++) {
				value->int3[i] = qb_pbj_read_I16(cxt);
			}
		}	break;
		case PBJ_TYPE_BOOL4:
		case PBJ_TYPE_INT4: {
			for(i = 0; i < 4; i++) {
				value->int4[i] = qb_pbj_read_I16(cxt);
			}
		}	break;
		case PBJ_TYPE_STRING: {
			value->string = qb_pbj_read_string(cxt, NULL);
		}	break;
	}
}

static qb_pbj_texture * qb_pbj_find_texture(qb_pbj_translator_context *cxt, const char *name) {
	uint32_t i;
	for(i = 0; i < cxt->pbj_texture_count; i++) {
		qb_pbj_texture *texture = &cxt->pbj_textures[i];
		if(strcmp(texture->name, name) == 0) {
			return texture;
		}
	}
	return NULL;
}

static void ZEND_FASTCALL qb_set_pbj_destination_channels(qb_pbj_translator_context *cxt, qb_pbj_address *address, uint32_t channel_mask, uint32_t dimension) {
	// dimension is used only if no destination channels are selected 
	// otherwise dimension is pertinent only to the source
	if(channel_mask) {
		uint32_t channel, bit;
		address->dimension = 1;
		address->channel_count = 0;
		address->all_channels = 0;
		for(bit = 0x08, channel = 0; channel < 4; channel++, bit >>= 1) {
			if(channel_mask & bit) {
				address->channels[address->channel_count++] = channel;
			}
		}
	} else {
		address->dimension = dimension;
		address->channel_count = 0;
	}
}

static void ZEND_FASTCALL qb_set_pbj_source_channels(qb_pbj_translator_context *cxt, qb_pbj_address *address, uint32_t channel_swizzle, uint32_t channel_count, uint32_t dimension) {
	if(dimension == 1) {
		uint32_t channel, i;
		address->dimension = 1;
		address->channel_count = channel_count;
		address->all_channels = 0;
		for(i = 0; i < channel_count; i++) {
			channel = (channel_swizzle >> ((3 - i) * 2)) & 0x03;
			address->channels[i] = channel;
		}
	} else {
		address->dimension = dimension;
		address->channel_count = 0;
	}
}

void qb_decode_pbj_binary(qb_pbj_translator_context *cxt) {
	uint32_t opcode, prev_opcode, i;
	float32_t f;
	cxt->pbj_data = (uint8_t *) cxt->compiler_context->external_code;
	cxt->pbj_data_end = cxt->pbj_data + cxt->compiler_context->external_code_length;

	qb_attach_new_array(cxt->pool, (void **) &cxt->pbj_conditionals, &cxt->pbj_conditional_count, sizeof(qb_pbj_op *), 8);
	qb_attach_new_array(cxt->pool, (void **) &cxt->pbj_parameters, &cxt->pbj_parameter_count, sizeof(qb_pbj_parameter), 8);
	qb_attach_new_array(cxt->pool, (void **) &cxt->pbj_textures, &cxt->pbj_texture_count, sizeof(qb_pbj_texture), 8);
	qb_attach_new_array(cxt->pool, (void **) &cxt->pbj_float_registers, &cxt->pbj_float_register_count, sizeof(qb_pbj_register), 256);
	qb_attach_new_array(cxt->pool, (void **) &cxt->pbj_int_registers, &cxt->pbj_int_register_count, sizeof(qb_pbj_register), 256);

	// obtain addresses to integer and float constants
	cxt->pbj_int_numerals = qb_allocate_address_pointers(cxt->pool, 17);
	for(i = 0; i < 17; i++) {
		if(i <= 4 || i == 12 || i == 16) {
			cxt->pbj_int_numerals[i] = qb_obtain_constant_U32(cxt->compiler_context, i);
		}
	}
	cxt->pbj_float_numerals = qb_allocate_address_pointers(cxt->pool, 3);
	for(i = 0, f = 0; i < 3; i++, f += 0.5) {
		cxt->pbj_float_numerals[i] = qb_obtain_constant_F32(cxt->compiler_context, f);
	}

	while(cxt->pbj_data < cxt->pbj_data_end) {
		opcode = qb_pbj_read_I08(cxt);
		if(opcode <= PBJ_ALL) {
			uint32_t bit_fields, channel_mask, channel_swizzle, channel_count, dimension;
			qb_pbj_op *pop;
			uint32_t pop_index;

			if(!cxt->pbj_ops) {				
				// each op is 8 bytes long 
				uint32_t bytes_remaining = (cxt->pbj_data_end - cxt->pbj_data) + 1;
				uint32_t op_count = bytes_remaining / 8;
				if(op_count == 0) {
					return;
				}
				qb_attach_new_array(cxt->pool, (void **) &cxt->pbj_ops, &cxt->pbj_op_count, sizeof(qb_pbj_op), op_count);
			}
			pop_index = cxt->pbj_op_count;
			pop = qb_enlarge_array((void **) &cxt->pbj_ops, 1);
			pop->opcode = opcode;

			// read the destination address
			pop->destination.register_id = qb_pbj_read_I16(cxt);
			bit_fields = qb_pbj_read_I08(cxt);
			channel_mask = bit_fields >> 4;
			channel_count = (bit_fields & 0x03) + 1;
			dimension = ((bit_fields >> 2) & 0x03) + 1;
			qb_set_pbj_destination_channels(cxt, &pop->destination, channel_mask, dimension);

			if(opcode == PBJ_LOAD_CONSTANT) {
				pop->source.channel_count = pop->source.dimension = 0;
				pop->image_id = 0;
				if(pop->destination.register_id & PBJ_REGISTER_INT) {
					pop->constant.int_value = qb_pbj_read_I32(cxt);
					pop->constant.type = PBJ_TYPE_INT;
				} else {			
					pop->constant.float_value = qb_pbj_read_F32(cxt);
					pop->constant.type = PBJ_TYPE_FLOAT;
				}
				// omit allocating registers for constant load operation
				// any constant loaded will be used eventually elsewhere
			} else {
				// read the source
				pop->source.register_id = qb_pbj_read_I16(cxt);
				channel_swizzle = qb_pbj_read_I08(cxt);
				qb_set_pbj_source_channels(cxt, &pop->source, channel_swizzle, channel_count, dimension);

				// read the image id (only used for sampling functions)
				pop->image_id = qb_pbj_read_I08(cxt);

				if(opcode == PBJ_IF) {
					// push the instruction onto the conditional stack
					qb_pbj_op **p_conditional_pop = qb_enlarge_array((void **) &cxt->pbj_conditionals, 1);
					*p_conditional_pop = pop;
					pop->destination.dimension = 0;
					pop->branch_target_index = 0;
				} else if(opcode == PBJ_ELSE || opcode == PBJ_END_IF) {
					// update the branch target index of the op at the top of the stack
					qb_pbj_op *related_pop;
					if(cxt->pbj_conditional_count == 0) {
						qb_abort("unexpected opcode: %02x", opcode);
					}
					related_pop = cxt->pbj_conditionals[cxt->pbj_conditional_count - 1];
					related_pop->branch_target_index = pop_index;
					if(opcode == PBJ_ELSE) {
						// replace the if with the else op
						cxt->pbj_conditionals[cxt->pbj_conditional_count - 1] = pop;
					} else {
						// pop the item off
						cxt->pbj_conditional_count--;
					}
					pop->source.dimension = 0;
					pop->destination.dimension = 0;
					pop->branch_target_index = 0;
				} else if(opcode == PBJ_SELECT) {
					// put the on-true and on-false sources in the next op
					qb_pbj_op *data_pop = qb_enlarge_array((void **) &cxt->pbj_ops, 1);
					data_pop->opcode = PBJ_OP_DATA;
					data_pop->source2.register_id = qb_pbj_read_I16(cxt);
					channel_swizzle = qb_pbj_read_I08(cxt);
					bit_fields = qb_pbj_read_I08(cxt);
					channel_count = (bit_fields >> 6) + 1;
					qb_set_pbj_source_channels(cxt, &data_pop->source2, channel_swizzle, channel_count, dimension);

					data_pop->source3.register_id = qb_pbj_read_I16(cxt);
					channel_swizzle = qb_pbj_read_I08(cxt);
					bit_fields = qb_pbj_read_I08(cxt);
					channel_count = (bit_fields >> 6) + 1;
					qb_set_pbj_source_channels(cxt, &data_pop->source3, channel_swizzle, channel_count, dimension);
				}
			}
		} else if(opcode == PBJ_KERNEL_METADATA || opcode == PBJ_PARAMETER_METADATA) {
			uint8_t value_type;
			const char *value_name;
			qb_pbj_parameter *last_parameter;
			qb_pbj_value *value = NULL, tmp;
			value_type = qb_pbj_read_I08(cxt);
			value_name = qb_pbj_read_string(cxt, NULL);
			if(opcode == PBJ_PARAMETER_METADATA && cxt->pbj_parameter_count > 0) {
				last_parameter = &cxt->pbj_parameters[cxt->pbj_parameter_count - 1];
				if(strcmp(value_name, "minValue") == 0) {
					value = &last_parameter->min_value;
				} else if(strcmp(value_name, "maxValue") == 0) {
					value = &last_parameter->max_value;
				} else if(strcmp(value_name, "defaultValue") == 0) {
					value = &last_parameter->default_value;
				}
			}
			if(!value) {
				value = &tmp;
			}
			qb_pbj_read_value(cxt, value_type, value);
			if(opcode == PBJ_PARAMETER_METADATA && value_type == PBJ_TYPE_STRING) {
				if(strcmp(value_name, "parameterType") == 0) {
					last_parameter->parameter_type = value->string;
				} else if(strcmp(value_name, "inputSizeName") == 0) {
					last_parameter->input_size_name = value->string;
				} else if(strcmp(value_name, "description") == 0) {
					last_parameter->description = value->string;
				} else if(strcmp(value_name, "displayName") == 0) {
					last_parameter->display_name = value->string;
				} else if(strcmp(value_name, "parameterType") == 0) {
					last_parameter->parameter_type = value->string;
				} 
			} else if(opcode == PBJ_KERNEL_METADATA) {
				if(strcmp(value_name, "description") == 0) {
					cxt->pbj_description = value->string;
				} else if(strcmp(value_name, "vendor") == 0) {
					cxt->pbj_vendor = value->string;
				} else if(strcmp(value_name, "displayName") == 0) {
					cxt->pbj_display_name = value->string;
				} else if(strcmp(value_name, "version") == 0) {
					cxt->pbj_version = value->int1;
				}
			}
		} else if(opcode == PBJ_PARAMETER_DATA) {
			qb_pbj_parameter *parameter = qb_enlarge_array((void **) &cxt->pbj_parameters, 1);
			uint32_t bit_fields, dimension, channel_mask;
			parameter->qualifier = qb_pbj_read_I08(cxt);
			parameter->type = qb_pbj_read_I08(cxt);
			parameter->destination.register_id = qb_pbj_read_I16(cxt);
			bit_fields = qb_pbj_read_I08(cxt);
			switch(parameter->type) {
				case PBJ_TYPE_FLOAT2X2: 
				case PBJ_TYPE_FLOAT3X3: 
				case PBJ_TYPE_FLOAT4X4: 
					dimension = (parameter->type - PBJ_TYPE_FLOAT2X2) + 2;
					channel_mask = 0;
					break;
				default:
					dimension = 1;
					channel_mask = bit_fields & 0x0F;
					break;
			}
			qb_set_pbj_destination_channels(cxt, &parameter->destination, channel_mask, dimension);
			parameter->name = qb_pbj_read_string(cxt, NULL);
			parameter->address = NULL;
			parameter->parameter_type = NULL;
			parameter->description = NULL;
			parameter->input_size_name = NULL;
			memset(&parameter->default_value, 0, sizeof(qb_pbj_value));
			memset(&parameter->min_value, 0, sizeof(qb_pbj_value));
			memset(&parameter->max_value, 0, sizeof(qb_pbj_value));
		} else if(opcode == PBJ_TEXTURE_DATA) {
			qb_pbj_texture *texture = qb_enlarge_array((void **) &cxt->pbj_textures, 1);
			texture->image_id = qb_pbj_read_I08(cxt);
			texture->channel_count = qb_pbj_read_I08(cxt);
			texture->name = qb_pbj_read_string(cxt, NULL);
			texture->address = NULL;
			texture->input_size = NULL;
		} else if(opcode == PBJ_KERNEL_NAME) {
			cxt->pbj_name = qb_pbj_read_string(cxt, &cxt->pbj_name_length);
		} else if(opcode == PBJ_VERSION_DATA) {
			qb_pbj_read_I32(cxt);
		} else {
			qb_abort("unknown opcode: %02x (previous opcode = %02x)", opcode, prev_opcode);
			break;
		}
		prev_opcode = opcode;
	}
	for(i = 0; i < cxt->pbj_parameter_count; i++) {
		qb_pbj_parameter *parameter = &cxt->pbj_parameters[i];
		if(strcmp(parameter->name, "_OutCoord") == 0) {
			cxt->pbj_out_coord = parameter;
		} else if(parameter->qualifier == PBJ_PARAMETER_OUT) {
			cxt->pbj_out_pixel = parameter;
		} else if(parameter->input_size_name) {
			qb_pbj_texture *texture = qb_pbj_find_texture(cxt, parameter->input_size_name);
			if(texture) {
				texture->input_size = parameter;
			}
		}
	}

	if(!cxt->pbj_out_pixel) {
		qb_abort("missing output pixel");
	}
	if(!cxt->pbj_out_coord) {
		qb_abort("missing output pixel coordinates");
	}
	if(cxt->pbj_conditional_count > 0) {
		qb_abort("missing end if");
	}
	if(cxt->pbj_out_pixel->type != PBJ_TYPE_FLOAT3 && cxt->pbj_out_pixel->type != PBJ_TYPE_FLOAT4) {
		qb_abort("output pixel is not float3 or float4");
	}
}

static void qb_pbj_translate_basic_op(qb_pbj_translator_context *cxt, qb_pbj_translator *t, qb_pbj_address **inputs, uint32_t input_count, qb_pbj_address *output) {
}

static void qb_pbj_translate_load_constant(qb_pbj_translator_context *cxt, qb_pbj_translator *t, qb_pbj_address **inputs, uint32_t input_count, qb_pbj_address *output) {
}

static void qb_pbj_translate_select(qb_pbj_translator_context *cxt, qb_pbj_translator *t, qb_pbj_address **inputs, uint32_t input_count, qb_pbj_address *output) {
}

static void qb_pbj_translate_if(qb_pbj_translator_context *cxt, qb_pbj_translator *t, qb_pbj_address **inputs, uint32_t input_count, qb_pbj_address *output) {
}

static void qb_pbj_translate_else(qb_pbj_translator_context *cxt, qb_pbj_translator *t, qb_pbj_address **inputs, uint32_t input_count, qb_pbj_address *output) {
	// jump over the else block
}

static void qb_pbj_translate_end_if(qb_pbj_translator_context *cxt, qb_pbj_translator *t, qb_pbj_address **inputs, uint32_t input_count, qb_pbj_address *output) {
	// do nothing
}

#define PBJ_RS				PBJ_READ_SOURCE
#define PBJ_WD				PBJ_WRITE_DESTINATION
#define PBJ_RS_WD			(PBJ_READ_SOURCE | PBJ_WRITE_DESTINATION)
#define PBJ_RS_RD_WD		(PBJ_READ_SOURCE | PBJ_READ_DESTINATION | PBJ_WRITE_DESTINATION)
#define PBJ_RS_RD_WD1		(PBJ_READ_SOURCE | PBJ_READ_DESTINATION | PBJ_WRITE_DESTINATION | PBJ_WRITE_SCALAR)
#define PBJ_RS_RD_WB		(PBJ_READ_SOURCE | PBJ_READ_DESTINATION | PBJ_WRITE_BOOL)
#define PBJ_RS_RS2_RS3_WD	(PBJ_READ_SOURCE | PBJ_READ_SOURCE2 | PBJ_READ_SOURCE3 | PBJ_WRITE_DESTINATION)

static qb_pbj_translator pbj_op_translators[] = {
	{	NULL,										0,					NULL						},	// PBJ_NOP
	{	qb_pbj_translate_basic_op,					PBJ_RS_RD_WD,		&factory_add				},	// PBJ_ADD
	{	qb_pbj_translate_basic_op,					PBJ_RS_RD_WD,		&factory_subtract			},	// PBJ_SUBTRACT
	{	qb_pbj_translate_basic_op,					PBJ_RS_RD_WD,		&factory_multiply			},	// PBJ_MULTIPLY
	{	qb_pbj_translate_basic_op,					PBJ_RS_WD,			&factory_divide				},	// PBJ_RECIPROCAL
	{	qb_pbj_translate_basic_op,					PBJ_RS_RD_WD,		&factory_divide				},	// PBJ_DIVIDE
	{	qb_pbj_translate_basic_op,					PBJ_RS_RD_WD,		&factory_atan2				},	// PBJ_ATAN2
	{	qb_pbj_translate_basic_op,					PBJ_RS_RD_WD,		&factory_pow				},	// PBJ_POW
	{	qb_pbj_translate_basic_op,					PBJ_RS_RD_WD,		&factory_floor_modulo		},	// PBJ_MOD
	{	qb_pbj_translate_basic_op,					PBJ_RS_RD_WD,		&factory_min				},	// PBJ_MIN
	{	qb_pbj_translate_basic_op,					PBJ_RS_RD_WD,		&factory_max				},	// PBJ_MAX
	{	qb_pbj_translate_basic_op,					PBJ_RS_RD_WD,		&factory_step				},	// PBJ_STEP
	{	qb_pbj_translate_basic_op,					PBJ_RS_WD,			&factory_sin				},	// PBJ_SIN
	{	qb_pbj_translate_basic_op,					PBJ_RS_WD,			&factory_cos				},	// PBJ_COS
	{	qb_pbj_translate_basic_op,					PBJ_RS_WD,			&factory_tan				},	// PBJ_TAN
	{	qb_pbj_translate_basic_op,					PBJ_RS_WD,			&factory_asin				},	// PBJ_ASIN
	{	qb_pbj_translate_basic_op,					PBJ_RS_WD,			&factory_acos				},	// PBJ_ACOS
	{	qb_pbj_translate_basic_op,					PBJ_RS_WD,			&factory_atan				},	// PBJ_ATAN
	{	qb_pbj_translate_basic_op,					PBJ_RS_WD,			&factory_exp				},	// PBJ_EXP
	{	qb_pbj_translate_basic_op,					PBJ_RS_WD,			&factory_exp2				},	// PBJ_EXP2
	{	qb_pbj_translate_basic_op,					PBJ_RS_WD,			&factory_log				},	// PBJ_LOG
	{	qb_pbj_translate_basic_op,					PBJ_RS_WD,			&factory_log2				},	// PBJ_LOG2
	{	qb_pbj_translate_basic_op,					PBJ_RS_WD,			&factory_sqrt				},	// PBJ_SQRT
	{	qb_pbj_translate_basic_op,					PBJ_RS_WD,			&factory_rsqrt			},	// PBJ_RSQRT
	{	qb_pbj_translate_basic_op,					PBJ_RS_WD,			&factory_abs				},	// PBJ_ABS
	{	qb_pbj_translate_basic_op,					PBJ_RS_WD,			&factory_sign				},	// PBJ_SIGN
	{	qb_pbj_translate_basic_op,					PBJ_RS_WD,			&factory_floor				},	// PBJ_FLOOR
	{	qb_pbj_translate_basic_op,					PBJ_RS_WD,			&factory_ceil				},	// PBJ_CEIL
	{	qb_pbj_translate_basic_op,					PBJ_RS_WD,			&factory_fract				},	// PBJ_FRACT
	{	qb_pbj_translate_basic_op,					PBJ_RS_WD,			&factory_assign				},	// PBJ_COPY
	{	qb_pbj_translate_basic_op,					PBJ_RS_WD,			&factory_assign				},	// PBJ_FLOAT_TO_INT
	{	qb_pbj_translate_basic_op,					PBJ_RS_WD,			&factory_assign				},	// PBJ_INT_TO_FLOAT
	{	qb_pbj_translate_basic_op,					PBJ_RS_RD_WD,		&factory_mm_mult_cm			},	// PBJ_MATRIX_MATRIX_MULTIPLY
	{	qb_pbj_translate_basic_op,					PBJ_RS_RD_WD,		&factory_vm_mult_cm			},	// PBJ_VECTOR_MATRIX_MULTIPLY
	{	qb_pbj_translate_basic_op,					PBJ_RS_RD_WD,		&factory_mv_mult_cm			},	// PBJ_MATRIX_VECTOR_MULTIPLY
	{	qb_pbj_translate_basic_op,					PBJ_RS_WD,			&factory_normalize			},	// PBJ_NORMALIZE
	{	qb_pbj_translate_basic_op,					PBJ_RS_WD,			&factory_length				},	// PBJ_LENGTH
	{	qb_pbj_translate_basic_op,					PBJ_RS_RD_WD1,		&factory_distance			},	// PBJ_DISTANCE
	{	qb_pbj_translate_basic_op,					PBJ_RS_RD_WD1,		&factory_dot_product		},	// PBJ_DOT_PRODUCT
	{	qb_pbj_translate_basic_op,					PBJ_RS_RD_WD,		&factory_cross_product		},	// PBJ_CROSS_PRODUCT
	{	qb_pbj_translate_basic_op,					PBJ_RS_RD_WB,		&factory_equal				},	// PBJ_EQUAL
	{	qb_pbj_translate_basic_op,					PBJ_RS_RD_WB,		&factory_not_equal			},	// PBJ_NOT_EQUAL
	{	qb_pbj_translate_basic_op,					PBJ_RS_RD_WB,		&factory_less_than			},	// PBJ_LESS_THAN
	{	qb_pbj_translate_basic_op,					PBJ_RS_RD_WB,		&factory_less_equal			},	// PBJ_LESS_THAN_EQUAL
	{	qb_pbj_translate_basic_op,					PBJ_RS_WD,			&factory_logical_not		},	// PBJ_LOGICAL_NOT
	{	qb_pbj_translate_basic_op,					PBJ_RS_RD_WD,		&factory_logical_and		},	// PBJ_LOGICAL_AND
	{	qb_pbj_translate_basic_op,					PBJ_RS_RD_WD,		&factory_logical_or			},	// PBJ_LOGICAL_OR
	{	qb_pbj_translate_basic_op,					PBJ_RS_RD_WD,		&factory_logical_xor		},	// PBJ_LOGICAL_XOR
	{	qb_pbj_translate_basic_op,					PBJ_RS_WD,			&factory_sample_nearest		},	// PBJ_SAMPLE_NEAREST
	{	qb_pbj_translate_basic_op,					PBJ_RS_WD,			&factory_sample_bilinear	},	// PBJ_SAMPLE_BILINEAR
	{	qb_pbj_translate_load_constant,				PBJ_WD,				NULL,						},	// PBJ_LOAD_CONSTANT
	{	qb_pbj_translate_select,					PBJ_RS_RS2_RS3_WD,	NULL,						},	// PBJ_SELECT
	{	qb_pbj_translate_if,						PBJ_RS,				NULL,						},	// PBJ_IF
	{	qb_pbj_translate_else,						0,					NULL,						},	// PBJ_ELSE
	{	qb_pbj_translate_end_if,					0,					NULL						},	// PBJ_END_IF
	{	qb_pbj_translate_basic_op,					0,					&factory_not_equal			},	// PBJ_FLOAT_TO_BOOL
	{	qb_pbj_translate_basic_op,					PBJ_RS_WD,			&factory_assign				},	// PBJ_BOOL_TO_FLOAT
	{	qb_pbj_translate_basic_op,					0,					&factory_not_equal			},	// PBJ_INT_TO_BOOL
	{	NULL,										PBJ_RS_WD,			&factory_assign				},	// PBJ_BOOL_TO_INT
	{	qb_pbj_translate_basic_op,					PBJ_RS_RD_WB,		&factory_set_equal			},	// PBJ_VECTOR_EQUAL
	{	qb_pbj_translate_basic_op,					PBJ_RS_RD_WB,		&factory_set_not_equal		},	// PBJ_VECTOR_NOT_EQUAL
	{	qb_pbj_translate_basic_op,					PBJ_RS_WD,			&factory_any				},	// PBJ_ANY
	{	qb_pbj_translate_basic_op,					PBJ_RS_WD,			&factory_all				},	// PBJ_ALL
	{	qb_pbj_translate_basic_op,					PBJ_RS_RS2_RS3_WD,	&factory_smooth_step		},	// PBJ_SMOOTH_STEP
};

static void qb_pbj_translate_current_instruction(qb_pbj_translator_context *cxt) {
}

static int32_t qb_match_pbj_addresses(qb_pbj_translator_context *cxt, qb_pbj_address *reg1_address, qb_pbj_address *reg2_address) {
	return (reg1_address->register_id == reg2_address->register_id && reg1_address->dimension == reg2_address->dimension 
		 && reg1_address->channel_count == reg2_address->channel_count && reg1_address->all_channels == reg2_address->all_channels);
}

typedef struct qb_pbj_op_info {
	uint32_t opcode;
	qb_pbj_address **src_ptr;
	qb_pbj_address **dst_ptr;
} qb_pbj_op_info;

static uint32_t qb_pbj_find_op_sequence(qb_pbj_translator_context *cxt, qb_pbj_op_info *seq, uint32_t seq_len) {
	// use Boyer-Moore-Horspool to locate the opcode sequence
	uint32_t i, j, last = seq_len - 1, skip;
    uint32_t bad_opcode_skip[PBJ_MAX_OPCODE + 1];
	for(i = 0; i <= PBJ_MAX_OPCODE; i++) {
        bad_opcode_skip[i] = seq_len;
	}
	for(i = 0; i < last; i++) {
		bad_opcode_skip[seq[i].opcode] = last - i;
	}
	for(j = 0; j + last < cxt->pbj_op_count; j += skip) {
		for(i = last; cxt->pbj_ops[j + i].opcode == seq[i].opcode; i--) {
			if(i == 0) {
				// found the opcode sequence--now verify that the addresses are correct
				int32_t correct = TRUE;
				for(i = 0; i < seq_len && correct; i++) {
					qb_pbj_op *pop = &cxt->pbj_ops[j + i];
					if(pop->opcode == PBJ_LOAD_CONSTANT) {
						// check the constant
						if(pop->constant.type == PBJ_TYPE_INT) {
							if(pop->constant.int_value != *((int32_t *) seq[i].src_ptr)) {
								correct = FALSE;
							}
						} else {
							if(pop->constant.float_value != *((float32_t *) seq[i].src_ptr)) {
								correct = FALSE;
							}
						}
					} else {
						if(seq[i].src_ptr) {
							// check the source:
							// if the pointer is null, set it
							// if not, check that the address match
							if(!*seq[i].src_ptr) {
								*seq[i].src_ptr = &pop->source;
							} else {
								if(!qb_match_pbj_addresses(cxt, &pop->source, *seq[i].src_ptr)) {
									correct = FALSE;
								}
							}
						}
					}
					if(seq[i].dst_ptr) {
						// check the destination--same logic
						if(!*seq[i].dst_ptr) {
							*seq[i].dst_ptr = &pop->destination;
						} else {
							if(!qb_match_pbj_addresses(cxt, &pop->destination, *seq[i].dst_ptr)) {
								correct = FALSE;
							}
						}
					}
				}
				return (correct) ? j : (uint32_t) -1;
			}
		}
		skip = bad_opcode_skip[cxt->pbj_ops[j + last].opcode];
    }
    return (uint32_t) -1;
}

static void qb_pbj_substitute_ops(qb_pbj_translator_context *cxt) {
	// look for smoothStep sequence
	qb_pbj_address *x = NULL, *edge0 = NULL, *edge1 = NULL, *result = NULL;
	qb_pbj_address *zero = NULL, *one = NULL, *var1 = NULL, *var2 = NULL, *var3 = NULL; 
	float32_t constants[4] = { 0.0, 1.0, 2.0, 3.0 }; 
	qb_pbj_op_info smooth_step_sequence[18] = { 
		{	PBJ_LOAD_CONSTANT,	(qb_pbj_address **)	&constants[0],	&zero	},
		{	PBJ_LOAD_CONSTANT,	(qb_pbj_address **)	&constants[1],	&one	},
		{	PBJ_COPY,			&x,									&var1	},
		{	PBJ_SUBTRACT,		&edge0,								&var1	},
		{	PBJ_COPY,			&edge1,								&var2	},
		{	PBJ_SUBTRACT,		&edge0,								&var2	},
		{	PBJ_RECIPROCAL,		&var2,								&var3	},
		{	PBJ_MULTIPLY,		&var3,								&var1	},
		{	PBJ_MAX,			&zero,								&var1	},
		{	PBJ_MIN,			&one,								&var1	},
		{	PBJ_LOAD_CONSTANT,	(qb_pbj_address **)	&constants[2],	&var2	},
		{	PBJ_MULTIPLY,		&var1,								&var2	},
		{	PBJ_LOAD_CONSTANT,	(qb_pbj_address **)	&constants[3],	&var3	},
		{	PBJ_SUBTRACT,		&var2,								&var3	},
		{	PBJ_COPY,			&var1,								&var2	},
		{	PBJ_MULTIPLY,		&var1,								&var2	},
		{	PBJ_MULTIPLY,		&var3,								&var2	},
		{	PBJ_COPY,			&var2,								&result	},
	};
	uint32_t start_index, seq_len = sizeof(smooth_step_sequence) / sizeof(qb_pbj_op_info);
	start_index = qb_pbj_find_op_sequence(cxt, smooth_step_sequence, seq_len);
	if(start_index != (uint32_t) -1) {
		qb_pbj_op *ss_pop = &cxt->pbj_ops[start_index];
		qb_pbj_op *ss_data_pop = &cxt->pbj_ops[start_index + 1];
		uint32_t i;

		// change the first and second ops to smoothStep
		ss_pop->opcode = PBJ_SMOOTH_STEP;
		memcpy(&ss_pop->source, x, sizeof(qb_pbj_address));
		memcpy(&ss_pop->destination, result, sizeof(qb_pbj_address));
		ss_data_pop->opcode = PBJ_OP_DATA;
		memcpy(&ss_data_pop->source2, edge0, sizeof(qb_pbj_address));
		memcpy(&ss_data_pop->source3, edge1, sizeof(qb_pbj_address));

		// make the other NOP
		for(i = 2; i < seq_len; i++) {
			qb_pbj_op *nop = &cxt->pbj_ops[start_index + i];
			nop->opcode = PBJ_NOP;
		}
	}
}

static int32_t qb_pbj_is_image(qb_pbj_translator_context *cxt, qb_address *address, uint32_t channel_count) {
	if(address && address->type == QB_TYPE_F32 && address->dimension_count == 3) {
		if(VALUE(U32, address->dimension_addresses[2]) == channel_count) {
			return TRUE;
		}
	}
	return FALSE;
}

static qb_variable * qb_pbj_find_argument(qb_pbj_translator_context *cxt, const char *name) {
	uint32_t i;
	for(i = 0; i < cxt->compiler_context->argument_count; i++) {
		qb_variable *qvar = cxt->compiler_context->variables[i];
		if(strcmp(qvar->name, name) == 0) {
			return qvar;
		}
	}
	return NULL;
}

static qb_variable * qb_pbj_find_input(qb_pbj_translator_context *cxt) {
	qb_variable *input_var = NULL;
	uint32_t i;
	for(i = 0; i < cxt->compiler_context->argument_count; i++) {
		qb_variable *qvar = cxt->compiler_context->variables[i];
		if(!(qvar->flags & QB_VARIABLE_BY_REF)) {
			if(qb_pbj_is_image(cxt, qvar->address, 4) || qb_pbj_is_image(cxt, qvar->address, 3)) {
				if(!input_var) {
					input_var = qvar;
				} else {
					// if there's more than one then the name ought to match
				}
			}
		}
	}
	return input_var;
}

static qb_variable * qb_pbj_find_output(qb_pbj_translator_context *cxt) {
	uint32_t i;
	for(i = 0; i < cxt->compiler_context->argument_count; i++) {
		qb_variable *qvar = cxt->compiler_context->variables[i];
		if(qvar->flags & QB_VARIABLE_BY_REF) {
			if(qb_pbj_is_image(cxt, qvar->address, 4) || qb_pbj_is_image(cxt, qvar->address, 3)) {
				return qvar;
			}
		}
	}
	return NULL;
}

static void qb_map_pbj_arguments(qb_pbj_translator_context *cxt) {
	uint32_t i;
	qb_variable *qvar;

	for(i = 0; i < cxt->pbj_texture_count; i++) {
		qb_pbj_texture *texture = &cxt->pbj_textures[i];
		qvar = qb_pbj_find_argument(cxt, texture->name);
		if(qvar) {
			if(qb_pbj_is_image(cxt, qvar->address, texture->channel_count)) {
				texture->address = qvar->address;
			} else {
				qb_abort("parameter '%s' is not of the correct type", qvar->name);
			} 
		} else {
			// if there's only one input image, then use that one
			qvar = qb_pbj_find_input(cxt);
			if(qvar) {
				texture->address = qvar->address;
			} else {
				qb_abort("input image '%s' must be passed to the function", texture->name);
			}
		}
		// premultiplication is applied to input images with alpha channel
		if(VALUE(U32, texture->address->dimension_addresses[2]) == 4) {
			texture->address->flags &= ~QB_ADDRESS_READ_ONLY;
		}
	}

	for(i = 0; i < cxt->pbj_parameter_count; i++) {
		qb_pbj_parameter *parameter = &cxt->pbj_parameters[i];
		if(parameter == cxt->pbj_out_pixel) {
			qvar = qb_pbj_find_output(cxt);
			if(qvar) {
				parameter->address = qvar->address;
			} else {
				qb_abort("a parameter must be passed by reference to the function to receive the result");
			} 
		} else if(parameter == cxt->pbj_out_coord) {
			// this is actually set within the function 
		} else {
			qvar = qb_pbj_find_argument(cxt, parameter->name);
			if(qvar) {
				if(!parameter->input_size_name) {
					uint32_t parameter_type = (parameter->destination.register_id & PBJ_REGISTER_INT) ? QB_TYPE_I32 : QB_TYPE_F32;
					if(qvar->address->type == parameter_type) {
						uint32_t parameter_size, element_count;
						if(parameter->destination.dimension > 1) {
							parameter_size = pbj_matrix_sizes[parameter->destination.dimension];
						} else {
							parameter_size = parameter->destination.channel_count;
						}
						if(SCALAR(qvar->address)) {
							element_count = 1;
						} else {
							element_count = ARRAY_SIZE(qvar->address);
						}
						if(element_count == parameter_size) {
							parameter->address = qvar->address;
						} else {
							qb_abort("parameter '%s' is not of the correct size", qvar->name);
						}
					} else {
						qb_abort("parameter '%s' is not of the correct type", qvar->name);
					}
				} else {
					qb_abort("parameter '%s' is obtained from the input image and cannot be passed separately", parameter->name);
				}

			} else {
				// see if it has a default value
				if(!parameter->default_value.type) {
					qb_abort("parameter '%s' does not have a default value and must be passed to the function", parameter->name);
				}
			}
		}
	}
}

void qb_translate_pbj_instructions(qb_pbj_translator_context *cxt) {
	// map function arguments to PB kernel parameters
	qb_map_pbj_arguments(cxt);

}

void qb_initialize_pbj_translator_context(qb_pbj_translator_context *cxt, qb_compiler_context *compiler_cxt TSRMLS_DC) {
	memset(cxt, 0, sizeof(qb_pbj_translator_context));
	cxt->compiler_context = compiler_cxt;

	SAVE_TSRMLS
}

void qb_free_pbj_translator_context(qb_pbj_translator_context *cxt) {
}

uint32_t pbj_matrix_sizes[] = { 0, 1, 4, 12, 16 };
